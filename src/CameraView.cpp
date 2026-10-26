#include "CameraView.h"

#include "CameraWorker.h"
#include "Config.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QTimer>
#include <algorithm>

CameraView::CameraView(CameraWorker *worker, int index, const OverlayStyle &style, QWidget *parent)
    : QWidget(parent), worker_(worker), index_(index), style_(style) {
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

    QFont f(style_.font, style_.size, style_.bold ? QFont::Bold : QFont::Normal);
    p.setFont(f);
    const int barH = QFontMetrics(f).height() + 8;
    const int barY = style_.atBottom ? height() - barH : 0;
    p.fillRect(0, barY, width(), barH, QColor(0, 0, 0, style_.barOpacity));
    const QRect bar(8, barY, width() - 16, barH);
    const QString txt = live ? QString("LIVE  %1fps  %2kbps  %3")
                                   .arg(worker_->fps(), 0, 'f', 0)
                                   .arg(worker_->kbps(), 0, 'f', 0)
                                   .arg(formatBytes(worker_->totalBytes()))
                             : QString(toString(state));
    if (style_.showName) {
        p.setPen(QColor(style_.nameColor));
        p.drawText(bar, Qt::AlignLeft | Qt::AlignVCenter, worker_->name());
    }
    if (style_.showStatus) {
        p.setPen(QColor(live ? style_.liveColor : style_.warnColor));
        p.drawText(bar, Qt::AlignRight | Qt::AlignVCenter, txt);
    }

    if (worker_->recording()) {
        // di sisi berlawanan dari bar agar tidak tertimpa
        p.setPen(QColor(255, 60, 60));
        p.setFont(QFont(style_.font, style_.size, QFont::Bold));
        const int y = style_.atBottom ? 16 : height() - 8;
        p.drawText(8, y, QString::fromUtf8("\xE2\x97\x8F REC"));
    }
    if (!live) {
        p.setPen(QColor(200, 200, 200));
        p.setFont(QFont("Segoe UI", 12));
        p.drawText(rect(), Qt::AlignCenter, "Menyambung ke kamera...");
    }
    if (selected_) {
        p.setPen(QPen(QColor("#1E88E5"), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(1, 1, -2, -2));
    }
}
