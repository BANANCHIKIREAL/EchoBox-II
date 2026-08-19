#include "mainwindow.h"
#include "waveformslider.h"
#include "visualizer.h"
#include "backgroundwidget.h"
#include "libraryscanner.h"
#include <cmath>
#include <QFontDatabase>
#include "logo.h"
#include "icons.h"
#include "discordrpc.h"

#include <QApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QSlider>
#include <QLabel>
#include <QListWidget>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QStatusBar>
#include <QFileInfo>
#include <QMediaMetaData>
#include <QMessageBox>
#include <QFrame>
#include <QLineEdit>
#include <QComboBox>
#include <QSystemTrayIcon>
#include <QStackedWidget>
#include <QVideoWidget>
#include <QDirIterator>
#include <QRandomGenerator>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QTextStream>
#include <QDir>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QCryptographicHash>
#include <QTimer>
#include <QAudioSink>
#include <QAudioSource>
#include <QMediaDevices>
#include <QDialogButtonBox>
#include <QProcess>
#include <QDateTime>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDesktopServices>
#include <QPushButton>
#include <functional>
#include <numeric>
#include <algorithm>

// ─── APO shared-memory bridge (Windows) ──────────────────────────────────────
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "../apo/ring.h"

// ─── Static data ─────────────────────────────────────────────────────────────

static const QString kAppVersion = "1.4.1";
// Не /releases/latest — этот эндпоинт у GitHub сознательно игнорирует
// pre-release-версии (наши beta.*), поэтому берём общий список и находим
// самую новую версию сами (ниже, в checkForUpdates)
static const QString kUpdateApiUrl =
    "https://api.github.com/repos/BANANCHIKIREAL/EchoBox-II/releases";

const QStringList MainWindow::VIDEO_EXTS = {"mp4","mkv","avi","mov","webm","flv","wmv","m2ts"};
const QStringList MainWindow::MEDIA_FILTER = {
    "Медиафайлы (*.mp3 *.mp4 *.wav *.ogg *.flac *.aac *.m4a *.mkv *.avi *.mov *.webm *.opus *.wma *.wmv)",
    "Аудио (*.mp3 *.wav *.ogg *.flac *.aac *.m4a *.opus *.wma)",
    "Видео (*.mp4 *.mkv *.avi *.mov *.webm *.wmv)",
    "Все файлы (*)"
};

// ─── Custom playlist delegate (right-align duration) ─────────────────────────

class PlaylistDelegate : public QStyledItemDelegate {
public:
    bool showIcons = true;
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &o, const QModelIndex &i) const override {
        QSize s = QStyledItemDelegate::sizeHint(o, i);
        return {s.width(), showIcons ? 44 : 32};
    }

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        p->save();

        // Background
        if (opt.state & QStyle::State_Selected)
            p->fillRect(opt.rect, QColor(0x45,0x47,0x5a));
        else if (opt.state & QStyle::State_MouseOver)
            p->fillRect(opt.rect, QColor(0x2a,0x2b,0x3d));

        // Duplicate highlight — drawn after base bg, before content
        if (idx.data(Qt::UserRole + 10).toBool())
            p->fillRect(opt.rect, QColor(0xeb, 0xa0, 0xac, 40));

        const int iconSz = 36;
        const int margin = 4;
        int textX = opt.rect.left() + 10;

        // Icon
        if (showIcons) {
            QIcon icon = idx.data(Qt::DecorationRole).value<QIcon>();
            QRect ir(opt.rect.left() + margin,
                     opt.rect.top() + (opt.rect.height() - iconSz) / 2,
                     iconSz, iconSz);
            if (!icon.isNull()) {
                QPixmap src = icon.pixmap(iconSz, iconSz);
                // Render with rounded corners into an off-screen pixmap
                QPixmap rounded(iconSz, iconSz);
                rounded.fill(Qt::transparent);
                {
                    QPainter rp(&rounded);
                    rp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
                    QPainterPath rpath;
                    rpath.addRoundedRect(QRectF(0, 0, iconSz, iconSz), 5, 5);
                    rp.setClipPath(rpath);
                    rp.drawPixmap(0, 0, src.scaled(iconSz, iconSz,
                        Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                }
                p->setRenderHint(QPainter::SmoothPixmapTransform);
                p->drawPixmap(ir, rounded);
            } else {
                // placeholder
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(0x31,0x32,0x44));
                p->setRenderHint(QPainter::Antialiasing);
                p->drawRoundedRect(ir, 5, 5);
                p->setPen(QColor(0x6c,0x70,0x86));
                p->setFont(QFont("Segoe UI", 14));
                p->drawText(ir, Qt::AlignCenter, "♪");
            }
            textX = ir.right() + 8;
        }

        // Duration on right
        const QString dur = idx.data(Qt::UserRole + 1).toString();
        int rightEdge = opt.rect.right() - 8;
        if (!dur.isEmpty()) {
            QFont df = p->font(); df.setPointSize(9); p->setFont(df);
            p->setPen(QColor(0x6c,0x70,0x86));
            int dw = p->fontMetrics().horizontalAdvance(dur) + 4;
            p->drawText(QRect(opt.rect.right() - dw - 8, opt.rect.top(), dw + 8, opt.rect.height()),
                        Qt::AlignRight | Qt::AlignVCenter, dur);
            rightEdge = opt.rect.right() - dw - 12;
        }

        // Track name
        QString text = idx.data(Qt::DisplayRole).toString();
        QFont tf = p->font(); tf.setPointSize(10); p->setFont(tf);
        p->setPen((opt.state & QStyle::State_Selected)
                  ? QColor(0xcb,0xa6,0xf7) : QColor(0xcd,0xd6,0xf4));
        p->drawText(QRect(textX, opt.rect.top(), rightEdge - textX, opt.rect.height()),
                    Qt::AlignVCenter | Qt::TextSingleLine, text);

        p->restore();
    }
};

static PlaylistDelegate *g_delegate = nullptr;

// ─── Constructor / Destructor ─────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings("EchoBox", "EchoBoxII")
{
    m_player      = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    // Metadata-only reader — no audio output, used for background icon scanning
    m_metaReader = new QMediaPlayer(this);
    connect(m_metaReader, &QMediaPlayer::metaDataChanged,
            this, &MainWindow::handleMetaReaderUpdate);
    connect(m_metaReader, &QMediaPlayer::mediaStatusChanged, this,
        [this](QMediaPlayer::MediaStatus s) {
            if (s == QMediaPlayer::LoadedMedia  ||
                s == QMediaPlayer::InvalidMedia ||
                s == QMediaPlayer::NoMedia)
                handleMetaReaderUpdate();
        });

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    {
        QAudioFormat fmt;
        fmt.setSampleRate(48000);
        fmt.setChannelCount(2);
        fmt.setSampleFormat(QAudioFormat::Float);
        m_audioBufferOut = new QAudioBufferOutput(fmt, this);
    }
    m_player->setAudioBufferOutput(m_audioBufferOut);
    connect(m_audioBufferOut, &QAudioBufferOutput::audioBufferReceived,
            this, &MainWindow::onAudioBuffer);
#endif

    // Discord RPC — замени ID на свой: discord.com/developers/applications
    m_discord = new DiscordRPC("1516933454309228684", this);

    m_streamArtNam = new QNetworkAccessManager(this);

    // Crossfade fade-in timer
    m_fadeInTimer = new QTimer(this);
    m_fadeInTimer->setInterval(40);
    connect(m_fadeInTimer, &QTimer::timeout, [this]{
        m_fadeFactor = qMin(1.0f, m_fadeFactor + 1.0f / float(m_cfg.crossfadeSecs * 25));
        applyVolume();
        if (m_fadeFactor >= 1.0f) m_fadeInTimer->stop();
    });

    setupMenuBar();
    setupUi();
    setupTray();
    setupConnections();
    applyTheme();
    loadSettings();

    setAcceptDrops(true);
    setMinimumSize(720, 540);
    resize(940, 660);
    setWindowTitle("EchoBox II");

    const QIcon icon(createLogo(128));
    setWindowIcon(icon);

    // Тихая фоновая проверка обновлений (не мешает старту, не спамит окнами)
    QTimer::singleShot(3000, this, [this]{ checkForUpdates(false); });
}

MainWindow::~MainWindow() { apoCloseRing(); saveSettings(); }

// ─── Menu bar ────────────────────────────────────────────────────────────────

void MainWindow::setupMenuBar() {
    // ── Файл ──────────────────────────────────────────────────────────────────
    QMenu *fm = menuBar()->addMenu("&Файл");
    fm->addAction("&Открыть файлы...", QKeySequence(Qt::CTRL | Qt::Key_O),
                  this, &MainWindow::openFiles);
    fm->addAction("Открыть &папку...", this, &MainWindow::openFolder);
    fm->addAction("🔗 Открыть по &ссылке...", QKeySequence(Qt::CTRL | Qt::Key_U),
                  this, &MainWindow::openUrlDialog);
    fm->addAction("📚 Сканировать библиотеку", this, &MainWindow::scanLibrary);
    m_recentMenu = fm->addMenu("&Недавние файлы");
    fm->addSeparator();
    fm->addAction("&Сохранить плейлист...", QKeySequence(Qt::CTRL | Qt::Key_S),
                  this, &MainWindow::savePlaylist);
    fm->addAction("&Загрузить плейлист...", QKeySequence(Qt::CTRL | Qt::Key_L),
                  this, &MainWindow::loadPlaylist);
    fm->addSeparator();
    fm->addAction("&Выход", QKeySequence(Qt::CTRL | Qt::Key_Q),
                  qApp, &QApplication::quit);

    // ── Воспроизведение ───────────────────────────────────────────────────────
    QMenu *pm = menuBar()->addMenu("&Воспроизведение");
    pm->addAction("Играть / Пауза", QKeySequence(Qt::Key_Space),
                  this, &MainWindow::togglePlayPause);
    pm->addAction("Стоп",       this, &MainWindow::stop);
    pm->addAction("Предыдущий", QKeySequence(Qt::CTRL | Qt::Key_Left),
                  this, &MainWindow::previous);
    pm->addAction("Следующий",  QKeySequence(Qt::CTRL | Qt::Key_Right),
                  this, &MainWindow::next);
    pm->addSeparator();

    m_shuffleAct = pm->addAction("Перемешать", this, &MainWindow::toggleShuffle);
    m_shuffleAct->setCheckable(true);

    QMenu *repMenu = pm->addMenu("Повтор");
    auto *rg = new QActionGroup(this);
    m_repeatOffAct = repMenu->addAction("Выкл.");
    m_repeatOneAct = repMenu->addAction("Один трек");
    m_repeatAllAct = repMenu->addAction("Весь плейлист");
    for (auto *a : {m_repeatOffAct, m_repeatOneAct, m_repeatAllAct})
        { a->setCheckable(true); rg->addAction(a); }
    m_repeatOffAct->setChecked(true);
    connect(m_repeatOffAct, &QAction::triggered, [this]{ m_repeat = RepeatMode::Off;  updateRepeatButton(); });
    connect(m_repeatOneAct, &QAction::triggered, [this]{ m_repeat = RepeatMode::One;  updateRepeatButton(); });
    connect(m_repeatAllAct, &QAction::triggered, [this]{ m_repeat = RepeatMode::All;  updateRepeatButton(); });

    QMenu *speedMenu = pm->addMenu("Скорость");
    auto *sg = new QActionGroup(this);
    const QStringList sl = {"0.5×","0.75×","1.0×","1.25×","1.5×","2.0×"};
    for (int i = 0; i < sl.size(); ++i) {
        QAction *a = speedMenu->addAction(sl[i]);
        a->setCheckable(true);
        sg->addAction(a);
        if (i == 2) a->setChecked(true);
        connect(a, &QAction::triggered, [this, i]{ m_speedCombo->setCurrentIndex(i); });
    }

    pm->addSeparator();
    QMenu *xfMenu = pm->addMenu("Кроссфейд");
    auto  *xfg    = new QActionGroup(this);
    const struct { const char *label; int secs; } xfItems[] = {
        {"Выкл.", 0}, {"2 сек.", 2}, {"3 сек.", 3}, {"5 сек.", 5}
    };
    for (const auto &it : xfItems) {
        QAction *a = xfMenu->addAction(it.label);
        a->setCheckable(true); xfg->addAction(a);
        if (it.secs == 0) a->setChecked(true);
        const int secs = it.secs;
        connect(a, &QAction::triggered, [this, secs]{
            m_cfg.crossfadeSecs = secs;
            m_settings.setValue("cfg/crossfadeSecs", secs);
        });
    }

    // ── Вид ───────────────────────────────────────────────────────────────────
    QMenu *vm = menuBar()->addMenu("&Вид");
    m_miniPlayerAct = vm->addAction("Мини-плеер", QKeySequence(Qt::Key_F11),
                                    this, &MainWindow::toggleMiniPlayer);
    m_miniPlayerAct->setCheckable(true);
    m_alwaysOnTopAct = vm->addAction("Поверх всех окон", this, &MainWindow::toggleAlwaysOnTop);
    m_alwaysOnTopAct->setCheckable(true);

    // ── Настройки / Справка ───────────────────────────────────────────────────
    QMenu *stMenu = menuBar()->addMenu("&Настройки");
    stMenu->addAction("Параметры...", QKeySequence(Qt::CTRL | Qt::Key_Comma),
                      this, &MainWindow::openSettings);

    QMenu *helpMenu = menuBar()->addMenu("&Справка");
    helpMenu->addAction("О программе", this, &MainWindow::showAbout);
    helpMenu->addAction("Проверить обновления...", [this]{ checkForUpdates(true); });
}

// ─── UI ──────────────────────────────────────────────────────────────────────

