#pragma once
#include <QWidget>

class CameraWorker;
struct OverlayStyle;
class QTimer;

// Widget satu kamera: menggambar frame terbaru + overlay nama/status/REC.
class CameraView : public QWidget {
    Q_OBJECT
public:
    CameraView(CameraWorker *worker, int index, const OverlayStyle &style, QWidget *parent = nullptr);

    void setSelected(bool s) { selected_ = s; update(); }

signals:
    void clicked(int index);
    void doubleClicked(int index);
    void mouseMoved();

protected:
    void resizeEvent(QResizeEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override { emit mouseMoved(); }
    void mousePressEvent(QMouseEvent *) override { emit clicked(index_); }
    void mouseDoubleClickEvent(QMouseEvent *) override { emit doubleClicked(index_); }
    void paintEvent(QPaintEvent *) override;

private:
    CameraWorker *worker_;
    int index_;
    int tick_ = 0;
    bool selected_ = false;
    const OverlayStyle &style_;
};
