#ifndef VIDEOFILTER_HPP
#define VIDEOFILTER_HPP

#include <ModuleParams.hpp>
#include <VideoFrame.hpp>

#include <QQueue>

#include "export.h"

class LIBEXPORT VideoFilter : public ModuleParams
{
public:
	class FrameBuffer
	{
	public:
		inline FrameBuffer(const VideoFrame &frame, double ts) :
			frame(frame),
			ts(ts)
		{}

		VideoFrame frame;
		double ts;
	};

	virtual ~VideoFilter()
	{}

	inline void clearBuffer()
	{
		internalQueue.clear();
	}

	bool removeLastFromInternalBuffer();

	virtual void filter(QQueue< FrameBuffer > &framesQueue) = 0;
protected:
	int addFramesToInternalQueue(QQueue< FrameBuffer > &framesQueue);

	inline double halfDelay(const FrameBuffer &f1, const FrameBuffer &f2) const
	{
		return (f1.ts - f2.ts) / 2.0;
	}

	QQueue< FrameBuffer > internalQueue;
};

#endif
