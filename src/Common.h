#pragma once
#include <QString>
#include <chrono>

// Waktu monotonic dalam detik (untuk timeout, pengukuran lag, fps).
inline double nowSec() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Ukuran data yang mudah dibaca (mis. "850 KB", "1.25 GB").
inline QString formatBytes(unsigned long long b) {
    const double kb = b / 1024.0, mb = kb / 1024.0, gb = mb / 1024.0;
    if (gb >= 1) return QString::number(gb, 'f', 2) + " GB";
    if (mb >= 1) return QString::number(mb, 'f', 1) + " MB";
    return QString::number(kb, 'f', 0) + " KB";
}

enum class CameraState { Connecting, Live, Reconnect };

inline const char *toString(CameraState s) {
    switch (s) {
    case CameraState::Live: return "LIVE";
    case CameraState::Reconnect: return "RECONNECT";
    default: return "CONNECTING";
    }
}
