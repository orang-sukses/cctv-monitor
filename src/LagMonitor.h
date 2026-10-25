#pragma once
#include <algorithm>

// Mengukur ketinggalan decode terhadap waktu nyata. Bila terlalu jauh (> maxLag),
// pemanggil sebaiknya membuang frame sampai keyframe berikutnya agar latency kembali minimal.
class LagMonitor {
public:
    // Return true selama decode sedang tertinggal.
    bool update(double ptsSec, double now, bool keyframe) {
        if (!started_) { t0_ = now; pts0_ = ptsSec; lagReset_ = now; started_ = true; }
        double lag = (now - t0_) - (ptsSec - pts0_);
        if (now - lagReset_ > 30) { lagMin_ = 1e9; lagReset_ = now; }   // redam drift clock kamera
        lagMin_ = std::min(lagMin_, lag);
        lag -= lagMin_;
        if (lag > kMaxLag) behind_ = true;
        else if (behind_ && lag < kOkLag && keyframe) behind_ = false;
        return behind_;
    }

private:
    static constexpr double kMaxLag = 0.6, kOkLag = 0.15;
    bool started_ = false, behind_ = false;
    double t0_ = 0, pts0_ = 0, lagMin_ = 1e9, lagReset_ = 0;
};
