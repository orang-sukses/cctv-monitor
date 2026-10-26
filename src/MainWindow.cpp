#include "MainWindow.h"

#include "CameraView.h"
#include "CameraWorker.h"

#include <QDateTime>
#include <QDir>
#include <QGridLayout>
#include <QKeyEvent>
#include <cmath>

MainWindow::MainWindow(const AppConfig &cfg) : cfg_(cfg) {
    setWindowTitle("CCTV Monitor");
    auto *host = new QWidget;
    host->setStyleSheet("background:#000");
    host->setMouseTracking(true);
    setMouseTracking(true);
    grid_ = new QGridLayout(host);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setSpacing(1);
    setCentralWidget(host);

    const int n = int(cfg_.cameras.size());
    cols_ = std::max(1, int(std::ceil(std::sqrt(double(n)))));
    for (int i = 0; i < n; ++i) {
        auto *w = new CameraWorker(cfg_.cameras[i], cfg_, this);
        auto *v = new CameraView(w, i, cfg_.overlay, host);
        connect(v, &CameraView::doubleClicked, this, &MainWindow::toggleSolo);
        connect(v, &CameraView::mouseMoved, this, &MainWindow::showCursor);
        workers_.push_back(w);
        views_.push_back(v);
        grid_->addWidget(v, i / cols_, i % cols_);
    }
    for (int r = 0; r < rows(); ++r) grid_->setRowStretch(r, 1);
    for (int c = 0; c < cols_; ++c) grid_->setColumnStretch(c, 1);

    hideCursorTimer_.setSingleShot(true);
    hideCursorTimer_.setInterval(2500);
    connect(&hideCursorTimer_, &QTimer::timeout, this, [this] { setCursor(Qt::BlankCursor); });
    hideCursorTimer_.start();

    for (auto *w : workers_) w->start();

    auto *usage = new QTimer(this);   // total pemakaian internet semua kamera di judul jendela
    usage->setInterval(1000);
    connect(usage, &QTimer::timeout, this, [this] {
        unsigned long long total = 0;
        double kbps = 0;
        for (auto *w : workers_) { total += w->totalBytes(); kbps += w->kbps(); }
        setWindowTitle(QString("CCTV Monitor [%1] - Internet: %2 (%3 Mbps)")
                           .arg(workers_[0]->hd() ? "HD" : "SD")
                           .arg(formatBytes(total)).arg(kbps / 1000.0, 0, 'f', 1));
    });
    usage->start();
}

MainWindow::~MainWindow() {
    for (auto *w : workers_) w->requestStop();   // berhenti paralel
    for (auto *w : workers_) w->stop();          // lalu tunggu (file rekaman ditutup rapi)
}

void MainWindow::showCursor() {
    unsetCursor();
    hideCursorTimer_.start();
}

void MainWindow::changeEvent(QEvent *e) {
    QMainWindow::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange) applyVisibility();   // diminimize: tidak di-render
}

void MainWindow::applyVisibility() {
    for (int i = 0; i < int(workers_.size()); ++i)
        workers_[i]->setVisible(!isMinimized() && (solo_ < 0 || i == solo_));
}

void MainWindow::toggleSolo(int idx) {
    if (idx < 0 || idx >= int(views_.size())) return;
    solo_ = (solo_ == idx) ? -1 : idx;
    for (int i = 0; i < int(views_.size()); ++i) {
        views_[i]->setVisible(solo_ < 0 || i == solo_);
        if (solo_ == i) grid_->addWidget(views_[i], 0, 0, rows(), cols_);   // satu kamera penuh layar
        else grid_->addWidget(views_[i], i / cols_, i % cols_);
    }
    applyVisibility();
}

void MainWindow::toggleRecording() {
    bool on = true;
    for (auto *w : workers_)
        if (w->recording()) on = false;
    for (auto *w : workers_) w->setRecording(on);
    for (auto *v : views_) v->update();
}

void MainWindow::snapshot() {
    QDir().mkpath(cfg_.snapshotsDir);
    const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    for (int i = 0; i < int(views_.size()); ++i) {
        const QImage img = workers_[i]->image();
        if (!img.isNull() && views_[i]->isVisible() && workers_[i]->state() == CameraState::Live)
            img.save(QString("%1/cam%2_%3.jpg").arg(cfg_.snapshotsDir).arg(i + 1).arg(ts), "JPG", 90);
    }
}

void MainWindow::keyPressEvent(QKeyEvent *e) {
    const int k = e->key();
    if (k >= Qt::Key_1 && k <= Qt::Key_9) {
        const int idx = k - Qt::Key_1;
        if (solo_ >= 0 && solo_ != idx) toggleSolo(solo_);   // kembali ke grid dulu
        toggleSolo(idx);
    } else if (k == Qt::Key_0 || k == Qt::Key_Escape) {
        if (solo_ >= 0) toggleSolo(solo_);
        else if (isFullScreen()) showNormal();
    } else if (k == Qt::Key_F || k == Qt::Key_F11) {
        isFullScreen() ? showNormal() : showFullScreen();
    } else if (k == Qt::Key_R) {
        toggleRecording();
    } else if (k == Qt::Key_S) {
        snapshot();
    } else if (k == Qt::Key_H) {
        const bool hd = !(!workers_.empty() && workers_[0]->hd());   // SD <-> HD untuk semua kamera
        for (auto *w : workers_) w->setHd(hd);
    } else if (k == Qt::Key_Q) {
        close();
    }
}
