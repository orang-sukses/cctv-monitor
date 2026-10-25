#include "VideoDecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

namespace {
bool isHwFormat(int fmt) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(AVPixelFormat(fmt));
    return d && (d->flags & AV_PIX_FMT_FLAG_HWACCEL);
}

AVPixelFormat pickFormat(AVCodecContext *, const AVPixelFormat *fmts) {
    for (const AVPixelFormat *p = fmts; *p != AV_PIX_FMT_NONE; ++p)
        if (*p == AV_PIX_FMT_D3D11) return *p;
    for (const AVPixelFormat *p = fmts; *p != AV_PIX_FMT_NONE; ++p)
        if (!isHwFormat(*p)) return *p;
    return AV_PIX_FMT_NONE;
}
}  // namespace

VideoDecoder::VideoDecoder() : frame_(av_frame_alloc()), sw_(av_frame_alloc()) {}

VideoDecoder::~VideoDecoder() {
    av_frame_free(&frame_);
    av_frame_free(&sw_);
    avcodec_free_context(&ctx_);
    av_buffer_unref(&hw_);
}

bool VideoDecoder::open(const AVStream *vs, bool tryHardware) {
    const AVCodec *codec = avcodec_find_decoder(vs->codecpar->codec_id);
    if (!codec) return false;
    for (int attempt = tryHardware ? 0 : 1; attempt < 2; ++attempt) {
        ctx_ = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(ctx_, vs->codecpar);
        ctx_->thread_type = FF_THREAD_SLICE;          // tanpa frame-threading = tanpa tambahan delay
        ctx_->thread_count = 0;                       // otomatis sesuai jumlah core
        ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
        ctx_->skip_loop_filter = AVDISCARD_NONREF;    // lebih ringan, kualitas hampir sama
#ifdef _WIN32
        if (attempt == 0 &&
            av_hwdevice_ctx_create(&hw_, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0) >= 0) {
            ctx_->hw_device_ctx = av_buffer_ref(hw_);
            ctx_->get_format = pickFormat;
        }
#endif
        if (avcodec_open2(ctx_, codec, nullptr) >= 0) return true;
        avcodec_free_context(&ctx_);
        av_buffer_unref(&hw_);
    }
    return false;
}

void VideoDecoder::setKeyframesOnly(bool on) {
    ctx_->skip_frame = on ? AVDISCARD_NONKEY : AVDISCARD_DEFAULT;
}

bool VideoDecoder::send(const AVPacket *pkt) { return avcodec_send_packet(ctx_, pkt) >= 0; }

bool VideoDecoder::receive() {
    av_frame_unref(frame_);
    return avcodec_receive_frame(ctx_, frame_) >= 0;
}

const AVFrame *VideoDecoder::systemFrame() {
    if (!isHwFormat(frame_->format)) return frame_;
    av_frame_unref(sw_);
    return av_hwframe_transfer_data(sw_, frame_, 0) >= 0 ? sw_ : nullptr;
}
