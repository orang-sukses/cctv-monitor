#include "MainWindow.h"

#include "CameraView.h"
#include "CameraWorker.h"

#include <QButtonGroup>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QAction>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QListWidget>
#include <QMenu>
#include <QScreen>
#include <QStyle>
#include <QSettings>
#include <QWindow>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
const int kLayouts[] = {1, 4, 9, 16};

const char *kStyle = R"(
QMainWindow, QWidget#root { background:#14171c; }
QWidget#sidebar { background:#1b1f26; border-right:1px solid #262b34; }
QLabel#brand { color:#f0f4fa; font:bold 15px 'Segoe UI'; padding:14px 14px 10px 14px; }
QListWidget { background:transparent; border:none; outline:none; color:#d5dbe5; font:12px 'Segoe UI'; }
QListWidget::item { padding:9px 14px; border-left:3px solid transparent; }
QListWidget::item:hover { background:#232832; }
QListWidget::item:selected { background:#22324a; border-left:3px solid #1e88e5; color:#ffffff; }
QWidget#toolbar { background:#1b1f26; border-top:1px solid #262b34; }
QToolButton { background:#252a33; color:#d5dbe5; border:1px solid #2d333e; border-radius:5px;
              padding:6px 12px; font:12px 'Segoe UI'; min-width:20px; }
QToolButton:hover { background:#2e3541; }
QToolButton:checked { background:#1e88e5; border-color:#1e88e5; color:white; }
QToolButton#rec:checked { background:#d93636; border-color:#d93636; }
QLabel#info { color:#9aa4b2; font:12px 'Segoe UI'; padding:0 6px; }
)";
}  // namespace

MainWindow::MainWindow(const AppConfig &cfg) : cfg_(cfg) {
    setWindowTitle("CCTV Monitor");
    setMouseTracking(true);

    const int n = int(cfg_.cameras.size());
    layout_ = 16;
    for (int l : kLayouts)
        if (l >= n) { layout_ = l; break; }   // awal: layout terkecil yang memuat semua kamera

    buildUi();

    for (int i = 0; i < n; ++i) {
        auto *w = new CameraWorker(cfg_.cameras[i], cfg_, this);
        auto *v = new CameraView(w, i, cfg_.overlay, host_);
        connect(v, &CameraView::clicked, this, &MainWindow::selectCamera);
        connect(v, &CameraView::doubleClicked, this, &MainWindow::toggleSolo);
        connect(v, &CameraView::mouseMoved, this, &MainWindow::showCursor);
        workers_.push_back(w);
        views_.push_back(v);
        list_->addItem(cfg_.cameras[i].name);
    }
    shown_.assign(n, false);
    relayout();

    hideCursorTimer_.setSingleShot(true);
    hideCursorTimer_.setInterval(2500);
    connect(&hideCursorTimer_, &QTimer::timeout, this, [this] {
        if (isFullScreen()) setCursor(Qt::BlankCursor);   // kursor hanya disembunyikan saat layar penuh
    });
    hideCursorTimer_.start();

    {
        QSettings st(QCoreApplication::applicationDirPath() + "/settings.ini", QSettings::IniFormat);
        autoScreen_ = st.value("auto_second_screen", true).toBool();
        marginPx_ = std::clamp(st.value("screen_margin", cfg_.screenMargin).toInt(), 0, 300);
    }

    // Ikon tray: jendela yang disembunyikan (tanpa monitor kedua) tetap bisa dibuka / diatur / ditutup.
    QIcon trayIcon = windowIcon();
    if (trayIcon.isNull()) trayIcon = QIcon(QCoreApplication::applicationDirPath() + "/CCTV.ico");
    if (trayIcon.isNull()) trayIcon = style()->standardIcon(QStyle::SP_ComputerIcon);
    tray_ = new QSystemTrayIcon(trayIcon, this);
    tray_->setToolTip("CCTV Monitor");
    auto *menu = new QMenu(this);
    QAction *actShow = menu->addAction("Tampilkan jendela");
    connect(actShow, &QAction::triggered, this, [this] { bringUp(); });
    trayAuto_ = menu->addAction("Otomatis layar kedua");
    trayAuto_->setCheckable(true);
    connect(trayAuto_, &QAction::triggered, this, [this] { setAutoScreen(!autoScreen_); });
    menu->addSeparator();
    QAction *actQuit = menu->addAction("Keluar");
    connect(actQuit, &QAction::triggered, qGuiApp, &QCoreApplication::quit);   // jendela boleh sedang tersembunyi
    tray_->setContextMenu(menu);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger) bringUp();
    });
    tray_->show();

    // Monitor dicolok / dicabut: tunggu sebentar sampai susunan layar stabil, lalu terapkan ulang.
    screenTimer_.setSingleShot(true);
    screenTimer_.setInterval(800);
    connect(&screenTimer_, &QTimer::timeout, this, &MainWindow::applyScreens);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this] { screenTimer_.start(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this] { screenTimer_.start(); });

    for (auto *w : workers_) w->start();

    auto *ui = new QTimer(this);   // status sidebar/toolbar dan total pemakaian internet
    ui->setInterval(500);
    connect(ui, &QTimer::timeout, this, &MainWindow::refreshUi);
    ui->start();
    refreshUi();
}

