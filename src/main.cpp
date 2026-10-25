// CCTV Monitor - aplikasi desktop (Qt6 + FFmpeg). RTSP dibaca langsung dari kamera,
// tanpa transcode dan tanpa web server.
#include "Config.h"
#include "MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QTimer>

extern "C" {
#include <libavutil/log.h>
}

int main(int argc, char **argv) {
    av_log_set_level(AV_LOG_QUIET);
    QApplication app(argc, argv);

    const QString base = QCoreApplication::applicationDirPath();
    QString path = qEnvironmentVariable("CCTV_CONFIG");
    if (path.isEmpty()) path = base + "/cameras.json";

    AppConfig cfg;
    QString error;
    if (!AppConfig::load(path, cfg, error)) {
        QMessageBox::critical(nullptr, "CCTV Monitor", error);
        return 1;
    }
    cfg.recordingsDir = base + "/recordings";
    cfg.snapshotsDir = base + "/snapshots";

    MainWindow win(cfg);
    const QStringList args = app.arguments();
    if (args.contains("--windowed") || qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        win.resize(1280, 720);
        win.show();
    } else {
        win.showFullScreen();
    }
    if (args.contains("--selftest")) QTimer::singleShot(6000, &app, &QCoreApplication::quit);
    return app.exec();
}