void MainWindow::setupUi() {
    m_aurora = new AuroraWidget(this);
    setCentralWidget(m_aurora);
    QVBoxLayout *root = new QVBoxLayout(m_aurora);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ══ TOP PANEL (album art + info + controls) ══════════════════════════════
    m_topWidget = new QWidget(this);
    QHBoxLayout *topL = new QHBoxLayout(m_topWidget);
    topL->setContentsMargins(14, 14, 14, 10);
    topL->setSpacing(18);

    // ── Left: media stack ────────────────────────────────────────────────────
    m_mediaStack = new QStackedWidget(this);
    m_mediaStack->setFixedSize(230, 230);
    m_mediaStack->setObjectName("mediaStack");

    m_albumArt = new QLabel(this);
    m_albumArt->setAlignment(Qt::AlignCenter);
    m_albumArt->setFixedSize(230, 230);

    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->setFixedSize(230, 230);
    m_player->setVideoOutput(m_videoWidget);

    m_mediaStack->addWidget(m_albumArt);
    m_mediaStack->addWidget(m_videoWidget);
    topL->addWidget(m_mediaStack);

    // ── Right: info + visualizer + controls ──────────────────────────────────
    QWidget *rp = new QWidget(this);
    QVBoxLayout *rl = new QVBoxLayout(rp);
    rl->setContentsMargins(4, 0, 0, 0);
    rl->setSpacing(0);

    // ── Track info ────────────────────────────────────────────────────────────
    m_titleLabel = new QLabel("EchoBox II", this);
    m_titleLabel->setObjectName("titleLabel");

    m_artistLabel = new QLabel("Перетащи файлы или открой через меню Файл", this);
    m_artistLabel->setObjectName("artistLabel");

    m_albumLabel = new QLabel("", this);
    m_albumLabel->setObjectName("albumLabel");

    rl->addWidget(m_titleLabel);
    rl->addSpacing(2);
    rl->addWidget(m_artistLabel);
    rl->addWidget(m_albumLabel);
    rl->addStretch(1);

    // ── Visualizer ────────────────────────────────────────────────────────────
    m_visualizer = new Visualizer(this);
    rl->addWidget(m_visualizer);
    rl->addSpacing(8);

    // ── Seek bar ──────────────────────────────────────────────────────────────
    m_seekSlider = new WaveformSlider(this);
    m_seekSlider->setRange(0, 0);
    m_seekSlider->setObjectName("seekSlider");

    m_timeLabel = new QLabel("0:00 / 0:00", this);
    m_timeLabel->setObjectName("timeLabel");
    m_timeLabel->setMinimumWidth(115);
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_timeLabel->setToolTip("Клик — переключить оставшееся/прошедшее");
    m_timeLabel->setCursor(Qt::PointingHandCursor);

    QHBoxLayout *seekL = new QHBoxLayout;
    seekL->setSpacing(10);
    seekL->setContentsMargins(0, 0, 0, 0);
    seekL->addWidget(m_seekSlider);
    seekL->addWidget(m_timeLabel);
    rl->addLayout(seekL);
    rl->addSpacing(14);

    // ── Button factory ────────────────────────────────────────────────────────
    auto mkBtn = [this](const QString &tip, const QString &id, int sz) -> QToolButton* {
        auto *b = new QToolButton(this);
        b->setToolTip(tip); b->setObjectName(id); b->setFixedSize(sz, sz);
        return b;
    };

    // ── Main transport: [⏮]  [▶▶BIG]  [⏭]   [⏹][⇄][↺] ──────────────────────
    m_prevBtn      = mkBtn("Предыдущий  Ctrl+←",         "ctrlBtn",  40);
    m_playPauseBtn = mkBtn("Играть / Пауза  Пробел",     "playBtn",  60);
    m_nextBtn      = mkBtn("Следующий  Ctrl+→",           "ctrlBtn",  40);
    m_stopBtn      = mkBtn("Стоп  S",                     "ctrlBtn",  34);
    m_shuffleBtn   = mkBtn("Перемешать",                  "toggleBtn",34);
    m_shuffleBtn->setCheckable(true);
    m_repeatBtn    = mkBtn("Повтор: выкл.",               "toggleBtn",34);
    m_repeatBtn->setCheckable(true);

    QHBoxLayout *c1 = new QHBoxLayout;
    c1->setSpacing(6);
    c1->addStretch();
    c1->addWidget(m_prevBtn);
    c1->addSpacing(6);
    c1->addWidget(m_playPauseBtn);
    c1->addSpacing(6);
    c1->addWidget(m_nextBtn);
    c1->addSpacing(20);
    c1->addWidget(m_stopBtn);
    c1->addSpacing(2);
    c1->addWidget(m_shuffleBtn);
    c1->addWidget(m_repeatBtn);
    c1->addStretch();
    rl->addLayout(c1);
    rl->addSpacing(10);

    // ── Volume + Speed row ────────────────────────────────────────────────────
    m_muteBtn = mkBtn("Выкл. звук  M", "muteBtn", 30);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setObjectName("volSlider");
    m_volumeSlider->setFixedWidth(130);

    m_volumeLabel = new QLabel("70%", this);
    m_volumeLabel->setObjectName("volLabel");
    m_volumeLabel->setMinimumWidth(38);

    m_speedCombo = new QComboBox(this);
    m_speedCombo->setObjectName("speedCombo");
    m_speedCombo->addItems({"0.5×","0.75×","1.0×","1.25×","1.5×","2.0×"});
    m_speedCombo->setCurrentIndex(2);
    m_speedCombo->setToolTip("Скорость воспроизведения");
    m_speedCombo->setFixedWidth(68);


    // Музыка → микрофон (через EchoBox APO, установи apo\install.bat)
    m_micBtn = mkBtn("ЛКМ — музыка в микрофон вкл/выкл\nПКМ — громкость и «только музыка»", "toggleBtn", 30);
    m_micBtn->setCheckable(true);
    m_micBtn->setIcon(Ico::microphone(QColor(0xa6, 0xad, 0xc8), 18));
    m_micBtn->setIconSize({18, 18});
    m_micBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_micBtn, &QToolButton::customContextMenuRequested,
            this, [this]{ showMicMenu(); });

    QHBoxLayout *c2 = new QHBoxLayout;
    c2->setSpacing(6);
    c2->addStretch();
    c2->addWidget(m_muteBtn);
    c2->addWidget(m_volumeSlider);
    c2->addWidget(m_volumeLabel);
    c2->addSpacing(16);
    c2->addWidget(m_speedCombo);
    c2->addSpacing(12);
    c2->addWidget(m_micBtn);
    c2->addStretch();
    rl->addLayout(c2);
    rl->addSpacing(4);

    topL->addWidget(rp, 1);
    root->addWidget(m_topWidget);

    // ── Separator ────────────────────────────────────────────────────────────
    m_separator = new QFrame(this);
    m_separator->setFrameShape(QFrame::HLine);
    m_separator->setObjectName("separator");
    root->addWidget(m_separator);

    // ══ MINI BAR (visible only in mini player mode) ══════════════════════════
    m_miniBar = new QWidget(this);
    m_miniBar->setObjectName("miniBar");
    m_miniBar->setAutoFillBackground(false);
    m_miniBar->hide();

    QHBoxLayout *miniL = new QHBoxLayout(m_miniBar);
    miniL->setContentsMargins(6, 4, 6, 4);
    miniL->setSpacing(4);

    const QColor mc(0xcd,0xd6,0xf4);
    const QColor pc(0x1e,0x1e,0x2e);
    const QColor tc(0xa6,0xad,0xc8);

    // Album art
    m_miniAlbumArt = new QLabel(this);
    m_miniAlbumArt->setObjectName("miniAlbumArt");
    m_miniAlbumArt->setFixedSize(40, 40);
    m_miniAlbumArt->setScaledContents(true);

    // Transport
    auto *miniPrev = new QToolButton(this); miniPrev->setObjectName("ctrlBtn"); miniPrev->setFixedSize(28,28);
    miniPrev->setIcon(Ico::prev(mc,16)); miniPrev->setIconSize({16,16});

    m_miniPlayBtn = new QToolButton(this); m_miniPlayBtn->setObjectName("playBtn"); m_miniPlayBtn->setFixedSize(36,36);
    m_miniPlayBtn->setIcon(Ico::play(pc,22)); m_miniPlayBtn->setIconSize({22,22});

    auto *miniNext = new QToolButton(this); miniNext->setObjectName("ctrlBtn"); miniNext->setFixedSize(28,28);
    miniNext->setIcon(Ico::next(mc,16)); miniNext->setIconSize({16,16});

    // Title
    m_miniTitle = new QLabel("EchoBox II", this);
    m_miniTitle->setObjectName("miniTitle");
    m_miniTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Shuffle + Repeat
    m_miniShuffleBtn = new QToolButton(this); m_miniShuffleBtn->setObjectName("toggleBtn");
    m_miniShuffleBtn->setFixedSize(26,26); m_miniShuffleBtn->setCheckable(true);
    m_miniShuffleBtn->setIcon(Ico::shuffle(tc,15)); m_miniShuffleBtn->setIconSize({15,15});

    m_miniRepeatBtn = new QToolButton(this); m_miniRepeatBtn->setObjectName("toggleBtn");
    m_miniRepeatBtn->setFixedSize(26,26); m_miniRepeatBtn->setCheckable(true);
    m_miniRepeatBtn->setIcon(Ico::repeatAll(tc,15)); m_miniRepeatBtn->setIconSize({15,15});

    // Volume
    m_miniMuteBtn = new QToolButton(this); m_miniMuteBtn->setObjectName("muteBtn");
    m_miniMuteBtn->setFixedSize(24,24);
    m_miniMuteBtn->setIcon(Ico::volume(3,mc,18)); m_miniMuteBtn->setIconSize({18,18});

    m_miniVolSlider = new QSlider(Qt::Horizontal, this);
    m_miniVolSlider->setObjectName("volSlider");
    m_miniVolSlider->setRange(0, 100); m_miniVolSlider->setValue(70);
    m_miniVolSlider->setFixedWidth(80);

    // Expand
    auto *miniExpand = new QToolButton(this);
    miniExpand->setObjectName("ctrlBtn"); miniExpand->setFixedSize(26,26);
    miniExpand->setIcon(Ico::expand(mc, 13)); miniExpand->setIconSize({13,13});
    miniExpand->setToolTip("Полный режим  F11");

    // Свернуть / закрыть — без системной рамки в мини-режиме своих кнопок нет
    auto *miniMinimize = new QToolButton(this);
    miniMinimize->setObjectName("ctrlBtn"); miniMinimize->setFixedSize(26,26);
    miniMinimize->setIcon(Ico::minimize(mc, 13)); miniMinimize->setIconSize({13,13});
    miniMinimize->setToolTip("Свернуть");

    auto *miniClose = new QToolButton(this);
    miniClose->setObjectName("ctrlBtn"); miniClose->setFixedSize(26,26);
    miniClose->setIcon(Ico::closeIcon(mc, 12)); miniClose->setIconSize({12,12});
    miniClose->setToolTip("Закрыть");

    // На всю ширину экрана, прижать к верху
    m_miniDockBtn = new QToolButton(this);
    m_miniDockBtn->setObjectName("toggleBtn"); m_miniDockBtn->setFixedSize(26,26);
    m_miniDockBtn->setCheckable(true);
    m_miniDockBtn->setIcon(Ico::dockTop(tc, 14)); m_miniDockBtn->setIconSize({14,14});
    m_miniDockBtn->setToolTip("На всю ширину экрана, к верху");

    m_miniWaveform = new WaveformSlider(this);
    m_miniWaveform->setFixedHeight(36);
    m_miniWaveform->setMinimumWidth(80);
    m_miniWaveform->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    miniL->addWidget(m_miniAlbumArt);
    miniL->addSpacing(2);
    miniL->addWidget(miniPrev);
    miniL->addWidget(m_miniPlayBtn);
    miniL->addWidget(miniNext);
    miniL->addWidget(m_miniTitle);
    miniL->addWidget(m_miniWaveform, 1);
    miniL->addWidget(m_miniShuffleBtn);
    miniL->addWidget(m_miniRepeatBtn);
    miniL->addSpacing(4);
    miniL->addWidget(m_miniMuteBtn);
    miniL->addWidget(m_miniVolSlider);
    miniL->addSpacing(2);
    miniL->addWidget(m_miniDockBtn);
    miniL->addWidget(miniExpand);
    miniL->addWidget(miniMinimize);
    miniL->addWidget(miniClose);
    root->addWidget(m_miniBar);

    connect(miniPrev,         &QToolButton::clicked, this, &MainWindow::previous);
    connect(miniNext,         &QToolButton::clicked, this, &MainWindow::next);
    connect(miniExpand,       &QToolButton::clicked, this, &MainWindow::toggleMiniPlayer);
    connect(miniMinimize,     &QToolButton::clicked, this, &MainWindow::showMinimized);
    connect(miniClose,        &QToolButton::clicked, this, &MainWindow::close);
    connect(m_miniDockBtn,    &QToolButton::clicked, this, &MainWindow::toggleMiniDock);
    connect(m_miniPlayBtn,    &QToolButton::clicked, this, &MainWindow::togglePlayPause);
    connect(m_miniShuffleBtn, &QToolButton::clicked, this, &MainWindow::toggleShuffle);
    connect(m_miniRepeatBtn,  &QToolButton::clicked, this, &MainWindow::cycleRepeat);
    connect(m_miniMuteBtn,    &QToolButton::clicked, this, &MainWindow::toggleMute);
    connect(m_miniVolSlider,  &QSlider::valueChanged, this, &MainWindow::setVolume);

    // Мини-плеер безрамочный — эти виджеты служат "ручкой" для перетаскивания окна
    m_miniBar->installEventFilter(this);
    m_miniTitle->installEventFilter(this);
    m_miniAlbumArt->installEventFilter(this);

    // ══ PLAYLIST PANEL ═══════════════════════════════════════════════════════
    m_playlistPanel = new QWidget(this);
    QVBoxLayout *plL = new QVBoxLayout(m_playlistPanel);
    plL->setContentsMargins(12, 4, 12, 10);
    plL->setSpacing(4);

    // ── Tabs row ─────────────────────────────────────────────────────────────
    {
        QHBoxLayout *tabRow = new QHBoxLayout;
        tabRow->setSpacing(4);
        tabRow->setContentsMargins(0, 0, 0, 0);

        m_tabBar = new QTabBar(this);
        m_tabBar->setObjectName("playlistTabBar");
        m_tabBar->setExpanding(false);
        m_tabBar->setMovable(false);
        m_tabBar->addTab("Плейлист 1");

        auto *newPlBtn = new QToolButton(this);
        newPlBtn->setText("+");
        newPlBtn->setObjectName("smallBtn");
        newPlBtn->setFixedSize(26, 26);
        newPlBtn->setToolTip("Новый плейлист");

        tabRow->addWidget(m_tabBar, 1);
        tabRow->addWidget(newPlBtn);
        plL->addLayout(tabRow);

        connect(newPlBtn, &QToolButton::clicked, this, &MainWindow::newPlaylist);
        connect(m_tabBar, &QTabBar::currentChanged,      this, &MainWindow::onTabChanged);
        connect(m_tabBar, &QTabBar::tabBarDoubleClicked, this, &MainWindow::onTabDoubleClicked);
        m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_tabBar, &QWidget::customContextMenuRequested,
                this, &MainWindow::onTabContextMenu);
    }

    m_playlists.append({"Плейлист 1", {}, -1});

    // Header row
    QHBoxLayout *plH = new QHBoxLayout;
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("  Поиск по названию, исполнителю, альбому...");
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setClearButtonEnabled(true);

    m_playlistInfo = new QLabel("0 треков", this);
    m_playlistInfo->setObjectName("playlistInfo");

    auto mkSmall = [this](const QString &t, const QString &tip) {
        auto *b = new QToolButton(this);
        b->setText(t); b->setToolTip(tip);
        b->setObjectName("smallBtn"); b->setFixedSize(26, 26);
        return b;
    };
    auto *upBtn  = mkSmall("↑","Вверх");
    auto *dnBtn  = mkSmall("↓","Вниз");
    auto *rmBtn  = mkSmall("✕","Удалить  Del");
    auto *clrBtn = mkSmall("⊘","Очистить плейлист");

    plH->addWidget(m_searchEdit, 1);
    plH->addWidget(m_playlistInfo);
    plH->addWidget(upBtn); plH->addWidget(dnBtn);
    plH->addWidget(rmBtn); plH->addWidget(clrBtn);
    plL->addLayout(plH);

    // List
    m_playlistWidget = new QListWidget(this);
    m_playlistWidget->setObjectName("playlist");
    m_playlistWidget->setAlternatingRowColors(true);
    m_playlistWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_playlistWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_playlistWidget->setDragDropMode(QAbstractItemView::InternalMove);
    g_delegate = new PlaylistDelegate(m_playlistWidget);
    m_playlistWidget->setItemDelegate(g_delegate);
    m_playlistWidget->setIconSize({36, 36});
    plL->addWidget(m_playlistWidget, 1);
    root->addWidget(m_playlistPanel, 1);

    connect(upBtn,  &QToolButton::clicked, this, &MainWindow::moveTrackUp);
    connect(dnBtn,  &QToolButton::clicked, this, &MainWindow::moveTrackDown);
    connect(rmBtn,  &QToolButton::clicked, this, &MainWindow::removeSelectedTracks);
    connect(clrBtn, &QToolButton::clicked, this, &MainWindow::clearPlaylist);

    // ── Transport icons ───────────────────────────────────────────────────────
    {
        const QColor ctrl(0xcd, 0xd6, 0xf4);
        const QColor play(0x1e, 0x1e, 0x2e);
        m_prevBtn->setIcon(Ico::prev(ctrl, 22));        m_prevBtn->setIconSize({22,22});
        m_playPauseBtn->setIcon(Ico::play(play, 30));  m_playPauseBtn->setIconSize({30,30});
        m_nextBtn->setIcon(Ico::next(ctrl, 22));        m_nextBtn->setIconSize({22,22});
        m_stopBtn->setIcon(Ico::stop(ctrl, 16));        m_stopBtn->setIconSize({16,16});
        m_muteBtn->setIcon(Ico::volume(3, ctrl, 20));   m_muteBtn->setIconSize({20,20});
        m_shuffleBtn->setIcon(Ico::shuffle(ctrl, 18));  m_shuffleBtn->setIconSize({18,18});
        m_repeatBtn->setIcon(Ico::repeatAll(ctrl, 18)); m_repeatBtn->setIconSize({18,18});
    }

    updateAlbumArt();
    statusBar()->showMessage("EchoBox II  —  открой файлы или перетащи их в окно");
}

// ─── System tray ─────────────────────────────────────────────────────────────

void MainWindow::setupTray() {
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient g(0, 0, 32, 32);
    g.setColorAt(0, QColor(0xcb, 0xa6, 0xf7));
    g.setColorAt(1, QColor(0x89, 0xb4, 0xfa));
    p.setBrush(g); p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, 30, 30);
    p.setPen(QColor(0x1e, 0x1e, 0x2e));
    p.setFont(QFont("Segoe UI", 15, QFont::Bold));
    p.drawText(QRect(0, 0, 32, 32), Qt::AlignCenter, "♪");

    m_tray = new QSystemTrayIcon(QIcon(pm), this);
    m_tray->setToolTip("EchoBox II");

    auto *tm = new QMenu(this);
    m_trayPlayAct = tm->addAction("▶  Играть", this, &MainWindow::togglePlayPause);
    tm->addAction("⏭  Следующий", this, &MainWindow::next);
    tm->addSeparator();
    tm->addAction("Показать окно", this, [this]{ show(); raise(); activateWindow(); });
    tm->addAction("Выход", qApp, &QApplication::quit);
    m_tray->setContextMenu(tm);
    m_tray->show();

    connect(m_tray, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::DoubleClick) { show(); raise(); activateWindow(); }
    });
}

// ─── Connections ─────────────────────────────────────────────────────────────

