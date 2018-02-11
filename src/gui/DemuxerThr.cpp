#include <DemuxerThr.hpp>

#include <PlayClass.hpp>
#include <AVThread.hpp>
#include <Writer.hpp>
#include <Main.hpp>

#include <Functions.hpp>
#include <SubsDec.hpp>
#include <Demuxer.hpp>
#include <Decoder.hpp>
#include <Reader.hpp>

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>

#include <math.h>

static inline bool getCurrentPlaying(int stream, const QList< StreamInfo * > &streamsInfo, const StreamInfo *streamInfo)
{
	return (stream > -1 && streamsInfo.count() > stream) && streamsInfo[stream] == streamInfo;
}
static QString getWriterName(AVThread *avThr)
{
	QString decName;
	if (avThr)
	{
		decName = avThr->dec->name();
		if (!decName.isEmpty())
			decName += ", ";
	}
	return (avThr && avThr->writer) ? " - <i>" + decName + avThr->writer->name() + "</i>" : QString();
}

static inline QString getCoverFile(const QString &title, const QString &artist, const QString &album)
{
	return QMPlay2Core.getSettingsDir() + "Covers/" + QCryptographicHash::hash((album.isEmpty() ? title.toUtf8() : album.toUtf8()) + artist.toUtf8(), QCryptographicHash::Md5).toHex();
}

class BufferInfo
{
public:
	inline BufferInfo() :
		remainingDuration(0.0), backwardDuration(0.0),
		firstPacketTime(-1), lastPacketTime(-1),
		remainingBytes(0), backwardBytes(0)
	{}

	inline void reset()
	{
		*this = BufferInfo();
	}

	double remainingDuration, backwardDuration;
	qint32 firstPacketTime, lastPacketTime;
	qint64 remainingBytes, backwardBytes;
};

/**/

DemuxerThr::DemuxerThr(PlayClass &playC) :
	playC(playC),
	url(playC.url),
	err(false), demuxerReady(false), hasCover(false),
	skipBufferSeek(false), localStream(true), unknownLength(true),
	updateBufferedTime(0.0)
{
	connect(this, SIGNAL(stopVADec()), this, SLOT(stopVADecSlot()));
}

QByteArray DemuxerThr::getCoverFromStream() const
{
	return demuxer ? demuxer->image(true) : QByteArray();
}

void DemuxerThr::loadImage()
{
	if (isDemuxerReady())
	{
		const QByteArray demuxerImage = demuxer->image();
		QImage img = QImage::fromData(demuxerImage);
		if (!img.isNull())
			emit QMPlay2Core.coverDataFromMediaFile(demuxerImage);
		else
		{
			if (img.isNull() && url.startsWith("file://") && QMPlay2Core.getSettings().getBool("ShowDirCovers")) //Ładowanie okładki z katalogu
			{
				const QString directory = Functions::filePath(url.mid(7));
				foreach (const QString &cover, QDir(directory).entryList(QStringList() << "cover" << "cover.*" << "folder" << "folder.*", QDir::Files))
				{
					const QString coverPath = directory + cover;
					img = QImage(coverPath);
					if (!img.isNull())
					{
						emit QMPlay2Core.coverFile(coverPath);
						break;
					}
				}
			}
			if (img.isNull() && !artist.isEmpty() && (!title.isEmpty() || !album.isEmpty())) //Ładowanie okładki z cache
			{
				QString coverPath = getCoverFile(title, artist, album);
				img = QImage(coverPath);
				if (!img.isNull())
					emit QMPlay2Core.coverFile(coverPath);
			}
		}
		hasCover = !img.isNull();
		emit playC.updateImage(img);
	}
}

