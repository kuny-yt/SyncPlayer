#ifndef VIDEOWRITER_HPP
#define VIDEOWRITER_HPP

#include <Writer.hpp>

class QMPlay2_OSD;
class VideoFrame;
class ImgScaler;

#include "export.h"

class VideoWriter : public Writer
{
public:
	qint64 write(const QByteArray &)
	{
		return 0;
	}

	virtual void writeVideo(const VideoFrame &videoFrame) = 0;
	virtual void writeOSD(const QList< const QMPlay2_OSD * > &osd) = 0;

	virtual bool HWAccellInit(int W, int H, const char *codec_name)
	{
		Q_UNUSED(W)
		Q_UNUSED(H)
		Q_UNUSED(codec_name)
		return false;
	}
	virtual bool HWAccellGetImg(const VideoFrame &videoFrame, void *dest, ImgScaler *yv12ToRGB32 = NULL) const
	{
		Q_UNUSED(videoFrame)
		Q_UNUSED(dest)
		Q_UNUSED(yv12ToRGB32)
		return false;
	}

	virtual bool open() = 0;
};

#endif //VIDEOWRITER_HPP