void MainWindow::setupConnections() {
    connect(m_playPauseBtn, &QToolButton::clicked, this, &MainWindow::togglePlayPause);
    connect(m_stopBtn,      &QToolButton::clicked, this, &MainWindow::stop);
    connect(m_prevBtn,      &QToolButton::clicked, this, &MainWindow::previous);
    connect(m_nextBtn,      &QToolButton::clicked, this, &MainWindow::next);
    connect(m_shuffleBtn,   &QToolButton::clicked, this, &MainWindow::toggleShuffle);
    connect(m_repeatBtn,    &QToolButton::clicked, this, &MainWindow::cycleRepeat);
    connect(m_muteBtn,      &QToolButton::clicked, this, &MainWindow::toggleMute);
    connect(m_micBtn,       &QToolButton::clicked, this, &MainWindow::toggleMicRouting);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::setVolume);
    connect(m_speedCombo,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSpeedChanged);

    connect(m_seekSlider, &WaveformSlider::sliderPressed,  [this]{ m_seeking = true; });
    connect(m_seekSlider, &WaveformSlider::sliderReleased, [this]{
        m_player->setPosition(m_seekSlider->value());
        m_seeking = false;
    });
    // Share peaks and keep mini waveform in sync
    connect(m_seekSlider, &WaveformSlider::peaksReady, m_miniWaveform, &WaveformSlider::setPeaks);
    connect(m_seekSlider, &WaveformSlider::peaksReady, this, &MainWindow::onWaveformPeaksReady);
    connect(m_miniWaveform, &WaveformSlider::sliderPressed,  [this]{ m_seeking = true; });
    connect(m_miniWaveform, &WaveformSlider::sliderReleased, [this]{
        m_player->setPosition(m_miniWaveform->value());
        m_seeking = false;
    });
    m_timeLabel->installEventFilter(this);

    connect(m_player, &QMediaPlayer::durationChanged,
            this, &MainWindow::onDurationChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &MainWindow::onPositionChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, &MainWindow::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &MainWindow::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::metaDataChanged,
            this, &MainWindow::onMetaDataChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &MainWindow::onError);

    connect(m_searchEdit,     &QLineEdit::textChanged,
            this, &MainWindow::onSearchChanged);
    connect(m_playlistWidget, &QListWidget::itemActivated,
            this, &MainWindow::onTrackActivated);
    connect(m_playlistWidget, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onPlaylistContextMenu);
    connect(m_playlistWidget->model(), &QAbstractItemModel::rowsMoved,
            this, &MainWindow::rebuildPlaylistFromWidget);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev) {
    if (obj == m_timeLabel && ev->type() == QEvent::MouseButtonPress) {
        toggleRemainingTime();
        return true;
    }
    // Мини-плеер безрамочный (нет системного заголовка) — тащим окно за
    // обложку/название/пустое место панели вместо системной шапки
    if (m_miniPlayer && (obj == m_miniBar || obj == m_miniTitle || obj == m_miniAlbumArt)) {
        if (ev->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(ev);
            if (me->button() == Qt::LeftButton) {
                m_miniDragging   = true;
                m_miniDragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
            }
        } else if (ev->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent*>(ev);
            if (m_miniDragging && (me->buttons() & Qt::LeftButton))
                move(me->globalPosition().toPoint() - m_miniDragOffset);
        } else if (ev->type() == QEvent::MouseButtonRelease) {
            m_miniDragging = false;
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

// ─── Theme ───────────────────────────────────────────────────────────────────

void MainWindow::applyTheme() {
    // Compute accent color variants
    const QColor ac = m_cfg.accentColor;
    const QString acH = ac.name();
    const QString acL = ac.lighter(112).name();   // play btn hover
    const QString acD = ac.darker(112).name();    // play btn pressed
    // Checked bg: dark panel tinted with accent
    const QColor acBg(qMax(0,ac.red()/5+0x18), qMax(0,ac.green()/5+0x14), qMax(0,ac.blue()/5+0x28));
    const QString acBgH = acBg.name();

    // Font
    const int fs = m_cfg.fontSizeIdx == 0 ? 11 : m_cfg.fontSizeIdx == 2 ? 15 : 13;
    const QString family = m_cfg.fontFamily.isEmpty() ? "Segoe UI" : m_cfg.fontFamily;
    QFont appFont(family, fs);
    qApp->setFont(appFont);

    QString ss = R"(
        /* ── Aurora background shows through; no global bg ── */
        QMainWindow { background: transparent; }
        QWidget {
            color: #cdd6f4;
            font-family: "FONTFAMILY", "Yu Gothic UI", sans-serif;
            font-size: FONTPX;
        }
        /* Semi-transparent panels so aurora bleeds through */
        QWidget#topWidget {
            background-color: rgba(24, 24, 37, 215);
            border-radius: 14px;
            margin: 4px 6px 0 6px;
        }
        QWidget#playlistPanel {
            background-color: rgba(20, 20, 32, 210);
            border-radius: 0 0 8px 8px;
            margin: 0 6px 6px 6px;
        }
        QWidget#miniBar {
            background-color: transparent;
        }
        QMenuBar {
            background-color: #181825;
            color: #cdd6f4;
            border-bottom: 1px solid #313244;
            padding: 2px 0;
        }
        QMenuBar::item { padding: 4px 10px; border-radius: 4px; }
        QMenuBar::item:selected { background-color: #313244; }
        QMenu {
            background-color: #1e1e2e;
            border: 1px solid #45475a;
            border-radius: 6px;
            padding: 4px 0;
        }
        QMenu::item { padding: 6px 24px 6px 12px; }
        QMenu::item:selected { background-color: #45475a; border-radius: 4px; }
        QMenu::item:disabled { color: #585b70; }
        QMenu::separator { height: 1px; background: #313244; margin: 4px 0; }

        QLabel#titleLabel {
            font-size: 17px;
            font-weight: bold;
            color: #cba6f7;
            padding-bottom: 2px;
        }
        QLabel#artistLabel { color: #a6adc8; font-size: 13px; }
        QLabel#albumLabel  { color: #6c7086; font-size: 11px; font-style: italic; }
        QLabel#timeLabel   { color: #a6adc8; font-size: 12px; font-variant-numeric: tabular-nums; }
        QLabel#volLabel    { color: #a6adc8; font-size: 12px; }
        QLabel#playlistInfo{ color: #6c7086; font-size: 11px; padding: 0 6px; }
        QLabel#miniTitle   { color: #cdd6f4; font-size: 13px; font-weight: bold; padding: 0 6px; }

        QFrame#separator { color: #313244; max-height: 1px; background: #313244; }
        QWidget#miniBar  { background-color: transparent; }
        QLabel#miniAlbumArt { border-radius: 6px; background-color: #313244; }

        QWidget#mediaStack {
            border-radius: 12px;
            background-color: #181825;
        }

        QToolButton#ctrlBtn {
            background-color: #313244;
            color: #cdd6f4;
            border: none;
            border-radius: 8px;
            font-size: 16px;
        }
        QToolButton#ctrlBtn:hover  { background-color: #45475a; }
        QToolButton#ctrlBtn:pressed{ background-color: #585b70; }

        QToolButton#playBtn {
            background-color: #cba6f7;
            color: #1e1e2e;
            border: none;
            border-radius: 30px;
        }
        QToolButton#playBtn:hover  { background-color: #d4b8f9; }
        QToolButton#playBtn:pressed{ background-color: #b389f4; }

        QToolButton#toggleBtn {
            background-color: #2a2b3d;
            color: #a6adc8;
            border: 1px solid #45475a;
            border-radius: 8px;
        }
        QToolButton#toggleBtn:hover   { background-color: #45475a; color: #cdd6f4; }
        QToolButton#toggleBtn:checked {
            background-color: #2d2040;
            color: #cba6f7;
            border: 1px solid #cba6f7;
        }
        QToolButton#muteBtn {
            background: transparent;
            border: none;
            font-size: 18px;
            color: #cdd6f4;
        }
        QToolButton#muteBtn:hover { color: #cba6f7; }

        QToolButton#smallBtn {
            background-color: #313244;
            color: #6c7086;
            border: none;
            border-radius: 5px;
            font-size: 13px;
        }
        QToolButton#smallBtn:hover { background-color: #45475a; color: #cdd6f4; }

        QSlider#seekSlider_unused { min-height: 20px; }
        QSlider#seekSlider_unused::groove:horizontal {
            height: 5px; background: #313244; border-radius: 3px;
        }
        QSlider#seekSlider_unused::sub-page:horizontal { background: #89b4fa; border-radius: 3px; }
        QSlider#seekSlider_unused::handle:horizontal {
            background: #cdd6f4;
            width: 14px; height: 14px;
            margin: -5px 0;
            border-radius: 7px;
            border: none;
        }
        QSlider#seekSlider::handle:horizontal:hover { background: #cba6f7; }

        QSlider#volSlider { min-height: 18px; }
        QSlider#volSlider::groove:horizontal {
            height: 4px; background: #313244; border-radius: 2px;
        }
        QSlider#volSlider::sub-page:horizontal { background: #a6e3a1; border-radius: 2px; }
        QSlider#volSlider::handle:horizontal {
            background: #cdd6f4;
            width: 12px; height: 12px;
            margin: -4px 0;
            border-radius: 6px;
            border: none;
        }
        QSlider#volSlider::handle:horizontal:hover { background: #a6e3a1; }

        QListWidget#playlist {
            background-color: #181825;
            border: 1px solid #313244;
            border-radius: 8px;
            alternate-background-color: #1e1e2e;
            outline: none;
            padding: 2px;
        }
        QListWidget#playlist::item {
            padding: 5px 9px;
            border-radius: 5px;
        }
        QListWidget#playlist::item:selected {
            background-color: #45475a;
            color: #cba6f7;
        }
        QListWidget#playlist::item:hover:!selected { background-color: #2a2b3d; }
        QListWidget#playlist::item:selected { background-color: #45475a; color: ACCENT; }

        QLineEdit#searchEdit {
            background-color: #181825;
            border: 1px solid #313244;
            border-radius: 6px;
            padding: 5px 10px;
            color: #cdd6f4;
            selection-background-color: #45475a;
        }
        QLineEdit#searchEdit:focus { border-color: #89b4fa; }

        QComboBox#speedCombo {
            background-color: #313244;
            border: 1px solid #45475a;
            border-radius: 6px;
            padding: 4px 8px;
            color: #cdd6f4;
        }
        QComboBox#speedCombo:hover { border-color: #6c7086; }
        QComboBox#speedCombo::drop-down { border: none; }
        QComboBox QAbstractItemView {
            background-color: #1e1e2e;
            border: 1px solid #45475a;
            selection-background-color: #45475a;
            color: #cdd6f4;
        }

        QStatusBar {
            background-color: #181825;
            color: #6c7086;
            border-top: 1px solid #313244;
            font-size: 11px;
        }
        QScrollBar:vertical {
            background: #181825;
            width: 7px; border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #45475a; border-radius: 4px; min-height: 20px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal {
            background: #181825; height: 7px; border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background: #45475a; border-radius: 4px;
        }

        QTabBar#playlistTabBar {
            background: transparent;
        }
        QTabBar#playlistTabBar::tab {
            background-color: #252537;
            color: #6c7086;
            padding: 5px 18px;
            border-radius: 6px 6px 0 0;
            border: 1px solid #313244;
            border-bottom: none;
            margin-right: 2px;
            font-size: 12px;
        }
        QTabBar#playlistTabBar::tab:selected {
            background-color: #313244;
            color: #cba6f7;
            font-weight: bold;
            border-color: #45475a;
        }
        QTabBar#playlistTabBar::tab:hover:!selected {
            background-color: #2d2d42;
            color: #cdd6f4;
        }

        /* ── Settings dialog ── */
        QDialog        { background-color: #1e1e2e; color: #cdd6f4; }
        QTabWidget::pane { border: 1px solid #313244; border-radius: 6px; }
        QTabBar#settingsTabs::tab { background-color: #252537; color: #6c7086; padding: 7px 18px; border-radius: 4px 4px 0 0; }
        QTabBar#settingsTabs::tab:selected { background-color: #313244; color: ACCENT; }
        QTabBar#settingsTabs::tab:hover:!selected { background-color: #2d2d42; color: #cdd6f4; }
        QGroupBox { border: 1px solid #313244; border-radius: 6px; margin-top: 6px; padding-top: 6px; color: #a6adc8; }
        QLabel#settingsHead { color: #cdd6f4; font-weight: bold; }
        QFrame#settingsSep  { background: #313244; max-height: 1px; }

        QRadioButton, QCheckBox { color: #cdd6f4; spacing: 8px; }
        QRadioButton::indicator {
            width: 17px; height: 17px;
            border: 2px solid #45475a; border-radius: 9px; background: #313244;
        }
        QRadioButton::indicator:hover  { border-color: ACCENT; }
        QRadioButton::indicator:checked {
            background: ACCENT; border-color: ACCENT;
            image: DOT_IMAGE;
        }
        QCheckBox::indicator {
            width: 17px; height: 17px;
            border: 2px solid #45475a; border-radius: 4px; background: #313244;
        }
        QCheckBox::indicator:hover  { border-color: ACCENT; }
        QCheckBox::indicator:checked {
            background: ACCENT; border-color: ACCENT;
            image: CHECK_IMAGE;
        }
        QCheckBox::indicator:indeterminate {
            background: ACCENT; border-color: ACCENT;
            image: DASH_IMAGE;
        }

        QPushButton { background-color: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 5px; padding: 5px 14px; min-height: 24px; }
        QPushButton:hover { background-color: #45475a; }
        QPushButton:pressed { background-color: #585b70; }
        QFormLayout QLabel { color: #a6adc8; }

        QToolButton#helpBtn {
            background: #313244; color: #6c7086; border: 1px solid #45475a;
            border-radius: 9px; font-size: 11px; font-weight: bold;
        }
        QToolButton#helpBtn:hover { background: #45475a; color: ACCENT; border-color: ACCENT; }
    )";

    // Apply dynamic values
    ss.replace("ACCENT",    acH);
    ss.replace("#cba6f7",   acH);
    ss.replace("#d4b8f9",   acL);   // play hover
    ss.replace("#b389f4",   acD);   // play pressed
    ss.replace("#2d2040",   acBgH); // toggle checked bg
    ss.replace("FONTPX",      QString::number(fs) + "px");
    ss.replace("FONTFAMILY",  family);

    // Indicator images: written to temp files once; Qt CSS file:// paths always work
    static QString s_checkUri, s_dotUri, s_dashUri;
    if (s_checkUri.isEmpty()) {
        const QString tmp = QDir::tempPath();
        auto save = [&tmp](const QString &name, std::function<void(QPainter &)> fn) -> QString {
            QPixmap pm(17, 17); pm.fill(Qt::transparent);
            QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing); fn(p);
            QString path = tmp + "/echobox_ind_" + name + ".png";
            pm.save(path, "PNG");
            return "url(\"" + path.replace('\\', '/') + "\")";
        };
        s_checkUri = save("check", [](QPainter &p) {
            p.setPen(QPen(Qt::white, 2.2f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(QPolygonF() << QPointF(3,9.5f) << QPointF(7,13.5f) << QPointF(14,3.5f));
        });
        s_dotUri = save("dot", [](QPainter &p) {
            p.setPen(Qt::NoPen); p.setBrush(Qt::white);
            p.drawEllipse(QPointF(8.5f, 8.5f), 3.5f, 3.5f);
        });
        s_dashUri = save("dash", [](QPainter &p) {
            p.setPen(QPen(Qt::white, 2.0f, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(4.0f, 8.5f), QPointF(13.0f, 8.5f));
        });
    }
    ss.replace("CHECK_IMAGE", s_checkUri);
    ss.replace("DOT_IMAGE",   s_dotUri);
    ss.replace("DASH_IMAGE",  s_dashUri);

    setStyleSheet(ss);


    // Art shape — radius applied directly to pixmap in updateAlbumArt/onMetaDataChanged
    m_mediaStack->setStyleSheet("QWidget#mediaStack{background-color:#181825;}");
    updateAlbumArt();

    // Visibility
    if (m_aurora) m_aurora->setLightMode(false);
    if (m_visualizer)  m_visualizer->setVisible(m_cfg.showVisualizer);
    statusBar()->setVisible(m_cfg.showStatusBar);

    // Update font size
    QFont f = font(); f.setPointSize(fs); setFont(f);

    // Refresh play button icon color (contrast against accent)
    const QColor iconC = ac.lightness() > 160 ? QColor(0x1e,0x1e,0x2e) : QColor(0xff,0xff,0xff);
    const bool playing = m_player && m_player->playbackState() == QMediaPlayer::PlayingState;

    if (m_playPauseBtn) {
        m_playPauseBtn->setIcon(playing ? Ico::pause(iconC,30) : Ico::play(iconC,30));
        m_playPauseBtn->setStyleSheet(QString(
            "QToolButton#playBtn{background-color:%1;border-radius:30px;border:none;}"
            "QToolButton#playBtn:hover{background-color:%2;}"
            "QToolButton#playBtn:pressed{background-color:%3;}").arg(acH,acL,acD));
    }
    if (m_miniPlayBtn) {
        m_miniPlayBtn->setIcon(playing ? Ico::pause(iconC,22) : Ico::play(iconC,22));
        m_miniPlayBtn->setStyleSheet(QString(
            "QToolButton#playBtn{background-color:%1;border-radius:18px;border:none;}"
            "QToolButton#playBtn:hover{background-color:%2;}"
            "QToolButton#playBtn:pressed{background-color:%3;}").arg(acH,acL,acD));
    }

    // Repaint playlist viewport so item icons remain visible after stylesheet change
    if (m_playlistWidget) m_playlistWidget->viewport()->update();

    const QColor wAccent(0xcb, 0xa6, 0xf7);
    const QColor wTrack (0x45, 0x47, 0x5a);
    if (m_seekSlider)  { m_seekSlider->setAccentColor(wAccent); m_seekSlider->setTrackColor(wTrack); }
    if (m_miniWaveform){ m_miniWaveform->setAccentColor(wAccent); m_miniWaveform->setTrackColor(wTrack); }
}

// ─── Settings ────────────────────────────────────────────────────────────────

void MainWindow::loadSettings() {
    restoreGeometry(m_settings.value("geometry").toByteArray());

    int vol = m_settings.value("volume", 70).toInt();
    m_volumeSlider->setValue(vol);
    m_audioOutput->setVolume(vol / 100.0f);
    m_volumeLabel->setText(QString("%1%").arg(vol));

    m_shuffle = m_settings.value("shuffle", false).toBool();
    m_shuffleBtn->setChecked(m_shuffle);
    if (m_shuffleAct) m_shuffleAct->setChecked(m_shuffle);

    m_repeat = static_cast<RepeatMode>(m_settings.value("repeat", 0).toInt());
    updateRepeatButton();

    int si = m_settings.value("speedIndex", 2).toInt();
    m_speedCombo->setCurrentIndex(si);

    m_recentFiles = m_settings.value("recentFiles").toStringList();
    refreshRecentMenu();

    // Load AppSettings
    m_cfg.theme          = m_settings.value("cfg/theme", "mocha").toString();
    m_cfg.accentColor    = QColor(m_settings.value("cfg/accentColor", "#cba6f7").toString());
    if (!m_cfg.accentColor.isValid()) m_cfg.accentColor = QColor(0xcb,0xa6,0xf7);
    m_cfg.fontSizeIdx    = m_settings.value("cfg/fontSizeIdx", 1).toInt();
    m_cfg.fontFamily     = m_settings.value("cfg/fontFamily",  "").toString();
    m_cfg.fontFilePath   = m_settings.value("cfg/fontFilePath","").toString();
    // Re-load custom font file so the family name is available
    if (!m_cfg.fontFilePath.isEmpty())
        QFontDatabase::addApplicationFont(m_cfg.fontFilePath);
    m_cfg.artShape       = m_settings.value("cfg/artShape", "rounded").toString();
    m_cfg.autoPlay       = m_settings.value("cfg/autoPlay", false).toBool();
    m_cfg.showVisualizer = m_settings.value("cfg/showVisualizer", true).toBool();
    m_cfg.crossfadeSecs  = m_settings.value("cfg/crossfadeSecs", 0).toInt();
    m_cfg.libraryFolder  = m_settings.value("cfg/libraryFolder", "").toString();
    m_cfg.playlistsFolder= m_settings.value("cfg/playlistsFolder", "").toString();
    m_cfg.iconsFolder    = m_settings.value("cfg/iconsFolder", "").toString();
    m_cfg.showTrackIcons = m_settings.value("cfg/showTrackIcons", true).toBool();
    m_cfg.showStatusBar  = m_settings.value("cfg/showStatusBar", true).toBool();
    m_cfg.closeToTray    = m_settings.value("cfg/closeToTray", true).toBool();
    m_cfg.discordEnabled = m_settings.value("cfg/discordEnabled", true).toBool();

    if (g_delegate) g_delegate->showIcons = m_cfg.showTrackIcons;
    applyTheme();
    loadPlaylistsFromFile();
}

void MainWindow::saveSettings() {
    m_settings.setValue("geometry",   saveGeometry());
    m_settings.setValue("volume",     m_volumeSlider->value());
    m_settings.setValue("shuffle",    m_shuffle);
    m_settings.setValue("repeat",     static_cast<int>(m_repeat));
    m_settings.setValue("speedIndex", m_speedCombo->currentIndex());
    m_settings.setValue("recentFiles",m_recentFiles);

    m_settings.setValue("cfg/theme",          m_cfg.theme);
    m_settings.setValue("cfg/accentColor",    m_cfg.accentColor.name());
    m_settings.setValue("cfg/fontSizeIdx",    m_cfg.fontSizeIdx);
    m_settings.setValue("cfg/fontFamily",     m_cfg.fontFamily);
    m_settings.setValue("cfg/fontFilePath",   m_cfg.fontFilePath);
    m_settings.setValue("cfg/artShape",       m_cfg.artShape);
    m_settings.setValue("cfg/autoPlay",       m_cfg.autoPlay);
    m_settings.setValue("cfg/showVisualizer", m_cfg.showVisualizer);
    m_settings.setValue("cfg/crossfadeSecs",  m_cfg.crossfadeSecs);
    m_settings.setValue("cfg/libraryFolder",  m_cfg.libraryFolder);
    m_settings.setValue("cfg/playlistsFolder",m_cfg.playlistsFolder);
    m_settings.setValue("cfg/iconsFolder",    m_cfg.iconsFolder);
    m_settings.setValue("cfg/showTrackIcons", m_cfg.showTrackIcons);
    m_settings.setValue("cfg/showStatusBar",  m_cfg.showStatusBar);
    m_settings.setValue("cfg/closeToTray",    m_cfg.closeToTray);
    m_settings.setValue("cfg/discordEnabled", m_cfg.discordEnabled);

    savePlaylistsToFile();
}

// ─── File operations ─────────────────────────────────────────────────────────

void MainWindow::openFiles() {
    const QString defaultDir = m_cfg.libraryFolder.isEmpty()
        ? m_settings.value("lastDir", QStandardPaths::writableLocation(QStandardPaths::MusicLocation)).toString()
        : m_cfg.libraryFolder;
    QStringList files = QFileDialog::getOpenFileNames(
        this, "Открыть медиафайлы", defaultDir, MEDIA_FILTER.join(";;"));
    if (files.isEmpty()) return;
    m_settings.setValue("lastDir", QFileInfo(files.first()).absolutePath());
    QList<QUrl> urls;
    for (const QString &f : files) { urls.append(QUrl::fromLocalFile(f)); addRecentFile(f); }
    addFiles(urls);
}

void MainWindow::openFolder() {
    const QString defaultDir = m_cfg.libraryFolder.isEmpty()
        ? m_settings.value("lastDir", QStandardPaths::writableLocation(QStandardPaths::MusicLocation)).toString()
        : m_cfg.libraryFolder;
    QString dir = QFileDialog::getExistingDirectory(this, "Открыть папку", defaultDir);
    if (dir.isEmpty()) return;
    m_settings.setValue("lastDir", dir);
    addFolder(dir);
}

void MainWindow::savePlaylist() {
    QString path = QFileDialog::getSaveFileName(
        this, "Сохранить плейлист",
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        "M3U8 Playlist (*.m3u8);;M3U Playlist (*.m3u)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    out << "#EXTM3U\n";
    for (const QUrl &u : m_playlist)
        out << (u.isLocalFile() ? u.toLocalFile() : u.toString()) << "\n";
    statusBar()->showMessage("Плейлист сохранён: " + path, 4000);
}

void MainWindow::loadPlaylist() {
    QString path = QFileDialog::getOpenFileName(
        this, "Загрузить плейлист",
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        "M3U Playlist (*.m3u8 *.m3u);;All Files (*)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QList<QUrl> urls;
    QStringList streamLinks;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (QFile::exists(line)) urls.append(QUrl::fromLocalFile(line));
        else if (line.startsWith("http://", Qt::CaseInsensitive) ||
                 line.startsWith("https://", Qt::CaseInsensitive))
            streamLinks.append(line);
    }
    for (const QString &link : streamLinks) openStreamUrl(link);
    addFiles(urls);
}

void MainWindow::openRecentFile(const QString &path) {
    if (!QFile::exists(path)) {
        statusBar()->showMessage("Файл не найден: " + path, 4000);
        return;
    }
    addFiles({QUrl::fromLocalFile(path)});
}

void MainWindow::addRecentFile(const QString &path) {
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > MAX_RECENT)
        m_recentFiles.removeLast();
    refreshRecentMenu();
}

void MainWindow::refreshRecentMenu() {
    if (!m_recentMenu) return;
    m_recentMenu->clear();
    if (m_recentFiles.isEmpty()) {
        m_recentMenu->addAction("(пусто)")->setEnabled(false);
        return;
    }
    for (const QString &p : m_recentFiles) {
        QAction *a = m_recentMenu->addAction(QFileInfo(p).fileName());
        a->setToolTip(p);
        connect(a, &QAction::triggered, [this, p]{ openRecentFile(p); });
    }
    m_recentMenu->addSeparator();
    m_recentMenu->addAction("Очистить список", [this]{
        m_recentFiles.clear(); refreshRecentMenu();
    });
}

// ─── Playlist management ─────────────────────────────────────────────────────

void MainWindow::addFiles(const QList<QUrl> &urls) {
    bool wasEmpty = m_playlist.isEmpty();
    for (const QUrl &url : urls) {
        const QString path = url.toLocalFile();
        if (path.isEmpty() || !QFile::exists(path)) continue;
        m_playlist.append(url);
        const QString name = QFileInfo(path).fileName();
        auto *item = new QListWidgetItem(
            QString("  %1.  %2").arg(m_playlist.size()).arg(name));
        item->setData(Qt::UserRole, url);
        m_playlistWidget->addItem(item);
        if (m_cfg.showTrackIcons) applyTrackIcon(item, url);
    }
    updatePlaylistInfo();
    updateDuplicateHighlights();
    scheduleScan(urls);
    if (wasEmpty && !m_playlist.isEmpty()) playTrack(0);
}

void MainWindow::addFolder(const QString &dir) {
    const QStringList exts = []{
        QStringList e;
        for (const QString &x : {"mp3","mp4","wav","ogg","flac","aac","m4a","mkv","avi","mov","webm","opus","wma","wmv"})
            e << ("*." + x);
        return e;
    }();
    QDirIterator it(dir, exts, QDir::Files, QDirIterator::Subdirectories);
    QList<QUrl> urls;
    while (it.hasNext()) urls.append(QUrl::fromLocalFile(it.next()));
    std::sort(urls.begin(), urls.end(), [](const QUrl &a, const QUrl &b){
        return QFileInfo(a.toLocalFile()).fileName().toLower() <
               QFileInfo(b.toLocalFile()).fileName().toLower();
    });
    addFiles(urls);
}

void MainWindow::clearPlaylist() {
    if (m_playlist.isEmpty()) return;
    if (QMessageBox::question(this, "Очистить плейлист",
            QString("Удалить все %1 треков из плейлиста?").arg(m_playlist.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    m_player->stop();
    m_playlist.clear();
    m_playlistWidget->clear();
    m_currentIndex = -1;
    m_titleLabel->setText("EchoBox II");
    m_artistLabel->setText("Перетащи файлы или открой через меню Файл");
    m_albumLabel->setText("");
    m_seekSlider->clearWaveform();
    m_seekSlider->setValue(0); m_seekSlider->setRange(0, 0);
    m_timeLabel->setText("0:00 / 0:00");
    setWindowTitle("EchoBox II");
    updatePlaylistInfo();
    m_coverPixmap = QPixmap();
    updateAlbumArt();
    statusBar()->showMessage("Плейлист очищен");
}

void MainWindow::removeSelectedTracks() {
    QList<QListWidgetItem*> sel = m_playlistWidget->selectedItems();
    if (sel.isEmpty()) return;
    const QString msg = sel.size() == 1
        ? QString("Удалить «%1» из плейлиста?").arg(sel.first()->text())
        : QString("Удалить %1 треков из плейлиста?").arg(sel.size());
    if (QMessageBox::question(this, "Удалить треки", msg,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    for (QListWidgetItem *item : sel) {
        QUrl url = item->data(Qt::UserRole).value<QUrl>();
        m_playlist.removeAll(url);
        delete item;
    }
    // renumber
    for (int i = 0; i < m_playlistWidget->count(); ++i) {
        QListWidgetItem *it = m_playlistWidget->item(i);
        const QUrl u = it->data(Qt::UserRole).value<QUrl>();
        it->setText(QString("  %1.  %2").arg(i+1).arg(playlistRowLabel(u)));
    }
    if (m_currentIndex >= m_playlist.size()) m_currentIndex = m_playlist.size() - 1;
    updatePlaylistInfo();
    updateDuplicateHighlights();
}

void MainWindow::moveTrackUp() {
    int row = m_playlistWidget->currentRow();
    if (row <= 0) return;
    m_playlist.swapItemsAt(row, row - 1);
    auto *item = m_playlistWidget->takeItem(row);
    m_playlistWidget->insertItem(row - 1, item);
    m_playlistWidget->setCurrentRow(row - 1);
    if (m_currentIndex == row) m_currentIndex = row - 1;
    else if (m_currentIndex == row - 1) m_currentIndex = row;
}

void MainWindow::moveTrackDown() {
    int row = m_playlistWidget->currentRow();
    if (row < 0 || row >= m_playlistWidget->count() - 1) return;
    m_playlist.swapItemsAt(row, row + 1);
    auto *item = m_playlistWidget->takeItem(row);
    m_playlistWidget->insertItem(row + 1, item);
    m_playlistWidget->setCurrentRow(row + 1);
    if (m_currentIndex == row) m_currentIndex = row + 1;
    else if (m_currentIndex == row + 1) m_currentIndex = row;
}

void MainWindow::rebuildPlaylistFromWidget() {
    QUrl curUrl;
    if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size())
        curUrl = m_playlist.at(m_currentIndex);
    m_playlist.clear();
    for (int i = 0; i < m_playlistWidget->count(); ++i)
        m_playlist.append(m_playlistWidget->item(i)->data(Qt::UserRole).value<QUrl>());
    m_currentIndex = curUrl.isEmpty() ? -1 : m_playlist.indexOf(curUrl);
}

void MainWindow::onSearchChanged(const QString &text) {
    const QString q = text.trimmed().toLower();
    const QColor dimColor(0x45, 0x47, 0x5a);
    const QColor normalColor(0xcd, 0xd6, 0xf4);

    int matches = 0;
    const int total = m_playlistWidget->count();

    for (int i = 0; i < total; ++i) {
        QListWidgetItem *it = m_playlistWidget->item(i);
        if (q.isEmpty()) {
            it->setForeground(normalColor);
            it->setHidden(false);
        } else {
            const QString dispText = it->text().toLower();
            const QString title    = it->data(Qt::UserRole + 2).toString().toLower();
            const QString artist   = it->data(Qt::UserRole + 3).toString().toLower();
            const QString album    = it->data(Qt::UserRole + 4).toString().toLower();
            const bool hit = dispText.contains(q) || title.contains(q)
                          || artist.contains(q)   || album.contains(q);
            it->setForeground(hit ? normalColor : dimColor);
            it->setHidden(false);
            if (hit) ++matches;
        }
    }

    if (!q.isEmpty())
        m_playlistInfo->setText(QString("%1 из %2").arg(matches).arg(total));
    else
        updatePlaylistInfo();
}

void MainWindow::onTrackActivated(QListWidgetItem *item) {
    QUrl url = item->data(Qt::UserRole).value<QUrl>();
    int idx = m_playlist.indexOf(url);
    if (idx >= 0) playTrack(idx);
}

void MainWindow::onPlaylistContextMenu(const QPoint &pos) {
    QListWidgetItem *item = m_playlistWidget->itemAt(pos);
    QMenu menu(this);
    if (item) {
        menu.addAction("▶  Воспроизвести", [this, item]{ onTrackActivated(item); });
        menu.addAction("✕  Удалить из плейлиста", this, &MainWindow::removeSelectedTracks);
        menu.addSeparator();
        QUrl itemUrl = item->data(Qt::UserRole).value<QUrl>();
        menu.addAction("Установить иконку...", [this, item, itemUrl]{
            QString img = QFileDialog::getOpenFileName(this, "Выбрать иконку",
                QString(), "Изображения (*.png *.jpg *.jpeg *.bmp *.webp)");
            if (img.isEmpty()) return;
            QPixmap full(img);
            if (full.isNull()) return;
            QString iconFile = trackIconPath(itemUrl);
            QDir().mkpath(QFileInfo(iconFile).absolutePath());
            // Save full-size (capped at 512px) — used for album art display
            const QPixmap large = (full.width() > 512 || full.height() > 512)
                ? full.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                : full;
            large.save(iconFile, "PNG");
            // Scale down only for the small playlist icon
            const QPixmap thumb = full.scaled(36, 36, Qt::KeepAspectRatioByExpanding,
                                              Qt::SmoothTransformation).copy(0, 0, 36, 36);
            item->setIcon(QIcon(thumb));
            // If this is the currently playing track, update the album art display too
            if (m_currentIndex >= 0 && m_playlist.value(m_currentIndex) == itemUrl) {
                m_coverPixmap = full;
                updateAlbumArt();
            }
        });
        if (QFile::exists(trackIconPath(itemUrl))) {
            menu.addAction("Убрать иконку", [this, item, itemUrl]{
                QFile::remove(trackIconPath(itemUrl));
                item->setIcon(QIcon());
                if (m_currentIndex >= 0 && m_playlist.value(m_currentIndex) == itemUrl) {
                    m_coverPixmap = QPixmap();
                    updateAlbumArt();
                }
            });
        }
        menu.addSeparator();
    }
    menu.addAction("Добавить файлы...", this, &MainWindow::openFiles);
    menu.addAction("Добавить папку...", this, &MainWindow::openFolder);
    menu.addSeparator();
    menu.addAction("Сохранить плейлист...", this, &MainWindow::savePlaylist);
    menu.addAction("Загрузить плейлист...", this, &MainWindow::loadPlaylist);
    menu.addSeparator();
    menu.addAction("Очистить всё", this, &MainWindow::clearPlaylist);
    menu.addSeparator();
    auto *removeDupAct = menu.addAction("Удалить дубликаты", [this]{
        const int n = m_playlistWidget->count();
        QSet<QString> seenPaths, seenMeta;
        QList<int> toRemove;
        for (int i = 0; i < n; ++i) {
            QListWidgetItem *it = m_playlistWidget->item(i);
            const QString path  = it->data(Qt::UserRole).value<QUrl>().toLocalFile().toLower();
            const QString title  = it->data(Qt::UserRole + 2).toString().toLower().trimmed();
            const QString artist = it->data(Qt::UserRole + 3).toString().toLower().trimmed();
            const QString meta   = artist + "||" + title;
            bool dup = false;
            if (!path.isEmpty() && seenPaths.contains(path))      dup = true;
            else if (!title.isEmpty() && seenMeta.contains(meta)) dup = true;
            if (dup) { toRemove.prepend(i); continue; }
            seenPaths.insert(path);
            if (!title.isEmpty()) seenMeta.insert(meta);
        }
        for (int i : toRemove) {
            const QUrl url = m_playlistWidget->item(i)->data(Qt::UserRole).value<QUrl>();
            delete m_playlistWidget->takeItem(i);
            m_playlist.removeAll(url);
            if (m_currentIndex == i)      m_currentIndex = -1;
            else if (m_currentIndex > i)  --m_currentIndex;
        }
        updatePlaylistInfo();
        updateDuplicateHighlights();
    });
    // Show grayed out if there are no duplicates
    bool hasDups = false;
    for (int i = 0; i < m_playlistWidget->count(); ++i)
        if (m_playlistWidget->item(i)->data(Qt::UserRole + 10).toBool()) { hasDups = true; break; }
    removeDupAct->setEnabled(hasDups);
    menu.exec(m_playlistWidget->mapToGlobal(pos));
}

void MainWindow::updatePlaylistInfo() {
    int n = m_playlist.size();
    m_playlistInfo->setText(n == 0 ? "Пусто" : QString("%1 трек%2").arg(n).arg(
        n == 1 ? "" : (n < 5 ? "а" : "ов")));
}

// ─── Playback ────────────────────────────────────────────────────────────────

void MainWindow::playTrack(int index) {
    if (index < 0 || index >= m_playlist.size()) return;

    // Сохранить позицию предыдущего трека
    saveTrackPosition();

    // Кроссфейд — если фейдим, начинаем с нуля и плавно поднимаем
    const bool wasCrossfading = m_crossfading;
    m_crossfading = false;
    if (wasCrossfading && m_cfg.crossfadeSecs > 0) {
        m_fadeFactor = 0.0f;
        m_fadeInTimer->start();
    } else {
        m_fadeFactor = 1.0f;
    }

    m_currentIndex = index;
    const QUrl url = m_playlist[index];

    m_playlistWidget->setCurrentRow(index);
    setCurrentTrackVisual(index);

    const QString name   = trackDisplayTitle(url);
    const QString artist = trackDisplayArtist(url);
    const QString mini   = artist.isEmpty() ? name : artist + "  —  " + name;
    m_titleLabel->setText(name);
    m_miniTitle->setText(mini);
    m_artistLabel->setText(artist);
    m_albumLabel->setText("");
    setWindowTitle("EchoBox II  —  " + mini);

    if (!url.isLocalFile()) {
        // Ссылка на музыку (SoundCloud/YouTube/др.) — сначала получить
        // прямой аудиопоток, реальный setSource() произойдёт асинхронно
        statusBar()->showMessage("Получение потока: " + name + " …");
        // Обложка могла быть скачана и закэширована при прошлом воспроизведении
        const QString icoFile = trackIconPath(url);
        m_coverPixmap = QFile::exists(icoFile) ? QPixmap(icoFile) : QPixmap();
        m_mediaStack->setCurrentWidget(m_albumArt);
        updateAlbumArt();
        beginStreamPlayback(index, url);
        return;
    }

    m_player->setSource(url);
    m_player->play();
    applyVolume();

    statusBar()->showMessage(url.toLocalFile());

    if (isVideoFile(url))
        m_mediaStack->setCurrentWidget(m_videoWidget);
    else {
        // Load cached icon file as album art fallback (manual or scanned)
        const QString icoFile = trackIconPath(url);
        m_coverPixmap = QFile::exists(icoFile) ? QPixmap(icoFile) : QPixmap();
        m_mediaStack->setCurrentWidget(m_albumArt);
        updateAlbumArt();
    }

    addRecentFile(url.toLocalFile());

    // Discord Rich Presence
    if (m_discord && m_cfg.discordEnabled)
        m_discord->updateActivity(m_titleLabel->text(), m_artistLabel->text());
}

// ─── Ссылки на музыку (SoundCloud/YouTube/др. через yt-dlp, прямые URL) ───────
//
// Плейлист хранит ИСХОДНУЮ ссылку пользователя как QUrl (стабильный ключ —
// сохраняется в плейлисты/M3U, участвует в поиске дублей). Для ссылок,
// которые требуют yt-dlp (SoundCloud/YouTube/др.), трек СКАЧИВАЕТСЯ целиком
// на диск перед воспроизведением и дальше играется как обычный локальный
// файл — тот же путь, что уже надёжно работает для остальной библиотеки.
// Отдавать плееру напрямую подписанный CDN-адрес ненадёжно: он собран из
// HLS-сегментов и/или требует специфичных HTTP-заголовков (например,
// googlevideo.com у YouTube без верного User-Agent отвечает 403) — а такое
// умеет только сам yt-dlp, не QMediaPlayer.

QString MainWindow::ytDlpPath() const {
    const QString bundled = QCoreApplication::applicationDirPath() + "/yt-dlp.exe";
    if (QFile::exists(bundled)) return bundled;
    return "yt-dlp";
}

QString MainWindow::streamCacheDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/streamcache";
}

bool MainWindow::looksLikeDirectMediaUrl(const QUrl &url) const {
    static const QStringList exts = {
        "mp3","ogg","wav","flac","m4a","aac","opus","wma","webm"
    };
    return exts.contains(QFileInfo(url.path()).suffix().toLower());
}

QString MainWindow::trackDisplayTitle(const QUrl &url) const {
    if (url.isLocalFile())
        return QFileInfo(url.toLocalFile()).completeBaseName();
    const auto it = m_streamTracks.constFind(url);
    if (it != m_streamTracks.constEnd() && !it->title.isEmpty())
        return it->title;
    return url.toString();
}

QString MainWindow::trackDisplayArtist(const QUrl &url) const {
    if (url.isLocalFile()) return QString();
    const auto it = m_streamTracks.constFind(url);
    return it != m_streamTracks.constEnd() ? it->artist : QString();
}

QString MainWindow::playlistRowLabel(const QUrl &url) const {
    if (url.isLocalFile())
        return QFileInfo(url.toLocalFile()).fileName();
    const QString title  = trackDisplayTitle(url);
    const QString artist = trackDisplayArtist(url);
    return "🔗 " + (artist.isEmpty() ? title : artist + "  —  " + title);
}

void MainWindow::openUrlDialog() {
    bool ok = false;
    const QString link = QInputDialog::getText(
        this, "Открыть по ссылке",
        "Ссылка на трек (SoundCloud, YouTube, Bandcamp, прямая ссылка\n"
        "на аудиофайл и т.п. — всё, что понимает yt-dlp):",
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || link.isEmpty()) return;
    openStreamUrl(link);
}

void MainWindow::openStreamUrl(const QString &link) {
    const QUrl url = QUrl::fromUserInput(link);
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) {
        statusBar()->showMessage("Некорректная ссылка: " + link, 5000);
        return;
    }
    if (m_playlist.contains(url)) {
        statusBar()->showMessage("Этот трек уже в плейлисте", 3000);
        return;
    }

    if (looksLikeDirectMediaUrl(url)) {
        addDirectStreamUrl(url);
        return;
    }

    const int index = insertStreamPlaceholder(url);
    const bool shouldAutoplay = (m_playlist.size() == 1);
    m_streamResolving.insert(url);
    downloadStreamTrack(url, [this, url, index, shouldAutoplay]
            (bool ok, QString localPath, QString title, QString artist, QString thumb) {
        m_streamResolving.remove(url);
        updateStreamPlaceholder(url, ok, localPath, title, artist, thumb);
        if (shouldAutoplay) {
            if (ok) playTrack(index);
            else    statusBar()->showMessage("Не удалось получить трек по ссылке", 6000);
        }
    });
}

void MainWindow::addDirectStreamUrl(const QUrl &url) {
    const bool wasEmpty = m_playlist.isEmpty();

    QString name = QFileInfo(url.path()).fileName();
    if (name.isEmpty()) name = url.toString();

    StreamTrackInfo info;
    info.title      = name;
    info.isDirectUrl = true;
    m_streamTracks[url] = info;

    m_playlist.append(url);
    const int index = m_playlist.size() - 1;
    auto *item = new QListWidgetItem(QString("  %1.  🔗 %2").arg(index + 1).arg(name));
    item->setData(Qt::UserRole, url);
    item->setData(Qt::UserRole + 2, name);
    item->setToolTip(url.toString());
    m_playlistWidget->addItem(item);

    updatePlaylistInfo();
    updateDuplicateHighlights();
    if (wasEmpty) playTrack(index);
}

int MainWindow::insertStreamPlaceholder(const QUrl &pageUrl) {
    StreamTrackInfo info;
    info.title = "Загрузка…";
    m_streamTracks[pageUrl] = info;

    m_playlist.append(pageUrl);
    const int index = m_playlist.size() - 1;

    auto *item = new QListWidgetItem(QString("  %1.  🔗 Загрузка…").arg(index + 1));
    item->setData(Qt::UserRole, pageUrl);
    item->setToolTip(pageUrl.toString());
    m_playlistWidget->addItem(item);

    updatePlaylistInfo();
    return index;
}

void MainWindow::updateStreamPlaceholder(const QUrl &pageUrl, bool ok, const QString &localPath,
                                          const QString &title, const QString &artist,
                                          const QString &thumbnailUrl) {
    const int idx = m_playlist.indexOf(pageUrl);
    if (idx < 0) return;

    QListWidgetItem *item = m_playlistWidget->item(idx);

    if (!ok) {
        if (item) item->setText(QString("  %1.  ⚠ Ошибка загрузки").arg(idx + 1));
        return;
    }

    StreamTrackInfo &info = m_streamTracks[pageUrl];
    info.localPath     = localPath;
    info.title         = title;
    info.artist        = artist;
    info.thumbnailUrl  = thumbnailUrl;

    const QString label = artist.isEmpty() ? title : artist + "  —  " + title;
    if (item) {
        item->setText(QString("  %1.  🔗 %2").arg(idx + 1).arg(label));
        item->setData(Qt::UserRole + 2, title);
        item->setData(Qt::UserRole + 3, artist);
    }
    statusBar()->showMessage("Трек готов: " + label, 4000);

    if (idx == m_currentIndex) {
        m_titleLabel->setText(title);
        m_miniTitle->setText(label);
        m_artistLabel->setText(artist);
        setWindowTitle("EchoBox II  —  " + label);
    }

    updateDuplicateHighlights();
    fetchStreamThumbnail(pageUrl, thumbnailUrl);
}

void MainWindow::fetchStreamThumbnail(const QUrl &pageUrl, const QString &thumbnailUrl) {
    if (thumbnailUrl.isEmpty()) return;

    const QString icoFile = trackIconPath(pageUrl);
    if (QFile::exists(icoFile)) {
        // Уже закэшировано с прошлого раза — просто применить
        const int idx = m_playlist.indexOf(pageUrl);
        if (idx < 0) return;
        if (m_cfg.showTrackIcons) {
            QListWidgetItem *item = m_playlistWidget->item(idx);
            if (item) applyTrackIcon(item, pageUrl);
        }
        if (idx == m_currentIndex) {
            m_coverPixmap = QPixmap(icoFile);
            updateAlbumArt();
        }
        return;
    }

    QNetworkReply *reply = m_streamArtNam->get(QNetworkRequest(QUrl(thumbnailUrl)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, pageUrl, icoFile]{
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;

        QPixmap pm;
        if (!pm.loadFromData(reply->readAll())) return;

        QDir().mkpath(QFileInfo(icoFile).absolutePath());
        // Полноразмерная обложка (до 512px) — для показа в плеере
        const QPixmap large = (pm.width() > 512 || pm.height() > 512)
            ? pm.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            : pm;
        large.save(icoFile, "PNG");

        const int idx = m_playlist.indexOf(pageUrl);
        if (idx < 0) return;

        if (m_cfg.showTrackIcons) {
            QListWidgetItem *item = m_playlistWidget->item(idx);
            if (item) item->setIcon(QIcon(pm.scaled(36, 36, Qt::KeepAspectRatioByExpanding,
                                                     Qt::SmoothTransformation).copy(0, 0, 36, 36)));
        }
        if (idx == m_currentIndex) {
            m_coverPixmap = large;
            updateAlbumArt();
        }
    });
}

void MainWindow::downloadStreamTrack(const QUrl &pageUrl,
        std::function<void(bool, QString, QString, QString, QString)> callback) {
    const QString cacheDir = streamCacheDir();
    QDir().mkpath(cacheDir);
    const QString hash = QCryptographicHash::hash(
        pageUrl.toString().toUtf8(), QCryptographicHash::Md5).toHex();
    const QString outTemplate = cacheDir + "/" + hash + ".%(ext)s";

    auto *proc = new QProcess(this);
    const QStringList args = {
        "--no-playlist", "--no-warnings", "--no-progress",
        // android/web-фоллбэк обходит текущий SABR-эксперимент YouTube, из-за
        // которого чистый audio-only формат недоступен для скачивания — для
        // других сайтов (SoundCloud и т.п.) этот youtube-специфичный аргумент
        // просто игнорируется
        "--extractor-args", "youtube:player_client=android,web",
        "-f", "bestaudio/best", "--print-json", "-o", outTemplate, pageUrl.toString()
    };

    connect(proc, &QProcess::errorOccurred, this,
        [this, proc, callback](QProcess::ProcessError err) {
            if (err != QProcess::FailedToStart) return;
            proc->deleteLater();
            if (!m_ytDlpMissingWarned) {
                m_ytDlpMissingWarned = true;
                QMessageBox::warning(this, "yt-dlp не найден",
                    "Для воспроизведения по ссылке (SoundCloud, YouTube и т.п.) нужна "
                    "утилита yt-dlp.\n\n"
                    "Установи её, например: pip install yt-dlp (или winget install "
                    "yt-dlp.yt-dlp), и убедись, что она доступна в PATH — либо просто "
                    "положи yt-dlp.exe рядом с EchoBoxII.exe.");
            }
            callback(false, QString(), QString(), QString(), QString());
        });

    connect(proc, &QProcess::finished, this,
        [this, proc, pageUrl, hash, cacheDir, callback](int exitCode, QProcess::ExitStatus exitStatus) {
            proc->deleteLater();
            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                callback(false, QString(), QString(), QString(), QString());
                return;
            }

            // Файл сохранён yt-dlp сам под расширением, которое выбрало (webm/m4a/opus/…)
            QDir dir(cacheDir);
            const QStringList found = dir.entryList({hash + ".*"}, QDir::Files);
            if (found.isEmpty()) {
                callback(false, QString(), QString(), QString(), QString());
                return;
            }
            const QString localPath = dir.filePath(found.first());

            // --print-json печатает метаданные одной JSON-строкой в stdout уже
            // после скачивания — берём последнюю валидную JSON-строку
            const QList<QByteArray> lines = proc->readAllStandardOutput().split('\n');
            QJsonObject obj;
            for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
                const QJsonDocument doc = QJsonDocument::fromJson(*it);
                if (doc.isObject()) { obj = doc.object(); break; }
            }

            QString title  = obj.value("title").toString();
            QString artist = obj.value("artist").toString();
            if (artist.isEmpty()) artist = obj.value("uploader").toString();
            if (title.isEmpty())  title  = pageUrl.toString();
            const QString thumb = obj.value("thumbnail").toString();

            callback(true, localPath, title, artist, thumb);
        });

    proc->start(ytDlpPath(), args);
}

void MainWindow::beginStreamPlayback(int index, const QUrl &pageUrl) {
    const auto it = m_streamTracks.find(pageUrl);

    if (it != m_streamTracks.end() && it->isDirectUrl) {
        commitStreamPlayback(index, pageUrl, pageUrl);
        return;
    }
    if (it != m_streamTracks.end() && !it->localPath.isEmpty() && QFile::exists(it->localPath)) {
        commitStreamPlayback(index, pageUrl, QUrl::fromLocalFile(it->localPath));
        return;
    }

    // Защита от гонки: если пользователь повторно жмёт play/трек, пока трек ещё
    // скачивается, не запускать второй параллельный процесс yt-dlp на тот же трек
    if (m_streamResolving.contains(pageUrl)) return;
    m_streamResolving.insert(pageUrl);

    statusBar()->showMessage("Загрузка трека: " + trackDisplayTitle(pageUrl) + " …");
    downloadStreamTrack(pageUrl, [this, index, pageUrl]
            (bool ok, QString localPath, QString title, QString artist, QString thumb) {
        m_streamResolving.remove(pageUrl);
        updateStreamPlaceholder(pageUrl, ok, localPath, title, artist, thumb);
        if (!ok) {
            statusBar()->showMessage("Не удалось скачать трек: " + pageUrl.toString(), 6000);
            return;
        }
        // Пользователь мог переключиться на другой трек, пока шло скачивание
        if (m_currentIndex != index || m_playlist.value(index) != pageUrl) return;
        commitStreamPlayback(index, pageUrl, QUrl::fromLocalFile(localPath));
    });
}

void MainWindow::commitStreamPlayback(int index, const QUrl &pageUrl, const QUrl &mediaSource) {
    Q_UNUSED(index);

    m_player->setSource(mediaSource);
    m_player->play();
    applyVolume();

    const QString name   = trackDisplayTitle(pageUrl);
    const QString artist = trackDisplayArtist(pageUrl);
    const QString mini   = artist.isEmpty() ? name : artist + "  —  " + name;
    m_titleLabel->setText(name);
    m_miniTitle->setText(mini);
    m_artistLabel->setText(artist);
    setWindowTitle("EchoBox II  —  " + mini);
    statusBar()->showMessage(pageUrl.toString());

    if (m_discord && m_cfg.discordEnabled)
        m_discord->updateActivity(m_titleLabel->text(), m_artistLabel->text());
}

void MainWindow::togglePlayPause() {
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else if (m_player->playbackState() == QMediaPlayer::PausedState) {
        m_player->play();
    } else if (!m_playlist.isEmpty()) {
        playTrack(qMax(m_currentIndex, 0));
    }
}

void MainWindow::stop() { m_player->stop(); }

void MainWindow::previous() {
    if (m_playlist.isEmpty()) return;
    int prevIdx = m_currentIndex - 1;
    if (prevIdx < 0) prevIdx = m_playlist.size() - 1; // always wrap on manual press
    playTrack(prevIdx);
}

void MainWindow::next() {
    // Manual next: always advance (wrap around, no stop)
    if (m_playlist.isEmpty()) return;
    if (m_repeat == RepeatMode::One) { playNext(false); return; }
    int nextIdx;
    if (m_shuffle) {
        if (m_playlist.size() == 1) { nextIdx = 0; }
        else {
            do { nextIdx = QRandomGenerator::global()->bounded(m_playlist.size()); }
            while (nextIdx == m_currentIndex);
        }
    } else {
        nextIdx = m_currentIndex + 1;
        if (nextIdx >= m_playlist.size()) nextIdx = 0; // always wrap on manual press
    }
    playTrack(nextIdx);
}

void MainWindow::nextAuto() { playNext(true); }

void MainWindow::playNext(bool respectRepeat) {
    if (m_playlist.isEmpty()) return;

    if (respectRepeat && m_repeat == RepeatMode::One) {
        m_player->setPosition(0);
        m_player->play();
        return;
    }

    int nextIdx;
    if (m_shuffle) {
        if (m_playlist.size() == 1) { nextIdx = 0; }
        else {
            do { nextIdx = QRandomGenerator::global()->bounded(m_playlist.size()); }
            while (nextIdx == m_currentIndex);
        }
    } else {
        nextIdx = m_currentIndex + 1;
    }

    if (nextIdx >= m_playlist.size()) {
        if (!respectRepeat || m_repeat == RepeatMode::All) nextIdx = 0;
        else { m_player->stop(); return; }
    }
    playTrack(nextIdx);
}

void MainWindow::setVolume(int v) {
    applyVolume();
    m_volumeLabel->setText(QString("%1%").arg(v));
    const int level = (v == 0) ? 0 : (v < 40) ? 1 : (v < 75) ? 2 : 3;
    const QColor vc(0xcd,0xd6,0xf4);
    m_muteBtn->setIcon(Ico::volume(level, vc, 22));
    if (m_miniMuteBtn)   m_miniMuteBtn->setIcon(Ico::volume(level, vc, 18));
    // Синхронизация слайдеров без рекурсии
    auto sync = [](QSlider *s, int val){
        if (s && s->value() != val) { s->blockSignals(true); s->setValue(val); s->blockSignals(false); }
    };
    sync(m_volumeSlider,  v);
    sync(m_miniVolSlider, v);
}

void MainWindow::toggleMute() {
    m_muted = !m_muted;
    if (m_muted) {
        m_lastVolume = m_volumeSlider->value();
        m_volumeSlider->setValue(0);
    } else {
        m_volumeSlider->setValue(m_lastVolume);
    }
}

void MainWindow::onSpeedChanged(int index) {
    const double speeds[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
    if (index >= 0 && index < 6)
        m_player->setPlaybackRate(speeds[index]);
}

void MainWindow::toggleShuffle() {
    m_shuffle = !m_shuffle;
    m_shuffleBtn->setChecked(m_shuffle);
    const QColor c = m_shuffle ? QColor(0xcb,0xa6,0xf7) : QColor(0xa6,0xad,0xc8);
    m_shuffleBtn->setIcon(Ico::shuffle(c, 18));
    if (m_miniShuffleBtn) { m_miniShuffleBtn->setChecked(m_shuffle); m_miniShuffleBtn->setIcon(Ico::shuffle(c, 15)); }
    if (m_shuffleAct) m_shuffleAct->setChecked(m_shuffle);
    statusBar()->showMessage(m_shuffle ? "Перемешивание включено" : "Перемешивание выключено", 2000);
}

void MainWindow::cycleRepeat() {
    switch (m_repeat) {
    case RepeatMode::Off: m_repeat = RepeatMode::All; break;
    case RepeatMode::All: m_repeat = RepeatMode::One; break;
    case RepeatMode::One: m_repeat = RepeatMode::Off; break;
    }
    updateRepeatButton();
}

void MainWindow::updateRepeatButton() {
    const QColor off(0xa6,0xad,0xc8);
    const QColor on (0xcb,0xa6,0xf7);
    switch (m_repeat) {
    case RepeatMode::Off:
        m_repeatBtn->setIcon(Ico::repeatAll(off, 18));
        m_repeatBtn->setToolTip("Повтор: выкл.");
        m_repeatBtn->setChecked(false);
        if (m_miniRepeatBtn) { m_miniRepeatBtn->setChecked(false); m_miniRepeatBtn->setIcon(Ico::repeatAll(off,15)); }
        if (m_repeatOffAct) m_repeatOffAct->setChecked(true);
        break;
    case RepeatMode::All:
        m_repeatBtn->setIcon(Ico::repeatAll(on, 18));
        m_repeatBtn->setToolTip("Повтор: весь плейлист");
        m_repeatBtn->setChecked(true);
        if (m_miniRepeatBtn) { m_miniRepeatBtn->setChecked(true); m_miniRepeatBtn->setIcon(Ico::repeatAll(on,15)); }
        if (m_repeatAllAct) m_repeatAllAct->setChecked(true);
        break;
    case RepeatMode::One:
        m_repeatBtn->setIcon(Ico::repeatOne(on, 18));
        m_repeatBtn->setToolTip("Повтор: один трек");
        m_repeatBtn->setChecked(true);
        if (m_miniRepeatBtn) { m_miniRepeatBtn->setChecked(true); m_miniRepeatBtn->setIcon(Ico::repeatOne(on,15)); }
        if (m_repeatOneAct) m_repeatOneAct->setChecked(true);
        break;
    }
}

void MainWindow::toggleMiniPlayer() {
    m_miniPlayer = !m_miniPlayer;

    m_topWidget->setVisible(!m_miniPlayer);
    m_separator->setVisible(!m_miniPlayer);
    m_playlistPanel->setVisible(!m_miniPlayer);
    m_miniBar->setVisible(m_miniPlayer);
    menuBar()->setVisible(!m_miniPlayer);
    statusBar()->setVisible(!m_miniPlayer);

    if (m_miniPlayer) {
        setMinimumSize(520, 52);
        setMaximumHeight(52);
        resize(720, 52);
    } else {
        // Выходим из мини-режима — если панель была прижата к верху, сбрасываем
        if (m_miniDocked) {
            m_miniDocked = false;
            if (m_miniDockBtn) m_miniDockBtn->setChecked(false);
        }
        setMinimumSize(720, 540);
        setMaximumHeight(QWIDGETSIZE_MAX);
        resize(940, 660);
    }

    // Без системного заголовка в мини-режиме — окно можно таскать за саму
    // панель (см. eventFilter). Изменение флагов скрывает окно, поэтому
    // после него обязательно вызываем show().
    setWindowFlag(Qt::FramelessWindowHint, m_miniPlayer);
    show();

    if (m_miniPlayerAct) m_miniPlayerAct->setChecked(m_miniPlayer);
}

void MainWindow::toggleMiniDock() {
    QScreen *scr = screen();
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (!scr) return;

    m_miniDocked = !m_miniDocked;
    const QRect avail = scr->availableGeometry();

    if (m_miniDocked) {
        m_miniUndockedGeometry = geometry();
        setGeometry(avail.left(), avail.top(), avail.width(), height());
    } else if (m_miniUndockedGeometry.isValid()) {
        setGeometry(m_miniUndockedGeometry);
    }
}

void MainWindow::toggleAlwaysOnTop() {
    bool on = windowFlags() & Qt::WindowStaysOnTopHint;
    setWindowFlag(Qt::WindowStaysOnTopHint, !on);
    show();
    if (m_alwaysOnTopAct) m_alwaysOnTopAct->setChecked(!on);
}

void MainWindow::toggleRemainingTime() {
    m_showRemaining = !m_showRemaining;
    updateTimeDisplay(m_player->position(), m_player->duration());
}

// ─── Player signal handlers ──────────────────────────────────────────────────

void MainWindow::onDurationChanged(qint64 duration) {
    m_seekSlider->setRange(0, static_cast<int>(duration));
    m_miniWaveform->setRange(0, static_cast<int>(duration));
    if (duration > 0) {
        const QUrl src = m_player->source();
        const auto cached = m_waveformCache.constFind(src);
        if (cached != m_waveformCache.constEnd()) {
            // Уже decoded в этой сессии — не гонять декодер по новой
            m_seekSlider->setPeaks(cached.value());
            m_miniWaveform->setPeaks(cached.value());
        } else {
            m_waveformLoadingUrl = src;
            m_seekSlider->loadWaveform(src, duration);
        }
    }
    updateTimeDisplay(m_player->position(), duration);

    // Store duration in list item
    if (m_currentIndex >= 0 && m_currentIndex < m_playlistWidget->count()) {
        QListWidgetItem *it = m_playlistWidget->item(m_currentIndex);
        if (it) it->setData(Qt::UserRole + 1, formatTime(duration));
    }
}

void MainWindow::onWaveformPeaksReady(QVector<float> peaks) {
    // peaksReady стреляет и промежуточными, и финальными пиками — просто
    // перезаписываем, последний вызов при декодировании всегда самый полный
    if (m_waveformLoadingUrl.isValid())
        m_waveformCache[m_waveformLoadingUrl] = peaks;
}

void MainWindow::onPositionChanged(qint64 position) {
    if (!m_seeking) {
        m_seekSlider->setValue(static_cast<int>(position));
        m_miniWaveform->setValue(static_cast<int>(position));
    }
    updateTimeDisplay(position, m_player->duration());

    // Crossfade fade-out
    if (m_cfg.crossfadeSecs > 0 && !m_fadeInTimer->isActive()) {
        const qint64 dur = m_player->duration();
        if (dur > 0) {
            const qint64 remaining = dur - position;
            const qint64 fadeMs    = qint64(m_cfg.crossfadeSecs) * 1000;
            if (remaining <= fadeMs && remaining > 0) {
                m_crossfading = true;
                m_fadeFactor  = qBound(0.0f, float(remaining) / float(fadeMs), 1.0f);
                applyVolume();
            }
        }
    }
}

void MainWindow::onPlaybackStateChanged(QMediaPlayer::PlaybackState state) {
    const bool playing = (state == QMediaPlayer::PlayingState);
    const QColor ac = m_cfg.accentColor;
    const QColor iconC = ac.lightness() > 160 ? QColor(0x1e,0x1e,0x2e) : QColor(0xff,0xff,0xff);
    m_playPauseBtn->setIcon(playing ? Ico::pause(iconC, 30) : Ico::play(iconC, 30));
    m_miniPlayBtn->setIcon(playing  ? Ico::pause(iconC, 22) : Ico::play(iconC, 22));
    m_visualizer->setActive(playing);
    if (m_trayPlayAct) m_trayPlayAct->setText(playing ? "⏸  Пауза" : "▶  Играть");
    if (m_discord && m_cfg.discordEnabled)
        m_discord->updateActivity(m_titleLabel->text(), m_artistLabel->text(), playing);
    if (!playing)
        saveTrackPosition();
}

void MainWindow::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia) {
        saveTrackPosition();
        nextAuto(); // respects repeat mode; manual next() always wraps
    }
    // Восстановление позиции (Feature 8)
    if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) {
        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
            const QVariant saved = m_settings.value(positionKey(m_playlist[m_currentIndex]));
            if (saved.isValid()) {
                const qint64 pos = saved.toLongLong();
                if (pos > 0 && m_player->duration() > 0 && pos < m_player->duration() - 8000) {
                    m_player->setPosition(pos);
                    statusBar()->showMessage(
                        QString("Продолжаем с %1").arg(formatTime(pos)), 4000);
                }
            }
        }
    }
}

static QPixmap applyRoundedCorners(const QPixmap &src, int sz, int radius); // defined below in Visuals

void MainWindow::onMetaDataChanged() {
    const QMediaMetaData meta = m_player->metaData();
    const QString title  = meta.value(QMediaMetaData::Title).toString();
    QString artist = meta.value(QMediaMetaData::AlbumArtist).toString();
    if (artist.isEmpty())
        artist = meta.value(QMediaMetaData::ContributingArtist).toString();
    const QString album = meta.value(QMediaMetaData::AlbumTitle).toString();

    if (!title.isEmpty()) {
        m_titleLabel->setText(title);
        const QString display = artist.isEmpty() ? title : artist + "  —  " + title;
        m_miniTitle->setText(display);
        setWindowTitle("EchoBox II  —  " + display);
    }
    if (!artist.isEmpty()) m_artistLabel->setText(artist);
    if (!album.isEmpty())  m_albumLabel->setText(album);

    // Store metadata on the current playlist item for smart search
    if (m_currentIndex >= 0 && m_currentIndex < m_playlistWidget->count()) {
        QListWidgetItem *it = m_playlistWidget->item(m_currentIndex);
        if (it) {
            if (!title.isEmpty())  it->setData(Qt::UserRole + 2, title);
            if (!artist.isEmpty()) it->setData(Qt::UserRole + 3, artist);
            if (!album.isEmpty())  it->setData(Qt::UserRole + 4, album);
        }
    }

    // Обновить Discord с реальным названием из тегов
    if (m_discord && m_cfg.discordEnabled && m_player->playbackState() == QMediaPlayer::PlayingState)
        m_discord->updateActivity(m_titleLabel->text(), m_artistLabel->text());

    // Album art from metadata
    QImage img = meta.value(QMediaMetaData::CoverArtImage).value<QImage>();
    if (img.isNull()) img = meta.value(QMediaMetaData::ThumbnailImage).value<QImage>();
    if (!img.isNull()) {
        m_coverPixmap = QPixmap::fromImage(img);
        m_albumArt->setPixmap(applyRoundedCorners(m_coverPixmap, 230, artRadius()));
        if (m_miniAlbumArt) m_miniAlbumArt->setPixmap(applyRoundedCorners(m_coverPixmap, 40, 6));
        m_mediaStack->setCurrentWidget(m_albumArt);

        // Immediately set track icon for the currently playing track
        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
            const QUrl &url       = m_playlist[m_currentIndex];
            const QString icoFile = trackIconPath(url);
            if (!QFile::exists(icoFile)) {
                QDir().mkpath(QFileInfo(icoFile).absolutePath());
                // Save full-size cover (capped at 512px) for sharp album art
                const QPixmap large = (m_coverPixmap.width() > 512 || m_coverPixmap.height() > 512)
                    ? m_coverPixmap.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                    : m_coverPixmap;
                large.save(icoFile, "PNG");
            }
            if (m_cfg.showTrackIcons) {
                QListWidgetItem *item = m_playlistWidget->item(m_currentIndex);
                if (item) item->setIcon(QIcon(
                    m_coverPixmap.scaled(36, 36, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                                 .copy(0, 0, 36, 36)));
            }
        }
        return;
    }
    updateAlbumArt();
}

void MainWindow::onError(QMediaPlayer::Error /*e*/, const QString &msg) {
    statusBar()->showMessage("Ошибка: " + msg, 6000);
}

void MainWindow::onAudioBuffer(const QAudioBuffer &buffer) {
    m_visualizer->feedAudioBuffer(buffer);

    // Compute RMS amplitude and feed aurora background
    if (m_aurora) {
        const float *data  = buffer.constData<float>();
        const int    total = buffer.frameCount() * buffer.format().channelCount();
        float rms = 0.f;
        for (int i = 0; i < total; ++i) rms += data[i] * data[i];
        if (total > 0) m_aurora->setAmplitude(std::sqrt(rms / total));
    }

    // Push music into the APO ring so it gets mixed into the microphone
    if (m_micRouting && m_apoRing)
        apoFeed(buffer);
}

// ─── Visuals ─────────────────────────────────────────────────────────────────

static QPixmap applyRoundedCorners(const QPixmap &src, int sz, int radius) {
    QPixmap dst(sz, sz);
    dst.fill(Qt::transparent);
    QPainter rp(&dst);
    rp.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    if (radius > 0) {
        QPainterPath path;
        path.addRoundedRect(QRectF(0, 0, sz, sz), radius, radius);
        rp.setClipPath(path);
    }
    rp.drawPixmap(0, 0,
        src.scaled(sz, sz, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
           .copy(0, 0, sz, sz));
    return dst;
}

int MainWindow::artRadius() const {
    if (m_cfg.artShape == "square") return 0;
    if (m_cfg.artShape == "circle") return 115;
    return 12; // rounded (default)
}

void MainWindow::updateAlbumArt() {
    const int r = artRadius();
    if (!m_coverPixmap.isNull()) {
        m_albumArt->setPixmap(applyRoundedCorners(m_coverPixmap, 230, r));
        if (m_miniAlbumArt) m_miniAlbumArt->setPixmap(applyRoundedCorners(m_coverPixmap, 40, 6));
        return;
    }
    QPixmap pm(230, 230);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient g(0, 0, 230, 230);
    g.setColorAt(0.0, QColor(0x31, 0x32, 0x44));
    g.setColorAt(1.0, QColor(0x18, 0x18, 0x25));
    p.setBrush(g); p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, 230, 230, r, r);
    p.setPen(QColor(0xcb, 0xa6, 0xf7, 60));
    p.setFont(QFont("Segoe UI", 88));
    p.drawText(QRect(0, 0, 230, 230), Qt::AlignCenter, "♪");
    m_albumArt->setPixmap(pm);
    if (m_miniAlbumArt) m_miniAlbumArt->setPixmap(applyRoundedCorners(pm, 40, 6));
}

void MainWindow::setCurrentTrackVisual(int index) {
    for (int i = 0; i < m_playlistWidget->count(); ++i) {
        QListWidgetItem *it = m_playlistWidget->item(i);
        if (i == index) {
            QFont f = it->font(); f.setBold(true); it->setFont(f);
            it->setForeground(QColor(0xcb, 0xa6, 0xf7));
        } else {
            QFont f = it->font(); f.setBold(false); it->setFont(f);
            it->setForeground(QColor(0xcd, 0xd6, 0xf4));
        }
    }
    m_playlistWidget->scrollToItem(m_playlistWidget->item(index));
}

void MainWindow::updateTimeDisplay(qint64 pos, qint64 dur) {
    if (m_showRemaining && dur > 0) {
        m_timeLabel->setText(QString("-%1 / %2").arg(formatTime(dur - pos), formatTime(dur)));
    } else {
        m_timeLabel->setText(QString("%1 / %2").arg(formatTime(pos), formatTime(dur)));
    }
}

// ─── About ───────────────────────────────────────────────────────────────────

void MainWindow::showAbout() {
    QMessageBox::about(this, "О программе EchoBox II",
        "<h2 style='color:#cba6f7'>EchoBox II  v" + kAppVersion + "</h2>"
        "<p>Современный медиаплеер на <b>C++ / Qt6</b></p>"
        "<p>Форматы: MP3, FLAC, OGG, WAV, AAC, M4A, OPUS,<br>"
        "MP4, MKV, AVI, MOV, WebM и другие</p>"
        "<hr>"
        "<p><b>Возможности:</b></p>"
        "<ul style='margin:0;padding-left:16px'>"
        "<li>Воспроизведение по ссылке — SoundCloud, YouTube и др. (Ctrl+U)</li>"
        "<li>Осциллограмма на слайдере перемотки</li>"
        "<li>Анимированный фон с частицами, реагирующий на музыку</li>"
        "<li>Умный поиск по названию, исполнителю и альбому</li>"
        "<li>Сканер библиотеки (Файл → Сканировать библиотеку)</li>"
        "<li>Кастомный шрифт интерфейса</li>"
        "<li>Discord Rich Presence</li>"
        "<li>Кроссфейд, память позиции, несколько плейлистов</li>"
        "</ul>"
        "<hr>"
        "<p><b>Горячие клавиши:</b></p>"
        "<table cellspacing='4'>"
        "<tr><td><b>Пробел</b></td><td>Играть / Пауза</td></tr>"
        "<tr><td><b>M</b></td><td>Выкл. / вкл. звук</td></tr>"
        "<tr><td><b>Ctrl+←/→</b></td><td>Предыдущий / Следующий</td></tr>"
        "<tr><td><b>←/→</b></td><td>Перемотка ±5 сек</td></tr>"
        "<tr><td><b>Shift+←/→</b></td><td>Перемотка ±30 сек</td></tr>"
        "<tr><td><b>↑/↓</b></td><td>Громкость ±5%</td></tr>"
        "<tr><td><b>Del</b></td><td>Удалить из плейлиста</td></tr>"
        "<tr><td><b>Ctrl+F</b></td><td>Фокус на поиск</td></tr>"
        "<tr><td><b>Ctrl+U</b></td><td>Открыть по ссылке</td></tr>"
        "<tr><td><b>F11</b></td><td>Мини-плеер</td></tr>"
        "</table>"
        "<hr>"
        "<p style='color:#6c7086'>© 2026 BANANCHIKIREAL · MIT License</p>");
}

// ─── Обновления ──────────────────────────────────────────────────────────────

static QVector<int> parseVersionNumbers(QString v) {
    if (v.startsWith('v', Qt::CaseInsensitive)) v.remove(0, 1);
    const int dashIdx = v.indexOf('-');
    const QString numsPart = (dashIdx >= 0) ? v.left(dashIdx) : v;
    QVector<int> nums;
    for (const QString &part : numsPart.split('.'))
        nums.append(part.toInt());
    while (nums.size() < 3) nums.append(0);
    return nums;
}

// true, если remote строго новее local (числа сравниваются по семверу;
// при равных числах релиз без "-beta"/"-rc" и т.п. считается новее)
static bool isNewerVersion(const QString &remote, const QString &local) {
    const QVector<int> r = parseVersionNumbers(remote);
    const QVector<int> l = parseVersionNumbers(local);
    for (int i = 0; i < 3; ++i)
        if (r[i] != l[i]) return r[i] > l[i];
    const bool remotePre = remote.contains('-');
    const bool localPre  = local.contains('-');
    if (remotePre != localPre) return localPre && !remotePre;
    return false;
}

void MainWindow::checkForUpdates(bool manual) {
    QNetworkRequest req((QUrl(kUpdateApiUrl)));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "EchoBoxII-UpdateCheck");

    QNetworkReply *reply = m_streamArtNam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual]{
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (manual) statusBar()->showMessage("Не удалось проверить обновления", 5000);
            return;
        }

        // Общий список содержит и pre-release-теги — ищем среди них
        // самый новый по номеру версии, а не просто первый в ответе
        const QJsonArray releases = QJsonDocument::fromJson(reply->readAll()).array();
        QJsonObject newest;
        QString tag;
        for (const QJsonValue &v : releases) {
            const QJsonObject obj = v.toObject();
            if (obj.value("draft").toBool()) continue;
            const QString t = obj.value("tag_name").toString();
            if (t.isEmpty()) continue;
            if (tag.isEmpty() || isNewerVersion(t, tag)) { tag = t; newest = obj; }
        }
        const QString url   = newest.value("html_url").toString();
        const QString notes = newest.value("body").toString();
        if (tag.isEmpty()) {
            if (manual) statusBar()->showMessage("Не удалось проверить обновления", 5000);
            return;
        }

        if (!isNewerVersion(tag, kAppVersion)) {
            if (manual) statusBar()->showMessage("У вас последняя версия", 4000);
            return;
        }

        // При фоновой (не ручной) проверке не спамим одним и тем же
        // релизом повторно, если пользователь уже его закрыл разок
        const QString skipKey = "update/skippedVersion";
        if (!manual && m_settings.value(skipKey).toString() == tag) return;

        QMessageBox box(this);
        box.setWindowTitle("Доступно обновление");
        box.setTextFormat(Qt::RichText);
        const QString notesHtml = notes.isEmpty() ? QString()
            : ("<p>" + notes.toHtmlEscaped().left(600).replace("\n", "<br>") + "</p>");
        box.setText(QString(
            "<h3 style='color:#cba6f7'>EchoBox II %1</h3>"
            "<p>У вас установлена версия %2</p>%3")
                .arg(tag, kAppVersion, notesHtml));
        QPushButton *downloadBtn = box.addButton("Скачать", QMessageBox::AcceptRole);
        box.addButton(manual ? "Закрыть" : "Пропустить эту версию", QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() == downloadBtn)
            QDesktopServices::openUrl(QUrl(url));
        else if (!manual)
            m_settings.setValue(skipKey, tag);
    });
}