void DemuxerThr::seek(bool doDemuxerSeek)
{
	if (doDemuxerSeek)
		seekMutex.lock();
	else if (skipBufferSeek || !seekMutex.tryLock())
		return;
	if (playC.seekTo >= 0 || playC.seekTo == SEEK_STREAM_RELOAD)
	{
		AVThread *aThr = (AVThread *)playC.aThr, *vThr = (AVThread *)playC.vThr;

		emit playC.chText(tr("Seeking"));
		playC.canUpdatePos = false;

		bool seekInBuffer = !skipBufferSeek;
		if (playC.seekTo == SEEK_STREAM_RELOAD) //po zmianie strumienia audio, wideo lub napisów lub po ponownym uruchomieniu odtwarzania
		{
			playC.seekTo = playC.pos;
			seekInBuffer = false;
		}

		const bool backward = playC.seekTo < (int)playC.pos;
		bool flush = false, aLocked = false, vLocked = false;

		skipBufferSeek = false;

		if (seekInBuffer && (!localStream || !backward))
		{
			playC.vPackets.lock();
			playC.aPackets.lock();
			playC.sPackets.lock();
			if
			(
				(playC.vPackets.packetsCount() || playC.aPackets.packetsCount()) &&
				playC.vPackets.seekTo(playC.seekTo, backward) &&
				playC.aPackets.seekTo(playC.seekTo, backward) &&
				playC.sPackets.seekTo(playC.seekTo, backward)
			)
			{
				doDemuxerSeek = false;
				flush = true;
				time = Functions::gettime() - updateBufferedTime; //zapewni, że updateBuffered będzie na "true";
				if (aThr)
					aLocked = aThr->lock();
				if (vThr)
					vLocked = vThr->lock();
			}
			else
			{
				emit playC.updateBufferedRange(-1, -1);
				updateBufferedTime = 0.0;
			}
			playC.vPackets.unlock();
			playC.aPackets.unlock();
			playC.sPackets.unlock();
		}

		if (doDemuxerSeek && demuxer->seek(playC.seekTo, backward))
			flush = true;
		else if (!doDemuxerSeek && !flush)
		{
			skipBufferSeek = true;
			if (!localStream && !unknownLength)
				demuxer->abort(); //Abort only the Demuxer, not IOController
		}

		if (flush)
		{
			playC.endOfStream = false;
			if (doDemuxerSeek)
				clearBuffers();
			else
				playC.flushAssEvents();
			if (!aLocked && aThr)
				aLocked = aThr->lock();
			if (!vLocked && vThr)
				vLocked = vThr->lock();
			playC.skipAudioFrame = playC.audio_current_pts = 0.0;
			playC.flushVideo = playC.flushAudio = true;
			if (playC.pos < 0.0) //skok po rozpoczęciu odtwarzania po uruchomieniu programu
                emit playC.updatePos((double)playC.seekTo); //uaktualnia suwak na pasku do wskazanej pozycji
			if (aLocked)
				aThr->unlock();
			if (vLocked)
				vThr->unlock();
		}

		if (!skipBufferSeek)
		{
			playC.canUpdatePos = true;
			playC.seekTo = SEEK_NOWHERE;
			if (!playC.paused)
				emit playC.chText(tr("Playback"));
			else
				playC.paused = false;
		}
	}
	seekMutex.unlock();
}

void DemuxerThr::stop()
{
	ioCtrl.abort();
	demuxer.abort();
}
void DemuxerThr::end()
{
	bool endMutexLocked = false;

	stopVAMutex.lock();
	if (currentThread() != qApp->thread())
	{
		endMutex.lock(); //Zablokuje główny wątek do czasu zniszczenia demuxer'a
		endMutexLocked = true;
		emit stopVADec(); //to wykonuje się w głównym wątku
		stopVAMutex.lock(); //i czeka na koniec wykonywania
		stopVAMutex.unlock();
	}
	else //wywołane z głównego wątku
		stopVADecSlot();

	demuxer.clear();

	if (endMutexLocked)
		endMutex.unlock(); //Jeżeli był zablokowany, odblokuje mutex
}

bool DemuxerThr::load(bool canEmitInfo)
{
	playC.loadMutex.lock();
	emit load(demuxer.rawPtr()); //to wykonuje się w głównym wątku
	playC.loadMutex.lock(); //i czeka na koniec wykonywania
	playC.loadMutex.unlock();
	if (canEmitInfo)
		emitInfo();
	return playC.audioStream >= 0 || playC.videoStream >= 0;
}

