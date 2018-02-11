#include <BlendDeint.hpp>
#include <VideoFilters.hpp>

BlendDeint::BlendDeint()
{
	addParam("W");
	addParam("H");
}

void BlendDeint::filter(QQueue< FrameBuffer > &framesQueue)
{
	int insertAt = addFramesToDeinterlace(framesQueue);
	while (!internalQueue.isEmpty())
	{
		FrameBuffer dequeued = internalQueue.dequeue();
		VideoFrame &videoFrame = dequeued.frame;
		videoFrame.setNoInterlaced();
		for (int p = 0; p < 3; ++p)
		{
			const int linesize = videoFrame.linesize[p];
			const quint8 *src = videoFrame.buffer[p].data() + linesize;
			quint8 *dst = videoFrame.buffer[p].data() + linesize;
			const int H = (p ? h >> 1 : h >> 0) - 2;
			for (int i = 0; i < H; ++i)
			{
				VideoFilters::averageTwoLines(dst, src, src + linesize, linesize);
				src += linesize;
				dst += linesize;
			}
		}
		framesQueue.insert(insertAt++, dequeued);
	}
}

bool BlendDeint::processParams(bool *)
{
	w = getParam("W").toInt();
	h = getParam("H").toInt();
	deintFlags = getParam("DeinterlaceFlags").toInt();
	if (w < 2 || h < 4 || (deintFlags & DoubleFramerate))
		return false;
	return true;
}