MainWindow::~MainWindow() {
    for (auto *w : workers_) w->requestStop();   // berhenti paralel
    for (auto *w : workers_) w->stop();          // lalu tunggu (file rekaman ditutup rapi)
}

QToolButton *MainWindow::makeButton(const QString &text, const QString &tip, bool checkable) {
    auto *b = new QToolButton;
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(checkable);
    b->setFocusPolicy(Qt::NoFocus);   // agar tombol keyboard tetap sampai ke jendela
    return b;
}

void MainWindow::buildUi() {
    auto *root = new QWidget;
    root->setObjectName("root");
    root->setStyleSheet(kStyle);
    root->setMouseTracking(true);
    setCentralWidget(root);
    auto *main = new QHBoxLayout(root);
    rootLayout_ = main;
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    // Sidebar: daftar kamera
    sidebar_ = new QWidget;
    sidebar_->setObjectName("sidebar");
    sidebar_->setFixedWidth(210);
    auto *sl = new QVBoxLayout(sidebar_);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);
    auto *brand = new QLabel("CC<span style='color:#50dc78'>TV</span> Monitor");
    brand->setObjectName("brand");
    brand->setTextFormat(Qt::RichText);
    sl->addWidget(brand);
    list_ = new QListWidget;
    list_->setFocusPolicy(Qt::NoFocus);
    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) { selectCamera(list_->row(it)); });
    connect(list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it) { toggleSolo(list_->row(it)); });
    sl->addWidget(list_, 1);
    main->addWidget(sidebar_);

    // Kanan: grid video + toolbar
    auto *right = new QWidget;
    auto *rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);

    host_ = new QWidget;
    host_->setStyleSheet("background:#000");
    host_->setMouseTracking(true);
    grid_ = new QGridLayout(host_);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setSpacing(1);
    rl->addWidget(host_, 1);

    toolbar_ = new QWidget;
    toolbar_->setObjectName("toolbar");
    auto *tl = new QHBoxLayout(toolbar_);
    tl->setContentsMargins(12, 8, 12, 8);
    tl->setSpacing(6);

    layoutGroup_ = new QButtonGroup(this);
    layoutGroup_->setExclusive(true);
    for (int l : kLayouts) {
        auto *b = makeButton(QString::number(l), QString("Tampilan %1 layar").arg(l), true);
        connect(b, &QToolButton::clicked, this, [this, l] { setLayoutCount(l); });
        layoutGroup_->addButton(b);
        layoutBtns_.push_back(b);
        tl->addWidget(b);
    }
    tl->addSpacing(10);
    auto *prev = makeButton("<", "Halaman sebelumnya");
    auto *next = makeButton(">", "Halaman berikutnya");
    connect(prev, &QToolButton::clicked, this, [this] { changePage(-1); });
    connect(next, &QToolButton::clicked, this, [this] { changePage(+1); });
    pageLabel_ = new QLabel;
    pageLabel_->setObjectName("info");
    tl->addWidget(prev);
    tl->addWidget(pageLabel_);
    tl->addWidget(next);
    tl->addSpacing(10);

    hdBtn_ = makeButton("SD", "Ganti kualitas SD / HD (H)");
    connect(hdBtn_, &QToolButton::clicked, this, &MainWindow::toggleHd);
    auto *snap = makeButton("Foto", "Ambil foto (S) - kamera terpilih, atau semua bila tidak ada yang dipilih");
    connect(snap, &QToolButton::clicked, this, &MainWindow::snapshot);
    recBtn_ = makeButton("Rekam", "Rekam (R) - kamera terpilih, atau semua bila tidak ada yang dipilih", true);
    recBtn_->setObjectName("rec");
    connect(recBtn_, &QToolButton::clicked, this, &MainWindow::toggleRecording);
    auto *full = makeButton("Layar Penuh", "Layar penuh (F / F11)");
    connect(full, &QToolButton::clicked, this, &MainWindow::toggleFullscreen);
    tl->addWidget(hdBtn_);
    tl->addWidget(snap);
    tl->addWidget(recBtn_);
    tl->addWidget(full);
    autoBtn_ = makeButton("Auto Layar 2", "Otomatis tampil layar penuh di monitor kedua (A)", true);
    connect(autoBtn_, &QToolButton::clicked, this, [this] { setAutoScreen(!autoScreen_); });
    tl->addWidget(autoBtn_);

    // Tombol mengambang (tampil saat layar penuh, saat toolbar tersembunyi) di pojok kanan atas.
    floatBtn_ = makeButton("Auto Layar 2", "Otomatis tampil layar penuh di monitor kedua (A)", true);
    floatBtn_->setParent(host_);
    floatBtn_->setStyleSheet("QToolButton{background:rgba(20,23,28,170);} QToolButton:checked{background:rgba(30,136,229,200);}");
    connect(floatBtn_, &QToolButton::clicked, this, [this] { setAutoScreen(!autoScreen_); });
    floatBtn_->hide();

    tl->addStretch(1);
    usageLabel_ = new QLabel;
    usageLabel_->setObjectName("info");
    tl->addWidget(usageLabel_);
    rl->addWidget(toolbar_);
    main->addWidget(right, 1);
}