// ─── Drag & Drop ─────────────────────────────────────────────────────────────

void MainWindow::dragEnterEvent(QDragEnterEvent *e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e) {
    QList<QUrl> localUrls;
    for (const QUrl &u : e->mimeData()->urls()) {
        if (u.isLocalFile()) localUrls.append(u);
        else if (u.scheme() == "http" || u.scheme() == "https") openStreamUrl(u.toString());
    }
    addFiles(localUrls);
}

// ─── Keyboard ────────────────────────────────────────────────────────────────

void MainWindow::keyPressEvent(QKeyEvent *e) {
    const bool ctrl  = e->modifiers() & Qt::ControlModifier;
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    switch (e->key()) {
    case Qt::Key_Space: togglePlayPause(); break;
    case Qt::Key_S:     stop();            break;
    case Qt::Key_M:     toggleMute();      break;
    case Qt::Key_F11:   toggleMiniPlayer(); break;
    case Qt::Key_Left:
        if (ctrl)  previous();
        else       m_player->setPosition(m_player->position() - (shift ? 30000 : 5000));
        break;
    case Qt::Key_Right:
        if (ctrl)  next();
        else       m_player->setPosition(m_player->position() + (shift ? 30000 : 5000));
        break;
    case Qt::Key_Up:
        m_volumeSlider->setValue(qMin(m_volumeSlider->value() + 5, 100)); break;
    case Qt::Key_Down:
        m_volumeSlider->setValue(qMax(m_volumeSlider->value() - 5, 0));   break;
    case Qt::Key_Delete:
        removeSelectedTracks(); break;
    case Qt::Key_F:
        if (ctrl) m_searchEdit->setFocus(); break;
    case Qt::Key_A:
        if (ctrl) m_playlistWidget->selectAll(); break;
    default: QMainWindow::keyPressEvent(e);
    }
}

