#include <FFReader.hpp>
#include <FFCommon.hpp>

extern "C"
{
	#include <libavformat/avio.h>
}

static int interruptCB(bool &aborted)
{
	return aborted;
}

/**/

FFReader::FFReader() :
	avioCtx(NULL),
	aborted(false), paused(false), canRead(false)
{}

bool FFReader::readyRead() const
{
	return canRead && !aborted;
}
bool FFReader::canSeek() const
{
	return avioCtx->seekable;
}

bool FFReader::seek(qint64 pos, int wh)
{
	return avio_seek(avioCtx, pos, wh) >= 0;
}
QByteArray FFReader::read(qint64 size)
{
	QByteArray arr;
	arr.resize(size);
	if (paused)
	{
		avio_pause(avioCtx, false);
		paused = false;
	}
	int ret = avio_read(avioCtx, (quint8 *)arr.data(), arr.size());
	if (ret > 0)
	{
		if (arr.size() > ret)
			arr.resize(ret);
		return arr;
	}
	canRead = false;
	return QByteArray();
}
void FFReader::pause()
{
	avio_pause(avioCtx, true);
	paused = true;
}
bool FFReader::atEnd() const
{
#if LIBAVFORMAT_VERSION_MAJOR >= 56
	return avio_feof(avioCtx);
#else
	return url_feof(avioCtx);
#endif
}
void FFReader::abort()
{
	aborted = true;
}

qint64 FFReader::size() const
{
	return avio_size(avioCtx);
}
qint64 FFReader::pos() const
{
	return avio_tell(avioCtx);
}
QString FFReader::name() const
{
	return FFReaderName;
}

bool FFReader::open()
{
	AVDictionary *options = NULL;
	const QString url = FFCommon::prepareUrl(getUrl(), options);
	AVIOInterruptCB interruptCB = {(int(*)(void*))::interruptCB, &aborted};
	if (avio_open2(&avioCtx, url.toUtf8(), AVIO_FLAG_READ, &interruptCB, &options) >= 0)
		return (canRead = true);
	return false;
}

FFReader::~FFReader()
{
	avio_close(avioCtx);
}
