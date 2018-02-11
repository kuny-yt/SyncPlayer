#include <MotionBlur.hpp>
#include <VideoFilters.hpp>
#include <VideoFrame.hpp>

MotionBlur::MotionBlur()
{
	addParam("W");
	addParam("H");
}

void MotionBlur::filter(QQueue< FrameBuffer > &framesQueue)
{
	int insertAt = addFramesToInternalQueue(framesQueue);
	while (internalQueue.count() >= 2)
	{
		FrameBuffer dequeued = internalQueue.dequeue();
		FrameBuffer lookup   = internalQueue.first();

		const VideoFrame &videoFrame1 = dequeued.frame;
		VideoFrame videoFrame2(h, (h + 1) >> 1, videoFrame1.linesize);
		const VideoFrame &videoFrame3 = lookup.frame;

		for (int p = 0; p < 3; ++p)
		{
			const quint8 *src1 = videoFrame1.buffer[p].data();
			const quint8 *src2 = videoFrame3.buffer[p].data();
			quint8 *dest = videoFrame2.buffer[p].data();
			const int linesize = videoFrame1.linesize[p];
			const int H = p ? h >> 1 : h;
			for (int i = 0; i < H; ++i)
			{
				VideoFilters::averageTwoLines(dest, src1, src2, linesize);
				dest += linesize;
				src1 += linesize;
				src2 += linesize;
			}
		}

		framesQueue.insert(insertAt++, dequeued);
		framesQueue.insert(insertAt++, FrameBuffer(videoFrame2, dequeued.ts + halfDelay(lookup, dequeued)));
	}
}

bool MotionBlur::processParams(bool *)
{
	w = getParam("W").toInt();
	h = getParam("H").toInt();
	if (w < 2 || h < 4)
		return false;
	return true;
}
