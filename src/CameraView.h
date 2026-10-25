#pragma once
#include <QWidget>

class CameraWorker;
class QTimer;

// Widget satu kamera: menggambar frame terbaru + overlay nama/status/REC.
class CameraView : public QWidget {
    Q_OBJECT
public:
    CameraView(CameraWorker *worker, int index, QWidget *parent = nullptr);

signals:
    void doubleClicked(int index);
    void mouseMoved();

protected:
    void resizeEvent(QResizeEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override { emit mouseMoved(); }
    void mouseDoubleClickEvent(QMouseEvent *) override { emit doubleClicked(index_); }
    void paintEvent(QPaintEvent *) override;

private:
    CameraWorker *worker_;
    int index_;
    int tick_ = 0;
};