// ─── Close → tray ────────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent *e) {
    saveTrackPosition();
    if (m_cfg.closeToTray && m_tray && m_tray->isVisible()) {
        hide();
        e->ignore();
        m_tray->showMessage("EchoBox II",
            "Продолжает работать в трее. Двойной клик — открыть окно.",
            QSystemTrayIcon::Information, 3000);
    } else {
        saveSettings();
        e->accept();
    }
}

// ─── Multi-playlist ──────────────────────────────────────────────────────────

void MainWindow::saveCurrentPlaylistState() {
    if (m_activePl < 0 || m_activePl >= m_playlists.size()) return;
    m_playlists[m_activePl].tracks       = m_playlist;
    m_playlists[m_activePl].currentTrack = m_currentIndex;
}

void MainWindow::loadPlaylistState(int index) {
    if (index < 0 || index >= m_playlists.size()) return;

    m_player->stop();
    m_playlist     = m_playlists[index].tracks;
    m_currentIndex = m_playlists[index].currentTrack;

    m_playlistWidget->clear();
    for (int i = 0; i < m_playlist.size(); ++i) {
        const QUrl &u = m_playlist[i];
        const QString name = playlistRowLabel(u);
        auto *item = new QListWidgetItem(QString("  %1.  %2").arg(i + 1).arg(name));
        item->setData(Qt::UserRole, u);
        m_playlistWidget->addItem(item);
        if (m_cfg.showTrackIcons) applyTrackIcon(item, u);
    }

    if (m_currentIndex >= 0 && m_currentIndex < m_playlistWidget->count())
        setCurrentTrackVisual(m_currentIndex);

    updatePlaylistInfo();
    m_seekSlider->clearWaveform();
    m_miniWaveform->clearWaveform();
    m_seekSlider->setValue(0);
    m_seekSlider->setRange(0, 0);
    m_miniWaveform->setValue(0);
    m_miniWaveform->setRange(0, 0);
    m_timeLabel->setText("0:00 / 0:00");
    m_coverPixmap = QPixmap();
    updateAlbumArt();

    // Scan for missing track icons in the background (local files only)
    QList<QUrl> noIcon;
    for (const QUrl &u : m_playlist)
        if (u.isLocalFile() && !QFile::exists(trackIconPath(u)))
            noIcon.append(u);
    if (!noIcon.isEmpty()) scheduleScan(noIcon);
}

