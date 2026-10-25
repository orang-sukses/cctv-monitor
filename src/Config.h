#pragma once
#include <QString>
#include <vector>

struct CameraConfig {
    QString name;
    QString url;      // stream utama (dipakai saat satu kamera diperbesar)
    QString subUrl;   // stream ringan untuk grid (opsional, kosong = pakai url)
};

// Tampilan teks overlay (nama kamera + status) pada tiap kamera.
struct OverlayStyle {
    bool atBottom = true;        // "position": "bottom" | "top"
    QString font = "Segoe UI";   // "font"
    int size = 9;                // "size" (pt)
    bool bold = true;            // "bold"
    QString nameColor = "#EBEBEB";   // "name_color"
    QString liveColor = "#50DC78";   // "live_color"
    QString warnColor = "#FFAA3C";   // "warn_color" (RECONNECT dll)
    int barOpacity = 150;        // "bar_opacity" 0-255
    bool showStatus = true;      // "show_status": tampilkan LIVE/fps/kbps
    bool showName = true;        // "show_name"
};

// Konfigurasi aplikasi dari cameras.json.
class AppConfig {
public:
    static bool load(const QString &path, AppConfig &out, QString &error);

    double maxFps = 0;          // 0 = tanpa batas
    bool hwaccel = true;        // decode GPU bila tersedia
    OverlayStyle overlay;
    std::vector<CameraConfig> cameras;
    QString recordingsDir, snapshotsDir;
};
