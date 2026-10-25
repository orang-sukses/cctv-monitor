#include "StreamInput.h"

#include "Common.h"

extern "C" {
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
}

Packet::Packet() : p_(av_packet_alloc()) {}
Packet::~Packet() { av_packet_free(&p_); }
void Packet::unref() { av_packet_unref(p_); }

StreamInput::~StreamInput() { avformat_close_input(&fmt_); }

int StreamInput::interruptCb(void *self) {
    auto *s = static_cast<StreamInput *>(self);
    return s->stop_.load() || nowSec() > s->deadline_.load();
}

bool StreamInput::open(const std::string &url) {
    fmt_ = avformat_alloc_context();
    if (!fmt_) return false;
    fmt_->interrupt_callback.callback = &StreamInput::interruptCb;
    fmt_->interrupt_callback.opaque = this;

    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);              // TCP: tanpa kehilangan paket
    av_dict_set(&opts, "fflags", "nobuffer+discardcorrupt", 0);
    av_dict_set(&opts, "flags", "low_delay", 0);
    av_dict_set(&opts, "avioflags", "direct", 0);                // tanpa buffering I/O tambahan
    av_dict_set(&opts, "max_delay", "0", 0);
    av_dict_set(&opts, "reorder_queue_size", "0", 0);
    av_dict_set(&opts, "analyzeduration", "500000", 0);
    av_dict_set(&opts, "probesize", "200000", 0);
    av_dict_set(&opts, "timeout", "5000000", 0);                 // timeout socket 5 dtk (us)

    deadline_ = nowSec() + 8.0;
    const int rc = avformat_open_input(&fmt_, url.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    if (rc < 0) return false;                                    // fmt_ sudah dibebaskan FFmpeg

    deadline_ = nowSec() + 8.0;
    if (avformat_find_stream_info(fmt_, nullptr) < 0) return false;
    videoIndex_ = av_find_best_stream(fmt_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    return videoIndex_ >= 0;
}

bool StreamInput::read(Packet &pkt) {
    deadline_ = nowSec() + 5.0;
    return av_read_frame(fmt_, pkt.get()) >= 0;
}

const AVStream *StreamInput::video() const { return fmt_->streams[videoIndex_]; }