void DemuxerThr::run()
{
	emit playC.chText(tr("Opening"));
	emit playC.setCurrentPlaying();

	emit QMPlay2Core.busyCursor();
	Functions::getDataIfHasPluginPrefix(url, &url, &name, NULL, &ioCtrl);
	emit QMPlay2Core.restoreCursor();
	if (ioCtrl.isAborted())
		return end();

	if (!Demuxer::create(url, demuxer) || demuxer.isAborted())
	{
		if (!demuxer.isAborted() && !demuxer)
		{
			QMPlay2Core.logError(tr("Cannot open") + ": " + url.remove("file://"));
			emit playC.updateCurrentEntry(QString(), -1);
			err = true;
		}
		return end();
	}

	QStringList filter;
	filter << "*.ass" << "*.ssa";
	foreach (const QString &ext, SubsDec::extensions())
		filter << "*." + ext;
	foreach (StreamInfo *streamInfo, demuxer->streamsInfo())
		if (streamInfo->type == QMPLAY2_TYPE_VIDEO) //napisów szukam tylko wtedy, jeżeli jest strumień wideo
		{
			QString directory = Functions::filePath(url.mid(7));
			QString fName = Functions::fileName(url, false).replace('_', ' ');
			foreach (const QString &subsFile, QDir(directory).entryList(filter, QDir::Files))
			{
				if (Functions::fileName(subsFile, false).replace('_', ' ').contains(fName, Qt::CaseInsensitive))
				{
					const QString fileSubsUrl = Functions::Url(directory + subsFile);
					if (!playC.fileSubsList.contains(fileSubsUrl))
						playC.fileSubsList += fileSubsUrl;
				}
			}
			break;
		}

	err = !load(false);
	if (err || demuxer.isAborted())
		return end();

	updatePlayingName = name.isEmpty() ? Functions::fileName(url, false) : name;

	if (playC.videoStream > -1)
		playC.frame_last_delay = getFrameDelay();

	/* ReplayGain */
	float gain_db = 0.0f, peak = 1.0f;
	if (!QMPlay2Core.getSettings().getBool("ReplayGain/Enabled") || !demuxer->getReplayGain(QMPlay2Core.getSettings().getBool("ReplayGain/Album"), gain_db, peak))
		playC.replayGain = 1.0;
	else
	{
		playC.replayGain = pow(10.0, gain_db / 20.0) * pow(10.0, QMPlay2Core.getSettings().getDouble("ReplayGain/Preamp") / 20.0);
		if (QMPlay2Core.getSettings().getBool("ReplayGain/PreventClipping") && peak * playC.replayGain > 1.0)
			playC.replayGain = 1.0 / peak;
	}

	if ((unknownLength = demuxer->length() < 0.0))
		updateBufferedSeconds = false;

	emit playC.updateLength(unknownLength ? 0.0 : demuxer->length());
	emit playC.chText(tr("Playback"));
	emit playC.playStateChanged(true);

	localStream = demuxer->localStream();
	time = localStream ? 0.0 : Functions::gettime();

	int forwardPackets = demuxer->dontUseBuffer() ? 1 : (localStream ? minBuffSizeLocal : minBuffSizeNetwork), backwardPackets;
	bool paused = false, demuxerPaused = false, waitingForFillBufferB = false;
	BufferInfo bufferInfo;
	int vS, aS;

	demuxerReady = true;

	updateCoverAndPlaying();
	connect(&QMPlay2Core, SIGNAL(updateCover(const QString &, const QString &, const QString &, const QByteArray &)), this, SLOT(updateCover(const QString &, const QString &, const QString &, const QByteArray &)));

	if (forwardPackets == 1 || localStream || unknownLength)
		PacketBuffer::setBackwardPackets((backwardPackets = 0));
	else
	{
		int percent = 25;
		switch (QMPlay2Core.getSettings().getUInt("BackwardBuffer"))
		{
			case 0:
				percent = 10;
				break;
			case 2:
				percent = 50;
				break;
		}
		PacketBuffer::setBackwardPackets((backwardPackets = forwardPackets * percent / 100));
		forwardPackets -= backwardPackets;
	}

	if (!localStream)
		setPriority(QThread::LowPriority); //Network streams should have low priority, because slow single core CPUs have problems with smooth video playing during buffering

	while (!demuxer.isAborted())
	{
		seek();

		AVThread *aThr = (AVThread *)playC.aThr, *vThr = (AVThread *)playC.vThr;
		err = (aThr && aThr->writer && !aThr->writer->readyWrite()) || (vThr && vThr->writer && !vThr->writer->readyWrite());
		if (demuxer.isAborted() || err)
			break;

		if (playC.paused)
		{
			if (!paused)
			{
				paused = true;
				emit playC.chText(tr("Paused"));
				emit playC.playStateChanged(false);
				playC.emptyBufferCond.wakeAll();
			}
		}
		else if (paused)
		{
			paused = demuxerPaused = false;
			emit playC.chText(tr("Playback"));
			emit playC.playStateChanged(true);
			playC.emptyBufferCond.wakeAll();
		}

		bool updateBuffered = localStream ? false : (Functions::gettime() - time >= updateBufferedTime);
		getAVBuffersSize(vS, aS, (updateBuffered || playC.waitForData) ? &bufferInfo : NULL);
		if (playC.endOfStream && !vS && !aS && canBreak(aThr, vThr))
			break;
		if (updateBuffered)
		{
			if (updateBufferedSeconds)
				emit playC.updateBufferedRange(bufferInfo.firstPacketTime, bufferInfo.lastPacketTime);
			emit playC.updateBuffered(bufferInfo.backwardBytes, bufferInfo.remainingBytes, bufferInfo.backwardDuration, bufferInfo.remainingDuration);
			if (demuxer->metadataChanged())
				updateCoverAndPlaying();
			waitingForFillBufferB = true;
			if (updateBufferedTime < 1.0)
				updateBufferedTime += 0.25;
			time = Functions::gettime();
		}
		else if (localStream && demuxer->metadataChanged())
			updateCoverAndPlaying();

		if (!localStream && !playC.waitForData && !playC.endOfStream && playIfBuffered > 0.0 && emptyBuffers(vS, aS))
		{
			playC.waitForData = true;
			updateBufferedTime = 0.0;
		}
		else if
		(
			playC.waitForData &&
			(
				playC.endOfStream                                ||
				bufferedAllPackets(vS, aS, forwardPackets)       || //bufor pełny
				(bufferInfo.remainingDuration >= playIfBuffered) ||
				(qFuzzyIsNull(bufferInfo.remainingDuration) && bufferedAllPackets(vS, aS, 2)) //bufor ma conajmniej 2 paczki, a nadal bez informacji o długości w [s]
			)
		)
		{
			playC.waitForData = false;
			if (!paused)
				playC.emptyBufferCond.wakeAll();
		}

		if (playC.endOfStream || bufferedAllPackets(vS, aS, forwardPackets))
		{
			if (paused && !demuxerPaused)
			{
				demuxerPaused = true;
				demuxer->pause();
			}

			//po zakończeniu buforowania należy odświeżyć informacje o buforowaniu
			if (paused && !waitingForFillBufferB && !playC.fillBufferB)
			{
				time = Functions::gettime() - updateBufferedTime; //zapewni, że updateBuffered będzie na "true";
				waitingForFillBufferB = true;
				continue;
			}

			bool loadError = false, first = true;
			while (!playC.fillBufferB)
			{
				qApp->processEvents();
				if (mustReloadStreams() && !load())
				{
					loadError = true;
					break;
				}
				if (playC.seekTo == SEEK_STREAM_RELOAD)
					break;
				if (!first)
					msleep(15);
				else
				{
					msleep(1);
					first = false;
				}
			}
			if (loadError)
				break;
			playC.fillBufferB = false;
			continue;
		}
		waitingForFillBufferB = false;

		Packet packet;
		int streamIdx = -1;
		if (demuxer->read(packet, streamIdx))
		{
			qApp->processEvents();

			if (mustReloadStreams() && !load())
				break;

			if (streamIdx < 0 || playC.seekTo == SEEK_STREAM_RELOAD)
				continue;

			if (streamIdx == playC.audioStream)
				playC.aPackets.put(packet);
			else if (streamIdx == playC.videoStream)
				playC.vPackets.put(packet);
			else if (streamIdx == playC.subtitlesStream)
				playC.sPackets.put(packet);

			if (!paused && !playC.waitForData)
				playC.emptyBufferCond.wakeAll();
		}
		else if (!skipBufferSeek)
		{
			getAVBuffersSize(vS, aS);
			if (vS || aS || !canBreak(aThr, vThr))
			{
				playC.endOfStream = true;
				if (!localStream)
					time = Functions::gettime() - updateBufferedTime; //zapewni, że updateBuffered będzie na "true";
			}
			else
				break;
		}
	}

	emit QMPlay2Core.updatePlaying(false, title, artist, album, demuxer->length(), false, updatePlayingName);

	playC.endOfStream = playC.canUpdatePos = false; //to musi tu być!
	end();
}

