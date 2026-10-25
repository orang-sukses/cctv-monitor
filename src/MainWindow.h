#pragma once
#include "Config.h"

#include <QMainWindow>
#include <QTimer>
#include <vector>

class CameraView;
class CameraWorker;
class QGridLayout;

// Jendela utama: grid kamera, mode solo (satu kamera penuh), rekam, snapshot, fullscreen.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const AppConfig &cfg);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent *) override;
    void changeEvent(QEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override { showCursor(); }

private:
    int rows() const { return (int(views_.size()) + cols_ - 1) / cols_; }
    void toggleSolo(int idx);
    void applyVisibility();
    void toggleRecording();
    void snapshot();
    void showCursor();

    AppConfig cfg_;
    QGridLayout *grid_;
    std::vector<CameraWorker *> workers_;
    std::vector<CameraView *> views_;
    int cols_ = 1, solo_ = -1;
    QTimer hideCursorTimer_;
};
