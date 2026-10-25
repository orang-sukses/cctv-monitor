#pragma once
#include <QString>
#include <vector>

struct CameraConfig {
    QString name;
    QString url;      // stream utama (dipakai saat satu kamera diperbesar)
    QString subUrl;   // stream ringan untuk grid (opsional, kosong = pakai url)
};

// Konfigurasi aplikasi dari cameras.json.
class AppConfig {
public:
    static bool load(const QString &path, AppConfig &out, QString &error);

    double maxFps = 0;          // 0 = tanpa batas
    bool hwaccel = true;        // decode GPU bila tersedia
    std::vector<CameraConfig> cameras;
    QString recordingsDir, snapshotsDir;
};