void DemuxerThr::updateCoverAndPlaying()
{
	foreach (const QMPlay2Tag &tag, demuxer->tags()) //wczytywanie tytuły, artysty i albumu
	{
		const QMPlay2Tags tagID = StreamInfo::getTag(tag.first);
		switch (tagID)
		{
			case QMPLAY2_TAG_TITLE:
				title = tag.second;
				break;
			case QMPLAY2_TAG_ARTIST:
				artist = tag.second;
				break;
			case QMPLAY2_TAG_ALBUM:
				album = tag.second;
				break;
			default:
				break;
		}
	}
	const bool showCovers = QMPlay2Core.getSettings().getBool("ShowCovers");
	if (showCovers)
		loadImage();
	emitInfo();
	emit QMPlay2Core.updatePlaying(true, title, artist, album, demuxer->length(), showCovers && !hasCover, updatePlayingName);
}

static void printOtherInfo(const QList< QMPlay2Tag > &other_info, QString &str)
{
	foreach (const QMPlay2Tag &tag, other_info)
		if (!tag.second.isEmpty())
		{
			QString value = tag.second;
			if (tag.first.toInt() == QMPLAY2_TAG_LANGUAGE)
				value = QMPlay2GUI.languages.value(value, tag.second).toLower();
			str += "<li><b>" + StreamInfo::getTagName(tag.first).toLower() + ":</b> " + value + "</li>";
		}
}
void DemuxerThr::addSubtitleStream(bool currentPlaying, QString &subtitlesStreams, int i, int subtitlesStreamCount, const QString &streamName, const QString &codecName, const QString &title, const QList< QMPlay2Tag > &other_info)
{
	subtitlesStreams += "<ul style='margin-top: 0px; margin-bottom: 0px;'>";
	if (currentPlaying)
		subtitlesStreams += "<u>";
	else
		subtitlesStreams += "<a style='text-decoration: none; color: black;' href='stream:" + streamName + QString::number(i) + "'>";
	subtitlesStreams += "<li><b>" + tr("Stream") + " " + QString::number(subtitlesStreamCount) + "</b></li>";
	if (currentPlaying)
		subtitlesStreams += "</u>";
	else
		subtitlesStreams += "</a>";
	subtitlesStreams += "<ul>";
	if (!title.isEmpty())
		subtitlesStreams += "<li><b>" + tr("title") + ":</b> " + title + "</li>";
	if (streamName == "fileSubs")
		subtitlesStreams += "<li><b>" + tr("loaded from file") + "</b></li>";
	if (!codecName.isEmpty())
		subtitlesStreams += "<li><b>" + tr("format") + ":</b> " + codecName + "</li>";
	printOtherInfo(other_info, subtitlesStreams);
	subtitlesStreams += "</ul></ul>";
}
void DemuxerThr::emitInfo()
{
	QString info;

	QString formatTitle = demuxer->title();
	if (formatTitle.isEmpty())
	{
		if (!name.isEmpty())
			info += "<b>" + tr("Title") + ":</b> " + name;
		formatTitle = name;
	}

	bool nameOrDescr = false;
	foreach (const QMPlay2Tag &tag, demuxer->tags())
		if (!tag.first.isEmpty())
		{
			QString txt = tag.second;
			txt.replace("<", "&#60;"); //Don't recognize as HTML tag
			if (tag.first == "0" || tag.first == "1") //Name and description
			{
				info += "<b>" + txt + "</b><br/>";
				nameOrDescr = true;
			}
			else
			{
				if (nameOrDescr)
				{
					info += "<br/>";
					nameOrDescr = false;
				}
				info += "<b>" + StreamInfo::getTagName(tag.first) + ":</b> " + txt + "<br/>";
			}
		}

	if (!info.isEmpty())
		info += "<br/>";

	QString realUrl;
	if (!Functions::splitPrefixAndUrlIfHasPluginPrefix(url, NULL, &realUrl, NULL))
		realUrl = url;
	if (!realUrl.startsWith("file://"))
		info += "<b>" + tr("Address") + ":</b> " + realUrl + "<br>";
	else
	{
		const QString pth = realUrl.right(realUrl.length() - 7);
		info += "<b>" + tr("File path") + ": </b> " + Functions::filePath(pth) + "<br/>";
		info += "<b>" + tr("File name") + ": </b> " + Functions::fileName(pth) + "<br/>";
	}

	if (demuxer->bitrate() > 0)
		info += "<b>" + tr("Bitrate") + ":</b> " + QString::number(demuxer->bitrate()) + "kbps<br/>";
	info += "<b>" + tr("Format") + ":</b> " + demuxer->name();

	if (!demuxer->image().isNull())
		info += "<br/><br/><a href='save_cover'>" + tr("Save cover picture") + "</a>";

	bool once = false;
	int chapterCount = 0;
	foreach (const ChapterInfo &chapter, demuxer->getChapters())
	{
		if (!once)
		{
			info += "<p style='margin-bottom: 0px;'><b><big>" + tr("Chapters") + ":</big></b></p>";
			once = true;
		}
		info += "<ul style='margin-top: 0px; margin-bottom: 0px;'>";
		info += "<li><a href='seek:" + QString::number(chapter.start) + "'>" + (chapter.title.isEmpty() ? tr("Chapter") + " " + QString::number(++chapterCount) : chapter.title) + "</a></li>";
		info += "</ul>";
	}

	bool videoPlaying = false, audioPlaying = false;

	const QList< StreamInfo * > streamsInfo = demuxer->streamsInfo();
	QString videoStreams, audioStreams, subtitlesStreams, attachmentStreams;
	int videoStreamCount = 0, audioStreamCount = 0, subtitlesStreamCount = 0, i = 0;
	foreach (StreamInfo *streamInfo, streamsInfo)
	{
		switch (streamInfo->type)
		{
			case QMPLAY2_TYPE_VIDEO:
			{
				const bool currentPlaying = getCurrentPlaying(playC.videoStream, streamsInfo, streamInfo);
				videoStreams += "<ul style='margin-top: 0px; margin-bottom: 0px;'><li>";
				if (currentPlaying)
				{
					videoPlaying = true;
					videoStreams += "<u>";
				}
				else
					videoStreams += "<a style='text-decoration: none; color: black;' href='stream:video" + QString::number(i) + "'>";
				videoStreams += "<b>" + tr("Stream") + " " + QString::number(++videoStreamCount) + "</b>";
				if (currentPlaying)
					videoStreams += "</u>" + getWriterName((AVThread *)playC.vThr);
				else
					videoStreams += "</a>";
				videoStreams += "</li><ul>";
				if (!streamInfo->title.isEmpty())
					videoStreams += "<li><b>" + tr("title") + ":</b> " + streamInfo->title + "</li>";
				if (!streamInfo->codec_name.isEmpty())
					videoStreams += "<li><b>" + tr("codec") + ":</b> " + streamInfo->codec_name + "</li>";
				videoStreams += "<li><b>" + tr("size") + ":</b> " + QString::number(streamInfo->W) + "x" + QString::number(streamInfo->H) + "</li>";
				videoStreams += "<li><b>" + tr("aspect ratio") + ":</b> " + QString::number(streamInfo->aspect_ratio) + "</li>";
				if (streamInfo->FPS)
					videoStreams += "<li><b>" + tr("FPS") + ":</b> " + QString::number(streamInfo->FPS) + "</li>";
				if (streamInfo->bitrate)
					videoStreams += "<li><b>" + tr("bitrate") + ":</b> " + QString::number(streamInfo->bitrate / 1000) + "kbps</li>";
				printOtherInfo(streamInfo->other_info, videoStreams);
				videoStreams += "</ul></ul>";
			} break;
			case QMPLAY2_TYPE_AUDIO:
			{
				const bool currentPlaying = getCurrentPlaying(playC.audioStream, streamsInfo, streamInfo);
				audioStreams += "<ul style='margin-top: 0px; margin-bottom: 0px;'><li>";
				if (currentPlaying)
				{
					audioPlaying = true;
					audioStreams += "<u>";
				}
				else
					audioStreams += "<a style='text-decoration: none; color: black;' href='stream:audio" + QString::number(i) + "'>";
				audioStreams += "<b>" + tr("Stream") + " " + QString::number(++audioStreamCount) + "</b>";
				if (currentPlaying)
					audioStreams += "</u>" + getWriterName((AVThread *)playC.aThr);
				else
					audioStreams += "</a>";
				audioStreams += "</li><ul>";
				if (!streamInfo->title.isEmpty())
					audioStreams += "<li><b>" + tr("title") + ":</b> " + streamInfo->title + "</li>";
				if (!streamInfo->artist.isEmpty())
					audioStreams += "<li><b>" + tr("artist") + ":</b> " + streamInfo->artist + "</li>";
				if (!streamInfo->codec_name.isEmpty())
					audioStreams += "<li><b>" + tr("codec") + ":</b> " + streamInfo->codec_name + "</li>";
				audioStreams += "<li><b>" + tr("sample rate") + ":</b> " + QString::number(streamInfo->sample_rate) + "Hz</li>";

				QString channels;
				if (streamInfo->channels == 1)
					channels = tr("mono");
				else if (streamInfo->channels == 2)
					channels = tr("stereo");
				else
					channels = QString::number(streamInfo->channels);
				audioStreams += "<li><b>" + tr("channels") + ":</b> " + channels + "</li>";

				if (streamInfo->bitrate)
					audioStreams += "<li><b>" + tr("bitrate") + ":</b> " + QString::number(streamInfo->bitrate / 1000) + "kbps</li>";
				printOtherInfo(streamInfo->other_info, audioStreams);
				audioStreams += "</ul></ul>";
			} break;
			case QMPLAY2_TYPE_SUBTITLE:
				addSubtitleStream(getCurrentPlaying(playC.subtitlesStream, streamsInfo, streamInfo), subtitlesStreams, i, ++subtitlesStreamCount, "subtitles", streamInfo->codec_name, streamInfo->title, streamInfo->other_info);
				break;
			case QMPLAY2_TYPE_ATTACHMENT:
			{
				attachmentStreams += "<ul style='margin-top: 0px; margin-bottom: 0px;'>";
				attachmentStreams += "<li><b>" + streamInfo->title + "</b> - " + Functions::sizeString(streamInfo->data.size()) + "</li>";
				attachmentStreams += "</ul>";
			} break;
			default:
				break;
		}
		++i;
	}
	i = 0;
	foreach (const QString &fName, playC.fileSubsList)
		addSubtitleStream(fName == playC.fileSubs, subtitlesStreams, i++, ++subtitlesStreamCount, "fileSubs", QString(), Functions::fileName(fName));

	if (!videoStreams.isEmpty())
		info += "<p style='margin-bottom: 0px;'><b><big>" + tr("Video streams") + ":</big></b></p>" + videoStreams;
	if (!audioStreams.isEmpty())
		info += "<p style='margin-bottom: 0px;'><b><big>" + tr("Audio streams") + ":</big></b></p>" + audioStreams;
	if (!subtitlesStreams.isEmpty())
		info += "<p style='margin-bottom: 0px;'><b><big>" + tr("Subtitles streams") + ":</big></b></p>" + subtitlesStreams;
	if (!attachmentStreams.isEmpty())
		info += "<p style='margin-bottom: 0px;'><b><big>" + tr("Attached files") + ":</big></b></p>" + attachmentStreams;

	emit playC.setInfo(info, videoPlaying, audioPlaying);
	emit playC.updateCurrentEntry(formatTitle, demuxer->length());
	emit playC.updateWindowTitle(formatTitle.isEmpty() ? Functions::fileName(url, false) : formatTitle);
}

