#include "CameraView.h"

#include "CameraWorker.h"

#include <QFont>
#include <QPainter>
#include <QTimer>
#include <algorithm>

CameraView::CameraView(CameraWorker *worker, int index, QWidget *parent)
    : QWidget(parent), worker_(worker), index_(index) {
    // repaint segera saat frame tiba (signal lintas-thread = queued, aman)
    connect(worker, &CameraWorker::frameReady, this, qOverload<>(&QWidget::update), Qt::QueuedConnection);
    setMinimumSize(160, 90);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);   // seluruh area dicat sendiri

    auto *t = new QTimer(this);              // hanya untuk refresh status/teks
    t->setInterval(500);
    connect(t, &QTimer::timeout, this, [this] {
        if (worker_->state() != CameraState::Live || (++tick_ % 2 == 0)) update();
    });
    t->start();
}

void CameraView::resizeEvent(QResizeEvent *) {
    const double dpr = devicePixelRatioF();
    worker_->setBox(std::max(2, int(width() * dpr)), std::max(2, int(height() * dpr)));
}

void CameraView::paintEvent(QPaintEvent *) {
    worker_->markPainted();
    QPainter p(this);
    p.fillRect(rect(), QColor(8, 8, 8));

    const CameraState state = worker_->state();
    const bool live = state == CameraState::Live;
    if (live) {
        const QImage img = worker_->image();
        if (!img.isNull()) {
            const double dpr = devicePixelRatioF();
            const double iw = img.width() / dpr, ih = img.height() / dpr;
            p.drawImage(QRectF((width() - iw) / 2, (height() - ih) / 2, iw, ih), img);
        }
    }

    p.setFont(QFont("Segoe UI", 9, QFont::Bold));
    p.fillRect(0, 0, width(), 22, QColor(0, 0, 0, 150));
    p.setPen(QColor(235, 235, 235));
    p.drawText(8, 16, worker_->name());
    const QString txt = live ? QString("LIVE  %1fps  %2kbps").arg(worker_->fps(), 0, 'f', 0).arg(worker_->kbps(), 0, 'f', 0)
                             : QString(toString(state));
    p.setPen(live ? QColor(80, 220, 120) : QColor(255, 170, 60));
    p.drawText(QRect(0, 0, width() - 8, 22), Qt::AlignRight | Qt::AlignVCenter, txt);

    if (worker_->recording()) {
        p.setPen(QColor(255, 60, 60));
        p.drawText(8, height() - 8, QString::fromUtf8("\xE2\x97\x8F REC"));
    }
    if (!live) {
        p.setPen(QColor(200, 200, 200));
        p.setFont(QFont("Segoe UI", 12));
        p.drawText(rect(), Qt::AlignCenter, "Menyambung ke kamera...");
    }
}
