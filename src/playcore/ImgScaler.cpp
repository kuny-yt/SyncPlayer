#include <ImgScaler.hpp>

#include <VideoFrame.hpp>

extern "C"
{
	#include <libswscale/swscale.h>
	#include <libavutil/mem.h>
}

bool ImgScaler::create(int _Wsrc, int _Hsrc, int _Wdst, int _Hdst)
{
	Wsrc = _Wsrc;
	Hsrc = _Hsrc;
	Wdst = _Wdst;
	Hdst = _Hdst;
	return (img_convert_ctx = sws_getCachedContext(img_convert_ctx, Wsrc, Hsrc, AV_PIX_FMT_YUV420P, Wdst, Hdst, AV_PIX_FMT_RGB32, SWS_BILINEAR, NULL, NULL, NULL));
}
bool ImgScaler::createArray(const size_t bytes)
{
	if (!arr)
		return (arr = av_malloc(bytes));
	return false;
}
void ImgScaler::scale(const VideoFrame &src, void *dst)
{
	if (img_convert_ctx)
	{
		if (!dst)
			dst = arr;
		const quint8 *srcData[3] = {src.buffer[0].data(), src.buffer[1].data(), src.buffer[2].data()};
		const int linesize = Wdst << 2;
		sws_scale(img_convert_ctx, srcData, src.linesize, 0, Hsrc, (uint8_t **)&dst, &linesize);
	}
}
void ImgScaler::scale(const void *src, void *dst)
{
	if (img_convert_ctx)
	{
		if (!dst)
			dst = arr;

		uint8_t *srcData[3];
		int srcLinesize[3];
		srcData[0] = (uint8_t *)src;
		srcData[2] = (uint8_t *)srcData[0] + (Wsrc * Hsrc);
		srcData[1] = (uint8_t *)srcData[2] + ((Wsrc >> 1) * (Hsrc >> 1));
		srcLinesize[0] = Wsrc;
		srcLinesize[1] = Wsrc >> 1;
		srcLinesize[2] = Wsrc >> 1;

		const int linesize = Wdst << 2;
		sws_scale(img_convert_ctx, srcData, srcLinesize, 0, Hsrc, (uint8_t **)&dst, &linesize);
	}
}
void ImgScaler::destroy()
{
	if (img_convert_ctx)
	{
		sws_freeContext(img_convert_ctx);
		img_convert_ctx = NULL;
	}
	if (arr)
	{
		av_free(arr);
		arr = NULL;
	}
}