bool DemuxerThr::mustReloadStreams()
{
	if
	(
		playC.reload ||
		(playC.choosenAudioStream     > -1 && playC.choosenAudioStream     != playC.audioStream    ) ||
		(playC.choosenVideoStream     > -1 && playC.choosenVideoStream     != playC.videoStream    ) ||
		(playC.choosenSubtitlesStream > -1 && playC.choosenSubtitlesStream != playC.subtitlesStream)
	)
	{
		if (playC.frame_last_delay <= 0.0 && playC.videoStream > -1)
			playC.frame_last_delay = getFrameDelay();
		playC.reload = true;
		return true;
	}
	return false;
}
bool DemuxerThr::bufferedAllPackets(int vS, int aS, int p)
{
	return
	(
		(playC.vThr && vS >= p && playC.aThr && aS >= p) ||
		(!playC.vThr && playC.aThr && aS >= p)           ||
		(playC.vThr && !playC.aThr && vS >= p)
	);
}
bool DemuxerThr::emptyBuffers(int vS, int aS)
{
	return (playC.vThr && playC.aThr && (vS <= 1 || aS <= 1)) || (!playC.vThr && playC.aThr && aS <= 1) || (playC.vThr && !playC.aThr && vS <= 1);
}
bool DemuxerThr::canBreak(const AVThread *avThr1, const AVThread *avThr2)
{
	return (!avThr1 || avThr1->isWaiting()) && (!avThr2 || avThr2->isWaiting());
}
void DemuxerThr::getAVBuffersSize(int &vS, int &aS, BufferInfo *bufferInfo)
{
	if (bufferInfo)
		bufferInfo->reset();

	playC.vPackets.lock();
	if (playC.vPackets.isEmpty())
		vS = 0;
	else
	{
		vS = playC.vPackets.remainingPacketsCount();
		if (bufferInfo)
		{
			bufferInfo->backwardBytes  += playC.vPackets.backwardBytes();
			bufferInfo->remainingBytes += playC.vPackets.remainingBytes();

			bufferInfo->backwardDuration  = playC.vPackets.backwardDuration();
			bufferInfo->remainingDuration = playC.vPackets.remainingDuration();

			bufferInfo->firstPacketTime = floor(playC.vPackets.firstPacketTime());
			bufferInfo->lastPacketTime  = ceil (playC.vPackets.lastPacketTime() );
		}
	}
	playC.vPackets.unlock();

	playC.aPackets.lock();
	if (playC.aPackets.isEmpty())
		aS = 0;
	else
	{
		aS = playC.aPackets.remainingPacketsCount();
		if (bufferInfo)
		{
			const qint32 firstAPacketTime = floor(playC.aPackets.firstPacketTime()), lastAPacketTime = ceil(playC.aPackets.lastPacketTime());

			bufferInfo->backwardBytes  += playC.aPackets.backwardBytes();
			bufferInfo->remainingBytes += playC.aPackets.remainingBytes();

			bufferInfo->backwardDuration  = bufferInfo->backwardDuration  > 0.0 ? qMax(bufferInfo->backwardDuration,  playC.aPackets.backwardDuration() ) : playC.aPackets.backwardDuration();
			bufferInfo->remainingDuration = bufferInfo->remainingDuration > 0.0 ? qMin(bufferInfo->remainingDuration, playC.aPackets.remainingDuration()) : playC.aPackets.remainingDuration();

			bufferInfo->firstPacketTime = bufferInfo->firstPacketTime >= 0 ? qMax(bufferInfo->firstPacketTime, firstAPacketTime) : firstAPacketTime;
			bufferInfo->lastPacketTime  = bufferInfo->lastPacketTime  >= 0 ? qMin(bufferInfo->lastPacketTime,  lastAPacketTime ) : lastAPacketTime;
		}
	}
	playC.aPackets.unlock();

	if (bufferInfo)
	{
		//niedokładność double
		if (bufferInfo->backwardDuration < 0.0)
			bufferInfo->backwardDuration = 0.0;
		if (bufferInfo->remainingDuration < 0.0)
			bufferInfo->remainingDuration = 0.0;
	}
}
void DemuxerThr::clearBuffers()
{
	playC.vPackets.clear();
	playC.aPackets.clear();
	playC.clearSubtitlesBuffer();
}

double DemuxerThr::getFrameDelay() const
{
	return 1.0 / demuxer->streamsInfo().at(playC.videoStream)->FPS;
}

void DemuxerThr::stopVADecSlot()
{
	clearBuffers();

	playC.stopVDec();
	playC.stopADec();

	stopVAMutex.unlock();

	endMutex.lock(); //Czeka do czasu zniszczenia demuxer'a - jeżeli wcześniej mutex był zablokowany (wykonał się z wątku)
	endMutex.unlock(); //odblokowywuje mutex
}
void DemuxerThr::updateCover(const QString &title, const QString &artist, const QString &album, const QByteArray &cover)
{
	const QImage coverImg = QImage::fromData(cover);
	if (!coverImg.isNull())
	{
		if (this->title == title && this->artist == artist && this->album == album)
			emit playC.updateImage(coverImg);
		QDir dir(QMPlay2Core.getSettingsDir());
		dir.mkdir("Covers");
		QFile f(getCoverFile(title, artist, album));
		if (f.open(QFile::WriteOnly))
		{
			f.write(cover);
			f.close();
			emit QMPlay2Core.coverFile(f.fileName());
		}
	}
}
