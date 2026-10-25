#include "Recorder.h"

#include <QDateTime>
#include <QDir>

extern "C" {
#include <libavformat/avformat.h>
}

Recorder::Recorder(QString dir, const QString &camName) : dir_(std::move(dir)), safeName_(camName) {
    for (QChar &c : safeName_)
        if (!c.isLetterOrNumber()) c = '_';
}

bool Recorder::open(const AVStream *in) {
    QDir().mkpath(dir_);
    const QByteArray path =
        (dir_ + "/" + safeName_ + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".mkv").toUtf8();
    if (avformat_alloc_output_context2(&oc_, nullptr, "matroska", path.constData()) < 0) return false;
    out_ = avformat_new_stream(oc_, nullptr);
    if (!out_ || avcodec_parameters_copy(out_->codecpar, in->codecpar) < 0 ||
        avio_open(&oc_->pb, path.constData(), AVIO_FLAG_WRITE) < 0) {
        avformat_free_context(oc_);
        oc_ = nullptr;
        return false;
    }
    out_->codecpar->codec_tag = 0;
    if (avformat_write_header(oc_, nullptr) < 0) {
        avio_closep(&oc_->pb);
        avformat_free_context(oc_);
        oc_ = nullptr;
        return false;
    }
    return true;
}

void Recorder::write(AVPacket *pkt, const AVStream *in, bool keyframe, double now) {
    if (!pkt->size) return;
    if (oc_ && now - start_ > kSegmentSec) close();
    if (!oc_) {
        if (!keyframe || !open(in)) return;     // mulai hanya di keyframe
        start_ = now;
    }
    av_packet_rescale_ts(pkt, in->time_base, out_->time_base);
    pkt->stream_index = out_->index;
    pkt->pos = -1;
    av_interleaved_write_frame(oc_, pkt);
}

void Recorder::close() {
    if (!oc_) return;
    av_write_trailer(oc_);
    avio_closep(&oc_->pb);
    avformat_free_context(oc_);
    oc_ = nullptr;
    out_ = nullptr;
}
