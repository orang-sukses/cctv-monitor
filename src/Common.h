#pragma once
#include <chrono>

// Waktu monotonic dalam detik (untuk timeout, pengukuran lag, fps).
inline double nowSec() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}


enum class CameraState { Connecting, Live, Reconnect };

inline const char *toString(CameraState s) {
    switch (s) {
    case CameraState::Live: return "LIVE";
    case CameraState::Reconnect: return "RECONNECT";
    default: return "CONNECTING";
    }
}