void MainWindow::newPlaylist() {
    const int n    = m_playlists.size() + 1;
    const QString name = QString("Плейлист %1").arg(n);
    m_playlists.append({name, {}, -1});
    m_tabBar->addTab(name);
    m_tabBar->setCurrentIndex(m_tabBar->count() - 1);
}

void MainWindow::onTabChanged(int index) {
    if (index == m_activePl || index < 0) return;
    saveCurrentPlaylistState();
    m_activePl = index;
    loadPlaylistState(index);
}

void MainWindow::onTabDoubleClicked(int index) {
    if (index < 0 || index >= m_playlists.size()) return;
    bool ok;
    const QString name = QInputDialog::getText(
        this, "Переименовать плейлист", "Название:",
        QLineEdit::Normal, m_playlists[index].name, &ok);
    if (ok && !name.trimmed().isEmpty()) {
        m_playlists[index].name = name.trimmed();
        m_tabBar->setTabText(index, name.trimmed());
    }
}

void MainWindow::onTabContextMenu(const QPoint &pos) {
    const int index = m_tabBar->tabAt(pos);
    if (index < 0) return;
    QMenu menu(this);
    menu.addAction("Переименовать", [this, index]{ onTabDoubleClicked(index); });
    auto *delAct = menu.addAction("Удалить плейлист", [this, index]{ deletePlaylist(index); });
    delAct->setEnabled(m_playlists.size() > 1);
    menu.exec(m_tabBar->mapToGlobal(pos));
}

