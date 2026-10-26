#include "CameraWorker.h"

#include "FrameScaler.h"
#include "LagMonitor.h"
#include "Recorder.h"
#include "StreamInput.h"
#include "VideoDecoder.h"

#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
}

namespace {
constexpr double kStallSec = 10;   // paket masuk tapi tak ada gambar

// Menghitung fps & kbps per detik.
class RateMeter {
public:
    explicit RateMeter(double now) : t_(now) {}
    void frame() { ++frames_; }
    void bytes(int n) { bytes_ += n; }
    bool tick(double now, double &fps, double &kbps) {
        const double dt = now - t_;
        if (dt < 1.0) return false;
        fps = frames_ / dt;
        kbps = bytes_ * 8.0 / 1000.0 / dt;
        frames_ = bytes_ = 0;
        t_ = now;
        return true;
    }

private:
    double t_;
    long frames_ = 0, bytes_ = 0;
};

// Membebaskan paket di akhir tiap iterasi (termasuk lewat continue/break).
struct PacketGuard {
    Packet &p;
    ~PacketGuard() { p.unref(); }
};
}  // namespace

CameraWorker::CameraWorker(const CameraConfig &cam, const AppConfig &cfg, QObject *parent)
    : QObject(parent), name_(cam.name), mainUrl_(cam.url.toStdString()),
      subUrl_((cam.subUrl.isEmpty() ? cam.url : cam.subUrl).toStdString()), recDir_(cfg.recordingsDir),
      minInterval_(cfg.maxFps > 0 ? 0.85 / cfg.maxFps : 0.0), useHw_(cfg.hwaccel), hd_(cfg.hd) {}

void CameraWorker::start() { thread_ = std::thread([this] { run(); }); }

void CameraWorker::requestStop() {
    {
        std::lock_guard<std::mutex> g(waitMx_);
        stop_ = true;
    }
    waitCv_.notify_all();
}

void CameraWorker::stop() {
    requestStop();
    if (thread_.joinable()) thread_.join();
}

void CameraWorker::sleepInterruptible(double sec) {
    std::unique_lock<std::mutex> l(waitMx_);
    waitCv_.wait_for(l, std::chrono::duration<double>(sec), [this] { return stop_.load(); });
}

void CameraWorker::run() {
    double delay = 1.0;
    while (!stop_) {
        const bool switched = session();
        if (stop_) break;
        state_ = CameraState::Reconnect;
        fps_ = kbps_ = 0;
        if (switched) { delay = 1.0; continue; }   // pindah grid<->solo: langsung sambung
        sleepInterruptible(delay);
        delay = std::min(delay * 2, 5.0);
    }
}

bool CameraWorker::session() {
    const std::string url = currentUrl();
    StreamInput in(stop_);
    if (!in.open(url)) return false;
    VideoDecoder dec;
    if (!dec.open(in.video(), useHw_)) return false;

    const AVStream *vs = in.video();
    const double tb = av_q2d(vs->time_base);
    FrameScaler scaler;
    LagMonitor lag;
    Recorder rec(recDir_, name_);
    Packet pkt;
    double lastOut = nowSec(), lastFrame = lastOut;
    RateMeter meter(lastOut);
    int skipping = -1;
    bool behind = false;

    while (!stop_ && in.read(pkt)) {
        PacketGuard guard{pkt};
        const double now = nowSec();
        if (currentUrl() != url) return true;
        if (pkt->stream_index != in.videoIndex()) continue;
        const bool key = pkt->flags & AV_PKT_FLAG_KEY;

        if (pkt->pts != AV_NOPTS_VALUE && tb > 0) behind = lag.update(pkt->pts * tb, now, key);
        const int wantSkip = (!visible_ || behind) ? 1 : 0;      // tersembunyi / tertinggal: keyframe saja
        if (skipping != wantSkip) {
            skipping = wantSkip;
            dec.setKeyframesOnly(wantSkip);
        }
        if (now - lastFrame > kStallSec) break;
        meter.bytes(pkt->size);
        totalBytes_ += pkt->size;

        if (dec.send(pkt.get())) {
            while (dec.receive()) {
                lastFrame = now;
                meter.frame();
                if (!visible_) continue;
                state_ = CameraState::Live;
                if (now - lastOut < minInterval_) continue;
                lastOut = now;
                const AVFrame *f = dec.systemFrame();   // unduh GPU->RAM hanya untuk frame yang ditampilkan
                if (f && scaler.scale(f, boxW_, boxH_, buffer_) && !paintPending_.exchange(true))
                    emit frameReady();                   // repaint beruntun digabung
            }
        }

        if (recording_) rec.write(pkt.get(), vs, key, now);   // terakhir: paket dikonsumsi muxer
        else rec.close();

        double f, k;
        if (meter.tick(now, f, k)) { fps_ = f; kbps_ = k; }
    }
    return false;
}