void MainWindow::relayout() {
    for (auto *v : views_) {
        grid_->removeWidget(v);
        v->hide();
    }
    page_ = std::clamp(page_, 0, pageCount() - 1);
    const int n = int(views_.size());
    const int cols = std::max(1, int(std::lround(std::sqrt(double(layout_)))));
    for (int i = 0; i < 4; ++i) {
        grid_->setRowStretch(i, i < cols ? 1 : 0);
        grid_->setColumnStretch(i, i < cols ? 1 : 0);
    }
    std::fill(shown_.begin(), shown_.end(), false);
    if (solo_ >= 0) {
        grid_->addWidget(views_[solo_], 0, 0, cols, cols);   // satu kamera penuh
        shown_[solo_] = true;
    } else {
        const int first = page_ * layout_;
        for (int i = first; i < n && i < first + layout_; ++i) {
            grid_->addWidget(views_[i], (i - first) / cols, (i - first) % cols);
            shown_[i] = true;
        }
    }
    for (int i = 0; i < n; ++i) views_[i]->setVisible(shown_[i]);
    applyVisibility();
}

void MainWindow::applyVisibility() {
    const bool on = isVisible() && !isMinimized();   // tersembunyi (tray) / diminimize: tidak di-render
    for (int i = 0; i < int(workers_.size()); ++i) workers_[i]->setVisible(on && shown_[i]);
}

void MainWindow::applyChrome() {
    const bool fs = isFullScreen();   // layar penuh: hanya video
    sidebar_->setVisible(!fs);
    toolbar_->setVisible(!fs);
    floatBtn_->setVisible(fs);
    applyMargin();
    placeFloatBtn();
}

// Layar penuh: beri ruang kosong di semua sisi agar tidak terpotong tepi monitor/TV (overscan).
void MainWindow::applyMargin() {
    const int m = isFullScreen() ? marginPx_ : 0;
    rootLayout_->setContentsMargins(m, m, m, m);
}

void MainWindow::adjustMargin(int delta) {
    marginPx_ = std::clamp(marginPx_ + delta, 0, 300);
    QSettings(QCoreApplication::applicationDirPath() + "/settings.ini", QSettings::IniFormat)
        .setValue("screen_margin", marginPx_);
    applyMargin();
    QTimer::singleShot(0, this, [this] { placeFloatBtn(); });
}

