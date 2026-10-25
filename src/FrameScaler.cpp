#include "FrameScaler.h"

#include <algorithm>

extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

FrameScaler::~FrameScaler() {
    if (sws_) sws_freeContext(sws_);
}

bool FrameScaler::scale(const AVFrame *f, int boxW, int boxH, FrameBuffer &out) {
    const double s = std::min(double(boxW) / f->width, double(boxH) / f->height);
    const int w = std::max(2, int(f->width * s) / 2 * 2);
    const int h = std::max(2, int(f->height * s) / 2 * 2);
    if (!sws_ || sw_ != f->width || sh_ != f->height || dw_ != w || dh_ != h || fmt_ != f->format) {
        if (sws_) sws_freeContext(sws_);
        sws_ = sws_getContext(f->width, f->height, AVPixelFormat(f->format), w, h,
                              AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        sw_ = f->width; sh_ = f->height; dw_ = w; dh_ = h; fmt_ = f->format;
        if (!sws_) return false;
    }
    QImage &img = out.acquire(w, h);
    uint8_t *dst[1] = {img.bits()};
    int stride[1] = {int(img.bytesPerLine())};
    sws_scale(sws_, f->data, f->linesize, 0, f->height, dst, stride);
    out.commit();
    return true;
}
