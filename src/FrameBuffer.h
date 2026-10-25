#pragma once
#include <QImage>
#include <mutex>

// Double buffer thread-safe: worker menulis ke buffer cadangan, lalu menukarnya
// jadi frame terbaru. Buffer dipakai ulang agar tidak ada alokasi per frame.
class FrameBuffer {
public:
    // Thread UI: salinan (shallow) frame terbaru.
    QImage take() const {
        std::lock_guard<std::mutex> g(mx_);
        return latest_;
    }
    // Thread worker: buffer untuk ditulis. Dibuat baru bila ukuran berubah atau masih dipegang painter.
    QImage &acquire(int w, int h) {
        std::lock_guard<std::mutex> g(mx_);
        if (spare_.width() != w || spare_.height() != h || !spare_.isDetached())
            spare_ = QImage(w, h, QImage::Format_RGB888);
        return spare_;
    }
    void commit() {
        std::lock_guard<std::mutex> g(mx_);
        std::swap(latest_, spare_);
    }

private:
    mutable std::mutex mx_;
    QImage latest_, spare_;
};