void MainWindow::placeFloatBtn() {
    floatBtn_->adjustSize();
    floatBtn_->move(host_->width() - floatBtn_->width() - 12, 12);
    floatBtn_->raise();
}

void MainWindow::resizeEvent(QResizeEvent *e) {
    QMainWindow::resizeEvent(e);
    QTimer::singleShot(0, this, [this] { placeFloatBtn(); });   // setelah layout menetapkan ukuran host_
}

QScreen *MainWindow::secondScreen() {
    for (QScreen *s : QGuiApplication::screens())
        if (s != QGuiApplication::primaryScreen()) return s;
    return nullptr;
}

// Dari ikon tray: ke monitor kedua bila ada (otomatis ON); jika tidak ada, jendela biasa.
void MainWindow::bringUp() {
    if (autoScreen_ && secondScreen()) {
        applyScreens();
    } else {
        showNormal();
    }
    raise();
    activateWindow();
}

// Layar kedua: ada -> tampil otomatis layar penuh di sana; tidak ada (1 monitor) -> tidak ada otomatisasi
// (jendela disembunyikan, tetap berjalan di tray). Otomatisasi mati -> jendela biasa.
void MainWindow::applyScreens() {
    if (!autoScreen_) {
        if (!isVisible()) showNormal();
        return;
    }
    QScreen *second = secondScreen();
    if (!second) {
        if (isVisible()) hide();
        return;
    }
    if (isVisible() && isFullScreen() && screen() == second) return;   // sudah di tempatnya
    if (isFullScreen()) showNormal();
    setScreen(second);
    if (windowHandle()) windowHandle()->setScreen(second);
    setGeometry(second->geometry());
    showFullScreen();
}

void MainWindow::setAutoScreen(bool on) {
    autoScreen_ = on;
    QSettings(QCoreApplication::applicationDirPath() + "/settings.ini", QSettings::IniFormat)
        .setValue("auto_second_screen", on);
    applyScreens();
    refreshUi();
}

