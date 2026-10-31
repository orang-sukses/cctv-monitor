#pragma once
#include "Config.h"

#include <QMainWindow>
#include <QTimer>
#include <QSystemTrayIcon>
#include <algorithm>
#include <vector>

class QAction;
class QScreen;
class CameraView;
class CameraWorker;
class QButtonGroup;
class QGridLayout;
class QLabel;
class QListWidget;
class QToolButton;
class QWidget;

// Jendela utama bergaya V380: sidebar daftar kamera, grid 1/4/9/16 layar, toolbar
// (rekam, foto, SD/HD, layar penuh), mode solo (satu kamera penuh).
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const AppConfig &cfg);
    ~MainWindow() override;

    // Tampilan awal: otomatis ke monitor kedua (layar penuh) bila ada; tanpa monitor kedua jendela disembunyikan.
    void startup(bool forceFullscreen);

protected:
    void keyPressEvent(QKeyEvent *) override;
    void changeEvent(QEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override { showCursor(); }
    void showEvent(QShowEvent *) override { applyVisibility(); }
    void hideEvent(QHideEvent *) override { applyVisibility(); }
    void resizeEvent(QResizeEvent *) override;

private:
    void buildUi();
    QToolButton *makeButton(const QString &text, const QString &tip, bool checkable = false);
    int pageCount() const { return std::max(1, (int(views_.size()) + layout_ - 1) / layout_); }
    void relayout();
    void applyVisibility();
    void applyChrome();
    void refreshUi();
    void selectCamera(int idx);
    void setLayoutCount(int n);
    void changePage(int delta);
    void toggleSolo(int idx);
    void toggleHd();
    void toggleRecording();
    void toggleFullscreen();
    void snapshot();
    void showCursor();
    void applyScreens();
    void bringUp();
    void applyMargin();
    void adjustMargin(int delta);
    static QScreen *secondScreen();
    void setAutoScreen(bool on);
    void placeFloatBtn();

    AppConfig cfg_;
    QWidget *host_ = nullptr, *sidebar_ = nullptr, *toolbar_ = nullptr;
    QGridLayout *grid_ = nullptr;
    QListWidget *list_ = nullptr;
    QLabel *pageLabel_ = nullptr, *usageLabel_ = nullptr;
    QToolButton *recBtn_ = nullptr, *hdBtn_ = nullptr;
    QButtonGroup *layoutGroup_ = nullptr;
    std::vector<QToolButton *> layoutBtns_;
    std::vector<CameraWorker *> workers_;
    std::vector<CameraView *> views_;
    std::vector<bool> shown_;
    int layout_ = 1, page_ = 0, sel_ = -1, solo_ = -1;   // sel_/solo_: -1 = tidak ada
    QTimer hideCursorTimer_, screenTimer_;
    class QHBoxLayout *rootLayout_ = nullptr;
    int marginPx_ = 40;        // margin layar penuh (px), "screen_margin"
    bool autoScreen_ = true;   // otomatisasi layar kedua (disimpan di settings.ini)
    QToolButton *autoBtn_ = nullptr, *floatBtn_ = nullptr;
    QSystemTrayIcon *tray_ = nullptr;
    QAction *trayAuto_ = nullptr;
};
