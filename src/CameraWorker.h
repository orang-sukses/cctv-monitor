#pragma once
#include "Common.h"
#include "Config.h"
#include "FrameBuffer.h"

#include <QObject>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

// Satu thread per kamera: ambil, decode, skala, simpan frame terbaru. Kamera yang
// putus tersambung ulang otomatis (backoff 1-5 dtk) tanpa mengganggu kamera lain.
class CameraWorker : public QObject {
    Q_OBJECT
public:
    CameraWorker(const CameraConfig &cam, const AppConfig &cfg, QObject *parent = nullptr);
    ~CameraWorker() override { stop(); }

    void start();
    void requestStop();          // non-blocking (agar semua kamera berhenti paralel)
    void stop();                 // requestStop + tunggu thread selesai

    const QString &name() const { return name_; }
    CameraState state() const { return state_; }
    double fps() const { return fps_; }
    double kbps() const { return kbps_; }
    unsigned long long totalBytes() const { return totalBytes_; }   // total data diterima sejak aplikasi jalan
    QImage image() const { return buffer_.take(); }

    // Dari thread UI
    void setVisible(bool v) { visible_ = v; }          // tidak tampil: decode keyframe saja
    void setHd(bool h) { hd_ = h; }                    // HD: stream utama (url), SD: sub-stream (sub_url)
    bool hd() const { return hd_; }
    void setRecording(bool r) { recording_ = r; }
    bool recording() const { return recording_; }
    void setBox(int w, int h) { boxW_ = w; boxH_ = h; }
    void markPainted() { paintPending_ = false; }

signals:
    void frameReady();

private:
    void run();
    bool session();              // true bila berhenti karena pindah stream grid<->solo
    void sleepInterruptible(double sec);
    std::string currentUrl() const { return hd_ ? mainUrl_ : subUrl_; }

    const QString name_;
    const std::string mainUrl_, subUrl_;
    const QString recDir_;
    const double minInterval_;   // faktor 0.85 mencegah frame terbuang karena jitter timestamp
    const bool useHw_;

    std::thread thread_;
    std::mutex waitMx_;
    std::condition_variable waitCv_;
    FrameBuffer buffer_;

    std::atomic<bool> stop_{false}, visible_{true}, hd_{false}, recording_{false}, paintPending_{false};
    std::atomic<CameraState> state_{CameraState::Connecting};
    std::atomic<int> boxW_{640}, boxH_{360};
    std::atomic<double> fps_{0}, kbps_{0};
    std::atomic<unsigned long long> totalBytes_{0};
};