void MainWindow::startup(bool forceFullscreen) {
    if (qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") { resize(1280, 720); show(); return; }
    if (autoScreen_) {
        applyScreens();   // 1 monitor: tidak tampil sendiri (buka lewat ikon tray)
    } else if (forceFullscreen) {
        showFullScreen();
    } else {
        resize(1280, 720);
        show();
    }
}

void MainWindow::refreshUi() {
    double kbps = 0;   // jumlah kecepatan terima semua kamera
    for (int i = 0; i < int(workers_.size()); ++i) {
        const CameraWorker *w = workers_[i];
        kbps += w->kbps();
        QListWidgetItem *it = list_->item(i);
        it->setText(QString::fromUtf8("\xE2\x97\x8F ") + w->name() + (w->recording() ? QString("   REC") : QString()));
        it->setForeground(QColor(w->state() == CameraState::Live ? "#50dc78" : "#ffaa3c"));
    }
    if (list_->currentRow() != sel_) list_->setCurrentRow(sel_);

    for (int i = 0; i < int(layoutBtns_.size()); ++i) layoutBtns_[i]->setChecked(kLayouts[i] == layout_);
    pageLabel_->setText(solo_ >= 0 ? "Solo" : QString("%1 / %2").arg(page_ + 1).arg(pageCount()));
    const bool hd = !workers_.empty() && workers_[0]->hd();
    hdBtn_->setText(hd ? "HD" : "SD");
    hdBtn_->setStyleSheet(hd ? "QToolButton{background:#1e88e5;border-color:#1e88e5;color:white;}" : "");

    bool rec = false;
    for (int i = 0; i < int(workers_.size()); ++i)
        if ((sel_ < 0 || i == sel_) && workers_[i]->recording()) rec = true;
    recBtn_->setChecked(rec);
    for (auto *b : {autoBtn_, floatBtn_}) {
        b->setChecked(autoScreen_);
        b->setText(autoScreen_ ? "Auto Layar 2: ON" : "Auto Layar 2: OFF");
    }
    trayAuto_->setChecked(autoScreen_);

    const QString usage = QString("Internet realtime: %1 Mbps (terima / read)").arg(kbps / 1000.0, 0, 'f', 2);
    usageLabel_->setText(usage);
    setWindowTitle(QString("CCTV Monitor [%1] - %2").arg(QString(hd ? "HD" : "SD"), usage));
}

void MainWindow::selectCamera(int idx) {
    if (idx < 0 || idx >= int(views_.size())) return;
    sel_ = idx;
    for (int i = 0; i < int(views_.size()); ++i) views_[i]->setSelected(i == idx);
    bool changed = false;
    if (solo_ >= 0 && solo_ != idx) {
        solo_ = idx;
        changed = true;
    } else if (solo_ < 0 && idx / layout_ != page_) {
        page_ = idx / layout_;
        changed = true;
    }
    if (changed) relayout();
    refreshUi();
}

void MainWindow::setLayoutCount(int n) {
    layout_ = n;
    page_ = (sel_ >= 0) ? sel_ / layout_ : 0;
    relayout();
    refreshUi();
}

void MainWindow::changePage(int delta) {
    if (solo_ >= 0) return;
    page_ = std::clamp(page_ + delta, 0, pageCount() - 1);
    relayout();
    refreshUi();
}

void MainWindow::toggleSolo(int idx) {
    if (idx < 0 || idx >= int(views_.size())) return;
    solo_ = (solo_ == idx) ? -1 : idx;
    if (solo_ >= 0) selectCamera(solo_);   // selectCamera tidak melakukan apa-apa bila solo_ sudah sama
    relayout();
    refreshUi();
}

void MainWindow::toggleHd() {
    const bool hd = !(!workers_.empty() && workers_[0]->hd());   // SD <-> HD untuk semua kamera
    for (auto *w : workers_) w->setHd(hd);
    refreshUi();
}

void MainWindow::toggleFullscreen() { isFullScreen() ? showNormal() : showFullScreen(); }

void MainWindow::showCursor() {
    unsetCursor();
    hideCursorTimer_.start();
}

void MainWindow::changeEvent(QEvent *e) {
    QMainWindow::changeEvent(e);
    if (e->type() == QEvent::WindowStateChange) {   // diminimize: tidak di-render
        applyVisibility();
        applyChrome();
    }
}

void MainWindow::toggleRecording() {
    std::vector<CameraWorker *> target;
    if (sel_ >= 0) target.push_back(workers_[sel_]);
    else target = workers_;
    bool on = true;
    for (auto *w : target)
        if (w->recording()) on = false;
    for (auto *w : target) w->setRecording(on);
    for (auto *v : views_) v->update();
    refreshUi();
}

void MainWindow::snapshot() {
    QDir().mkpath(cfg_.snapshotsDir);
    const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    for (int i = 0; i < int(views_.size()); ++i) {
        if (sel_ >= 0 && i != sel_) continue;
        const QImage img = workers_[i]->image();
        if (!img.isNull() && views_[i]->isVisible() && workers_[i]->state() == CameraState::Live)
            img.save(QString("%1/cam%2_%3.jpg").arg(cfg_.snapshotsDir).arg(i + 1).arg(ts), "JPG", 90);
    }
}

void MainWindow::keyPressEvent(QKeyEvent *e) {
    const int k = e->key();
    if (k >= Qt::Key_1 && k <= Qt::Key_9) {
        toggleSolo(k - Qt::Key_1);
    } else if (k == Qt::Key_0 || k == Qt::Key_Escape) {
        if (solo_ >= 0) toggleSolo(solo_);
        else if (isFullScreen()) showNormal();
    } else if (k == Qt::Key_F || k == Qt::Key_F11) {
        toggleFullscreen();
    } else if (k == Qt::Key_R) {
        toggleRecording();
    } else if (k == Qt::Key_S) {
        snapshot();
    } else if (k == Qt::Key_H) {
        toggleHd();
    } else if (k == Qt::Key_A) {
        setAutoScreen(!autoScreen_);
    } else if (k == Qt::Key_Plus || k == Qt::Key_Equal) {
        adjustMargin(+4);          // margin layar penuh lebih besar
    } else if (k == Qt::Key_Minus) {
        adjustMargin(-4);
    } else if (k == Qt::Key_Left || k == Qt::Key_PageUp) {
        changePage(-1);
    } else if (k == Qt::Key_Right || k == Qt::Key_PageDown) {
        changePage(+1);
    } else if (k == Qt::Key_Q) {
        close();
    }
}