void MainWindow::deletePlaylist(int index) {
    if (m_playlists.size() <= 1 || index < 0 || index >= m_playlists.size()) return;

    const QString plName = m_playlists[index].name;
    const int trackCount = (index == m_activePl)
        ? m_playlist.size()
        : m_playlists[index].tracks.size();

    QString msg = QString("Удалить плейлист «%1»?").arg(plName);
    if (trackCount > 0)
        msg += QString("\n%1 трек%2 будут потеряны.").arg(trackCount)
               .arg(trackCount == 1 ? "" : trackCount < 5 ? "а" : "ов");

    if (QMessageBox::question(this, "Удалить плейлист", msg,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    m_tabBar->blockSignals(true);
    m_tabBar->removeTab(index);
    m_playlists.removeAt(index);

    if (m_activePl >= m_playlists.size())
        m_activePl = m_playlists.size() - 1;
    else if (m_activePl > index)
        --m_activePl;

    m_tabBar->setCurrentIndex(m_activePl);
    m_tabBar->blockSignals(false);

    loadPlaylistState(m_activePl);
}

// ─── Volume helper ───────────────────────────────────────────────────────────

void MainWindow::applyVolume() {
    m_audioOutput->setVolume(m_volumeSlider->value() / 100.0f * m_fadeFactor);
}

// ─── Position memory (Feature 8) ─────────────────────────────────────────────

QString MainWindow::positionKey(const QUrl &url) const {
    return "pos/" + QString::number(qHash(url.toString()));
}

void MainWindow::saveTrackPosition() {
    if (m_currentIndex < 0 || m_currentIndex >= m_playlist.size()) return;
    const qint64 pos = m_player->position();
    const qint64 dur = m_player->duration();
    const QUrl  &url = m_playlist[m_currentIndex];
    if (dur > 30000 && pos > 8000 && pos < dur - 8000)
        m_settings.setValue(positionKey(url), pos);
    else
        m_settings.remove(positionKey(url));
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

bool MainWindow::isVideoFile(const QUrl &url) const {
    return VIDEO_EXTS.contains(QFileInfo(url.toLocalFile()).suffix().toLower());
}

QString MainWindow::formatTime(qint64 ms) {
    const int s   = static_cast<int>(ms / 1000);
    const int hrs = s / 3600;
    const int min = (s % 3600) / 60;
    const int sec = s % 60;
    if (hrs > 0)
        return QString("%1:%2:%3").arg(hrs)
               .arg(min, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
    return QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0'));
}

// ─── Background metadata / icon scanner ──────────────────────────────────────

void MainWindow::scheduleScan(const QList<QUrl> &urls) {
    for (const QUrl &u : urls)
        if (!m_metaScanQueue.contains(u))
            m_metaScanQueue.append(u);  // always scan — reads metadata for smart search
    if (!m_scanInProgress && !m_metaScanQueue.isEmpty())
        advanceMetaScan();
}

void MainWindow::advanceMetaScan() {
    if (m_metaScanQueue.isEmpty()) { m_scanInProgress = false; return; }
    m_scanInProgress = true;
    m_metaReader->setSource(m_metaScanQueue.first());
}

void MainWindow::handleMetaReaderUpdate() {
    if (m_metaScanQueue.isEmpty()) return;
    const QUrl url = m_metaScanQueue.first();
    if (m_metaReader->source() != url) return;

    const QMediaMetaData meta = m_metaReader->metaData();
    const auto s = m_metaReader->mediaStatus();
    const bool ready = (s == QMediaPlayer::LoadedMedia  ||
                        s == QMediaPlayer::BufferedMedia ||
                        s == QMediaPlayer::InvalidMedia  ||
                        s == QMediaPlayer::NoMedia);
    if (!ready) return;

    // Store title / artist / album on the playlist item for smart search
    const int idx = m_playlist.indexOf(url);
    if (idx >= 0) {
        QListWidgetItem *item = m_playlistWidget->item(idx);
        if (item) {
            const QString title  = meta.value(QMediaMetaData::Title).toString();
            const QString artist = meta.value(QMediaMetaData::ContributingArtist).toString().isEmpty()
                                 ? meta.value(QMediaMetaData::AlbumArtist).toString()
                                 : meta.value(QMediaMetaData::ContributingArtist).toString();
            const QString album  = meta.value(QMediaMetaData::AlbumTitle).toString();
            if (!title.isEmpty())  item->setData(Qt::UserRole + 2, title);
            if (!artist.isEmpty()) item->setData(Qt::UserRole + 3, artist);
            if (!album.isEmpty())  item->setData(Qt::UserRole + 4, album);
        }
    }

    // Save cover art icon if not cached yet
    const QString icoFile = trackIconPath(url);
    if (!QFile::exists(icoFile)) {
        QImage img = meta.value(QMediaMetaData::CoverArtImage).value<QImage>();
        if (img.isNull()) img = meta.value(QMediaMetaData::ThumbnailImage).value<QImage>();
        if (!img.isNull()) {
            QDir().mkpath(QFileInfo(icoFile).absolutePath());
            const QPixmap full = QPixmap::fromImage(img);
            // Save full-size (capped at 512px) for sharp album art display
            const QPixmap large = (full.width() > 512 || full.height() > 512)
                ? full.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                : full;
            large.save(icoFile, "PNG");
            if (m_cfg.showTrackIcons && idx >= 0) {
                QListWidgetItem *item = m_playlistWidget->item(idx);
                if (item) {
                    const QPixmap thumb = full.scaled(36, 36, Qt::KeepAspectRatioByExpanding,
                                                      Qt::SmoothTransformation).copy(0, 0, 36, 36);
                    item->setIcon(QIcon(thumb));
                }
            }
        }
    } else if (m_cfg.showTrackIcons && idx >= 0) {
        QListWidgetItem *item = m_playlistWidget->item(idx);
        if (item && item->icon().isNull())
            item->setIcon(QIcon(icoFile));
    }

    updateDuplicateHighlights();
    m_metaScanQueue.removeFirst();
    QTimer::singleShot(30, this, &MainWindow::advanceMetaScan);
}

// ─── Track icon helpers ───────────────────────────────────────────────────────

QString MainWindow::trackIconPath(const QUrl &url) const {
    const QString hash = QCryptographicHash::hash(
        url.toString().toUtf8(), QCryptographicHash::Md5).toHex();
    const QString base = m_cfg.iconsFolder.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/icons"
        : m_cfg.iconsFolder;
    return base + "/" + hash + ".png";
}

void MainWindow::applyTrackIcon(QListWidgetItem *item, const QUrl &url) {
    const QString f = trackIconPath(url);
    if (QFile::exists(f)) {
        QPixmap pm(f);
        if (!pm.isNull()) {
            // Icon files are now full-size — scale down for the 36px playlist slot
            item->setIcon(QIcon(pm.scaled(36, 36, Qt::KeepAspectRatioByExpanding,
                                          Qt::SmoothTransformation).copy(0, 0, 36, 36)));
        }
    }
}

void MainWindow::reloadTrackIcons() {
    for (int i = 0; i < m_playlistWidget->count(); ++i) {
        QListWidgetItem *it = m_playlistWidget->item(i);
        const QUrl url = it->data(Qt::UserRole).value<QUrl>();
        if (m_cfg.showTrackIcons) applyTrackIcon(it, url);
        else                  it->setIcon(QIcon());
    }
    if (g_delegate) { g_delegate->showIcons = m_cfg.showTrackIcons; m_playlistWidget->update(); }
}

// ─── Playlist persistence ─────────────────────────────────────────────────────

void MainWindow::savePlaylistsToFile() {
    saveCurrentPlaylistState();
    const QString playlistsDir = m_cfg.playlistsFolder.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : m_cfg.playlistsFolder;
    const QString dir = playlistsDir;
    QDir().mkpath(dir);

    QJsonArray arr;
    for (const auto &pl : m_playlists) {
        QJsonObject obj;
        obj["name"]         = pl.name;
        obj["currentTrack"] = pl.currentTrack;
        QJsonArray tracks;
        for (const QUrl &u : pl.tracks) tracks.append(u.toString());
        obj["tracks"] = tracks;
        arr.append(obj);
    }

    QJsonObject root;
    root["playlists"] = arr;
    root["activePl"]  = m_activePl;

    QFile f(dir + "/playlists.json");
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson());
}

void MainWindow::loadPlaylistsFromFile() {
    const QString baseDir = m_cfg.playlistsFolder.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : m_cfg.playlistsFolder;
    const QString path = baseDir + "/playlists.json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;

    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonArray  arr  = root["playlists"].toArray();
    if (arr.isEmpty()) return;

    m_playlists.clear();
    m_tabBar->blockSignals(true);
    while (m_tabBar->count() > 0) m_tabBar->removeTab(0);

    for (const QJsonValue &v : arr) {
        const QJsonObject obj = v.toObject();
        PlaylistEntry entry;
        entry.name         = obj["name"].toString();
        entry.currentTrack = obj["currentTrack"].toInt(-1);
        for (const QJsonValue &t : obj["tracks"].toArray())
            entry.tracks.append(QUrl(t.toString()));
        m_playlists.append(entry);
        m_tabBar->addTab(entry.name);
    }

    m_activePl = qBound(0, root["activePl"].toInt(0), m_playlists.size() - 1);
    m_tabBar->setCurrentIndex(m_activePl);
    m_tabBar->blockSignals(false);
    loadPlaylistState(m_activePl);
}

// ─── Settings dialog ──────────────────────────────────────────────────────────

void MainWindow::openSettings() {
    const AppSettings savedCfg = m_cfg;

    SettingsDialog dlg(m_cfg, this);
    connect(&dlg, &SettingsDialog::applied, this, [this](const AppSettings &s) {
        const AppSettings prev = m_cfg;
        m_cfg = s;
        applyTheme();
        if (m_cfg.showTrackIcons != prev.showTrackIcons ||
            m_cfg.iconsFolder    != prev.iconsFolder) {
            if (g_delegate) g_delegate->showIcons = m_cfg.showTrackIcons;
            reloadTrackIcons();
        }
        if (!m_cfg.discordEnabled && m_discord) m_discord->clearActivity();
    });

    auto applyIconsIfNeeded = [this](const AppSettings &prev) {
        if (g_delegate) g_delegate->showIcons = m_cfg.showTrackIcons;
        if (m_cfg.showTrackIcons != prev.showTrackIcons ||
            m_cfg.iconsFolder    != prev.iconsFolder)
            reloadTrackIcons();
        else
            m_playlistWidget->viewport()->update();
    };

    const AppSettings liveApplied = m_cfg; // state after live-apply (may differ from savedCfg)
    if (dlg.exec() == QDialog::Accepted) {
        const AppSettings prev = m_cfg;
        m_cfg = dlg.result();
        applyTheme();
        applyIconsIfNeeded(prev);
        if (!m_cfg.discordEnabled && m_discord) m_discord->clearActivity();
        saveSettings();
        statusBar()->showMessage("Настройки сохранены", 2000);
    } else {
        // Cancel — revert to state before dialog opened
        const AppSettings prev = liveApplied;
        m_cfg = savedCfg;
        applyTheme();
        applyIconsIfNeeded(prev);
    }
}

// ─── Library scanner ──────────────────────────────────────────────────────────

void MainWindow::scanLibrary()
{
    const QString folder = m_cfg.libraryFolder;
    if (folder.isEmpty() || !QDir(folder).exists()) {
        statusBar()->showMessage(
            "Папка библиотеки не задана. Укажи её в Настройки → Файлы.", 5000);
        return;
    }

    // Stop previous scan if running
    if (m_libraryThread && m_libraryThread->isRunning()) {
        if (m_libraryScanner) m_libraryScanner->cancel();
        m_libraryThread->quit();
        m_libraryThread->wait(2000);
    }

    // Find or create "Библиотека" tab
    if (m_libraryPlIdx < 0 || m_libraryPlIdx >= m_playlists.size()) {
        saveCurrentPlaylistState();
        const QString name = "📚 Библиотека";
        m_playlists.append({name, {}, -1});
        m_tabBar->addTab(name);
        m_libraryPlIdx = m_playlists.size() - 1;
        m_tabBar->setCurrentIndex(m_libraryPlIdx);
    } else {
        // Clear existing library playlist
        saveCurrentPlaylistState();
        m_playlists[m_libraryPlIdx].tracks.clear();
        m_playlists[m_libraryPlIdx].currentTrack = -1;
        m_tabBar->setCurrentIndex(m_libraryPlIdx);
        loadPlaylistState(m_libraryPlIdx);
    }

    statusBar()->showMessage("Сканирование библиотеки...");

    // Setup file system watcher
    if (!m_libraryWatcher) {
        m_libraryWatcher = new QFileSystemWatcher(this);
        connect(m_libraryWatcher, &QFileSystemWatcher::directoryChanged,
                this, &MainWindow::onLibraryDirChanged);
    }
    m_libraryWatcher->removePaths(m_libraryWatcher->directories());
    m_libraryWatcher->addPath(folder);

    // Launch scanner in background thread
    m_libraryScanner = new LibraryScanner(folder);
    m_libraryThread  = new QThread(this);
    m_libraryScanner->moveToThread(m_libraryThread);

    connect(m_libraryThread,  &QThread::started,
            m_libraryScanner, &LibraryScanner::run);
    connect(m_libraryScanner, &LibraryScanner::batchReady,
            this, &MainWindow::onLibraryBatch,     Qt::QueuedConnection);
    connect(m_libraryScanner, &LibraryScanner::progress,
            this, &MainWindow::onLibraryProgress,  Qt::QueuedConnection);
    connect(m_libraryScanner, &LibraryScanner::finished,
            this, &MainWindow::onLibraryFinished,  Qt::QueuedConnection);
    connect(m_libraryScanner, &LibraryScanner::finished,
            m_libraryThread,  &QThread::quit);
    connect(m_libraryThread,  &QThread::finished,
            m_libraryScanner, &QObject::deleteLater);
    connect(m_libraryThread,  &QThread::finished,
            m_libraryThread,  &QObject::deleteLater);

    m_libraryThread->start();
}

void MainWindow::onLibraryBatch(QList<QUrl> batch)
{
    if (m_libraryPlIdx < 0 || m_libraryPlIdx >= m_playlists.size()) return;

    // If library tab is active, add directly to widget; otherwise store in data
    const bool isActive = (m_activePl == m_libraryPlIdx);

    // Sort batch by filename
    std::sort(batch.begin(), batch.end(), [](const QUrl &a, const QUrl &b){
        return QFileInfo(a.toLocalFile()).fileName().toLower()
             < QFileInfo(b.toLocalFile()).fileName().toLower();
    });

    for (const QUrl &url : batch) {
        if (m_playlists[m_libraryPlIdx].tracks.contains(url)) continue;
        m_playlists[m_libraryPlIdx].tracks.append(url);

        if (isActive) {
            const int n    = m_playlist.size();
            const QString name = QFileInfo(url.toLocalFile()).fileName();
            auto *item = new QListWidgetItem(QString("  %1.  %2").arg(n + 1).arg(name));
            item->setData(Qt::UserRole, url);
            m_playlistWidget->addItem(item);
            m_playlist.append(url);
            if (m_cfg.showTrackIcons) applyTrackIcon(item, url);
        }
    }

    if (isActive) {
        updatePlaylistInfo();
        updateDuplicateHighlights();
        scheduleScan(batch);
    }
}

void MainWindow::onLibraryProgress(int found, int /*scanned*/)
{
    statusBar()->showMessage(QString("Сканирование библиотеки: найдено %1 треков...").arg(found));
}

void MainWindow::onLibraryFinished(int total)
{
    statusBar()->showMessage(
        QString("Библиотека: %1 треков найдено  ·  %2")
            .arg(total)
            .arg(m_cfg.libraryFolder), 8000);

    // If the library tab wasn't active during scan, refresh it now
    if (m_activePl == m_libraryPlIdx)
        updatePlaylistInfo();
}

void MainWindow::onLibraryDirChanged(const QString &/*path*/)
{
    // New files appeared — offer a rescan via status bar message
    statusBar()->showMessage(
        "Обнаружены новые файлы в библиотеке. "
        "Файл → Сканировать библиотеку для обновления.", 8000);
}

void MainWindow::updateDuplicateHighlights()
{
    const int n = m_playlistWidget->count();
    if (n == 0) return;

    QHash<QString, int> pathCount;
    for (int i = 0; i < n; ++i) {
        QListWidgetItem *it = m_playlistWidget->item(i);
        const QString path = it->data(Qt::UserRole).value<QUrl>().toLocalFile().toLower();
        if (!path.isEmpty()) pathCount[path]++;  // пустой путь — ссылка на музыку, не файл
    }

    QHash<QString, int> metaCount;
    for (int i = 0; i < n; ++i) {
        QListWidgetItem *it = m_playlistWidget->item(i);
        const QString title  = it->data(Qt::UserRole + 2).toString().toLower().trimmed();
        const QString artist = it->data(Qt::UserRole + 3).toString().toLower().trimmed();
        if (!title.isEmpty())
            metaCount[artist + "||" + title]++;
    }

    for (int i = 0; i < n; ++i) {
        QListWidgetItem *it = m_playlistWidget->item(i);
        const QString path    = it->data(Qt::UserRole).value<QUrl>().toLocalFile().toLower();
        const QString title   = it->data(Qt::UserRole + 2).toString().toLower().trimmed();
        const QString artist  = it->data(Qt::UserRole + 3).toString().toLower().trimmed();
        const QString metaKey = artist + "||" + title;

        const bool isDup = (!path.isEmpty() && pathCount.value(path) > 1)
                        || (!title.isEmpty() && metaCount.value(metaKey) > 1);

        it->setData(Qt::UserRole + 10, isDup);
    }
    m_playlistWidget->viewport()->update();
}

// ─── APO mic routing (music → microphone via EchoBox APO) ────────────────────

void MainWindow::toggleMicRouting()
{
    if (m_micRouting) {
        m_micRouting = false;
        if (m_apoOpenTimer) { m_apoOpenTimer->stop(); }
        apoCloseRing();
        if (m_micBtn) m_micBtn->setChecked(false);
        statusBar()->showMessage("Музыка в микрофон: выкл", 2500);
        return;
    }

    m_micRouting = true;
    if (m_micBtn) m_micBtn->setChecked(true);

    // The ring is created inside audiodg by the APO — it only exists while some
    // app has the mic open. Poll until it appears.
    if (!m_apoOpenTimer) {
        m_apoOpenTimer = new QTimer(this);
        m_apoOpenTimer->setInterval(500);
        connect(m_apoOpenTimer, &QTimer::timeout, this, &MainWindow::apoTryOpenRing);
    }
    apoTryOpenRing();          // try immediately
    if (!m_apoRing) {
        m_apoOpenTimer->start();
        statusBar()->showMessage(
            "Музыка в микрофон: ожидаю микрофон... "
            "(открой голосовой чат; если не установлено — запусти apo\\install.bat)", 8000);
    }
}

void MainWindow::showMicMenu()
{
    QMenu menu(this);

    auto *hdr = menu.addAction(m_micRouting ? "Музыка в микрофон: ВКЛ" : "Музыка в микрофон: выкл");
    hdr->setEnabled(false);
    menu.addSeparator();

    auto *blockAct = menu.addAction("Только музыка (заглушить микрофон)");
    blockAct->setCheckable(true);
    blockAct->setChecked(m_apoBlockVoice);
    connect(blockAct, &QAction::toggled, this, [this](bool on){
        m_apoBlockVoice = on;
        apoPushControls();
        statusBar()->showMessage(on ? "Только музыка — микрофон заглушён"
                                    : "Микрофон + музыка", 2500);
    });

    auto *gateAct = menu.addAction("Шумоподавление голоса");
    gateAct->setCheckable(true);
    gateAct->setChecked(m_apoNoiseGate);
    gateAct->setToolTip("Убирает посторонний шум (клики, фон), оставляя голос");
    connect(gateAct, &QAction::toggled, this, [this](bool on){
        m_apoNoiseGate = on;
        apoPushControls();
        statusBar()->showMessage(on ? "Шумоподавление: вкл" : "Шумоподавление: выкл", 2500);
    });

    // Сила шумоподавления (порог гейта)
    auto *gateMenu = menu.addMenu("Сила шумоподавления");
    struct { const char *label; float thr; } gates[] = {
        {"Слабое (тихий фон)",   0.008f},
        {"Среднее",              0.02f},
        {"Сильное (шумно вокруг)",0.05f},
    };
    auto *ggrp = new QActionGroup(gateMenu);
    ggrp->setExclusive(true);
    for (auto &g : gates) {
        auto *a = gateMenu->addAction(g.label);
        a->setCheckable(true);
        a->setActionGroup(ggrp);
        if (qFuzzyCompare(m_apoGateThresh, g.thr)) a->setChecked(true);
        const float thr = g.thr;
        connect(a, &QAction::triggered, this, [this, thr]{
            m_apoGateThresh = thr;
            apoPushControls();
            statusBar()->showMessage("Порог шумоподавления обновлён", 2000);
        });
    }

    menu.addSeparator();
    auto *volHdr = menu.addAction("Громкость музыки:");
    volHdr->setEnabled(false);

    struct { const char *label; float gain; } levels[] = {
        {"50%", 0.5f}, {"100%", 1.0f}, {"150%", 1.5f},
        {"200%", 2.0f}, {"300%", 3.0f},
    };
    auto *grp = new QActionGroup(&menu);
    grp->setExclusive(true);
    for (auto &lv : levels) {
        auto *a = menu.addAction(lv.label);
        a->setCheckable(true);
        a->setActionGroup(grp);
        if (qFuzzyCompare(m_apoMusicGain, lv.gain)) a->setChecked(true);
        const float g = lv.gain;
        connect(a, &QAction::triggered, this, [this, g]{
            m_apoMusicGain = g;
            apoPushControls();
            statusBar()->showMessage(
                QString("Громкость музыки: %1%").arg(int(g * 100)), 2500);
        });
    }

    menu.exec(QCursor::pos());
}

void MainWindow::apoTryOpenRing()
{
    if (!m_micRouting) { if (m_apoOpenTimer) m_apoOpenTimer->stop(); return; }
    if (m_apoRing) { if (m_apoOpenTimer) m_apoOpenTimer->stop(); return; }

    HANDLE h = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, kEchoBoxRingName);
    if (!h) return; // APO ещё не активен — ждём

    auto *ring = static_cast<EchoBoxRing *>(
        MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (!ring) { CloseHandle(h); return; }

    if (ring->magic != kEchoBoxRingMagic) { UnmapViewOfFile(ring); CloseHandle(h); return; }

    // We are the producer — declare our sample rate.
    ring->sampleRate = 48000;
    m_apoWritePos    = ring->writePos;

    m_apoMapping = h;
    m_apoRing    = ring;
    apoPushControls();               // передаём текущие громкость/режим
    if (m_apoOpenTimer) m_apoOpenTimer->stop();
    statusBar()->showMessage("Музыка в микрофон: включено ✓  играй трек", 4000);
}

void MainWindow::apoPushControls()
{
    auto *ring = static_cast<EchoBoxRing *>(m_apoRing);
    if (!ring) return;
    ring->musicGain  = m_apoMusicGain;
    ring->blockVoice = m_apoBlockVoice ? 1u : 0u;
    ring->noiseGate  = m_apoNoiseGate ? 1u : 0u;
    ring->gateThresh = m_apoGateThresh;
}

void MainWindow::apoCloseRing()
{
    if (m_apoRing)    { UnmapViewOfFile(m_apoRing);          m_apoRing    = nullptr; }
    if (m_apoMapping) { CloseHandle((HANDLE)m_apoMapping);   m_apoMapping = nullptr; }
}

void MainWindow::apoFeed(const QAudioBuffer &buffer)
{
    auto *ring = static_cast<EchoBoxRing *>(m_apoRing);
    if (!ring) return;

    const QAudioFormat &f = buffer.format();
    const int inCh     = f.channelCount();
    const int inFrames = buffer.frameCount();
    const int inRate   = f.sampleRate();
    if (inFrames <= 0 || inCh <= 0) return;

    // Read source sample → normalized float, any format
    auto rd = [&](int frame, int ch) -> float {
        const int i = frame * inCh + ch;
        switch (f.sampleFormat()) {
        case QAudioFormat::Float: return buffer.constData<float>()[i];
        case QAudioFormat::Int16: return buffer.constData<int16_t>()[i] / 32768.0f;
        case QAudioFormat::Int32: return buffer.constData<int32_t>()[i] / 2147483648.0f;
        default: return 0.0f;
        }
    };

    const uint32_t cap     = ring->capacityFrames;
    const uint32_t outRate = ring->sampleRate ? ring->sampleRate : 48000;
    uint32_t       wp      = m_apoWritePos % cap;

    auto pushFrame = [&](float L, float R) {
        ring->data[wp * 2]     = L;
        ring->data[wp * 2 + 1] = R;
        wp = (wp + 1) % cap;
    };

    if (inRate == (int)outRate) {
        for (int fr = 0; fr < inFrames; ++fr) {
            const float L = rd(fr, 0);
            const float R = inCh >= 2 ? rd(fr, 1) : L;
            pushFrame(L, R);
        }
    } else {
        // Linear resample inRate → outRate
        const double ratio    = double(inRate) / double(outRate);
        const int    outCount = int(double(inFrames) / ratio);
        for (int o = 0; o < outCount; ++o) {
            const double pos = o * ratio;
            const int    i0  = int(pos);
            const int    i1  = std::min(i0 + 1, inFrames - 1);
            const float  t   = float(pos - i0);
            const float  L   = rd(i0, 0) * (1 - t) + rd(i1, 0) * t;
            const float  R   = inCh >= 2 ? rd(i0, 1) * (1 - t) + rd(i1, 1) * t : L;
            pushFrame(L, R);
        }
    }

    m_apoWritePos   = wp;
    ring->writePos  = wp;          // publish
    ring->playerAlive = 1;
}
