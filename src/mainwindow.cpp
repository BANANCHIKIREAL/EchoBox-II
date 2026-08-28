#include "mainwindow.h"
#include "waveformslider.h"
#include "visualizer.h"
#include "backgroundwidget.h"
#include "libraryscanner.h"
#include <cmath>
#include <QFontDatabase>
#include "logo.h"
#include "icons.h"
#include "materialicons.h"
#include "discordrpc.h"
#include "thememanager.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QScreen>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QGraphicsOpacityEffect>
#include <QEasingCurve>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialog>
#include <QToolButton>
#include <QSlider>
#include <QLabel>
#include <QListWidget>
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
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
#include <QProgressBar>
#include <QDirIterator>
#include <QRandomGenerator>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QTextStream>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTextDocument>
#include <QRegularExpression>
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
#include <QThreadPool>
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
#include <QStyle>
#include <QPointer>
#include <QSharedPointer>
#include <functional>
#include <numeric>
#include <algorithm>

class ScrollingLabel final : public QLabel {
public:
    explicit ScrollingLabel(QWidget *parent = nullptr) : QLabel(parent) {
        m_timer.setInterval(16);
        connect(&m_timer, &QTimer::timeout, this, [this] {
            if (m_lastText != text()) {
                m_lastText = text();
                m_offset = 0.0;
                m_holdFrames = 65;
            }
            if (fontMetrics().horizontalAdvance(text()) <= contentsRect().width()) {
                m_offset = 0.0;
                update();
                return;
            }
            if (m_holdFrames > 0) {
                --m_holdFrames;
            } else {
                m_offset += 0.55;
                const qreal cycle = fontMetrics().horizontalAdvance(text()) + 46.0;
                if (m_offset >= cycle) {
                    m_offset = 0.0;
                    m_holdFrames = 38;
                }
            }
            update();
        });
        m_timer.start();
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        const QRect area = contentsRect();
        const QFontMetrics metrics(font());
        const int textWidth = metrics.horizontalAdvance(text());
        if (textWidth <= area.width()) {
            QLabel::paintEvent(event);
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setClipRect(area);
        painter.setFont(font());
        painter.setPen(palette().color(QPalette::WindowText));
        const qreal baseline = area.center().y()
            + (metrics.ascent() - metrics.descent()) / 2.0;
        const qreal firstX = area.left() - m_offset;
        painter.drawText(QPointF(firstX, baseline), text());
        painter.drawText(QPointF(firstX + textWidth + 46.0, baseline), text());
    }

private:
    QTimer m_timer;
    QString m_lastText;
    qreal m_offset = 0.0;
    int m_holdFrames = 65;
};

class LiquidGlassWidget final : public QWidget {
public:
    explicit LiquidGlassWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
        setMouseTracking(true);
        m_lightPos = QPointF(0.25, 0.12);
        m_timer.setInterval(16);
        connect(&m_timer, &QTimer::timeout, this, [this] {
            m_glow += (m_targetGlow - m_glow) * 0.18;
            if (qAbs(m_glow - m_targetGlow) < 0.01) {
                m_glow = m_targetGlow;
                m_timer.stop();
            }
            update();
        });
        qApp->installEventFilter(this);
    }

    void setGlassEnabled(bool enabled, const QColor &accent) {
        m_enabled = enabled;
        m_accent = accent;
        setAttribute(Qt::WA_StyledBackground, !enabled);
        for (QWidget *child : findChildren<QWidget *>())
            child->setMouseTracking(enabled);
        update();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (!m_enabled) return QWidget::eventFilter(watched, event);
        auto *widget = qobject_cast<QWidget *>(watched);
        if (!widget || (widget != this && !isAncestorOf(widget)))
            return QWidget::eventFilter(watched, event);

        if (event->type() == QEvent::MouseMove) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            const QPoint local = mapFromGlobal(mouse->globalPosition().toPoint());
            if (rect().contains(local) && width() > 0 && height() > 0) {
                m_lightPos = QPointF(qBound(0.0, local.x() / qreal(width()), 1.0),
                                     qBound(0.0, local.y() / qreal(height()), 1.0));
                m_targetGlow = 1.0;
                if (!m_timer.isActive()) m_timer.start();
                update();
            }
        } else if (event->type() == QEvent::Leave) {
            const QPoint local = mapFromGlobal(QCursor::pos());
            if (!rect().contains(local)) {
                m_targetGlow = 0.0;
                if (!m_timer.isActive()) m_timer.start();
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent *event) override {
        if (!m_enabled) {
            QWidget::paintEvent(event);
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF bounds = QRectF(rect()).adjusted(0.7, 0.7, -0.7, -0.7);
        const qreal radius = qMin<qreal>(28.0, bounds.height() * 0.48);
        QPainterPath glass;
        glass.addRoundedRect(bounds, radius, radius);
        painter.setClipPath(glass);

        QLinearGradient body(bounds.topLeft(), bounds.bottomRight());
        body.setColorAt(0.0, QColor(245, 251, 255, 58));
        body.setColorAt(0.22, QColor(180, 220, 255, 31));
        body.setColorAt(0.58, QColor(76, 112, 151, 48));
        body.setColorAt(1.0, QColor(8, 20, 38, 112));
        painter.fillPath(glass, body);

        const QPointF glowCenter(
            bounds.left() + m_lightPos.x() * bounds.width(),
            bounds.top() + m_lightPos.y() * bounds.height());
        QRadialGradient lens(glowCenter, qMax(bounds.width(), bounds.height()) * 0.72);
        lens.setColorAt(0.0, QColor(255, 255, 255, 48 + int(m_glow * 55)));
        lens.setColorAt(0.20, QColor(222, 242, 255, 27 + int(m_glow * 30)));
        lens.setColorAt(0.55, QColor(m_accent.red(), m_accent.green(), m_accent.blue(),
                                     8 + int(m_glow * 15)));
        lens.setColorAt(1.0, Qt::transparent);
        painter.fillPath(glass, lens);

        QLinearGradient lowerLens(0, bounds.bottom() - radius, 0, bounds.bottom());
        lowerLens.setColorAt(0.0, Qt::transparent);
        lowerLens.setColorAt(0.64, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 13));
        lowerLens.setColorAt(1.0, QColor(220, 240, 255, 34));
        painter.fillRect(bounds, lowerLens);

        painter.setClipping(false);
        QLinearGradient rim(bounds.topLeft(), bounds.bottomRight());
        rim.setColorAt(0.0, QColor(255, 255, 255, 185));
        rim.setColorAt(0.28, QColor(230, 246, 255, 82));
        rim.setColorAt(0.62, QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 65));
        rim.setColorAt(1.0, QColor(255, 255, 255, 38));
        painter.setPen(QPen(rim, 1.25));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(glass);

        const QRectF inner = bounds.adjusted(2.2, 2.2, -2.2, -2.2);
        QPainterPath innerPath;
        innerPath.addRoundedRect(inner, qMax<qreal>(1.0, radius - 2.2),
                                 qMax<qreal>(1.0, radius - 2.2));
        painter.setPen(QPen(QColor(255, 255, 255, 28), 0.8));
        painter.drawPath(innerPath);
    }

    void enterEvent(QEnterEvent *event) override {
        m_targetGlow = 1.0;
        if (!m_timer.isActive()) m_timer.start();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override {
        m_targetGlow = 0.0;
        if (!m_timer.isActive()) m_timer.start();
        QWidget::leaveEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (width() > 0 && height() > 0) {
            m_lightPos = QPointF(event->position().x() / width(),
                                 event->position().y() / height());
            update();
        }
        QWidget::mouseMoveEvent(event);
    }

private:
    bool m_enabled = false;
    QColor m_accent = QColor("#9ed7ff");
    QPointF m_lightPos;
    QTimer m_timer;
    qreal m_glow = 0.0;
    qreal m_targetGlow = 0.0;
};

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <shellapi.h>
#include "../apo/ring.h"


static const QString kAppVersion = QStringLiteral(ECHOBOX_VERSION);
static const QString kUpdateApiUrl =
    "https://api.github.com/repos/BANANCHIKIREAL/EchoBox-II/releases";

static const char *kAutostartRegPath =
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char *kAutostartRegKey = "EchoBoxII";

static bool isLaunchOnStartupEnabled() {
    QSettings runKey(kAutostartRegPath, QSettings::NativeFormat);
    return runKey.contains(kAutostartRegKey);
}

static void setLaunchOnStartup(bool enabled) {
    QSettings runKey(kAutostartRegPath, QSettings::NativeFormat);
    if (enabled) {
        const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        runKey.setValue(kAutostartRegKey, "\"" + exePath + "\"");
    } else {
        runKey.remove(kAutostartRegKey);
    }
}

static bool setWindowsShortcutIcon(const QString &shortcutPath,
                                   const QString &iconPath) {
    IShellLinkW *shellLink = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW,
                                  reinterpret_cast<void **>(&shellLink));
    if (FAILED(hr) || !shellLink) return false;

    IPersistFile *persist = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile,
                                   reinterpret_cast<void **>(&persist));
    if (SUCCEEDED(hr) && persist) {
        hr = persist->Load(reinterpret_cast<LPCWSTR>(shortcutPath.utf16()), STGM_READWRITE);
        if (SUCCEEDED(hr))
            hr = shellLink->SetIconLocation(
                reinterpret_cast<LPCWSTR>(iconPath.utf16()), 0);
        if (SUCCEEDED(hr))
            hr = persist->Save(reinterpret_cast<LPCWSTR>(shortcutPath.utf16()), TRUE);
        persist->Release();
    }
    shellLink->Release();
    return SUCCEEDED(hr);
}

const QStringList MainWindow::VIDEO_EXTS = {"mp4","mkv","avi","mov","webm","flv","wmv","m2ts"};
const QStringList MainWindow::MEDIA_FILTER = {
    "Медиафайлы (*.mp3 *.mp4 *.wav *.ogg *.flac *.aac *.m4a *.mkv *.avi *.mov *.webm *.opus *.wma *.wmv)",
    "Аудио (*.mp3 *.wav *.ogg *.flac *.aac *.m4a *.opus *.wma)",
    "Видео (*.mp4 *.mkv *.avi *.mov *.webm *.wmv)",
    "Все файлы (*)"
};


class SmoothPlaylistWidget final : public QListWidget {
public:
    explicit SmoothPlaylistWidget(QWidget *parent = nullptr)
        : QListWidget(parent),
          m_scrollAnimation(new QPropertyAnimation(verticalScrollBar(), "value", this)) {
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    }

protected:
    void wheelEvent(QWheelEvent *event) override {
        int delta = 0;
        if (!event->pixelDelta().isNull()) {
            delta = -event->pixelDelta().y();
        } else if (!event->angleDelta().isNull()) {
            delta = -qRound(event->angleDelta().y() / 120.0 * 72.0);
        }

        if (delta == 0) {
            QListWidget::wheelEvent(event);
            return;
        }

        QScrollBar *bar = verticalScrollBar();
        const int base = m_scrollAnimation->state() == QAbstractAnimation::Running
            ? m_scrollTarget : bar->value();
        m_scrollTarget = qBound(bar->minimum(), base + delta, bar->maximum());

        m_scrollAnimation->stop();
        m_scrollAnimation->setStartValue(bar->value());
        m_scrollAnimation->setEndValue(m_scrollTarget);
        m_scrollAnimation->setDuration(qBound(110, 110 + qAbs(m_scrollTarget - bar->value()), 230));
        m_scrollAnimation->start();
        event->accept();
    }

private:
    QPropertyAnimation *m_scrollAnimation = nullptr;
    int m_scrollTarget = 0;
};


class PlaylistDelegate : public QStyledItemDelegate {
public:
    bool showIcons = true;
    using QStyledItemDelegate::QStyledItemDelegate;

    void setTheme(const ThemePalette &theme) {
        m_liquid = theme.id == "liquid";
        m_selectedBg = theme.surface2;
        m_hoverBg = theme.surface1;
        m_placeholderBg = theme.surface0;
        m_muted = theme.overlay0;
        m_text = theme.text;
        m_accent = theme.accent;
        m_duplicate = theme.danger;
    }

    QSize sizeHint(const QStyleOptionViewItem &o, const QModelIndex &i) const override {
        QSize s = QStyledItemDelegate::sizeHint(o, i);
        return {s.width(), showIcons ? 44 : 32};
    }

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        p->save();
        p->setClipRect(opt.rect);

        if (opt.state & QStyle::State_Selected)
            p->fillRect(opt.rect, m_selectedBg);
        else if (opt.state & QStyle::State_MouseOver)
            p->fillRect(opt.rect, m_hoverBg);

        if (idx.data(Qt::UserRole + 10).toBool())
            p->fillRect(opt.rect, QColor(m_duplicate.red(), m_duplicate.green(),
                                         m_duplicate.blue(), 40));

        const int iconSz = 32;
        const int margin = 6;
        int textX = opt.rect.left() + 10;

        if (showIcons) {
            QIcon icon = idx.data(Qt::DecorationRole).value<QIcon>();
            QRect ir(opt.rect.left() + margin,
                     opt.rect.top() + (opt.rect.height() - iconSz) / 2,
                     iconSz, iconSz);
            if (!icon.isNull()) {
                QPixmap src = icon.pixmap(iconSz, iconSz);
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
                p->setRenderHint(QPainter::Antialiasing);
                if (m_liquid) {
                    QPainterPath tile;
                    tile.addRoundedRect(QRectF(ir).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
                    QLinearGradient glass(ir.topLeft(), ir.bottomRight());
                    glass.setColorAt(0.0, QColor(230, 247, 255, 88));
                    glass.setColorAt(0.35, QColor(m_accent.red(), m_accent.green(),
                                                  m_accent.blue(), 62));
                    glass.setColorAt(1.0, QColor(12, 37, 66, 175));
                    p->setPen(QPen(QColor(235, 249, 255, 145), 1));
                    p->setBrush(glass);
                    p->drawPath(tile);
                    const qreal barW = 2.7;
                    const qreal gap = 2.2;
                    const qreal heights[] = {7.0, 13.0, 18.0, 13.0, 7.0};
                    const qreal total = 5.0 * barW + 4.0 * gap;
                    const qreal start = ir.center().x() - total / 2.0;
                    p->setPen(Qt::NoPen);
                    p->setBrush(QColor(225, 247, 255, 235));
                    for (int bar = 0; bar < 5; ++bar) {
                        const qreal h = heights[bar];
                        p->drawRoundedRect(QRectF(start + bar * (barW + gap),
                                                  ir.center().y() - h / 2.0,
                                                  barW, h), 1.35, 1.35);
                    }
                } else {
                    p->setPen(Qt::NoPen);
                    p->setBrush(m_placeholderBg);
                    p->drawRoundedRect(ir, 5, 5);
                    Ico::music(m_muted, 18).paint(p, ir, Qt::AlignCenter);
                }
            }
            textX = ir.right() + 8;
        }

        const QString dur = idx.data(Qt::UserRole + 1).toString();
        int rightEdge = opt.rect.right() - 8;
        if (!dur.isEmpty()) {
            QFont df = p->font(); df.setPointSize(9); p->setFont(df);
            p->setPen(m_muted);
            int dw = p->fontMetrics().horizontalAdvance(dur) + 4;
            p->drawText(QRect(opt.rect.right() - dw - 8, opt.rect.top(), dw + 8, opt.rect.height()),
                        Qt::AlignRight | Qt::AlignVCenter, dur);
            rightEdge = opt.rect.right() - dw - 12;
        }

        QString text = idx.data(Qt::DisplayRole).toString();
        QFont tf = p->font(); tf.setPointSize(10); p->setFont(tf);
        p->setPen((opt.state & QStyle::State_Selected)
                  ? m_accent : m_text);
        p->drawText(QRect(textX, opt.rect.top(), rightEdge - textX, opt.rect.height()),
                    Qt::AlignVCenter | Qt::TextSingleLine, text);

        p->restore();
    }

private:
    bool m_liquid = false;
    QColor m_selectedBg {0x45,0x47,0x5a};
    QColor m_hoverBg {0x2a,0x2b,0x3d};
    QColor m_placeholderBg {0x31,0x32,0x44};
    QColor m_muted {0x6c,0x70,0x86};
    QColor m_text {0xcd,0xd6,0xf4};
    QColor m_accent {0xcb,0xa6,0xf7};
    QColor m_duplicate {0xeb,0xa0,0xac};
};

static PlaylistDelegate *g_delegate = nullptr;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings("EchoBox", "EchoBoxII")
{
    m_player      = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);

    m_eqEngine = new AudioEngine(this);
    connect(m_eqEngine, &AudioEngine::ready, this, [this]{
        const QUrl current = m_player->source();
        const bool stillWanted = m_eqPending && m_cfg.eqEnabled &&
            current == m_eqSource && current.isLocalFile();
        if (!stillWanted) return;

        m_eqEngine->setPosition(m_player->position());
        m_eqEngine->setVolume(m_volumeSlider->value() / 100.0f * m_fadeFactor);
        if (m_player->playbackState() == QMediaPlayer::PlayingState)
            m_eqEngine->play();
        m_eqPending = false;
        m_eqActive = true;
        applyVolume();
        statusBar()->showMessage("Эквалайзер включён", 2500);
    });
    connect(m_eqEngine, &AudioEngine::decodeError, this, [this](const QString &msg){
        if (!m_eqPending && !m_eqActive) return;
        m_eqPending = false;
        m_eqActive = false;
        m_eqSource = QUrl();
        applyVolume();
        showCopyableError("Эквалайзер недоступен для этого трека",
            "Не удалось декодировать трек для эквалайзера — играет обычный "
            "плеер без него.\n\n" + msg);
    });

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

    m_discord = new DiscordRPC("1516933454309228684", this);

    m_streamArtNam = new QNetworkAccessManager(this);

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
    statusBar()->setSizeGripEnabled(false);
    applyTheme();
    loadSettings();

    setAcceptDrops(true);
    setMinimumSize(720, 540);
    resize(940, 660);
    setWindowTitle("EchoBox II");

    const QIcon icon(createLogo(128, ThemeManager::palette("mocha"), m_cfg.appIconStyle));
    setWindowIcon(icon);
    syncShellShortcutIcon();

    QTimer::singleShot(3000, this, [this]{ if (m_cfg.autoCheckUpdates) checkForUpdates(false); });
}

MainWindow::~MainWindow() { apoCloseRing(); saveSettings(); }

bool MainWindow::startsMinimized() const { return m_cfg.startMinimized; }


void MainWindow::setupMenuBar() {
    QMenu *fm = menuBar()->addMenu("&Файл");
    fm->addAction("&Открыть файлы...", QKeySequence(Qt::CTRL | Qt::Key_O),
                  this, &MainWindow::openFiles);
    fm->addAction("Открыть &папку...", this, &MainWindow::openFolder);
    m_openUrlAct = fm->addAction("Открыть по &ссылке...", this, &MainWindow::openUrlDialog);
    m_openUrlAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));
    m_openUrlAct->setIcon(Ico::link(QColor(0xba, 0xc2, 0xde), 18));
    m_scanLibraryAct = fm->addAction("Сканировать библиотеку", this, &MainWindow::scanLibrary);
    m_scanLibraryAct->setIcon(Ico::folder(QColor(0xba, 0xc2, 0xde), 18));
    m_recentMenu = fm->addMenu("&Недавние файлы");
    fm->addSeparator();
    fm->addAction("&Сохранить плейлист...", QKeySequence(Qt::CTRL | Qt::Key_S),
                  this, &MainWindow::savePlaylist);
    fm->addAction("&Загрузить плейлист...", QKeySequence(Qt::CTRL | Qt::Key_L),
                  this, &MainWindow::loadPlaylist);
    fm->addSeparator();
    fm->addAction("&Выход", QKeySequence(Qt::CTRL | Qt::Key_Q),
                  qApp, &QApplication::quit);

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

    QMenu *vm = menuBar()->addMenu("&Вид");
    m_miniPlayerAct = vm->addAction("Мини-плеер", QKeySequence(Qt::Key_F11),
                                    this, &MainWindow::toggleMiniPlayer);
    m_miniPlayerAct->setCheckable(true);
    m_alwaysOnTopAct = vm->addAction("Поверх всех окон", this, &MainWindow::toggleAlwaysOnTop);
    m_alwaysOnTopAct->setCheckable(true);

    QMenu *stMenu = menuBar()->addMenu("&Настройки");
    stMenu->addAction("Параметры...", QKeySequence(Qt::CTRL | Qt::Key_Comma),
                      this, &MainWindow::openSettings);

    QMenu *helpMenu = menuBar()->addMenu("&Справка");
    helpMenu->addAction("О программе", this, &MainWindow::showAbout);
    helpMenu->addAction("Проверить обновления...", [this]{ checkForUpdates(true); });
}


void MainWindow::setupUi() {
    m_aurora = new AuroraWidget(this);
    setCentralWidget(m_aurora);
    QVBoxLayout *root = new QVBoxLayout(m_aurora);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_contentShell = new QWidget(this);
    m_contentShell->setObjectName("contentShell");
    auto *shellLayout = new QHBoxLayout(m_contentShell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);

    m_modernSidebar = new LiquidGlassWidget(this);
    m_modernSidebar->setObjectName("modernSidebar");
    m_modernSidebar->setFixedWidth(190);
    auto *sidebarLayout = new QVBoxLayout(m_modernSidebar);
    sidebarLayout->setContentsMargins(14, 16, 14, 14);
    sidebarLayout->setSpacing(7);

    auto *brandRow = new QHBoxLayout;
    brandRow->setContentsMargins(2, 0, 2, 12);
    brandRow->setSpacing(9);
    m_modernBrandIcon = new QLabel(m_modernSidebar);
    m_modernBrandIcon->setFixedSize(34, 34);
    m_modernBrandIcon->setPixmap(
        createLogo(34, ThemeManager::palette("mocha"), m_cfg.appIconStyle));
    m_modernBrandIcon->setScaledContents(true);
    auto *brandText = new QLabel("EchoBox II", m_modernSidebar);
    brandText->setObjectName("modernBrand");
    brandRow->addWidget(m_modernBrandIcon);
    brandRow->addWidget(brandText);
    brandRow->addStretch();
    sidebarLayout->addLayout(brandRow);

    auto makeNavButton = [this](const QString &text, const QIcon &icon) {
        auto *button = new QToolButton(m_modernSidebar);
        button->setObjectName("modernNavButton");
        button->setText(text);
        button->setIcon(icon);
        button->setIconSize({18, 18});
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setFixedHeight(38);
        return button;
    };
    const QColor navColor(0xa6, 0xad, 0xc8);
    m_modernHomeBtn = makeNavButton("Главная", Ico::music(navColor, 18));
    m_modernHomeBtn->setCheckable(true);
    m_modernHomeBtn->setChecked(true);
    m_modernSearchBtn = makeNavButton("Поиск", Ico::sliders(navColor, 18));
    m_modernPlaylistsBtn = makeNavButton("Плейлисты", Ico::music(navColor, 18));
    m_modernLibraryBtn = makeNavButton("Библиотека", Ico::folder(navColor, 18));
    m_modernOpenBtn = makeNavButton("Добавить музыку", Ico::download(navColor, 18));
    m_modernFolderBtn = makeNavButton("Открыть папку", Ico::folder(navColor, 18));
    m_modernLinkBtn = makeNavButton("Музыка по ссылке", Ico::link(navColor, 18));
    sidebarLayout->addWidget(m_modernHomeBtn);
    sidebarLayout->addWidget(m_modernSearchBtn);
    sidebarLayout->addSpacing(8);
    auto *libraryTitle = new QLabel("Моя музыка", m_modernSidebar);
    libraryTitle->setObjectName("modernSectionTitle");
    sidebarLayout->addWidget(libraryTitle);
    sidebarLayout->addWidget(m_modernPlaylistsBtn);
    sidebarLayout->addWidget(m_modernLibraryBtn);
    sidebarLayout->addWidget(m_modernOpenBtn);
    sidebarLayout->addWidget(m_modernFolderBtn);
    sidebarLayout->addWidget(m_modernLinkBtn);
    sidebarLayout->addStretch();
    m_modernSettingsBtn = makeNavButton("Настройки", Ico::sliders(navColor, 18));
    sidebarLayout->addWidget(m_modernSettingsBtn);
    m_modernSidebar->hide();

    m_mainColumn = new QWidget(this);
    m_mainColumn->setObjectName("modernMainColumn");
    m_mainColumnLayout = new QVBoxLayout(m_mainColumn);
    m_mainColumnLayout->setContentsMargins(0, 0, 0, 0);
    m_mainColumnLayout->setSpacing(0);
    shellLayout->addWidget(m_modernSidebar);
    shellLayout->addWidget(m_mainColumn, 1);
    root->addWidget(m_contentShell, 1);

    m_topWidget = new LiquidGlassWidget(this);
    m_topWidget->setObjectName("topWidget");
    QHBoxLayout *topL = new QHBoxLayout(m_topWidget);
    topL->setContentsMargins(14, 14, 14, 10);
    topL->setSpacing(18);

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

    QWidget *rp = new QWidget(this);
    QVBoxLayout *rl = new QVBoxLayout(rp);
    rl->setContentsMargins(4, 0, 0, 0);
    rl->setSpacing(0);

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

    m_loadingBanner = new QWidget(this);
    m_loadingBanner->setObjectName("loadingBanner");
    auto *lbL = new QVBoxLayout(m_loadingBanner);
    lbL->setContentsMargins(12, 10, 12, 11);
    lbL->setSpacing(8);

    auto *loadingHeader = new QHBoxLayout;
    loadingHeader->setContentsMargins(0, 0, 0, 0);
    loadingHeader->setSpacing(9);
    m_loadingIcon = new QLabel(m_loadingBanner);
    m_loadingIcon->setObjectName("loadingIcon");
    m_loadingIcon->setAlignment(Qt::AlignCenter);
    m_loadingIcon->setFixedSize(30, 30);
    m_loadingIcon->setPixmap(Ico::download(QColor("#1e1e2e"), 17).pixmap(17, 17));
    m_loadingText = new QLabel(m_loadingBanner);
    m_loadingText->setObjectName("loadingText");
    m_loadingText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_loadingText->setTextInteractionFlags(Qt::NoTextInteraction);
    m_loadingPercent = new QLabel(QString::fromUtf8("•••"), m_loadingBanner);
    m_loadingPercent->setObjectName("loadingPercent");
    m_loadingPercent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_loadingPercent->setMinimumWidth(42);
    loadingHeader->addWidget(m_loadingIcon);
    loadingHeader->addWidget(m_loadingText, 1);
    loadingHeader->addWidget(m_loadingPercent);

    m_loadingBar = new QProgressBar(this);
    m_loadingBar->setObjectName("loadingBar");
    m_loadingBar->setRange(0, 0);
    m_loadingBar->setTextVisible(false);
    m_loadingBar->setFixedHeight(9);
    m_loadingBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    lbL->addLayout(loadingHeader);
    lbL->addWidget(m_loadingBar);
    m_loadingBanner->setVisible(false);
    rl->addSpacing(8);
    rl->addWidget(m_loadingBanner);

    rl->addStretch(1);

    m_visualizer = new Visualizer(this);
    rl->addWidget(m_visualizer);
    rl->addSpacing(8);

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

    auto mkBtn = [this](const QString &tip, const QString &id, int sz) -> QToolButton* {
        auto *b = new QToolButton(this);
        b->setToolTip(tip); b->setObjectName(id); b->setFixedSize(sz, sz);
        return b;
    };

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

    m_micBtn = mkBtn("ЛКМ — музыка в микрофон вкл/выкл\nПКМ — громкость и «только музыка»", "toggleBtn", 30);
    m_micBtn->setCheckable(true);
    m_micBtn->setIcon(Ico::microphone(QColor(0xa6, 0xad, 0xc8), 18));
    m_micBtn->setIconSize({18, 18});
    m_micBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_micBtn, &QToolButton::customContextMenuRequested,
            this, [this]{ showMicMenu(); });
    m_micBtn->setVisible(false);

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
    m_mainColumnLayout->addWidget(m_topWidget);

    m_separator = new QFrame(this);
    m_separator->setFrameShape(QFrame::HLine);
    m_separator->setObjectName("separator");
    m_mainColumnLayout->addWidget(m_separator);

    m_miniBar = new LiquidGlassWidget(this);
    m_miniBar->setObjectName("miniBar");
    m_miniBar->setAutoFillBackground(false);
    m_miniBar->hide();

    QHBoxLayout *miniL = new QHBoxLayout(m_miniBar);
    miniL->setContentsMargins(6, 4, 6, 4);
    miniL->setSpacing(4);

    const QColor mc(0xcd,0xd6,0xf4);
    const QColor pc(0x1e,0x1e,0x2e);
    const QColor tc(0xa6,0xad,0xc8);

    m_miniAlbumArt = new QLabel(this);
    m_miniAlbumArt->setObjectName("miniAlbumArt");
    m_miniAlbumArt->setFixedSize(40, 40);
    m_miniAlbumArt->setScaledContents(true);

    m_miniPrevBtn = new QToolButton(this); m_miniPrevBtn->setObjectName("ctrlBtn"); m_miniPrevBtn->setFixedSize(28,28);
    m_miniPrevBtn->setIcon(Ico::prev(mc,16)); m_miniPrevBtn->setIconSize({16,16});

    m_miniPlayBtn = new QToolButton(this); m_miniPlayBtn->setObjectName("playBtn"); m_miniPlayBtn->setFixedSize(36,36);
    m_miniPlayBtn->setIcon(Ico::play(pc,22)); m_miniPlayBtn->setIconSize({22,22});

    m_miniNextBtn = new QToolButton(this); m_miniNextBtn->setObjectName("ctrlBtn"); m_miniNextBtn->setFixedSize(28,28);
    m_miniNextBtn->setIcon(Ico::next(mc,16)); m_miniNextBtn->setIconSize({16,16});

    m_miniTitle = new ScrollingLabel(this);
    m_miniTitle->setText("EchoBox II");
    m_miniTitle->setObjectName("miniTitle");
    m_miniTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_miniShuffleBtn = new QToolButton(this); m_miniShuffleBtn->setObjectName("toggleBtn");
    m_miniShuffleBtn->setFixedSize(26,26); m_miniShuffleBtn->setCheckable(true);
    m_miniShuffleBtn->setIcon(Ico::shuffle(tc,15)); m_miniShuffleBtn->setIconSize({15,15});

    m_miniRepeatBtn = new QToolButton(this); m_miniRepeatBtn->setObjectName("toggleBtn");
    m_miniRepeatBtn->setFixedSize(26,26); m_miniRepeatBtn->setCheckable(true);
    m_miniRepeatBtn->setIcon(Ico::repeatAll(tc,15)); m_miniRepeatBtn->setIconSize({15,15});

    m_miniMoreBtn = new QToolButton(this);
    m_miniMoreBtn->setObjectName("moreBtn");
    m_miniMoreBtn->setFixedSize(30, 30);
    m_miniMoreBtn->setIcon(MaterialIco::icon("more_horiz", tc, 19));
    m_miniMoreBtn->setIconSize({19, 19});
    m_miniMoreBtn->setToolTip("Дополнительные функции");
    m_miniMoreBtn->hide();

    m_miniMoreMenu = new QMenu(this);
    m_miniMoreMenu->setObjectName("liquidMoreMenu");
    m_miniStopAct = m_miniMoreMenu->addAction(
        Ico::stop(tc, 16), "Стоп", this, &MainWindow::stop);
    m_miniShuffleAct = m_miniMoreMenu->addAction(
        Ico::shuffle(tc, 16), "Перемешивание", this, &MainWindow::toggleShuffle);
    m_miniShuffleAct->setCheckable(true);
    m_miniRepeatAct = m_miniMoreMenu->addAction(
        Ico::repeatAll(tc, 16), "Повтор: выключен", this, &MainWindow::cycleRepeat);
    m_miniRepeatAct->setCheckable(true);
    m_miniMoreMenu->addSeparator();
    m_miniSpeedMenu = m_miniMoreMenu->addMenu(
        Ico::equalizer(tc, 16), "Скорость · 1.0×");
    auto *miniSpeedGroup = new QActionGroup(m_miniSpeedMenu);
    miniSpeedGroup->setExclusive(true);
    const QStringList miniSpeedLabels = {"0.5×", "0.75×", "1.0×", "1.25×", "1.5×", "2.0×"};
    for (int i = 0; i < miniSpeedLabels.size(); ++i) {
        QAction *action = m_miniSpeedMenu->addAction(miniSpeedLabels[i]);
        action->setCheckable(true);
        action->setData(i);
        miniSpeedGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, i] {
            m_speedCombo->setCurrentIndex(i);
        });
    }
    connect(m_miniMoreMenu, &QMenu::aboutToShow, this, [this] {
        const int speedIndex = m_speedCombo->currentIndex();
        for (QAction *action : m_miniSpeedMenu->actions())
            action->setChecked(action->data().toInt() == speedIndex);
        m_miniShuffleAct->setChecked(m_shuffle);
        m_miniSpeedMenu->setTitle("Скорость · " + m_speedCombo->currentText());
    });

    m_miniMuteBtn = new QToolButton(this); m_miniMuteBtn->setObjectName("muteBtn");
    m_miniMuteBtn->setFixedSize(24,24);
    m_miniMuteBtn->setIcon(Ico::volume(3,mc,18)); m_miniMuteBtn->setIconSize({18,18});

    m_miniVolSlider = new QSlider(Qt::Horizontal, this);
    m_miniVolSlider->setObjectName("volSlider");
    m_miniVolSlider->setRange(0, 100); m_miniVolSlider->setValue(70);
    m_miniVolSlider->setFixedWidth(80);

    m_miniExpandBtn = new QToolButton(this);
    m_miniExpandBtn->setObjectName("ctrlBtn"); m_miniExpandBtn->setFixedSize(26,26);
    m_miniExpandBtn->setIcon(Ico::expand(mc, 13)); m_miniExpandBtn->setIconSize({13,13});
    m_miniExpandBtn->setToolTip("Полный режим  F11");

    m_miniMinimizeBtn = new QToolButton(this);
    m_miniMinimizeBtn->setObjectName("ctrlBtn"); m_miniMinimizeBtn->setFixedSize(26,26);
    m_miniMinimizeBtn->setIcon(Ico::minimize(mc, 13)); m_miniMinimizeBtn->setIconSize({13,13});
    m_miniMinimizeBtn->setToolTip("Свернуть");

    m_miniCloseBtn = new QToolButton(this);
    m_miniCloseBtn->setObjectName("ctrlBtn"); m_miniCloseBtn->setFixedSize(26,26);
    m_miniCloseBtn->setIcon(Ico::closeIcon(mc, 12)); m_miniCloseBtn->setIconSize({12,12});
    m_miniCloseBtn->setToolTip("Закрыть");

    m_miniDockBtn = new QToolButton(this);
    m_miniDockBtn->setObjectName("toggleBtn"); m_miniDockBtn->setFixedSize(26,26);
    m_miniDockBtn->setCheckable(true);
    m_miniDockBtn->setIcon(Ico::dockTop(tc, 14)); m_miniDockBtn->setIconSize({14,14});
    m_miniDockBtn->setToolTip("На всю ширину экрана, к верху");

    m_miniWaveform = new WaveformSlider(this);
    m_miniWaveform->setFixedHeight(36);
    m_miniWaveform->setMinimumWidth(160);
    m_miniWaveform->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_miniLoadingPanel = new QWidget(this);
    m_miniLoadingPanel->setObjectName("miniLoadingPanel");
    auto *miniLoadingLayout = new QHBoxLayout(m_miniLoadingPanel);
    miniLoadingLayout->setContentsMargins(10, 6, 10, 6);
    miniLoadingLayout->setSpacing(8);
    m_miniLoadingIcon = new QLabel(m_miniLoadingPanel);
    m_miniLoadingIcon->setObjectName("miniLoadingIcon");
    m_miniLoadingIcon->setAlignment(Qt::AlignCenter);
    m_miniLoadingIcon->setFixedSize(22, 22);
    m_miniLoadingIcon->setPixmap(Ico::download(tc, 14).pixmap(14, 14));
    auto *miniLoadingBody = new QVBoxLayout;
    miniLoadingBody->setContentsMargins(0, 0, 0, 0);
    miniLoadingBody->setSpacing(4);
    auto *miniLoadingHeader = new QHBoxLayout;
    miniLoadingHeader->setContentsMargins(0, 0, 0, 0);
    miniLoadingHeader->setSpacing(8);
    m_miniLoadingText = new QLabel("Загрузка трека", m_miniLoadingPanel);
    m_miniLoadingText->setObjectName("miniLoadingText");
    m_miniLoadingText->setMinimumWidth(130);
    m_miniLoadingText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_miniLoadingText->setTextInteractionFlags(Qt::NoTextInteraction);
    m_miniLoadingBar = new QProgressBar(m_miniLoadingPanel);
    m_miniLoadingBar->setObjectName("miniLoadingBar");
    m_miniLoadingBar->setRange(0, 0);
    m_miniLoadingBar->setTextVisible(false);
    m_miniLoadingBar->setFixedHeight(5);
    m_miniLoadingBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_miniLoadingPercent = new QLabel(QString::fromUtf8("•••"), m_miniLoadingPanel);
    m_miniLoadingPercent->setObjectName("miniLoadingPercent");
    m_miniLoadingPercent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_miniLoadingPercent->setFixedWidth(38);
    miniLoadingHeader->addWidget(m_miniLoadingText, 1);
    miniLoadingHeader->addWidget(m_miniLoadingPercent);
    miniLoadingBody->addLayout(miniLoadingHeader);
    miniLoadingBody->addWidget(m_miniLoadingBar);
    miniLoadingLayout->addWidget(m_miniLoadingIcon, 0, Qt::AlignVCenter);
    miniLoadingLayout->addLayout(miniLoadingBody, 1);
    m_miniLoadingPanel->setMinimumWidth(270);
    m_miniLoadingPanel->setMaximumWidth(360);
    m_miniLoadingPanel->setVisible(false);

    miniL->addWidget(m_miniAlbumArt);
    miniL->addSpacing(2);
    miniL->addWidget(m_miniPrevBtn);
    miniL->addWidget(m_miniPlayBtn);
    miniL->addWidget(m_miniNextBtn);
    miniL->addWidget(m_miniTitle);
    miniL->addWidget(m_miniLoadingPanel);
    miniL->addWidget(m_miniWaveform, 1);
    miniL->addWidget(m_miniShuffleBtn);
    miniL->addWidget(m_miniRepeatBtn);
    miniL->addWidget(m_miniMoreBtn);
    miniL->addSpacing(4);
    miniL->addWidget(m_miniMuteBtn);
    miniL->addWidget(m_miniVolSlider);
    miniL->addSpacing(2);
    miniL->addWidget(m_miniDockBtn);
    miniL->addWidget(m_miniExpandBtn);
    miniL->addWidget(m_miniMinimizeBtn);
    miniL->addWidget(m_miniCloseBtn);
    root->addWidget(m_miniBar);

    connect(m_miniPrevBtn,     &QToolButton::clicked, this, &MainWindow::previous);
    connect(m_miniNextBtn,     &QToolButton::clicked, this, &MainWindow::next);
    connect(m_miniExpandBtn,   &QToolButton::clicked, this, &MainWindow::toggleMiniPlayer);
    connect(m_miniMinimizeBtn, &QToolButton::clicked, this, &MainWindow::showMinimized);
    connect(m_miniCloseBtn,    &QToolButton::clicked, this, &MainWindow::close);
    connect(m_miniDockBtn,    &QToolButton::clicked, this, &MainWindow::toggleMiniDock);
    connect(m_miniPlayBtn,    &QToolButton::clicked, this, &MainWindow::togglePlayPause);
    connect(m_miniShuffleBtn, &QToolButton::clicked, this, &MainWindow::toggleShuffle);
    connect(m_miniRepeatBtn,  &QToolButton::clicked, this, &MainWindow::cycleRepeat);
    connect(m_miniMoreBtn,    &QToolButton::clicked, this, &MainWindow::showMiniMoreMenu);
    connect(m_miniMuteBtn,    &QToolButton::clicked, this, &MainWindow::toggleMute);
    connect(m_miniVolSlider,  &QSlider::valueChanged, this, &MainWindow::setVolume);

    m_miniBar->installEventFilter(this);
    m_miniTitle->installEventFilter(this);
    m_miniAlbumArt->installEventFilter(this);

    m_playlistPanel = new LiquidGlassWidget(this);
    m_playlistPanel->setObjectName("playlistPanel");
    QVBoxLayout *plL = new QVBoxLayout(m_playlistPanel);
    plL->setContentsMargins(12, 0, 12, 10);
    plL->setSpacing(4);

    {
        QHBoxLayout *tabRow = new QHBoxLayout;
        tabRow->setSpacing(4);
        tabRow->setContentsMargins(0, 0, 0, 6);

        m_tabBar = new QTabBar(this);
        m_tabBar->setObjectName("playlistTabBar");
        m_tabBar->setIconSize({18, 18});
        m_tabBar->setExpanding(false);
        m_tabBar->setMovable(false);
        m_tabBar->addTab("Плейлист 1");

        m_newPlaylistBtn = new QToolButton(this);
        m_newPlaylistBtn->setText("+");
        m_newPlaylistBtn->setObjectName("smallBtn");
        m_newPlaylistBtn->setFixedSize(32, 32);
        m_newPlaylistBtn->setToolTip("Новый плейлист");

        tabRow->addWidget(m_tabBar, 1, Qt::AlignTop);
        tabRow->addWidget(m_newPlaylistBtn, 0, Qt::AlignTop);
        plL->addLayout(tabRow);

        connect(m_newPlaylistBtn, &QToolButton::clicked, this, &MainWindow::newPlaylist);
        connect(m_tabBar, &QTabBar::currentChanged,      this, &MainWindow::onTabChanged);
        connect(m_tabBar, &QTabBar::tabBarDoubleClicked, this, &MainWindow::onTabDoubleClicked);
        m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_tabBar, &QWidget::customContextMenuRequested,
                this, &MainWindow::onTabContextMenu);
    }

    m_playlists.append({"Плейлист 1", {}, -1});
    m_tabBar->setTabIcon(0, playlistIcon(0, 18));

    QHBoxLayout *plH = new QHBoxLayout;
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("  Поиск по названию, исполнителю, альбому...");
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setClearButtonEnabled(true);

    m_playlistInfo = new QLabel("0 треков", this);
    m_playlistInfo->setObjectName("playlistInfo");

    auto mkSmall = [this](const QIcon &icon, const QString &tip) {
        auto *b = new QToolButton(this);
        b->setIcon(icon); b->setIconSize({18, 18}); b->setToolTip(tip);
        b->setObjectName("smallBtn"); b->setFixedSize(32, 32);
        return b;
    };
    const QColor smallIconColor(0xa6, 0xad, 0xc8);
    m_playlistUpBtn = mkSmall(Ico::arrowUp(smallIconColor, 15), "Вверх");
    m_playlistDownBtn = mkSmall(Ico::arrowDown(smallIconColor, 15), "Вниз");
    m_playlistRemoveBtn = mkSmall(Ico::closeIcon(smallIconColor, 15), "Удалить  Del");

    m_playlistClearBtn = new QToolButton(this);
    m_playlistClearBtn->setObjectName("clearBtn");
    m_playlistClearBtn->setFixedSize(32, 32);
    m_playlistClearBtn->setIcon(Ico::trash(smallIconColor, 15));
    m_playlistClearBtn->setIconSize({18, 18});
    m_playlistClearBtn->setToolTip("Очистить плейлист");

    plH->addWidget(m_searchEdit, 1);
    plH->addWidget(m_playlistInfo);
    plH->addWidget(m_playlistUpBtn); plH->addWidget(m_playlistDownBtn);
    plH->addWidget(m_playlistRemoveBtn); plH->addWidget(m_playlistClearBtn);
    plL->addLayout(plH);

    m_playlistWidget = new SmoothPlaylistWidget(this);
    m_playlistWidget->setObjectName("playlist");
    m_playlistWidget->setAlternatingRowColors(true);
    m_playlistWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_playlistWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_playlistWidget->setDragDropMode(QAbstractItemView::InternalMove);
    g_delegate = new PlaylistDelegate(m_playlistWidget);
    m_playlistWidget->setItemDelegate(g_delegate);
    m_playlistWidget->setIconSize({36, 36});
    plL->addWidget(m_playlistWidget, 1);
    m_mainColumnLayout->addWidget(m_playlistPanel, 1);

    connect(m_modernHomeBtn, &QToolButton::clicked, this, [this] {
        showPlaylistTracks();
        m_searchEdit->clear();
        if (m_tabBar->count() > 0) m_tabBar->setCurrentIndex(0);
        m_playlistWidget->setFocus();
    });
    connect(m_modernSearchBtn, &QToolButton::clicked, this, [this] {
        showSearchOverlay();
    });
    connect(m_modernPlaylistsBtn, &QToolButton::clicked,
            this, &MainWindow::showPlaylistBrowser);
    connect(m_modernLibraryBtn, &QToolButton::clicked, this, [this] {
        if (m_libraryPlIdx >= 0 && m_libraryPlIdx < m_tabBar->count())
            m_tabBar->setCurrentIndex(m_libraryPlIdx);
        else
            scanLibrary();
    });
    connect(m_modernOpenBtn, &QToolButton::clicked, this, &MainWindow::openFiles);
    connect(m_modernFolderBtn, &QToolButton::clicked, this, &MainWindow::openFolder);
    connect(m_modernLinkBtn, &QToolButton::clicked, this, &MainWindow::openUrlDialog);
    connect(m_modernSettingsBtn, &QToolButton::clicked, this, &MainWindow::openSettings);

    connect(m_playlistUpBtn, &QToolButton::clicked, this, &MainWindow::moveTrackUp);
    connect(m_playlistDownBtn, &QToolButton::clicked, this, &MainWindow::moveTrackDown);
    connect(m_playlistRemoveBtn, &QToolButton::clicked, this, &MainWindow::removeSelectedTracks);
    connect(m_playlistClearBtn, &QToolButton::clicked, this, &MainWindow::clearPlaylist);

    {
        const QColor ctrl(0xcd, 0xd6, 0xf4);
        const QColor play(0x1e, 0x1e, 0x2e);
        m_prevBtn->setIcon(Ico::prev(ctrl, 22));        m_prevBtn->setIconSize({22,22});
        m_playPauseBtn->setIcon(Ico::play(play, 36));  m_playPauseBtn->setIconSize({36,36});
        m_nextBtn->setIcon(Ico::next(ctrl, 22));        m_nextBtn->setIconSize({22,22});
        m_stopBtn->setIcon(Ico::stop(ctrl, 16));        m_stopBtn->setIconSize({16,16});
        m_muteBtn->setIcon(Ico::volume(3, ctrl, 20));   m_muteBtn->setIconSize({20,20});
        m_shuffleBtn->setIcon(Ico::shuffle(ctrl, 18));  m_shuffleBtn->setIconSize({18,18});
        m_repeatBtn->setIcon(Ico::repeatAll(ctrl, 18)); m_repeatBtn->setIconSize({18,18});
    }

    updateAlbumArt();
    statusBar()->showMessage("EchoBox II  —  открой файлы или перетащи их в окно");
}


void MainWindow::setupTray() {
    const ThemePalette theme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
    m_tray = new QSystemTrayIcon(QIcon(createLogo(64, ThemeManager::palette("mocha"))), this);
    m_tray->setToolTip("EchoBox II");

    auto *tm = new QMenu(this);
    m_trayPlayAct = tm->addAction(Ico::play(theme.text, 16), "Играть", this, &MainWindow::togglePlayPause);
    m_trayNextAct = tm->addAction(Ico::next(theme.text, 16), "Следующий", this, &MainWindow::next);
    tm->addSeparator();
    tm->addAction("Показать окно", this, [this]{ show(); raise(); activateWindow(); });
    tm->addAction("Выход", qApp, &QApplication::quit);
    m_tray->setContextMenu(tm);
    m_tray->show();

    connect(m_tray, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::DoubleClick) { show(); raise(); activateWindow(); }
    });
}


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
        playerSeek(m_seekSlider->value());
        m_seeking = false;
    });
    connect(m_seekSlider, &WaveformSlider::peaksReady, this,
            [this](const QUrl &url, qint64 duration, const QVector<float> &peaks) {
        m_waveformCache.rememberPartial(url, duration, peaks);
        if (m_player->source() == url) m_miniWaveform->setPartialPeaks(peaks);
    });
    connect(m_seekSlider, &WaveformSlider::waveformReady,
            this, &MainWindow::onWaveformReady);
    connect(m_miniWaveform, &WaveformSlider::sliderPressed,  [this]{ m_seeking = true; });
    connect(m_miniWaveform, &WaveformSlider::sliderReleased, [this]{
        playerSeek(m_miniWaveform->value());
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
    if (obj == m_searchPopupEdit && ev->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(ev);
        if (key->key() == Qt::Key_Escape) {
            hideSearchOverlay();
            return true;
        }
    }
    if (obj == m_timeLabel && ev->type() == QEvent::MouseButtonPress) {
        toggleRemainingTime();
        return true;
    }
    if (m_miniPlayer && !m_miniDocked && (obj == m_miniBar || obj == m_miniTitle || obj == m_miniAlbumArt)) {
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


void MainWindow::applyTheme() {
    const ThemePalette theme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
    const bool liquidGlass = theme.id == "liquid";
    const QColor ac = theme.accent;
    const QString acH = ac.name();
    const QString acL = ac.lighter(112).name();
    const QString acD = ac.darker(112).name();
    const QColor acBg(qMax(0,ac.red()/5+0x18), qMax(0,ac.green()/5+0x14), qMax(0,ac.blue()/5+0x28));
    const QString acBgH = acBg.name();

    const int fs = m_cfg.fontSizeIdx == 0 ? 11 : m_cfg.fontSizeIdx == 2 ? 15 : 13;
    const QString family = m_cfg.fontFamily.isEmpty() ? "Segoe UI" : m_cfg.fontFamily;
    QFont appFont(family, fs);
    qApp->setFont(appFont);

    QString ss = R"(
        QMainWindow { background: transparent; }
        QWidget {
            color: #cdd6f4;
            font-family: "FONTFAMILY", "Yu Gothic UI", sans-serif;
            font-size: FONTPX;
        }
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
        QWidget#contentShell, QWidget#modernMainColumn { background: transparent; }
        QWidget#modernSidebar {
            background-color: #181825;
            border-right: 1px solid #313244;
        }
        QLabel#modernBrand { color: #cdd6f4; font-size: 15px; font-weight: 700; }
        QLabel#modernSectionTitle {
            color: #6c7086;
            padding: 6px 7px 2px 7px;
            font-size: 10px;
            font-weight: 700;
        }
        QToolButton#modernNavButton {
            color: #a6adc8;
            background: transparent;
            border: 1px solid transparent;
            border-radius: 9px;
            padding: 0 10px;
            text-align: left;
            font-size: 12px;
        }
        QToolButton#modernNavButton:hover {
            color: #cdd6f4;
            background-color: #313244;
            border-color: #45475a;
        }
        QToolButton#modernNavButton:checked {
            color: ACCENT;
            background-color: #313244;
            border-color: #45475a;
            font-weight: 700;
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

        QWidget#loadingBanner {
            background-color: rgba(49, 50, 68, 205);
            border: 1px solid ACCENT;
            border-radius: 12px;
        }
        QLabel#loadingIcon {
            background-color: ACCENT;
            color: #1e1e2e;
            border-radius: 13px;
            font-size: 17px;
            font-weight: bold;
        }
        QLabel#loadingText {
            color: #cdd6f4;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#miniLoadingText {
            color: #cdd6f4;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel#loadingPercent, QLabel#miniLoadingPercent {
            color: ACCENT;
            font-size: 12px;
            font-weight: bold;
        }
        QProgressBar#loadingBar {
            background-color: #45475a;
            border: none;
            border-radius: 4px;
        }
        QProgressBar#loadingBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                        stop:0 ACCENT, stop:1 #89b4fa);
            border-radius: 4px;
        }
        QWidget#miniLoadingPanel {
            background-color: rgba(49, 50, 68, 220);
            border: 1px solid #45475a;
            border-radius: 10px;
        }
        QLabel#miniLoadingIcon {
            color: ACCENT;
            font-size: 15px;
            font-weight: bold;
        }
        QProgressBar#miniLoadingBar {
            background-color: #45475a;
            border: none;
            border-radius: 3px;
        }
        QProgressBar#miniLoadingBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                        stop:0 ACCENT, stop:1 #89b4fa);
            border-radius: 3px;
        }

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

        QToolButton#clearBtn {
            background-color: #313244;
            border: none;
            border-radius: 5px;
        }
        QToolButton#clearBtn:hover { background-color: #45283a; }

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
        QSlider#volSlider::sub-page:horizontal { background: ACCENT; border-radius: 2px; }
        QSlider#volSlider::handle:horizontal {
            background: #cdd6f4;
            width: 12px; height: 12px;
            margin: -4px 0;
            border-radius: 6px;
            border: none;
        }
        QSlider#volSlider::handle:horizontal:hover { background: ACCENT; }

        QSlider::groove:vertical {
            width: 4px; background: #313244; border-radius: 2px;
        }
        QSlider::sub-page:vertical { background: #45475a; border-radius: 2px; }
        QSlider::add-page:vertical { background: ACCENT; border-radius: 2px; }
        QSlider::handle:vertical {
            background: #cdd6f4;
            width: 14px; height: 14px;
            margin: 0 -5px;
            border-radius: 7px;
            border: none;
        }
        QSlider::handle:vertical:hover { background: ACCENT; }

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

        QLineEdit {
            background-color: #181825;
            border: 1px solid #313244;
            border-radius: 6px;
            padding: 5px 8px;
            color: #cdd6f4;
            selection-background-color: #45475a;
        }
        QLineEdit:focus { border-color: ACCENT; }
        QLineEdit:disabled { color: #6c7086; background-color: #1a1a27; }

        QComboBox {
            background-color: #181825;
            border: 1px solid #313244;
            border-radius: 6px;
            padding: 4px 8px;
            color: #cdd6f4;
        }
        QComboBox:hover { border-color: #45475a; }
        QComboBox:focus { border-color: ACCENT; }
        QComboBox::drop-down { border: none; width: 22px; }

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
            background: transparent;
            width: 8px; margin: 2px 1px 2px 0;
        }
        QScrollBar::handle:vertical {
            background: #45475a; border-radius: 4px; min-height: 28px;
        }
        QScrollBar::handle:vertical:hover   { background: #585b70; }
        QScrollBar::handle:vertical:pressed { background: #cba6f7; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: none; background: none; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

        QScrollBar:horizontal {
            background: transparent;
            height: 8px; margin: 0 2px 1px 2px;
        }
        QScrollBar::handle:horizontal {
            background: #45475a; border-radius: 4px; min-width: 28px;
        }
        QScrollBar::handle:horizontal:hover   { background: #585b70; }
        QScrollBar::handle:horizontal:pressed { background: #cba6f7; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; border: none; background: none; }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }

        QTabBar#playlistTabBar {
            background: transparent;
        }
        QTabBar#playlistTabBar::tab {
            background-color: #252537;
            color: #6c7086;
            padding: 4px 18px;
            border-radius: 7px;
            border: 1px solid #313244;
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

        QDialog        { background-color: #1e1e2e; color: #cdd6f4; }

        QWidget#settingsHeader {
            background-color: #181825;
            border-bottom: 1px solid #313244;
        }
        QLabel#settingsTitle    { color: #cdd6f4; font-size: 17px; font-weight: bold; }
        QLabel#settingsSubtitle { color: #6c7086; font-size: 12px; }

        QWidget#settingsSidebarWrap { background-color: #181825; border-right: 1px solid #313244; }
        QListWidget#settingsSidebar {
            background: transparent;
            border: none;
            outline: none;
            padding: 0px;
        }
        QListWidget#settingsSidebar::item {
            color: #a6adc8;
            border-radius: 7px;
            padding-left: 10px;
            margin: 1px 0px;
        }
        QListWidget#settingsSidebar::item:hover:!selected {
            background-color: #24243a;
            color: #cdd6f4;
        }
        QListWidget#settingsSidebar::item:selected {
            background-color: #313244;
            color: ACCENT;
            font-weight: bold;
        }

        QListWidget#appIconGrid {
            background-color: #181825;
            border: 1px solid #313244;
            border-radius: 10px;
            outline: none;
            padding: 5px;
        }
        QListWidget#appIconGrid::item {
            color: #a6adc8;
            border: 2px solid transparent;
            border-radius: 10px;
            padding: 3px;
        }
        QListWidget#appIconGrid::item:hover:!selected {
            background-color: #24243a;
            color: #cdd6f4;
        }
        QListWidget#appIconGrid::item:selected {
            background-color: #313244;
            border-color: ACCENT;
            color: ACCENT;
            font-weight: bold;
        }
        QLabel#playlistBrowserTitle {
            color: #cdd6f4;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel#playlistBrowserSubtitle { color: #7f849c; }
        QLabel#playlistCoverPreview {
            background-color: #181825;
            border: 1px solid #45475a;
            border-radius: 18px;
            padding: 5px;
        }
        QListWidget#playlistBrowserGrid {
            background-color: #181825;
            border: 1px solid #313244;
            border-radius: 16px;
            outline: none;
            padding: 10px;
        }
        QListWidget#playlistBrowserGrid::item {
            color: #a6adc8;
            border: 1px solid transparent;
            border-radius: 14px;
            padding: 8px;
        }
        QListWidget#playlistBrowserGrid::item:hover:!selected {
            color: #cdd6f4;
            background-color: #24243a;
            border-color: #313244;
        }
        QListWidget#playlistBrowserGrid::item:selected {
            color: #cdd6f4;
            background-color: #313244;
            border-color: ACCENT;
        }
        QPushButton#playlistBrowserOpen {
            color: #1e1e2e;
            background-color: ACCENT;
            border-color: ACCENT;
            font-weight: 700;
        }

        QStackedWidget#settingsStack { background-color: #1e1e2e; }
        QScrollArea#appearanceScroll {
            background-color: #1e1e2e;
            border: none;
        }
        QWidget#appearanceScrollContent { background-color: #1e1e2e; }

        QWidget#settingsFooter { border-top: 1px solid #313244; }
        QPushButton#settingsOkBtn {
            background-color: ACCENT; color: #1e1e2e; border: none; font-weight: bold;
        }
        QPushButton#settingsOkBtn:hover   { background-color: #d4b8f9; }
        QPushButton#settingsOkBtn:pressed { background-color: #b389f4; }

        QWidget#aboutHeader {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 rgba(203,166,247,45), stop:1 rgba(137,180,250,25));
            border-bottom: 1px solid #313244;
        }
        QLabel#aboutName    { color: ACCENT; font-size: 22px; font-weight: bold; }
        QLabel#aboutVersion {
            color: #a6adc8; font-size: 11px;
            background-color: rgba(255,255,255,20);
            border-radius: 9px; padding: 2px 10px;
        }
        QLabel#aboutTagline     { color: #a6adc8; font-size: 12px; }
        QLabel#aboutFeatureText { color: #cdd6f4; font-size: 12px; }
        QFrame#aboutSep         { background: #292941; max-height: 1px; border: none; }
        QLabel#aboutKeyChip {
            color: #cdd6f4; font-size: 11px;
            background-color: #292941; border: 1px solid #313244;
            border-radius: 9px; padding: 3px 9px;
        }
        QLabel#aboutLink    { font-size: 12px; }
        QLabel#aboutLicense { color: #6c7086; font-size: 11px; }
        QWidget#aboutFooter { background-color: rgba(24,24,37,140); border-top: 1px solid #313244; }
        QPushButton#aboutCloseBtn {
            background-color: ACCENT; color: #1e1e2e; border: none;
            border-radius: 8px; font-weight: bold; padding: 0 30px;
        }
        QPushButton#aboutCloseBtn:hover   { background-color: #d4b8f9; }
        QPushButton#aboutCloseBtn:pressed { background-color: #b389f4; }

        QGroupBox { border: 1px solid #313244; border-radius: 6px; margin-top: 6px; padding-top: 6px; color: #a6adc8; }
        QLabel#settingsHead { color: #cdd6f4; font-weight: bold; font-size: 13px; }
        QFrame#settingsSep  { background: #292941; max-height: 1px; }

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

    ThemeManager::applyPaletteTokens(ss, theme);

    ss.replace("ACCENT",    acH);
    ss.replace("#cba6f7",   acH);
    ss.replace("#d4b8f9",   acL);
    ss.replace("#b389f4",   acD);
    ss.replace("#2d2040",   acBgH);
    ss.replace("FONTPX",      QString::number(fs) + "px");
    ss.replace("FONTFAMILY",  family);

    if (liquidGlass) {
        ss += R"(
            QWidget#topWidget {
                background: transparent;
                border: none;
                margin: 8px 10px 6px 10px;
            }
            QWidget#playlistPanel {
                background: transparent;
                border: none;
                margin: 0px;
            }
            QWidget#modernSidebar {
                background: transparent;
                border: none;
            }
            QWidget#miniBar {
                background: transparent;
                border: none;
            }
            QMenuBar, QWidget#settingsHeader, QWidget#settingsSidebarWrap,
            QWidget#settingsFooter, QWidget#aboutFooter {
                background-color: rgba(14,29,52,225);
                border-color: rgba(205,232,255,70);
            }
            QMenu, QDialog, QStackedWidget#settingsStack,
            QScrollArea#appearanceScroll, QWidget#appearanceScrollContent {
                background-color: rgba(17,34,59,245);
                border-color: rgba(205,232,255,72);
            }
            QDialog#settingsDialog {
                background-color: rgba(8,22,40,248);
            }
            QWidget#settingsHeader {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 rgba(145,207,255,32), stop:1 rgba(15,37,64,218));
                border-bottom: 1px solid rgba(235,249,255,72);
            }
            QLabel#settingsTitle {
                color: rgba(245,251,255,250);
                font-size: 21px;
                font-weight: 700;
            }
            QLabel#settingsSubtitle {
                color: rgba(184,216,240,205);
                font-size: 12px;
            }
            QWidget#settingsSidebarWrap {
                background-color: rgba(8,24,43,190);
                border-right: 1px solid rgba(235,249,255,52);
            }
            QListWidget#settingsSidebar::item {
                color: rgba(196,221,240,220);
                margin: 3px 1px;
                padding-left: 13px;
                border: 1px solid transparent;
                border-radius: 13px;
            }
            QListWidget#settingsSidebar::item:hover:!selected {
                color: #f1f9ff;
                background-color: rgba(165,215,255,25);
                border-color: rgba(235,249,255,40);
            }
            QListWidget#settingsSidebar::item:selected {
                color: #f5fbff;
                background-color: rgba(158,215,255,48);
                border-color: rgba(235,249,255,100);
            }
            QWidget#settingsSectionHeader {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                    stop:0 rgba(193,228,255,27), stop:1 rgba(70,125,175,16));
                border: 1px solid rgba(225,243,255,52);
                border-radius: 12px;
            }
            QLabel#settingsHead {
                color: rgba(239,248,255,246);
                font-size: 13px;
                font-weight: 650;
            }
            QFrame#settingsSep {
                color: transparent;
                background: transparent;
                border: none;
                min-height: 7px;
                max-height: 7px;
            }
            QWidget#settingsFooter {
                background-color: rgba(8,24,43,225);
                border-top: 1px solid rgba(235,249,255,58);
            }
            QDialog#settingsDialog QLineEdit,
            QDialog#settingsDialog QComboBox,
            QDialog#settingsDialog QFontComboBox,
            QDialog#settingsDialog QTextEdit {
                min-height: 32px;
                background-color: rgba(7,21,39,125);
                border: 1px solid rgba(225,243,255,65);
                border-radius: 11px;
                padding: 4px 10px;
                selection-background-color: ACCENT;
            }
            QDialog#settingsDialog QLineEdit:focus,
            QDialog#settingsDialog QComboBox:focus,
            QDialog#settingsDialog QTextEdit:focus {
                background-color: rgba(10,29,51,170);
                border-color: ACCENT;
            }
            QPushButton#settingsOkBtn {
                min-height: 34px;
                border-radius: 12px;
                padding: 5px 20px;
            }
            QListWidget#playlist, QListWidget#appIconGrid,
            QListWidget#playlistBrowserGrid, QLineEdit,
            QComboBox, QLineEdit#searchEdit, QComboBox#speedCombo,
            QWidget#loadingBanner, QWidget#miniLoadingPanel {
                background-color: rgba(11,27,48,92);
                border: 1px solid rgba(225,243,255,98);
                border-radius: 12px;
            }
            QListWidget#playlist {
                alternate-background-color: rgba(85,145,205,18);
            }
            QListWidget#playlist::item {
                margin: 2px 1px;
                padding: 8px 10px;
                border-radius: 9px;
            }
            QListWidget#playlist::item:hover:!selected {
                background-color: rgba(150,205,255,32);
            }
            QListWidget#playlist::item:selected {
                background-color: rgba(158,215,255,58);
            }
            QToolButton#ctrlBtn, QToolButton#toggleBtn, QToolButton#smallBtn,
            QToolButton#clearBtn, QToolButton#moreBtn, QPushButton {
                background-color: rgba(130,180,225,24);
                border: 1px solid rgba(210,235,255,58);
                border-radius: 10px;
            }
            QToolButton#ctrlBtn:hover, QToolButton#toggleBtn:hover,
            QToolButton#smallBtn:hover, QToolButton#moreBtn:hover, QPushButton:hover {
                background-color: rgba(165,215,255,48);
                border-color: rgba(225,242,255,105);
            }
            QMenu#liquidMoreMenu {
                background-color: rgba(13,31,55,248);
                border: 1px solid rgba(225,243,255,100);
                border-radius: 14px;
                padding: 7px;
            }
            QMenu#liquidMoreMenu::item {
                min-width: 178px;
                padding: 9px 30px 9px 27px;
                margin: 2px;
                border-radius: 9px;
            }
            QMenu#liquidMoreMenu::item:selected {
                background-color: rgba(158,215,255,45);
                border: 1px solid rgba(225,243,255,70);
            }
            QMenu#liquidMoreMenu::separator {
                height: 1px;
                background-color: rgba(225,243,255,38);
                margin: 6px 8px;
            }
            QToolButton#modernNavButton:hover,
            QToolButton#modernNavButton:checked {
                background-color: rgba(158,215,255,34);
                border-color: rgba(210,235,255,64);
            }
            QToolButton#modernNavButton {
                border: none;
                border-bottom: 1px solid rgba(225,243,255,24);
                border-radius: 10px;
            }
            QToolButton#modernNavButton:hover,
            QToolButton#modernNavButton:checked {
                border: 1px solid rgba(225,243,255,86);
            }
            QWidget#searchPopup {
                background: transparent;
                border: none;
            }
            QLineEdit#searchPopupEdit {
                background-color: rgba(8,22,40,105);
                border: 1px solid rgba(235,249,255,125);
                border-radius: 14px;
                padding: 9px 13px;
                font-size: 15px;
            }
            QWidget#miniLoadingPanel {
                background-color: rgba(9,25,45,112);
                border: 1px solid rgba(235,249,255,108);
                border-radius: 15px;
            }
            QLabel#miniLoadingIcon {
                background-color: rgba(185,225,255,30);
                border: 1px solid rgba(235,249,255,70);
                border-radius: 10px;
            }
            QLabel#miniLoadingText {
                color: rgba(242,249,255,245);
                font-size: 11px;
                font-weight: 600;
            }
            QLabel#miniLoadingPercent {
                color: rgba(205,235,255,220);
                font-size: 11px;
            }
            QProgressBar#miniLoadingBar {
                background-color: rgba(210,235,255,25);
                border: 1px solid rgba(225,243,255,34);
                border-radius: 3px;
            }
            QProgressBar#miniLoadingBar::chunk {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 ACCENT, stop:0.55 #8fdcff, stop:1 #d8f4ff);
                border-radius: 3px;
            }
            QTabBar#playlistTabBar::tab {
                background-color: rgba(100,145,190,20);
                border-color: rgba(210,235,255,55);
                border-radius: 10px;
                padding: 6px 18px;
            }
            QTabBar#playlistTabBar::tab:selected {
                background-color: rgba(158,215,255,42);
                border-color: rgba(210,235,255,90);
            }
            QStatusBar {
                background-color: rgba(11,25,45,220);
                border-top-color: rgba(210,235,255,58);
            }
        )";
    }

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

    if (auto *glass = dynamic_cast<LiquidGlassWidget *>(m_modernSidebar))
        glass->setGlassEnabled(liquidGlass, theme.accent);
    if (auto *glass = dynamic_cast<LiquidGlassWidget *>(m_miniBar))
        glass->setGlassEnabled(liquidGlass, theme.accent);
    if (auto *glass = dynamic_cast<LiquidGlassWidget *>(m_topWidget))
        glass->setGlassEnabled(liquidGlass, theme.accent);
    if (auto *glass = dynamic_cast<LiquidGlassWidget *>(m_playlistPanel))
        glass->setGlassEnabled(liquidGlass, theme.accent);
    if (auto *glass = dynamic_cast<LiquidGlassWidget *>(m_searchPopup))
        glass->setGlassEnabled(liquidGlass, theme.accent);


    if (liquidGlass) {
        m_mediaStack->setStyleSheet(
            "QWidget#mediaStack{background-color:rgba(12,27,48,175);"
            "border:1px solid rgba(210,235,255,78);border-radius:18px;}");
    } else {
        m_mediaStack->setStyleSheet(QString(
            "QWidget#mediaStack{background-color:%1;}").arg(theme.mantle.name()));
    }
    updateAlbumArt();

    if (m_aurora) {
        m_aurora->setLightMode(false);
        m_aurora->setThemeColors(theme.crust.darker(112), theme.mantle,
                                 theme.crust.darker(125),
                                 ThemeManager::visualColors(theme));
    }
    if (m_visualizer) {
        m_visualizer->setThemeColors(theme.mantle,
            {theme.teal, theme.accent2, theme.accent, theme.danger});
        m_visualizer->setVisible(m_cfg.showVisualizer);
    }
    statusBar()->setVisible(!m_miniPlayer && !m_cfg.modernLayout && m_cfg.showStatusBar);

    QFont f = font(); f.setPointSize(fs); setFont(f);

    const QColor iconC = ac.lightness() > 160 ? theme.crust : QColor(0xff,0xff,0xff);
    const bool playing = m_player && m_player->playbackState() == QMediaPlayer::PlayingState;

    if (m_playPauseBtn) {
        m_playPauseBtn->setIcon(playing
            ? uiIcon("pause", iconC, 36, Ico::pause(iconC, 36))
            : uiIcon("play_arrow", iconC, 36, Ico::play(iconC, 36)));
        m_playPauseBtn->setStyleSheet(QString(
            "QToolButton#playBtn{background-color:%1;border-radius:30px;border:none;}"
            "QToolButton#playBtn:hover{background-color:%2;}"
            "QToolButton#playBtn:pressed{background-color:%3;}").arg(acH,acL,acD));
    }
    if (m_miniPlayBtn) {
        m_miniPlayBtn->setIcon(playing
            ? uiIcon("pause", iconC, 22, Ico::pause(iconC, 22))
            : uiIcon("play_arrow", iconC, 22, Ico::play(iconC, 22)));
        m_miniPlayBtn->setStyleSheet(QString(
            "QToolButton#playBtn{background-color:%1;border-radius:18px;border:none;}"
            "QToolButton#playBtn:hover{background-color:%2;}"
            "QToolButton#playBtn:pressed{background-color:%3;}").arg(acH,acL,acD));
    }

    const QColor controlColor = theme.text;
    if (m_prevBtn) m_prevBtn->setIcon(uiIcon(
        "skip_previous", controlColor, 22, Ico::prev(controlColor, 22)));
    if (m_nextBtn) m_nextBtn->setIcon(uiIcon(
        "skip_next", controlColor, 22, Ico::next(controlColor, 22)));
    if (m_stopBtn) m_stopBtn->setIcon(uiIcon(
        "stop", controlColor, 16, Ico::stop(controlColor, 16)));
    const QColor secondaryIconColor = theme.subtext0;
    if (m_miniPrevBtn) m_miniPrevBtn->setIcon(uiIcon(
        "skip_previous", secondaryIconColor, 16, Ico::prev(secondaryIconColor, 16)));
    if (m_miniNextBtn) m_miniNextBtn->setIcon(uiIcon(
        "skip_next", secondaryIconColor, 16, Ico::next(secondaryIconColor, 16)));
    if (m_miniMoreBtn) m_miniMoreBtn->setIcon(uiIcon(
        "more_horiz", secondaryIconColor, 19,
        MaterialIco::icon("more_horiz", secondaryIconColor, 19)));
    if (m_miniStopAct) m_miniStopAct->setIcon(uiIcon(
        "stop", secondaryIconColor, 16, Ico::stop(secondaryIconColor, 16)));
    if (m_miniShuffleAct) {
        const QColor color = m_shuffle ? theme.accent : secondaryIconColor;
        m_miniShuffleAct->setIcon(uiIcon(
            "shuffle", color, 16, Ico::shuffle(color, 16)));
        m_miniShuffleAct->setChecked(m_shuffle);
    }
    if (m_miniSpeedMenu) m_miniSpeedMenu->setIcon(uiIcon(
        "equalizer", secondaryIconColor, 16, Ico::equalizer(secondaryIconColor, 16)));
    if (m_miniExpandBtn) m_miniExpandBtn->setIcon(uiIcon(
        "fullscreen", secondaryIconColor, 13, Ico::expand(secondaryIconColor, 13)));
    if (m_miniMinimizeBtn) m_miniMinimizeBtn->setIcon(uiIcon(
        "minimize", secondaryIconColor, 13, Ico::minimize(secondaryIconColor, 13)));
    if (m_miniCloseBtn) m_miniCloseBtn->setIcon(uiIcon(
        "close", secondaryIconColor, 12, Ico::closeIcon(secondaryIconColor, 12)));
    if (m_miniDockBtn) {
        const QColor dockColor = m_miniDockBtn->isChecked()
            ? theme.accent : secondaryIconColor;
        m_miniDockBtn->setIcon(uiIcon(
            "vertical_align_top", dockColor, 14, Ico::dockTop(dockColor, 14)));
    }
    if (m_micBtn) m_micBtn->setIcon(uiIcon(
        "mic", secondaryIconColor, 18, Ico::microphone(secondaryIconColor, 18)));
    if (m_openUrlAct) m_openUrlAct->setIcon(uiIcon(
        "link", secondaryIconColor, 18, Ico::link(secondaryIconColor, 18)));
    if (m_scanLibraryAct) m_scanLibraryAct->setIcon(uiIcon(
        "folder", secondaryIconColor, 18, Ico::folder(secondaryIconColor, 18)));
    std::function<void(QMenu *)> updateMenuIcons = [&](QMenu *menu) {
        if (!menu) return;
        for (QAction *action : menu->actions()) {
            if (action->isSeparator()) continue;
            QString label = action->text();
            label.remove('&');
            QString symbol = "tune";
            QIcon fallback = Ico::sliders(secondaryIconColor, 18);
            if (label.contains("Открыть файлы") || label.contains("Добавить")) {
                symbol = "library_music"; fallback = Ico::music(secondaryIconColor, 18);
            } else if (label.contains("папку", Qt::CaseInsensitive)
                       || label.contains("библиотек", Qt::CaseInsensitive)
                       || label.contains("Недавние")) {
                symbol = "folder"; fallback = Ico::folder(secondaryIconColor, 18);
            } else if (label.contains("ссылк", Qt::CaseInsensitive)) {
                symbol = "link"; fallback = Ico::link(secondaryIconColor, 18);
            } else if (label.contains("Сохранить")) {
                symbol = "vertical_align_top"; fallback = Ico::dockTop(secondaryIconColor, 18);
            } else if (label.contains("Загрузить") || label.contains("обновлен", Qt::CaseInsensitive)) {
                symbol = "download"; fallback = Ico::download(secondaryIconColor, 18);
            } else if (label.contains("Выход") || label.contains("Закрыть")) {
                symbol = "close"; fallback = Ico::closeIcon(secondaryIconColor, 18);
            } else if (label.contains("Играть") || label.contains("Пауза")) {
                symbol = "play_arrow"; fallback = Ico::play(secondaryIconColor, 18);
            } else if (label == "Стоп") {
                symbol = "stop"; fallback = Ico::stop(secondaryIconColor, 18);
            } else if (label.contains("Предыдущ")) {
                symbol = "skip_previous"; fallback = Ico::prev(secondaryIconColor, 18);
            } else if (label.contains("Следующ")) {
                symbol = "skip_next"; fallback = Ico::next(secondaryIconColor, 18);
            } else if (label.contains("Перемеш")) {
                symbol = "shuffle"; fallback = Ico::shuffle(secondaryIconColor, 18);
            } else if (label.contains("Повтор") || label.contains("Один трек")
                       || label.contains("Весь плейлист") || label == "Выкл.") {
                symbol = "repeat"; fallback = Ico::repeatAll(secondaryIconColor, 18);
            } else if (label.contains("Скорость") || label.contains("Кроссфейд")
                       || label.endsWith(QString::fromUtf8("×")) || label.contains("сек.")) {
                symbol = "equalizer"; fallback = Ico::equalizer(secondaryIconColor, 18);
            } else if (label.contains("Мини-плеер")) {
                symbol = "fullscreen"; fallback = Ico::expand(secondaryIconColor, 18);
            } else if (label.contains("Поверх")) {
                symbol = "vertical_align_top"; fallback = Ico::dockTop(secondaryIconColor, 18);
            } else if (label.contains("О программе")) {
                symbol = "music_note"; fallback = Ico::music(secondaryIconColor, 18);
            }
            action->setIcon(uiIcon(symbol, secondaryIconColor, 18, fallback));
            action->setIconVisibleInMenu(true);
            if (action->menu()) updateMenuIcons(action->menu());
        }
    };
    for (QAction *topAction : menuBar()->actions())
        if (topAction->menu()) updateMenuIcons(topAction->menu());
    if (m_trayPlayAct) {
        m_trayPlayAct->setIcon(playing
            ? uiIcon("pause", theme.text, 16, Ico::pause(theme.text, 16))
            : uiIcon("play_arrow", theme.text, 16, Ico::play(theme.text, 16)));
    }
    if (m_trayNextAct) m_trayNextAct->setIcon(uiIcon(
        "skip_next", theme.text, 16, Ico::next(theme.text, 16)));
    const QColor shuffleColor = m_shuffle ? theme.accent : theme.subtext0;
    if (m_shuffleBtn) m_shuffleBtn->setIcon(uiIcon(
        "shuffle", shuffleColor, 18, Ico::shuffle(shuffleColor, 18)));
    if (m_miniShuffleBtn) m_miniShuffleBtn->setIcon(uiIcon(
        "shuffle", shuffleColor, 15, Ico::shuffle(shuffleColor, 15)));
    if (m_playlistUpBtn) m_playlistUpBtn->setIcon(uiIcon(
        "arrow_upward", theme.text, 18, Ico::arrowUp(theme.text, 18)));
    if (m_playlistDownBtn) m_playlistDownBtn->setIcon(uiIcon(
        "arrow_downward", theme.text, 18, Ico::arrowDown(theme.text, 18)));
    if (m_playlistRemoveBtn) m_playlistRemoveBtn->setIcon(uiIcon(
        "close", theme.text, 18, Ico::closeIcon(theme.text, 18)));
    if (m_playlistClearBtn) m_playlistClearBtn->setIcon(uiIcon(
        "delete", theme.text, 18, Ico::trash(theme.text, 18)));
    if (m_newPlaylistBtn) {
        m_newPlaylistBtn->setText(QString());
        m_newPlaylistBtn->setIcon(MaterialIco::icon("add", theme.text, 18));
        m_newPlaylistBtn->setIconSize({18, 18});
    }
    if (m_modernBrandIcon)
        m_modernBrandIcon->setPixmap(createLogo(34, ThemeManager::palette("mocha"), m_cfg.appIconStyle));
    if (m_modernHomeBtn) m_modernHomeBtn->setIcon(uiIcon(
        "home", theme.subtext0, 18, Ico::music(theme.subtext0, 18)));
    if (m_modernSearchBtn) m_modernSearchBtn->setIcon(uiIcon(
        "search", theme.subtext0, 18, Ico::sliders(theme.subtext0, 18)));
    if (m_modernPlaylistsBtn) m_modernPlaylistsBtn->setIcon(uiIcon(
        "queue_music", theme.subtext0, 18, Ico::music(theme.subtext0, 18)));
    if (m_modernLibraryBtn) m_modernLibraryBtn->setIcon(uiIcon(
        "library_music", theme.subtext0, 18, Ico::folder(theme.subtext0, 18)));
    if (m_modernOpenBtn) m_modernOpenBtn->setIcon(uiIcon(
        "download", theme.subtext0, 18, Ico::download(theme.subtext0, 18)));
    if (m_modernFolderBtn) m_modernFolderBtn->setIcon(uiIcon(
        "folder", theme.subtext0, 18, Ico::folder(theme.subtext0, 18)));
    if (m_modernLinkBtn) m_modernLinkBtn->setIcon(uiIcon(
        "link", theme.subtext0, 18, Ico::link(theme.subtext0, 18)));
    if (m_modernSettingsBtn) m_modernSettingsBtn->setIcon(uiIcon(
        "tune", theme.subtext0, 18, Ico::sliders(theme.subtext0, 18)));
    if (m_loadingIcon)
        m_loadingIcon->setPixmap(uiIcon(
            "download", theme.base, 17, Ico::download(theme.base, 17)).pixmap(17, 17));
    if (m_miniLoadingIcon)
        m_miniLoadingIcon->setPixmap(uiIcon(
            "download", theme.accent, 14, Ico::download(theme.accent, 14)).pixmap(14, 14));
    setVolume(m_volumeSlider->value());
    updateRepeatButton();

    if (g_delegate) g_delegate->setTheme(theme);
    if (m_tabBar) {
        for (int i = 0; i < m_tabBar->count() && i < m_playlists.size(); ++i)
            m_tabBar->setTabIcon(i, playlistIcon(i, 18));
    }
    if (m_playlistWidget) m_playlistWidget->viewport()->update();

    const QIcon appIcon(createLogo(128, ThemeManager::palette("mocha"), m_cfg.appIconStyle));
    qApp->setWindowIcon(appIcon);
    setWindowIcon(appIcon);
    if (m_tray) m_tray->setIcon(appIcon);

    if (m_seekSlider) {
        m_seekSlider->setAccentColor(theme.accent);
        m_seekSlider->setTrackColor(theme.surface2);
        QColor background = theme.base;
        if (liquidGlass) background.setAlpha(150);
        m_seekSlider->setBackgroundColor(background);
    }
    if (m_miniWaveform) {
        m_miniWaveform->setAccentColor(theme.accent);
        m_miniWaveform->setTrackColor(theme.surface2);
        QColor background = theme.base;
        if (liquidGlass) background.setAlpha(150);
        m_miniWaveform->setBackgroundColor(background);
    }
    applyModernLayout();
}

QIcon MainWindow::uiIcon(const QString &symbol, const QColor &color, int size,
                         const QIcon &fallback) const {
    if (m_cfg.theme != "liquid") return fallback;
    const QIcon material = MaterialIco::icon(symbol, color, size);
    return material.isNull() ? fallback : material;
}

void MainWindow::applyModernLayout() {
    const bool modern = m_cfg.modernLayout && !m_miniPlayer;
    const bool classic = !m_miniPlayer && !modern;
    const bool floatingGlass = modern && m_cfg.theme == "liquid";
    const bool compactLiquidControls = m_cfg.theme == "liquid" && (modern || m_miniPlayer);

    if (m_aurora && m_aurora->layout()) {
        m_aurora->layout()->setContentsMargins(
            floatingGlass ? 12 : 0, floatingGlass ? 12 : 0,
            floatingGlass ? 12 : 0, floatingGlass ? 12 : 0);
        m_aurora->layout()->setSpacing(floatingGlass ? 12 : 0);
    }
    if (m_contentShell && m_contentShell->layout())
        m_contentShell->layout()->setSpacing(floatingGlass ? 12 : 0);
    if (m_modernSidebar)
        m_modernSidebar->setFixedWidth(floatingGlass ? 204 : 190);

    if (m_miniBar) {
        m_miniBar->setFixedHeight(modern ? 68 : 52);
        if (auto *barLayout = qobject_cast<QHBoxLayout *>(m_miniBar->layout())) {
            barLayout->setContentsMargins(modern ? 12 : 6, modern ? 8 : 4,
                                          modern ? 12 : 6, modern ? 8 : 4);
            barLayout->setSpacing(modern ? 6 : 4);
        }
    }
    if (m_miniAlbumArt) m_miniAlbumArt->setFixedSize(modern ? 48 : 40, modern ? 48 : 40);
    if (m_miniPrevBtn) m_miniPrevBtn->setFixedSize(modern ? 34 : 28, modern ? 34 : 28);
    if (m_miniPlayBtn) m_miniPlayBtn->setFixedSize(modern ? 44 : 36, modern ? 44 : 36);
    if (m_miniNextBtn) m_miniNextBtn->setFixedSize(modern ? 34 : 28, modern ? 34 : 28);
    if (m_miniWaveform) m_miniWaveform->setFixedHeight(modern ? 42 : 36);
    if (m_miniTitle) {
        m_miniTitle->setMinimumWidth(modern ? 120 : 100);
        m_miniTitle->setMaximumWidth(modern ? 165 : 220);
    }
    if (m_miniWaveform)
        m_miniWaveform->setMinimumWidth(modern ? 420 : 160);

    if (m_modernSidebar) m_modernSidebar->setVisible(modern);
    if (m_topWidget) m_topWidget->setVisible(classic);
    if (m_separator) m_separator->setVisible(classic);
    if (m_playlistPanel) m_playlistPanel->setVisible(!m_miniPlayer);
    if (m_miniBar) m_miniBar->setVisible(m_miniPlayer || modern);

    if (m_miniDockBtn) m_miniDockBtn->setVisible(m_miniPlayer);
    if (m_miniExpandBtn) m_miniExpandBtn->setVisible(m_miniPlayer);
    if (m_miniMinimizeBtn) m_miniMinimizeBtn->setVisible(m_miniPlayer);
    if (m_miniCloseBtn) m_miniCloseBtn->setVisible(m_miniPlayer);
    if (m_miniShuffleBtn) m_miniShuffleBtn->setVisible(!compactLiquidControls);
    if (m_miniRepeatBtn) m_miniRepeatBtn->setVisible(!compactLiquidControls);
    if (m_miniMoreBtn) m_miniMoreBtn->setVisible(compactLiquidControls);

    if (menuBar()) menuBar()->setVisible(!m_miniPlayer);
    if (statusBar())
        statusBar()->setVisible(classic && m_cfg.showStatusBar);

    if (m_playlistPanel && m_playlistPanel->layout()) {
        m_playlistPanel->layout()->setContentsMargins(
            modern ? 18 : 12, modern ? 16 : 0,
            modern ? 18 : 12, modern ? 14 : 10);
        m_playlistPanel->layout()->setSpacing(modern ? 8 : 4);
    }
    if (m_modernHomeBtn && modern) m_modernHomeBtn->setChecked(true);
}

void MainWindow::showMiniMoreMenu() {
    if (!m_miniMoreBtn || !m_miniMoreMenu) return;
    if (m_miniMoreMenu->isVisible()) {
        m_miniMoreMenu->hide();
        return;
    }

    m_miniMoreMenu->ensurePolished();
    m_miniMoreMenu->adjustSize();
    const QSize menuSize = m_miniMoreMenu->sizeHint();
    QPoint target = m_miniMoreBtn->mapToGlobal(
        QPoint(m_miniMoreBtn->width() - menuSize.width(), -menuSize.height() - 9));
    if (QScreen *screen = QGuiApplication::screenAt(target)) {
        const QRect available = screen->availableGeometry().adjusted(6, 6, -6, -6);
        target.setX(qBound(available.left(), target.x(),
                           available.right() - menuSize.width() + 1));
        target.setY(qBound(available.top(), target.y(),
                           available.bottom() - menuSize.height() + 1));
    }

    m_miniMoreMenu->setWindowOpacity(0.0);
    m_miniMoreMenu->popup(target);
    QPointer<QMenu> menu(m_miniMoreMenu);
    QTimer::singleShot(0, this, [menu] {
        if (!menu || !menu->isVisible()) return;
        const QRect endGeometry = menu->geometry();
        const QRect startGeometry = endGeometry.translated(0, 12);
        menu->setGeometry(startGeometry);

        auto *slide = new QPropertyAnimation(menu, "geometry", menu);
        slide->setDuration(180);
        slide->setStartValue(startGeometry);
        slide->setEndValue(endGeometry);
        slide->setEasingCurve(QEasingCurve::OutCubic);
        slide->start(QAbstractAnimation::DeleteWhenStopped);

        auto *fade = new QPropertyAnimation(menu, "windowOpacity", menu);
        fade->setDuration(150);
        fade->setStartValue(0.0);
        fade->setEndValue(1.0);
        fade->setEasingCurve(QEasingCurve::OutCubic);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void MainWindow::syncShellShortcutIcon() {
    const QString iconRoot = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/app-icons";
    if (!QDir().mkpath(iconRoot)) return;

    QString styleName = m_cfg.appIconStyle.toLower();
    styleName.remove(QRegularExpression("[^a-z0-9_-]"));
    if (styleName.isEmpty()) styleName = "classic";
    const QString iconPath = iconRoot + "/EchoBoxII-" + styleName + ".ico";
    const QPixmap iconPixmap = createLogo(
        256, ThemeManager::palette("mocha"), styleName);
    if (!iconPixmap.save(iconPath, "ICO")) return;

    QStringList roots = {
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation),
        qEnvironmentVariable("PUBLIC") + "/Desktop",
        qEnvironmentVariable("APPDATA")
            + "/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar"
    };

    QSet<QString> shortcuts;
    for (const QString &root : roots) {
        if (root.isEmpty() || !QDir(root).exists()) continue;
        QDirIterator it(root, {"*.lnk"}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (QFileInfo(path).completeBaseName().contains(
                    "EchoBox", Qt::CaseInsensitive))
                shortcuts.insert(QDir::toNativeSeparators(path));
        }
    }
    if (shortcuts.isEmpty()) return;

    const HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) return;

    for (const QString &shortcut : shortcuts) {
        if (setWindowsShortcutIcon(shortcut,
                                   QDir::toNativeSeparators(iconPath))) {
            SHChangeNotify(SHCNE_UPDATEITEM,
                           SHCNF_PATHW | SHCNF_FLUSHNOWAIT,
                           reinterpret_cast<LPCWSTR>(shortcut.utf16()), nullptr);
        }
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT,
                   nullptr, nullptr);
    if (shouldUninitialize) CoUninitialize();
}


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

    m_cfg.theme          = m_settings.value("cfg/theme", "mocha").toString();
    m_cfg.accentColor    = QColor(m_settings.value("cfg/accentColor", "#cba6f7").toString());
    if (!m_cfg.accentColor.isValid()) m_cfg.accentColor = QColor(0xcb,0xa6,0xf7);
    m_cfg.fontSizeIdx    = m_settings.value("cfg/fontSizeIdx", 1).toInt();
    m_cfg.fontFamily     = m_settings.value("cfg/fontFamily",  "").toString();
    m_cfg.fontFilePath   = m_settings.value("cfg/fontFilePath","").toString();
    if (!m_cfg.fontFilePath.isEmpty())
        QFontDatabase::addApplicationFont(m_cfg.fontFilePath);
    m_cfg.artShape       = m_settings.value("cfg/artShape", "rounded").toString();
    m_cfg.appIconStyle   = m_settings.value("cfg/appIconStyle", "classic").toString();
    m_cfg.autoPlay       = m_settings.value("cfg/autoPlay", false).toBool();
    m_cfg.showVisualizer = m_settings.value("cfg/showVisualizer", true).toBool();
    m_cfg.crossfadeSecs  = m_settings.value("cfg/crossfadeSecs", 0).toInt();
    m_cfg.libraryFolder  = m_settings.value("cfg/libraryFolder", "").toString();
    m_cfg.playlistsFolder= m_settings.value("cfg/playlistsFolder", "").toString();
    m_cfg.iconsFolder    = m_settings.value("cfg/iconsFolder", "").toString();
    m_cfg.showTrackIcons = m_settings.value("cfg/showTrackIcons", true).toBool();
    m_cfg.showStatusBar  = m_settings.value("cfg/showStatusBar", true).toBool();
    m_cfg.closeToTray    = m_settings.value("cfg/closeToTray", true).toBool();
    m_cfg.modernLayout   = m_settings.value("cfg/modernLayout", false).toBool();
    m_cfg.discordEnabled = m_settings.value("cfg/discordEnabled", true).toBool();
    m_cfg.ytDlpCookiesBrowser  = m_settings.value("cfg/ytDlpCookiesBrowser", "").toString();
    m_cfg.streamAudioQuality   = m_settings.value("cfg/streamAudioQuality", "best").toString();

    m_cfg.startMinimized   = m_settings.value("cfg/startMinimized", false).toBool();
    m_cfg.autoCheckUpdates = m_settings.value("cfg/autoCheckUpdates", true).toBool();
    m_cfg.confirmDelete    = m_settings.value("cfg/confirmDelete", true).toBool();
    m_cfg.seekStepSecs     = m_settings.value("cfg/seekStepSecs", 5).toInt();
    m_cfg.volumeStep       = m_settings.value("cfg/volumeStep", 5).toInt();
    m_cfg.eqEnabled        = m_settings.value("cfg/eqEnabled", false).toBool();
    for (int i = 0; i < kEqBandCount; ++i)
        m_cfg.eqBands[i] = m_settings.value(QString("cfg/eqBand%1").arg(i), 0.0).toFloat();
    m_cfg.launchOnStartup  = isLaunchOnStartupEnabled();

    const bool alwaysOnTop = m_settings.value("view/alwaysOnTop", false).toBool();
    setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
    if (m_alwaysOnTopAct) m_alwaysOnTopAct->setChecked(alwaysOnTop);

    m_eqEngine->setEqEnabled(m_cfg.eqEnabled);
    for (int i = 0; i < kEqBandCount; ++i)
        m_eqEngine->setEqBandGain(i, m_cfg.eqBands[i]);

    if (g_delegate) g_delegate->showIcons = m_cfg.showTrackIcons;
    applyTheme();
    loadStreamTracksFromFile();
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
    m_settings.setValue("cfg/appIconStyle",   m_cfg.appIconStyle);
    m_settings.setValue("cfg/autoPlay",       m_cfg.autoPlay);
    m_settings.setValue("cfg/showVisualizer", m_cfg.showVisualizer);
    m_settings.setValue("cfg/crossfadeSecs",  m_cfg.crossfadeSecs);
    m_settings.setValue("cfg/libraryFolder",  m_cfg.libraryFolder);
    m_settings.setValue("cfg/playlistsFolder",m_cfg.playlistsFolder);
    m_settings.setValue("cfg/iconsFolder",    m_cfg.iconsFolder);
    m_settings.setValue("cfg/showTrackIcons", m_cfg.showTrackIcons);
    m_settings.setValue("cfg/showStatusBar",  m_cfg.showStatusBar);
    m_settings.setValue("cfg/closeToTray",    m_cfg.closeToTray);
    m_settings.setValue("cfg/modernLayout",   m_cfg.modernLayout);
    m_settings.setValue("cfg/discordEnabled", m_cfg.discordEnabled);
    m_settings.setValue("cfg/ytDlpCookiesBrowser", m_cfg.ytDlpCookiesBrowser);
    m_settings.setValue("cfg/streamAudioQuality",  m_cfg.streamAudioQuality);
    m_settings.setValue("cfg/startMinimized",      m_cfg.startMinimized);
    m_settings.setValue("cfg/autoCheckUpdates",    m_cfg.autoCheckUpdates);
    m_settings.setValue("cfg/confirmDelete",       m_cfg.confirmDelete);
    m_settings.setValue("cfg/seekStepSecs",        m_cfg.seekStepSecs);
    m_settings.setValue("cfg/volumeStep",          m_cfg.volumeStep);
    m_settings.setValue("cfg/eqEnabled",           m_cfg.eqEnabled);
    m_settings.setValue("view/alwaysOnTop",
                        windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    for (int i = 0; i < kEqBandCount; ++i)
        m_settings.setValue(QString("cfg/eqBand%1").arg(i), m_cfg.eqBands[i]);

    savePlaylistsToFile();
    saveStreamTracksToFile();
}


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
    if (!m_searchEdit->text().trimmed().isEmpty())
        onSearchChanged(m_searchEdit->text());
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
    if (m_cfg.confirmDelete &&
        QMessageBox::question(this, "Очистить плейлист",
            QString("Удалить все %1 треков из плейлиста?").arg(m_playlist.size()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    m_player->stop();
    stopEqEngine();
    m_playlist.clear();
    m_playlistWidget->clear();
    m_currentIndex = -1;
    m_titleLabel->setText("EchoBox II");
    m_artistLabel->setText("Перетащи файлы или открой через меню Файл");
    m_albumLabel->setText("");
    resetWaveformUi();
    setWindowTitle("EchoBox II");
    updatePlaylistInfo();
    m_coverPixmap = QPixmap();
    updateAlbumArt();
    fadeInWidget(m_albumArt, 300);
    fadeInWidget(m_titleLabel, 280);
    fadeInWidget(m_artistLabel, 280);
    statusBar()->showMessage("Плейлист очищен");
}

void MainWindow::removeSelectedTracks() {
    QList<QListWidgetItem*> sel = m_playlistWidget->selectedItems();
    if (sel.isEmpty()) return;
    const QString msg = sel.size() == 1
        ? QString("Удалить «%1» из плейлиста?").arg(sel.first()->text())
        : QString("Удалить %1 треков из плейлиста?").arg(sel.size());
    if (m_cfg.confirmDelete &&
        QMessageBox::question(this, "Удалить треки", msg,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    for (QListWidgetItem *item : sel) {
        QUrl url = item->data(Qt::UserRole).value<QUrl>();
        m_playlist.removeAll(url);
        delete item;
    }
    for (int i = 0; i < m_playlistWidget->count(); ++i) {
        QListWidgetItem *it = m_playlistWidget->item(i);
        const QUrl u = it->data(Qt::UserRole).value<QUrl>();
        it->setText(QString("  %1.  %2").arg(i+1).arg(playlistRowLabel(u)));
    }
    if (m_currentIndex >= m_playlist.size()) m_currentIndex = m_playlist.size() - 1;
    updatePlaylistInfo();
    updateDuplicateHighlights();
}

void MainWindow::copySelectedTracksToPlaylist(int targetIndex) {
    if (targetIndex < 0 || targetIndex >= m_playlists.size() || targetIndex == m_activePl) return;
    const QList<QListWidgetItem*> sel = m_playlistWidget->selectedItems();
    if (sel.isEmpty()) return;

    PlaylistEntry &target = m_playlists[targetIndex];
    int added = 0;
    for (QListWidgetItem *item : sel) {
        const QUrl url = item->data(Qt::UserRole).value<QUrl>();
        if (target.tracks.contains(url)) continue;
        target.tracks.append(url);
        ++added;
    }
    savePlaylistsToFile();
    statusBar()->showMessage(added > 0
        ? QString("Скопировано треков: %1 → «%2»").arg(added).arg(target.name)
        : "Эти треки уже есть в выбранном плейлисте", 4000);
}

void MainWindow::moveSelectedTracksToPlaylist(int targetIndex) {
    if (targetIndex < 0 || targetIndex >= m_playlists.size() || targetIndex == m_activePl) return;
    const QList<QListWidgetItem*> sel = m_playlistWidget->selectedItems();
    if (sel.isEmpty()) return;

    PlaylistEntry &target = m_playlists[targetIndex];
    int moved = 0;
    for (QListWidgetItem *item : sel) {
        const QUrl url = item->data(Qt::UserRole).value<QUrl>();
        if (!target.tracks.contains(url)) { target.tracks.append(url); ++moved; }
    }

    for (QListWidgetItem *item : sel) {
        const QUrl url = item->data(Qt::UserRole).value<QUrl>();
        m_playlist.removeAll(url);
        delete item;
    }
    for (int i = 0; i < m_playlistWidget->count(); ++i) {
        QListWidgetItem *it = m_playlistWidget->item(i);
        const QUrl u = it->data(Qt::UserRole).value<QUrl>();
        it->setText(QString("  %1.  %2").arg(i + 1).arg(playlistRowLabel(u)));
    }
    if (m_currentIndex >= m_playlist.size()) m_currentIndex = m_playlist.size() - 1;
    updatePlaylistInfo();
    updateDuplicateHighlights();
    savePlaylistsToFile();
    statusBar()->showMessage(
        QString("Перемещено треков: %1 → «%2»").arg(moved).arg(target.name), 4000);
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
    const QString query = text.trimmed();

    int matches = 0;
    const int total = m_playlistWidget->count();

    for (int i = 0; i < total; ++i) {
        QListWidgetItem *it = m_playlistWidget->item(i);
        if (query.isEmpty()) {
            it->setHidden(false);
        } else {
            const QUrl url = it->data(Qt::UserRole).toUrl();
            const QStringList searchable = {
                it->text(),
                it->data(Qt::UserRole + 2).toString(),
                it->data(Qt::UserRole + 3).toString(),
                it->data(Qt::UserRole + 4).toString(),
                url.isLocalFile() ? QFileInfo(url.toLocalFile()).completeBaseName()
                                  : url.toString(),
            };
            bool hit = false;
            for (const QString &value : searchable) {
                if (value.contains(query, Qt::CaseInsensitive)) {
                    hit = true;
                    break;
                }
            }
            it->setHidden(!hit);
            if (hit) ++matches;
        }
    }

    if (!query.isEmpty())
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
        const ThemePalette theme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
        menu.addAction(uiIcon("play_arrow", theme.text, 16, Ico::play(theme.text, 16)),
                       "Воспроизвести", [this, item]{ onTrackActivated(item); });
        menu.addAction(uiIcon("delete", theme.text, 16, Ico::trash(theme.text, 16)),
                       "Удалить из плейлиста", this, &MainWindow::removeSelectedTracks);
        if (m_playlists.size() > 1) {
            QMenu *moveMenu = menu.addMenu("Переместить в плейлист");
            QMenu *copyMenu = menu.addMenu("Копировать в плейлист");
            for (int i = 0; i < m_playlists.size(); ++i) {
                if (i == m_activePl) continue;
                const QString name = m_playlists[i].name;
                moveMenu->addAction(name, [this, i]{ moveSelectedTracksToPlaylist(i); });
                copyMenu->addAction(name, [this, i]{ copySelectedTracksToPlaylist(i); });
            }
        }
        menu.addSeparator();
        QUrl itemUrl = item->data(Qt::UserRole).value<QUrl>();
        if (!itemUrl.isLocalFile()) {
            menu.addAction(uiIcon("link", theme.text, 16, Ico::link(theme.text, 16)),
                           "Копировать ссылку", [this, itemUrl] {
                QGuiApplication::clipboard()->setText(itemUrl.toString());
                statusBar()->showMessage("Ссылка скопирована", 2500);
            });
        }
        menu.addAction("Установить иконку...", [this, item, itemUrl]{
            QString img = QFileDialog::getOpenFileName(this, "Выбрать иконку",
                QString(), "Изображения (*.png *.jpg *.jpeg *.bmp *.webp)");
            if (img.isEmpty()) return;
            QPixmap full(img);
            if (full.isNull()) return;
            QString iconFile = trackIconPath(itemUrl);
            QDir().mkpath(QFileInfo(iconFile).absolutePath());
            const QPixmap large = (full.width() > 512 || full.height() > 512)
                ? full.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                : full;
            large.save(iconFile, "PNG");
            const QPixmap thumb = full.scaled(36, 36, Qt::KeepAspectRatioByExpanding,
                                              Qt::SmoothTransformation).copy(0, 0, 36, 36);
            item->setIcon(QIcon(thumb));
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


void MainWindow::playTrack(int index) {
    if (index < 0 || index >= m_playlist.size()) return;

    saveTrackPosition();

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
    resetWaveformUi();

    m_coverPixmap = QPixmap();
    updateAlbumArt();

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
    fadeInWidget(m_titleLabel, 280);
    fadeInWidget(m_artistLabel, 280);
    fadeInWidget(m_miniTitle, 280);

    if (!url.isLocalFile()) {
        statusBar()->showMessage("Получение потока: " + name + " …");
        const QString icoFile = trackIconPath(url);
        m_coverPixmap = QFile::exists(icoFile) ? QPixmap(icoFile) : QPixmap();
        m_mediaStack->setCurrentWidget(m_albumArt);
        updateAlbumArt();
        fadeInWidget(m_albumArt, 320);
        beginStreamPlayback(index, url);
        return;
    }

    m_player->setSource(url);
    m_player->play();
    syncEqEngineToCurrentTrack();
    applyVolume();

    statusBar()->showMessage(url.toLocalFile());

    if (isVideoFile(url))
        m_mediaStack->setCurrentWidget(m_videoWidget);
    else {
        const QString icoFile = trackIconPath(url);
        m_coverPixmap = QFile::exists(icoFile) ? QPixmap(icoFile) : QPixmap();
        m_mediaStack->setCurrentWidget(m_albumArt);
        updateAlbumArt();
        fadeInWidget(m_albumArt, 320);
    }

    addRecentFile(url.toLocalFile());

    if (m_discord && m_cfg.discordEnabled)
        m_discord->updateActivity(m_titleLabel->text(), m_artistLabel->text());
}


QString MainWindow::ytDlpPath() const {
    const QString bundled = QCoreApplication::applicationDirPath() + "/yt-dlp.exe";
    if (QFile::exists(bundled)) return bundled;
    const QString installed = QStandardPaths::findExecutable("yt-dlp.exe");
    if (!installed.isEmpty()) return installed;
    return bundled;
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
        return artist.isEmpty() ? title : artist + "  —  " + title;
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

    if (url.host().contains("music.yandex.", Qt::CaseInsensitive)) {
        QMessageBox::warning(this, "Не поддерживается",
            "Яндекс.Музыка не поддерживается: сервис блокирует такие запросы "
            "как автоматические и требует авторизацию в браузере, которую "
            "обойти не получается.");
        return;
    }

    if (looksLikeDirectMediaUrl(url)) {
        addDirectStreamUrl(url);
        return;
    }

    const int index = insertStreamPlaceholder(url);
    const bool shouldAutoplay = (m_playlist.size() == 1);
    m_streamResolving.insert(url);
    showLoadingBanner("Получение трека: " + trackDisplayTitle(url) + " …");
    downloadStreamTrack(url, [this, url, index, shouldAutoplay]
            (bool ok, QString localPath, QString title, QString artist, QString thumb, QString errorMsg) {
        m_streamResolving.remove(url);
        updateStreamPlaceholder(url, ok, localPath, title, artist, thumb, errorMsg);
        if (shouldAutoplay && ok) {
            playTrack(index);
        } else {
            hideLoadingBanner();
            if (shouldAutoplay)
                showCopyableError("Не удалось получить трек", errorMsg);
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
    saveStreamTracksToFile();

    m_playlist.append(url);
    const int index = m_playlist.size() - 1;
    auto *item = new QListWidgetItem(QString("  %1.  %2").arg(index + 1).arg(name));
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

    auto *item = new QListWidgetItem(QString("  %1.  Загрузка…").arg(index + 1));
    item->setData(Qt::UserRole, pageUrl);
    item->setToolTip(pageUrl.toString());
    m_playlistWidget->addItem(item);

    updatePlaylistInfo();
    if (!m_searchEdit->text().trimmed().isEmpty())
        onSearchChanged(m_searchEdit->text());
    return index;
}

void MainWindow::updateStreamPlaceholder(const QUrl &pageUrl, bool ok, const QString &localPath,
                                          const QString &title, const QString &artist,
                                          const QString &thumbnailUrl, const QString &errorMsg) {
    const int idx = m_playlist.indexOf(pageUrl);
    if (idx < 0) return;

    QListWidgetItem *item = m_playlistWidget->item(idx);

    if (!ok) {
        if (item) {
            item->setText(QString("  %1.  ⚠ Ошибка загрузки").arg(idx + 1));
            item->setToolTip(errorMsg.isEmpty() ? "Неизвестная ошибка" : errorMsg);
        }
        return;
    }

    StreamTrackInfo &info = m_streamTracks[pageUrl];
    info.localPath     = localPath;
    info.title         = title;
    info.artist        = artist;
    info.thumbnailUrl  = thumbnailUrl;
    saveStreamTracksToFile();

    const QString label = artist.isEmpty() ? title : artist + "  —  " + title;
    if (item) {
        item->setText(QString("  %1.  %2").arg(idx + 1).arg(label));
        item->setData(Qt::UserRole + 2, title);
        item->setData(Qt::UserRole + 3, artist);
    }
    statusBar()->showMessage("Трек готов: " + label, 4000);

    if (idx == m_currentIndex) {
        m_titleLabel->setText(title);
        m_miniTitle->setText(label);
        m_artistLabel->setText(artist);
        setWindowTitle("EchoBox II  —  " + label);
        fadeInWidget(m_titleLabel, 280);
        fadeInWidget(m_artistLabel, 280);
        fadeInWidget(m_miniTitle, 280);
    }

    updateDuplicateHighlights();
    if (!m_searchEdit->text().trimmed().isEmpty())
        onSearchChanged(m_searchEdit->text());
    fetchStreamThumbnail(pageUrl, thumbnailUrl);
}

void MainWindow::fetchStreamThumbnail(const QUrl &pageUrl, const QString &thumbnailUrl) {
    if (thumbnailUrl.isEmpty()) return;

    const QString icoFile = trackIconPath(pageUrl);
    if (QFile::exists(icoFile)) {
        const int idx = m_playlist.indexOf(pageUrl);
        if (idx < 0) return;
        if (m_cfg.showTrackIcons) {
            QListWidgetItem *item = m_playlistWidget->item(idx);
            if (item) applyTrackIcon(item, pageUrl);
        }
        if (idx == m_currentIndex) {
            m_coverPixmap = QPixmap(icoFile);
            updateAlbumArt();
            fadeInWidget(m_albumArt, 320);
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
            fadeInWidget(m_albumArt, 320);
        }
    });
}

void MainWindow::downloadStreamTrack(const QUrl &pageUrl,
        std::function<void(bool, QString, QString, QString, QString, QString)> callback) {
    const QString cacheDir = streamCacheDir();
    QDir().mkpath(cacheDir);
    const QString hash = QCryptographicHash::hash(
        pageUrl.toString().toUtf8(), QCryptographicHash::Md5).toHex();
    const QString outTemplate = cacheDir + "/" + hash + ".%(ext)s";

    auto *proc = new QProcess(this);
    QStringList args = {
        "--no-playlist", "--no-warnings", "--newline", "--no-color", "--progress",
        "--extractor-args", "youtube:player_client=android,web",
    };
    if (!m_cfg.ytDlpCookiesBrowser.isEmpty())
        args << "--cookies-from-browser" << m_cfg.ytDlpCookiesBrowser;
    QString format = "bestaudio/best";
    if (m_cfg.streamAudioQuality == "medium") format = "bestaudio[abr<=128]/bestaudio/best";
    else if (m_cfg.streamAudioQuality == "low") format = "bestaudio[abr<=64]/bestaudio/best";
    args << "-f" << format << "--print-json" << "-o" << outTemplate << pageUrl.toString();

    auto stderrData = QSharedPointer<QByteArray>::create();
    auto progressRemainder = QSharedPointer<QByteArray>::create();
    connect(proc, &QProcess::readyReadStandardError, this,
            [this, proc, stderrData, progressRemainder] {
        const QByteArray chunk = proc->readAllStandardError();
        stderrData->append(chunk);
        progressRemainder->append(chunk);
        const QList<QByteArray> lines = progressRemainder->split('\n');
        *progressRemainder = lines.isEmpty() ? QByteArray() : lines.last();
        static const QRegularExpression progressPattern(
            R"(\[download\]\s+([0-9]+(?:\.[0-9]+)?)%)");
        for (int i = 0; i + 1 < lines.size(); ++i) {
            const QRegularExpressionMatch match = progressPattern.match(
                QString::fromUtf8(lines[i]));
            if (!match.hasMatch()) continue;
            const int percent = qBound(0, qRound(match.captured(1).toDouble()), 100);
            setLoadingProgress(percent);
            updateLoadingText(QString("Загрузка трека · %1%").arg(percent));
        }
    });

    connect(proc, &QProcess::errorOccurred, this,
        [this, proc, callback](QProcess::ProcessError err) {
            if (err != QProcess::FailedToStart) return;
            const QString attemptedPath = QDir::toNativeSeparators(proc->program());
            const QString systemError = proc->errorString();
            proc->deleteLater();
            if (!m_ytDlpMissingWarned) {
                m_ytDlpMissingWarned = true;
                QMessageBox::warning(this, "yt-dlp не найден",
                    "Компонент для загрузки музыки по ссылке отсутствует или Windows "
                    "не разрешила его запустить.\n\nПроверенный путь:\n" + attemptedPath +
                    "\n\nОтвет Windows:\n" + systemError +
                    "\n\nyt-dlp.exe должен находиться рядом с EchoBoxII.exe.");
            }
            callback(false, QString(), QString(), QString(), QString(),
                     "Не удалось запустить yt-dlp.exe: " + systemError);
        });

    connect(proc, &QProcess::finished, this,
        [this, proc, pageUrl, hash, cacheDir, callback, stderrData]
        (int exitCode, QProcess::ExitStatus exitStatus) {
            stderrData->append(proc->readAllStandardError());
            proc->deleteLater();
            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                const QStringList errLines = QString::fromUtf8(*stderrData)
                    .split('\n', Qt::SkipEmptyParts);
                const QString lastErr = errLines.isEmpty() ? QString() : errLines.last().trimmed();
                callback(false, QString(), QString(), QString(), QString(),
                    lastErr.isEmpty()
                        ? QString("yt-dlp завершился с ошибкой (код %1)").arg(exitCode)
                        : lastErr);
                return;
            }

            QDir dir(cacheDir);
            const QStringList found = dir.entryList({hash + ".*"}, QDir::Files);
            if (found.isEmpty()) {
                callback(false, QString(), QString(), QString(), QString(),
                         "yt-dlp отработал успешно, но файл не найден в кэше");
                return;
            }
            const QString localPath = dir.filePath(found.first());

            setLoadingProgress(100);
            updateLoadingText("Загрузка трека · 100%");

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

            callback(true, localPath, title, artist, thumb, QString());
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

    if (m_streamResolving.contains(pageUrl)) return;
    m_streamResolving.insert(pageUrl);

    showLoadingBanner("Загрузка трека: " + trackDisplayTitle(pageUrl) + " …");
    downloadStreamTrack(pageUrl, [this, index, pageUrl]
            (bool ok, QString localPath, QString title, QString artist, QString thumb, QString errorMsg) {
        m_streamResolving.remove(pageUrl);
        updateStreamPlaceholder(pageUrl, ok, localPath, title, artist, thumb, errorMsg);
        if (!ok) {
            hideLoadingBanner();
            showCopyableError("Не удалось скачать трек", errorMsg);
            return;
        }
        if (m_currentIndex != index || m_playlist.value(index) != pageUrl) { hideLoadingBanner(); return; }
        commitStreamPlayback(index, pageUrl, QUrl::fromLocalFile(localPath));
    });
}

void MainWindow::commitStreamPlayback(int index, const QUrl &pageUrl, const QUrl &mediaSource) {
    Q_UNUSED(index);

    hideLoadingBanner();
    m_player->setSource(mediaSource);
    m_player->play();
    syncEqEngineToCurrentTrack();
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
        if (m_eqActive) m_eqEngine->pause();
    } else if (m_player->playbackState() == QMediaPlayer::PausedState) {
        m_player->play();
        if (m_eqActive) m_eqEngine->play();
    } else if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()
               && !m_player->source().isEmpty()) {
        playerSeek(0);
        m_player->play();
        syncEqEngineToCurrentTrack();
    } else if (!m_playlist.isEmpty()) {
        playTrack(qMax(m_currentIndex, 0));
    }
}

void MainWindow::stop() {
    m_player->stop();
    stopEqEngine();
    popButtonIcon(m_stopBtn);
}

void MainWindow::previous() {
    popButtonIcon(m_prevBtn);
    if (m_playlist.isEmpty()) return;
    int prevIdx = m_currentIndex - 1;
    if (prevIdx < 0) prevIdx = m_playlist.size() - 1;
    playTrack(prevIdx);
}

void MainWindow::next() {
    popButtonIcon(m_nextBtn);
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
        if (nextIdx >= m_playlist.size()) nextIdx = 0;
    }
    playTrack(nextIdx);
}

void MainWindow::nextAuto() { playNext(true); }

void MainWindow::playNext(bool respectRepeat) {
    if (m_playlist.isEmpty()) return;

    if (respectRepeat && m_repeat == RepeatMode::One) {
        playerSeek(0);
        m_player->play();
        if (m_eqActive) m_eqEngine->play();
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
        else { m_player->stop(); stopEqEngine(); return; }
    }
    playTrack(nextIdx);
}

void MainWindow::setVolume(int v) {
    applyVolume();
    m_volumeLabel->setText(QString("%1%").arg(v));
    const int level = (v == 0) ? 0 : (v < 40) ? 1 : (v < 75) ? 2 : 3;
    const QColor vc = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor).text;
    const QString volumeSymbol = level == 0 ? "volume_off" : "volume_up";
    m_muteBtn->setIcon(uiIcon(volumeSymbol, vc, 22, Ico::volume(level, vc, 22)));
    if (m_miniMuteBtn)
        m_miniMuteBtn->setIcon(uiIcon(volumeSymbol, vc, 18, Ico::volume(level, vc, 18)));
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
    popButtonIcon(m_muteBtn);
    if (m_miniMuteBtn) popButtonIcon(m_miniMuteBtn);
}

void MainWindow::onSpeedChanged(int index) {
    const double speeds[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
    if (index >= 0 && index < 6) {
        m_player->setPlaybackRate(speeds[index]);
        m_eqEngine->setPlaybackRate(speeds[index]);
        if (m_miniSpeedMenu)
            m_miniSpeedMenu->setTitle("Скорость · " + m_speedCombo->itemText(index));
    }
}

void MainWindow::toggleShuffle() {
    m_shuffle = !m_shuffle;
    m_shuffleBtn->setChecked(m_shuffle);
    const ThemePalette theme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
    const QColor c = m_shuffle ? theme.accent : theme.subtext0;
    m_shuffleBtn->setIcon(uiIcon("shuffle", c, 18, Ico::shuffle(c, 18)));
    if (m_miniShuffleBtn) {
        m_miniShuffleBtn->setChecked(m_shuffle);
        m_miniShuffleBtn->setIcon(uiIcon("shuffle", c, 15, Ico::shuffle(c, 15)));
    }
    if (m_shuffleAct) m_shuffleAct->setChecked(m_shuffle);
    if (m_miniShuffleAct) {
        m_miniShuffleAct->setChecked(m_shuffle);
        m_miniShuffleAct->setIcon(uiIcon("shuffle", c, 16, Ico::shuffle(c, 16)));
    }
    statusBar()->showMessage(m_shuffle ? "Перемешивание включено" : "Перемешивание выключено", 2000);
    popButtonIcon(m_shuffleBtn);
    if (m_miniShuffleBtn) popButtonIcon(m_miniShuffleBtn);
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
    const ThemePalette theme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
    const QColor off = theme.subtext0;
    const QColor on = theme.accent;
    switch (m_repeat) {
    case RepeatMode::Off:
        m_repeatBtn->setIcon(uiIcon("repeat", off, 18, Ico::repeatAll(off, 18)));
        m_repeatBtn->setToolTip("Повтор: выкл.");
        m_repeatBtn->setChecked(false);
        if (m_miniRepeatBtn) { m_miniRepeatBtn->setChecked(false); m_miniRepeatBtn->setIcon(uiIcon("repeat", off, 15, Ico::repeatAll(off,15))); }
        if (m_repeatOffAct) m_repeatOffAct->setChecked(true);
        if (m_miniRepeatAct) {
            m_miniRepeatAct->setText("Повтор: выключен");
            m_miniRepeatAct->setChecked(false);
            m_miniRepeatAct->setIcon(uiIcon("repeat", off, 16, Ico::repeatAll(off, 16)));
        }
        break;
    case RepeatMode::All:
        m_repeatBtn->setIcon(uiIcon("repeat", on, 18, Ico::repeatAll(on, 18)));
        m_repeatBtn->setToolTip("Повтор: весь плейлист");
        m_repeatBtn->setChecked(true);
        if (m_miniRepeatBtn) { m_miniRepeatBtn->setChecked(true); m_miniRepeatBtn->setIcon(uiIcon("repeat", on, 15, Ico::repeatAll(on,15))); }
        if (m_repeatAllAct) m_repeatAllAct->setChecked(true);
        if (m_miniRepeatAct) {
            m_miniRepeatAct->setText("Повтор: весь плейлист");
            m_miniRepeatAct->setChecked(true);
            m_miniRepeatAct->setIcon(uiIcon("repeat", on, 16, Ico::repeatAll(on, 16)));
        }
        break;
    case RepeatMode::One:
        m_repeatBtn->setIcon(uiIcon("repeat_one", on, 18, Ico::repeatOne(on, 18)));
        m_repeatBtn->setToolTip("Повтор: один трек");
        m_repeatBtn->setChecked(true);
        if (m_miniRepeatBtn) { m_miniRepeatBtn->setChecked(true); m_miniRepeatBtn->setIcon(uiIcon("repeat_one", on, 15, Ico::repeatOne(on,15))); }
        if (m_repeatOneAct) m_repeatOneAct->setChecked(true);
        if (m_miniRepeatAct) {
            m_miniRepeatAct->setText("Повтор: один трек");
            m_miniRepeatAct->setChecked(true);
            m_miniRepeatAct->setIcon(uiIcon("repeat_one", on, 16, Ico::repeatOne(on, 16)));
        }
        break;
    }
    popButtonIcon(m_repeatBtn);
    if (m_miniRepeatBtn) popButtonIcon(m_miniRepeatBtn);
}

void MainWindow::toggleMiniPlayer() {
    if (m_miniTransitioning) {
        if (m_miniPlayerAct) m_miniPlayerAct->setChecked(m_miniPlayer);
        return;
    }

    m_miniTransitioning = true;
    const bool enteringMini = !m_miniPlayer;
    const QRect startGeometry = geometry();
    if (enteringMini)
        m_fullPlayerGeometry = startGeometry;

    auto *fadeOut = new QPropertyAnimation(this, "windowOpacity", this);
    fadeOut->setDuration(120);
    fadeOut->setStartValue(windowOpacity());
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::InCubic);
    connect(fadeOut, &QPropertyAnimation::finished, this,
            [this, enteringMini, startGeometry] {
        m_miniPlayer = enteringMini;

        m_topWidget->setVisible(!m_miniPlayer);
        m_separator->setVisible(!m_miniPlayer);
        m_playlistPanel->setVisible(!m_miniPlayer);
        m_miniBar->setVisible(m_miniPlayer);
        menuBar()->setVisible(!m_miniPlayer);
        statusBar()->setVisible(!m_miniPlayer && m_cfg.showStatusBar);
        applyModernLayout();

        if (!m_miniPlayer && m_miniDocked) {
            m_miniDocked = false;
            if (m_miniDockBtn) {
                m_miniDockBtn->setChecked(false);
                const ThemePalette theme =
                    ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
                m_miniDockBtn->setIcon(uiIcon(
                    "vertical_align_top", theme.subtext0, 14,
                    Ico::dockTop(theme.subtext0, 14)));
            }
        }

        setMinimumSize(0, 0);
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        setWindowFlag(Qt::FramelessWindowHint, m_miniPlayer);
        show();
        setWindowOpacity(0.0);
        setGeometry(startGeometry);

        QRect targetGeometry;
        if (m_miniPlayer) {
            targetGeometry = QRect(startGeometry.topLeft(), QSize(720, 52));
        } else if (m_fullPlayerGeometry.isValid()) {
            targetGeometry = m_fullPlayerGeometry;
        } else {
            targetGeometry = QRect(startGeometry.topLeft(), QSize(940, 660));
        }

        auto *geometryAnim = new QPropertyAnimation(this, "geometry", this);
        geometryAnim->setDuration(360);
        geometryAnim->setStartValue(startGeometry);
        geometryAnim->setEndValue(targetGeometry);
        geometryAnim->setEasingCurve(QEasingCurve::OutCubic);

        auto *fadeIn = new QPropertyAnimation(this, "windowOpacity", this);
        fadeIn->setDuration(260);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->setEasingCurve(QEasingCurve::OutCubic);

        connect(geometryAnim, &QPropertyAnimation::finished, this,
                [this, targetGeometry] {
            setGeometry(targetGeometry);
            if (m_miniPlayer) {
                setMinimumSize(520, 52);
                setMaximumHeight(52);
            } else {
                setMinimumSize(720, 540);
                setMaximumHeight(QWIDGETSIZE_MAX);
            }
            applyModernLayout();
            setWindowOpacity(1.0);
            m_miniTransitioning = false;
            if (m_miniPlayerAct)
                m_miniPlayerAct->setChecked(m_miniPlayer);
        });

        geometryAnim->start(QAbstractAnimation::DeleteWhenStopped);
        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    });
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::toggleMiniDock() {
    QScreen *scr = screen();
    if (!scr) scr = QGuiApplication::primaryScreen();
    if (!scr) return;

    m_miniDocked = !m_miniDocked;
    if (m_miniDockBtn) {
        const ThemePalette theme =
            ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
        const QColor dockColor = m_miniDocked ? theme.accent : theme.subtext0;
        m_miniDockBtn->setIcon(uiIcon(
            "vertical_align_top", dockColor, 14, Ico::dockTop(dockColor, 14)));
    }
    const QRect avail = scr->availableGeometry();

    QRect target;
    if (m_miniDocked) {
        m_miniUndockedGeometry = geometry();
        target = QRect(avail.left(), avail.top(), avail.width(), height());
    } else if (m_miniUndockedGeometry.isValid()) {
        target = m_miniUndockedGeometry;
    } else {
        return;
    }

    auto *anim = new QPropertyAnimation(this, "geometry", this);
    anim->setDuration(380);
    anim->setStartValue(geometry());
    anim->setEndValue(target);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::toggleAlwaysOnTop() {
    const bool enabled = !windowFlags().testFlag(Qt::WindowStaysOnTopHint);
    setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
    show();
    if (m_alwaysOnTopAct) m_alwaysOnTopAct->setChecked(enabled);
    m_settings.setValue("view/alwaysOnTop", enabled);
}

void MainWindow::toggleRemainingTime() {
    m_showRemaining = !m_showRemaining;
    updateTimeDisplay(m_player->position(), m_player->duration());
}


void MainWindow::onDurationChanged(qint64 duration) {
    m_seekSlider->setRange(0, static_cast<int>(duration));
    m_miniWaveform->setRange(0, static_cast<int>(duration));
    if (duration > 0) {
        const QUrl src = m_player->source();
        QVector<float> cached;
        if (m_waveformCache.load(src, duration, &cached)) {
            if (cached.size() >= WaveformSlider::targetBinCount()) {
                applyWaveformPeaks(cached);
            } else {
                m_miniWaveform->setPartialPeaks(cached);
                m_seekSlider->loadWaveform(src, duration, cached);
            }
        } else {
            m_miniWaveform->clearWaveform();
            m_seekSlider->loadWaveform(src, duration);
        }
    }
    updateTimeDisplay(m_player->position(), duration);

    if (m_currentIndex >= 0 && m_currentIndex < m_playlistWidget->count()) {
        QListWidgetItem *it = m_playlistWidget->item(m_currentIndex);
        if (it) it->setData(Qt::UserRole + 1, formatTime(duration));
    }
}

void MainWindow::applyWaveformPeaks(const QVector<float> &peaks) {
    m_seekSlider->setPeaks(peaks);
    m_miniWaveform->setPeaks(peaks);
}

void MainWindow::resetWaveformUi() {
    m_seekSlider->clearWaveform();
    m_miniWaveform->clearWaveform();
    m_seekSlider->setValue(0);
    m_seekSlider->setRange(0, 0);
    m_miniWaveform->setValue(0);
    m_miniWaveform->setRange(0, 0);
    m_timeLabel->setText("0:00 / 0:00");
}

void MainWindow::onWaveformReady(const QUrl &url, qint64 duration,
                                 QVector<float> peaks) {
    m_waveformCache.rememberPartial(url, duration, peaks);
    if (m_player->source() == url) applyWaveformPeaks(peaks);
    QThreadPool::globalInstance()->start([url, duration, peaks] {
        WaveformCache diskCache;
        diskCache.save(url, duration, peaks);
    });
}

void MainWindow::onPositionChanged(qint64 position) {
    if (!m_seeking) {
        m_seekSlider->setValue(static_cast<int>(position));
        m_miniWaveform->setValue(static_cast<int>(position));
    }
    updateTimeDisplay(position, m_player->duration());

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
    const ThemePalette theme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
    const QColor iconC = theme.accent.lightness() > 160 ? theme.crust : QColor(0xff,0xff,0xff);
    m_playPauseBtn->setIcon(playing
        ? uiIcon("pause", iconC, 36, Ico::pause(iconC, 36))
        : uiIcon("play_arrow", iconC, 36, Ico::play(iconC, 36)));
    m_miniPlayBtn->setIcon(playing
        ? uiIcon("pause", iconC, 22, Ico::pause(iconC, 22))
        : uiIcon("play_arrow", iconC, 22, Ico::play(iconC, 22)));
    popButtonIcon(m_playPauseBtn);
    popButtonIcon(m_miniPlayBtn);
    m_visualizer->setActive(playing);
    if (m_trayPlayAct) {
        m_trayPlayAct->setText(playing ? "Пауза" : "Играть");
        m_trayPlayAct->setIcon(playing
            ? uiIcon("pause", theme.text, 16, Ico::pause(theme.text, 16))
            : uiIcon("play_arrow", theme.text, 16, Ico::play(theme.text, 16)));
    }
    if (m_discord && m_cfg.discordEnabled)
        m_discord->updateActivity(m_titleLabel->text(), m_artistLabel->text(), playing);
    if (!playing)
        saveTrackPosition();
}

void MainWindow::onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia) {
        saveTrackPosition();
        nextAuto();
    }
    if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) {
        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
            const QVariant saved = m_settings.value(positionKey(m_playlist[m_currentIndex]));
            if (saved.isValid()) {
                const qint64 pos = saved.toLongLong();
                if (pos > 0 && m_player->duration() > 0 && pos < m_player->duration() - 8000) {
                    playerSeek(pos);
                    statusBar()->showMessage(
                        QString("Продолжаем с %1").arg(formatTime(pos)), 4000);
                }
            }
        }
    }
}

static QPixmap applyRoundedCorners(const QPixmap &src, int sz, int radius);

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

    if (m_currentIndex >= 0 && m_currentIndex < m_playlistWidget->count()) {
        QListWidgetItem *it = m_playlistWidget->item(m_currentIndex);
        if (it) {
            if (!title.isEmpty())  it->setData(Qt::UserRole + 2, title);
            if (!artist.isEmpty()) it->setData(Qt::UserRole + 3, artist);
            if (!album.isEmpty())  it->setData(Qt::UserRole + 4, album);
        }
    }
    if (!m_searchEdit->text().trimmed().isEmpty())
        onSearchChanged(m_searchEdit->text());

    if (m_discord && m_cfg.discordEnabled && m_player->playbackState() == QMediaPlayer::PlayingState)
        m_discord->updateActivity(m_titleLabel->text(), m_artistLabel->text());

    QImage img = meta.value(QMediaMetaData::CoverArtImage).value<QImage>();
    if (img.isNull()) img = meta.value(QMediaMetaData::ThumbnailImage).value<QImage>();
    if (!img.isNull()) {
        m_coverPixmap = QPixmap::fromImage(img);
        m_albumArt->setPixmap(applyRoundedCorners(m_coverPixmap, 230, artRadius()));
        if (m_miniAlbumArt) m_miniAlbumArt->setPixmap(applyRoundedCorners(m_coverPixmap, 40, 6));
        const bool currentIsVideo = m_currentIndex >= 0
            && m_currentIndex < m_playlist.size()
            && isVideoFile(m_playlist[m_currentIndex]);
        if (!currentIsVideo)
            m_mediaStack->setCurrentWidget(m_albumArt);

        if (m_currentIndex >= 0 && m_currentIndex < m_playlist.size()) {
            const QUrl &url       = m_playlist[m_currentIndex];
            const QString icoFile = trackIconPath(url);
            if (!QFile::exists(icoFile)) {
                QDir().mkpath(QFileInfo(icoFile).absolutePath());
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

    if (m_aurora) {
        const float *data  = buffer.constData<float>();
        const int    total = buffer.frameCount() * buffer.format().channelCount();
        float rms = 0.f;
        for (int i = 0; i < total; ++i) rms += data[i] * data[i];
        if (total > 0) m_aurora->setAmplitude(std::sqrt(rms / total));
    }

    if (m_micRouting && m_apoRing)
        apoFeed(buffer);
}


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
    return 12;
}


void MainWindow::fadeInWidget(QWidget *w, int durationMs) {
    if (!w) return;
    auto *effect = new QGraphicsOpacityEffect(w);
    w->setGraphicsEffect(effect);
    auto *anim = new QPropertyAnimation(effect, "opacity", w);
    anim->setDuration(durationMs);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, w, [w]{ w->setGraphicsEffect(nullptr); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::fadeOutWidget(QWidget *w, int durationMs) {
    if (!w || !w->isVisible()) return;
    auto *effect = new QGraphicsOpacityEffect(w);
    w->setGraphicsEffect(effect);
    auto *anim = new QPropertyAnimation(effect, "opacity", w);
    anim->setDuration(durationMs);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, w, [w]{
        w->setGraphicsEffect(nullptr);
        w->setVisible(false);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::showLoadingBanner(const QString &text) {
    m_loadingText->setText(text);
    m_miniLoadingText->setText(text);
    m_loadingBar->setRange(0, 100);
    m_loadingBar->setValue(0);
    m_loadingPercent->clear();
    m_miniLoadingBar->setRange(0, 100);
    m_miniLoadingBar->setValue(0);
    m_miniLoadingPercent->clear();
    m_miniTitle->setVisible(false);
    m_miniWaveform->setVisible(false);
    m_miniLoadingPanel->setVisible(true);
    m_miniLoadingPanel->setToolTip(text);
    if (m_loadingAnim) { m_loadingAnim->stop(); m_loadingAnim = nullptr; }
    if (m_loadingBanner->isVisible()) return;

    m_loadingBanner->setVisible(true);
    auto *effect = new QGraphicsOpacityEffect(m_loadingBanner);
    m_loadingBanner->setGraphicsEffect(effect);
    auto *anim = new QPropertyAnimation(effect, "opacity", m_loadingBanner);
    anim->setDuration(220);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    m_loadingAnim = anim;
    connect(anim, &QPropertyAnimation::finished, this, [this, anim]{
        if (m_loadingAnim == anim) {
            m_loadingBanner->setGraphicsEffect(nullptr);
            m_loadingAnim = nullptr;
        }
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::updateLoadingText(const QString &text) {
    if (m_loadingBanner->isVisible()) m_loadingText->setText(text);
    m_miniLoadingText->setText(text);
    m_miniLoadingPanel->setToolTip(text);
}

void MainWindow::setLoadingProgress(int percent) {
    m_miniLoadingBar->setRange(0, 100);
    if (percent >= 0) {
        const int value = qBound(0, percent, 100);
        m_miniLoadingBar->setValue(value);
        m_miniLoadingPercent->setText(QString::number(value) + "%");
    } else {
        m_miniLoadingPercent->clear();
    }
    if (!m_loadingBanner->isVisible()) return;
    if (percent < 0) {
        m_loadingBar->setRange(0, 100);
        m_loadingBar->setValue(0);
        m_loadingPercent->clear();
    } else {
        const int value = qBound(0, percent, 100);
        m_loadingBar->setRange(0, 100);
        m_loadingBar->setValue(value);
        m_loadingPercent->setText(QString::number(value) + "%");
    }
}

void MainWindow::hideLoadingBanner() {
    m_miniLoadingPanel->setVisible(false);
    m_miniTitle->setVisible(true);
    m_miniWaveform->setVisible(true);
    if (!m_loadingBanner->isVisible()) return;
    if (m_loadingAnim) { m_loadingAnim->stop(); m_loadingAnim = nullptr; }

    auto *effect = new QGraphicsOpacityEffect(m_loadingBanner);
    m_loadingBanner->setGraphicsEffect(effect);
    auto *anim = new QPropertyAnimation(effect, "opacity", m_loadingBanner);
    anim->setDuration(220);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);
    m_loadingAnim = anim;
    connect(anim, &QPropertyAnimation::finished, this, [this, anim]{
        if (m_loadingAnim == anim) {
            m_loadingBanner->setGraphicsEffect(nullptr);
            m_loadingBanner->setVisible(false);
            m_loadingAnim = nullptr;
        }
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

static QString suggestFixForError(const QString &errorMsg) {
    const QString e = errorMsg.toLower();
    if (e.contains("dpapi") || e.contains("failed to decrypt")) {
        return "Похоже, дело в куки браузера. Попробуй: Настройки → Интеграции → "
               "«Ссылки на музыку» → выключи (или смени браузер на Firefox — он не "
               "использует новую схему шифрования Chrome, из-за которой это происходит).";
    }
    if (e.contains("sign in") || e.contains("login required") || e.contains("private video") ||
        e.contains("age-restricted") || e.contains("age restricted")) {
        return "Похоже, трек/видео требует входа в аккаунт на сайте-источнике. Попробуй "
               "указать браузер с активной сессией: Настройки → Интеграции → «Ссылки на музыку».";
    }
    if (e.contains("unsupported url") || e.contains("no extractor")) {
        return "yt-dlp не понимает эту ссылку. Проверь, что она открывается в браузере и "
               "ведёт на конкретный трек/видео, а не на плейлист/подборку целиком.";
    }
    if (e.contains("http error 429") || e.contains("too many requests")) {
        return "Сайт временно ограничил запросы с твоего адреса — подожди немного и попробуй снова.";
    }
    if (e.contains("unable to download webpage") || e.contains("failed to resolve") ||
        e.contains("connection") || e.contains("timed out") || e.contains("network")) {
        return "Похоже на проблему с интернет-соединением — проверь подключение (или VPN, "
               "если сайт заблокирован у провайдера) и попробуй снова.";
    }
    if (e.contains("video unavailable") || e.contains("this video is unavailable") ||
        e.contains("content isn't available") || e.contains("not available")) {
        return "Трек/видео недоступен на сайте-источнике: удалён, скрыт автором или "
               "заблокирован в твоём регионе.";
    }
    if (e.contains("antivirus") || e.contains("не найден или не может быть запущен")) {
        return "";
    }
    return QString();
}

void MainWindow::showCopyableError(const QString &title, const QString &message) {
    const QString hint = suggestFixForError(message);
    const QString fullText = hint.isEmpty() ? message : (message + "\n\n💡 " + hint);

    QMessageBox box(QMessageBox::Warning, title, fullText, QMessageBox::NoButton, this);
    box.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    QPushButton *copyBtn = box.addButton("Копировать", QMessageBox::ActionRole);
    box.addButton("Закрыть", QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == copyBtn) {
        QGuiApplication::clipboard()->setText(fullText);
        statusBar()->showMessage("Текст ошибки скопирован в буфер обмена", 3000);
    }
}

void MainWindow::popButtonIcon(QToolButton *btn) {
    if (!btn) return;

    auto *prevAnim = qobject_cast<QPropertyAnimation*>(
        btn->property("popAnim").value<QObject*>());
    const QSize normal = prevAnim ? prevAnim->endValue().toSize() : btn->iconSize();
    if (prevAnim) prevAnim->stop();

    auto *anim = new QPropertyAnimation(btn, "iconSize", btn);
    anim->setDuration(240);
    anim->setStartValue(normal * 0.7);
    anim->setEndValue(normal);
    anim->setEasingCurve(QEasingCurve::OutBack);
    btn->setProperty("popAnim", QVariant::fromValue<QObject*>(anim));
    connect(anim, &QPropertyAnimation::finished, btn, [btn]{
        btn->setProperty("popAnim", QVariant());
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::animateTrackHighlight(const QUrl &url) {
    const ThemePalette theme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
    auto *anim = new QVariantAnimation(this);
    anim->setDuration(320);
    anim->setStartValue(theme.text);
    anim->setEndValue(theme.accent);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this, [this, url](const QVariant &v) {
        const int idx = m_playlist.indexOf(url);
        if (idx < 0) return;
        QListWidgetItem *it = m_playlistWidget->item(idx);
        if (it) it->setForeground(v.value<QColor>());
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::updateAlbumArt() {
    const int r = artRadius();
    if (!m_coverPixmap.isNull()) {
        m_albumArt->setPixmap(applyRoundedCorners(m_coverPixmap, 230, r));
        if (m_miniAlbumArt) m_miniAlbumArt->setPixmap(applyRoundedCorners(m_coverPixmap, 40, 6));
        return;
    }
    const ThemePalette theme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
    auto placeholder = [&](int size, int radius) {
        QPixmap image(size, size);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF bounds(0.7, 0.7, size - 1.4, size - 1.4);
        QPainterPath shape;
        shape.addRoundedRect(bounds, radius, radius);

        QLinearGradient background(bounds.topLeft(), bounds.bottomRight());
        if (theme.id == "liquid") {
            background.setColorAt(0.0, QColor(228, 246, 255, 105));
            background.setColorAt(0.3, QColor(theme.accent.red(), theme.accent.green(),
                                               theme.accent.blue(), 82));
            background.setColorAt(1.0, QColor(8, 29, 53, 225));
        } else {
            background.setColorAt(0.0, theme.surface1);
            background.setColorAt(1.0, theme.mantle);
        }
        painter.setBrush(background);
        painter.setPen(theme.id == "liquid"
            ? QPen(QColor(238, 250, 255, 170), qMax(1.0, size * 0.014))
            : Qt::NoPen);
        painter.drawPath(shape);

        if (theme.id == "liquid") {
            const qreal barWidth = size * 0.062;
            const qreal gap = size * 0.036;
            const qreal heights[] = {0.24, 0.39, 0.54, 0.39, 0.24};
            const qreal totalWidth = 5.0 * barWidth + 4.0 * gap;
            const qreal startX = size * 0.5 - totalWidth * 0.5;
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(235, 250, 255, 240));
            for (int bar = 0; bar < 5; ++bar) {
                const qreal height = size * heights[bar];
                painter.drawRoundedRect(
                    QRectF(startX + bar * (barWidth + gap),
                           size * 0.5 - height * 0.5,
                           barWidth, height),
                    barWidth * 0.48, barWidth * 0.48);
            }
        } else {
            const int glyphSize = int(size * 0.42);
            const QColor noteColor = theme.accent.darker(128);
            Ico::music(noteColor, glyphSize).paint(
                &painter, QRect((size - glyphSize) / 2, (size - glyphSize) / 2,
                                glyphSize, glyphSize));
        }
        return image;
    };

    m_albumArt->setPixmap(placeholder(230, r));
    if (m_miniAlbumArt) m_miniAlbumArt->setPixmap(placeholder(40, 7));
}

void MainWindow::setCurrentTrackVisual(int index) {
    const ThemePalette theme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
    for (int i = 0; i < m_playlistWidget->count(); ++i) {
        QListWidgetItem *it = m_playlistWidget->item(i);
        QFont f = it->font(); f.setBold(i == index); it->setFont(f);
        if (i != index) it->setForeground(theme.text);
    }
    if (index >= 0 && index < m_playlist.size())
        animateTrackHighlight(m_playlist[index]);
    m_playlistWidget->scrollToItem(m_playlistWidget->item(index));
}

void MainWindow::updateTimeDisplay(qint64 pos, qint64 dur) {
    if (m_showRemaining && dur > 0) {
        m_timeLabel->setText(QString("-%1 / %2").arg(formatTime(dur - pos), formatTime(dur)));
    } else {
        m_timeLabel->setText(QString("%1 / %2").arg(formatTime(pos), formatTime(dur)));
    }
}


void MainWindow::showAbout() {
    QDialog dlg(this);
    dlg.setWindowTitle("О программе");
    dlg.setFixedWidth(460);
    dlg.setWindowFlags(dlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto *root = new QVBoxLayout(&dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QWidget(&dlg);
    header->setObjectName("aboutHeader");
    auto *headerL = new QVBoxLayout(header);
    headerL->setContentsMargins(0, 30, 0, 22);
    headerL->setSpacing(8);

    auto *logoLbl = new QLabel(header);
    logoLbl->setPixmap(createLogo(88, ThemeManager::palette("mocha"), m_cfg.appIconStyle));
    logoLbl->setFixedSize(88, 88);
    headerL->addWidget(logoLbl, 0, Qt::AlignHCenter);

    auto *nameLbl = new QLabel("EchoBox II", header);
    nameLbl->setObjectName("aboutName");
    nameLbl->setAlignment(Qt::AlignHCenter);
    headerL->addWidget(nameLbl);

    auto *verLbl = new QLabel("версия " + kAppVersion, header);
    verLbl->setObjectName("aboutVersion");
    verLbl->setAlignment(Qt::AlignCenter);
    headerL->addWidget(verLbl, 0, Qt::AlignHCenter);

    root->addWidget(header);

    auto *body = new QWidget(&dlg);
    auto *bodyL = new QVBoxLayout(body);
    bodyL->setContentsMargins(30, 22, 30, 18);
    bodyL->setSpacing(16);

    auto *tagline = new QLabel(
        "Современный медиаплеер на C++ / Qt6 — локальные файлы, ссылки "
        "на музыку, живой визуализатор и эквалайзер", body);
    tagline->setObjectName("aboutTagline");
    tagline->setWordWrap(true);
    tagline->setAlignment(Qt::AlignHCenter);
    bodyL->addWidget(tagline);

    const QColor featIconColor =
        ThemeManager::palette(m_cfg.theme, m_cfg.accentColor).accent;
    const struct { QIcon icon; QString text; } feats[] = {
        { uiIcon("link", featIconColor, 16, Ico::link(featIconColor, 16)), "Ссылки: SoundCloud, YouTube и другие" },
        { uiIcon("equalizer", featIconColor, 16, Ico::equalizer(featIconColor, 16)), "8-полосный графический эквалайзер" },
        { uiIcon("tune", featIconColor, 16, Ico::sliders(featIconColor, 16)), "Десятки настроек почти для всего" },
        { uiIcon("fullscreen", featIconColor, 16, Ico::windowIcon(featIconColor, 16)), "Мини-плеер, докинг к верху экрана" },
        { uiIcon("folder", featIconColor, 16, Ico::folder(featIconColor, 16)), "Сканер библиотеки, умный поиск" },
        { uiIcon("play_arrow", featIconColor, 16, Ico::play(featIconColor, 16)), "Кроссфейд, плейлисты, память позиции" },
    };
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(1, 1);
    for (int i = 0; i < int(sizeof(feats) / sizeof(feats[0])); ++i) {
        auto *iconLbl = new QLabel(body);
        iconLbl->setPixmap(feats[i].icon.pixmap(16, 16));
        iconLbl->setFixedWidth(20);
        auto *textLbl = new QLabel(feats[i].text, body);
        textLbl->setObjectName("aboutFeatureText");
        grid->addWidget(iconLbl, i, 0);
        grid->addWidget(textLbl, i, 1);
    }
    bodyL->addLayout(grid);

    auto *sep = new QFrame(body);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("aboutSep");
    bodyL->addWidget(sep);

    auto *keysRow = new QHBoxLayout;
    keysRow->setSpacing(6);
    const QStringList keyChips = {"Space — играть", "Ctrl+U — по ссылке", "F11 — мини-плеер"};
    for (const QString &chip : keyChips) {
        auto *lbl = new QLabel(chip, body);
        lbl->setObjectName("aboutKeyChip");
        keysRow->addWidget(lbl);
    }
    keysRow->addStretch();
    bodyL->addLayout(keysRow);

    auto *linkLbl = new QLabel(
        "<a href='https://github.com/BANANCHIKIREAL/EchoBox-II' "
        "style='color:#89b4fa;text-decoration:none;'>"
        "github.com/BANANCHIKIREAL/EchoBox-II</a>", body);
    linkLbl->setOpenExternalLinks(true);
    linkLbl->setAlignment(Qt::AlignHCenter);
    linkLbl->setObjectName("aboutLink");
    bodyL->addWidget(linkLbl);

    auto *licenseLbl = new QLabel("© 2026 BANANCHIKIREAL · MIT + Commons Clause", body);
    licenseLbl->setObjectName("aboutLicense");
    licenseLbl->setAlignment(Qt::AlignHCenter);
    bodyL->addWidget(licenseLbl);

    root->addWidget(body);

    auto *footer = new QWidget(&dlg);
    footer->setObjectName("aboutFooter");
    auto *footerL = new QHBoxLayout(footer);
    footerL->setContentsMargins(24, 14, 24, 14);
    auto *closeBtn = new QPushButton("Закрыть", footer);
    closeBtn->setObjectName("aboutCloseBtn");
    closeBtn->setFixedHeight(34);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    footerL->addStretch();
    footerL->addWidget(closeBtn);
    footerL->addStretch();
    root->addWidget(footer);

    dlg.setWindowOpacity(0.0);
    auto *fadeAnim = new QPropertyAnimation(&dlg, "windowOpacity", &dlg);
    fadeAnim->setDuration(320);
    fadeAnim->setStartValue(0.0);
    fadeAnim->setEndValue(1.0);
    fadeAnim->setEasingCurve(QEasingCurve::OutCubic);
    QTimer::singleShot(0, &dlg, [fadeAnim]{ fadeAnim->start(QAbstractAnimation::DeleteWhenStopped); });

    dlg.exec();
}


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

struct TrustedReleaseAsset {
    QString name;
    QString url;
    QString digest;
    qint64 size = -1;
};

static bool isSafeReleaseTag(const QString &tag) {
    static const QRegularExpression pattern(
        R"(^v?\d+\.\d+\.\d+(?:[-.][0-9A-Za-z.-]+)?$)");
    return pattern.match(tag).hasMatch();
}

static bool isValidSha256Digest(const QString &digest) {
    static const QRegularExpression pattern(R"(^sha256:[0-9a-fA-F]{64}$)");
    return pattern.match(digest).hasMatch();
}

static bool isTrustedReleaseAssetUrl(const TrustedReleaseAsset &asset,
                                     const QString &tag) {
    const QUrl url(asset.url);
    if (!url.isValid() || url.scheme().compare("https", Qt::CaseInsensitive) != 0 ||
        url.host().compare("github.com", Qt::CaseInsensitive) != 0 ||
        !isSafeReleaseTag(tag) || asset.name.isEmpty())
        return false;

    const QString expectedPath = QString(
        "/BANANCHIKIREAL/EchoBox-II/releases/download/%1/%2")
        .arg(tag, asset.name);
    return url.path() == expectedPath && asset.size > 0 &&
           isValidSha256Digest(asset.digest);
}

void MainWindow::checkForUpdates(bool manual) {
    QNetworkRequest req((QUrl(kUpdateApiUrl)));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2026-03-10");
    req.setRawHeader("User-Agent", "EchoBoxII-UpdateCheck");

    QNetworkReply *reply = m_streamArtNam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual]{
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (manual) statusBar()->showMessage("Не удалось проверить обновления", 5000);
            return;
        }

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

        const QString skipKey = "update/skippedVersion";
        if (!manual && m_settings.value(skipKey).toString() == tag) return;

        if (!isSafeReleaseTag(tag)) {
            if (manual)
                statusBar()->showMessage("GitHub вернул небезопасный номер версии", 6000);
            return;
        }

        QString version = tag;
        if (version.startsWith('v', Qt::CaseInsensitive)) version.remove(0, 1);
        const QString expectedInstallerName =
            QString("EchoBoxII-Setup-%1.exe").arg(version);
        const QString expectedZipName =
            QString("EchoBox-II-%1-portable.zip").arg(version);
        TrustedReleaseAsset installerAsset;
        TrustedReleaseAsset zipAsset;
        for (const QJsonValue &a : newest.value("assets").toArray()) {
            const QJsonObject ao = a.toObject();
            const QString name = ao.value("name").toString();
            TrustedReleaseAsset candidate;
            candidate.name = name;
            candidate.url = ao.value("browser_download_url").toString();
            candidate.digest = ao.value("digest").toString();
            candidate.size = qint64(ao.value("size").toDouble(-1));
            if (name == expectedInstallerName)
                installerAsset = candidate;
            else if (name == expectedZipName)
                zipAsset = candidate;
        }
        const QString appDir = QCoreApplication::applicationDirPath();
        const bool installedBuild = QFileInfo(appDir + "/unins000.exe").exists();
        TrustedReleaseAsset selectedAsset = installedBuild
            ? installerAsset
            : (!zipAsset.url.isEmpty() ? zipAsset : installerAsset);
        const bool immutableRelease = newest.value("immutable").toBool(false);
        const bool trustedAsset = immutableRelease &&
                                  isTrustedReleaseAssetUrl(selectedAsset, tag);
        if (!trustedAsset) selectedAsset = {};

        QDialog dialog(this);
        dialog.setObjectName("updateDialog");
        dialog.setWindowTitle("Доступно обновление");
        const ThemePalette updateTheme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
        dialog.setWindowIcon(QIcon(createLogo(64, ThemeManager::palette("mocha"), m_cfg.appIconStyle)));
        dialog.setModal(true);
        dialog.setFixedWidth(560);
        dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);

        auto *root = new QVBoxLayout(&dialog);
        root->setContentsMargins(28, 26, 28, 24);
        root->setSpacing(18);

        auto *header = new QHBoxLayout;
        header->setSpacing(16);
        auto *logo = new QLabel(&dialog);
        logo->setFixedSize(64, 64);
        logo->setPixmap(createLogo(64, ThemeManager::palette("mocha"), m_cfg.appIconStyle));
        header->addWidget(logo, 0, Qt::AlignTop);

        auto *heading = new QVBoxLayout;
        heading->setSpacing(4);
        auto *badge = new QLabel("  НОВАЯ ВЕРСИЯ  ", &dialog);
        badge->setObjectName("updateBadge");
        badge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        auto *titleLabel = new QLabel("Обновление EchoBox II", &dialog);
        titleLabel->setObjectName("updateTitle");
        auto *subtitleLabel = new QLabel(
            "Новая версия уже готова к установке", &dialog);
        subtitleLabel->setObjectName("updateSubtitle");
        heading->addWidget(badge, 0, Qt::AlignLeft);
        heading->addWidget(titleLabel);
        heading->addWidget(subtitleLabel);
        header->addLayout(heading, 1);
        root->addLayout(header);

        auto *versionCard = new QFrame(&dialog);
        versionCard->setObjectName("updateVersionCard");
        auto *versionLayout = new QHBoxLayout(versionCard);
        versionLayout->setContentsMargins(18, 13, 18, 13);
        auto *currentVersion = new QLabel(
            "Установлена  <b>" + kAppVersion.toHtmlEscaped() + "</b>", versionCard);
        currentVersion->setTextFormat(Qt::RichText);
        auto *arrow = new QLabel(QString::fromUtf8("→"), versionCard);
        arrow->setObjectName("updateArrow");
        auto *newVersion = new QLabel(
            "Доступна  <b>" + tag.toHtmlEscaped() + "</b>", versionCard);
        newVersion->setTextFormat(Qt::RichText);
        versionLayout->addWidget(currentVersion);
        versionLayout->addStretch();
        versionLayout->addWidget(arrow);
        versionLayout->addStretch();
        versionLayout->addWidget(newVersion);
        root->addWidget(versionCard);

        auto *securityCard = new QFrame(&dialog);
        securityCard->setObjectName(trustedAsset
            ? "updateSecurityCard" : "updateSecurityWarning");
        auto *securityLayout = new QHBoxLayout(securityCard);
        securityLayout->setContentsMargins(14, 10, 14, 10);
        securityLayout->setSpacing(10);
        auto *securityBadge = new QLabel(trustedAsset ? QString::fromUtf8("✓")
                                                       : QString::fromUtf8("!"), securityCard);
        securityBadge->setObjectName("updateSecurityBadge");
        securityBadge->setAlignment(Qt::AlignCenter);
        securityBadge->setFixedSize(24, 24);
        auto *securityText = new QLabel(securityCard);
        securityText->setObjectName("updateSecurityText");
        securityText->setWordWrap(true);
        securityText->setText(trustedAsset
            ? QString("Официальный неизменяемый релиз GitHub · размер %1 МБ\n"
                      "Перед запуском файл будет проверен по SHA-256.")
                  .arg(QString::number(selectedAsset.size / 1048576.0, 'f', 1))
            : QStringLiteral("Безопасная автоустановка недоступна: релиз изменяемый либо "
                             "GitHub не предоставил ожидаемый файл с SHA-256. Можно "
                             "открыть страницу релиза."));
        securityLayout->addWidget(securityBadge, 0, Qt::AlignTop);
        securityLayout->addWidget(securityText, 1);
        root->addWidget(securityCard);

        auto *notesTitle = new QLabel("Что изменилось", &dialog);
        notesTitle->setObjectName("updateNotesTitle");
        root->addWidget(notesTitle);

        auto *notesView = new QTextBrowser(&dialog);
        notesView->setObjectName("updateNotes");
        notesView->setOpenExternalLinks(true);
        notesView->setMinimumHeight(110);
        notesView->setMaximumHeight(190);
        notesView->document()->setDefaultStyleSheet(QString(
            "a { color: %1; text-decoration: none; } "
            "p { margin: 3px 0 8px 0; } li { margin-bottom: 4px; }")
                .arg(m_cfg.accentColor.name()));
        const QRegularExpression fullChangelogPattern(
            R"(\*\*Full Changelog\*\*:\s*(https?://\S+))",
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch changelogMatch =
            fullChangelogPattern.match(notes);
        const QString changelogUrl = changelogMatch.hasMatch()
            ? changelogMatch.captured(1)
            : QString();
        const QString changelogMarkdown = changelogUrl.isEmpty()
            ? QString()
            : QString("[Полный список изменений](%1)").arg(changelogUrl);

        QString detailProbe = notes;
        detailProbe.remove(fullChangelogPattern);
        const bool needsCommitList = detailProbe.trimmed().isEmpty();

        QString notesMarkdown = notes.left(4000);
        notesMarkdown.replace(fullChangelogPattern, changelogMarkdown);
        if (needsCommitList) {
            notesMarkdown = QStringLiteral("Загружаем список изменений…");
            if (!changelogMarkdown.isEmpty())
                notesMarkdown += "\n\n" + changelogMarkdown;
        } else if (notesMarkdown.trimmed().isEmpty()) {
            notesMarkdown = QStringLiteral("Подробное описание релиза не опубликовано.");
        }
        notesView->setMarkdown(notesMarkdown);
        root->addWidget(notesView);

        if (needsCommitList) {
            const QString localTag = kAppVersion.startsWith('v', Qt::CaseInsensitive)
                ? kAppVersion : "v" + kAppVersion;
            const QString compareApi = QString(
                "https://api.github.com/repos/BANANCHIKIREAL/EchoBox-II/compare/%1...%2")
                .arg(QString::fromLatin1(QUrl::toPercentEncoding(localTag)),
                     QString::fromLatin1(QUrl::toPercentEncoding(tag)));
            QNetworkRequest compareRequest{QUrl(compareApi)};
            compareRequest.setRawHeader("Accept", "application/vnd.github+json");
            compareRequest.setRawHeader("User-Agent", "EchoBoxII-UpdateCheck");
            QNetworkReply *compareReply = m_streamArtNam->get(compareRequest);
            const QPointer<QTextBrowser> notesGuard(notesView);
            connect(compareReply, &QNetworkReply::finished, this,
                    [compareReply, notesGuard, tag, changelogMarkdown] {
                compareReply->deleteLater();
                if (!notesGuard) return;

                QStringList changes;
                if (compareReply->error() == QNetworkReply::NoError) {
                    const QJsonArray commits = QJsonDocument::fromJson(
                        compareReply->readAll()).object().value("commits").toArray();
                    const int first = qMax(0, commits.size() - 16);
                    for (int i = first; i < commits.size(); ++i) {
                        const QString message = commits[i].toObject()
                            .value("commit").toObject().value("message").toString();
                        const QStringList lines = message.split('\n');
                        QString subject = lines.value(0).trimmed();
                        QString body = lines.mid(1).join(' ').simplified();
                        if (subject.startsWith("Release EchoBox II", Qt::CaseInsensitive)
                            && !body.isEmpty())
                            subject = body;
                        if (subject.isEmpty()) continue;
                        if (subject.size() > 260) subject = subject.left(257) + "…";
                        subject.replace('\\', "\\\\");
                        subject.replace('*', "\\*");
                        subject.replace('_', "\\_");
                        subject.replace('[', "\\[");
                        subject.replace(']', "\\]");
                        changes.append("- " + subject);
                    }
                }

                QString markdown;
                if (changes.isEmpty()) {
                    markdown = QStringLiteral(
                        "Подробный список изменений получить не удалось.");
                } else {
                    markdown = QString("### Изменения в %1\n\n%2")
                        .arg(tag.toHtmlEscaped(), changes.join('\n'));
                }
                if (!changelogMarkdown.isEmpty())
                    markdown += "\n\n" + changelogMarkdown;
                notesGuard->setMarkdown(markdown);
            });
        }

        if (!url.isEmpty()) {
            auto *releaseLink = new QLabel(
                QString("<a style=\"color:%1;text-decoration:none\" href=\"%2\">"
                        "Открыть страницу релиза ↗</a>")
                    .arg(m_cfg.accentColor.name(), url.toHtmlEscaped()), &dialog);
            releaseLink->setObjectName("updateReleaseLink");
            releaseLink->setTextFormat(Qt::RichText);
            releaseLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
            releaseLink->setOpenExternalLinks(true);
            root->addWidget(releaseLink);
        }

        auto *buttons = new QHBoxLayout;
        buttons->setSpacing(10);
        buttons->addStretch();
        auto *laterButton = new QPushButton(
            manual ? "Закрыть" : "Пропустить версию", &dialog);
        laterButton->setObjectName("updateSecondary");
        auto *installButton = new QPushButton(
            selectedAsset.url.isEmpty() ? "Открыть страницу" : "Скачать и проверить", &dialog);
        installButton->setObjectName("updatePrimary");
        installButton->setDefault(true);
        buttons->addWidget(laterButton);
        buttons->addWidget(installButton);
        root->addLayout(buttons);

        QString dialogStyle = R"(
            QDialog#updateDialog {
                background-color: #181825;
                border: 1px solid #45475a;
            }
            QLabel { color: #cdd6f4; }
            QLabel#updateBadge {
                color: #181825;
                background-color: ACCENT;
                border-radius: 8px;
                padding: 3px 7px;
                font-size: 10px;
                font-weight: 800;
            }
            QLabel#updateTitle { color: #f5e0dc; font-size: 22px; font-weight: 700; }
            QLabel#updateSubtitle { color: #a6adc8; font-size: 12px; }
            QFrame#updateVersionCard {
                background-color: #1e1e2e;
                border: 1px solid #313244;
                border-radius: 12px;
            }
            QLabel#updateArrow { color: ACCENT; font-size: 22px; font-weight: 700; }
            QFrame#updateSecurityCard {
                background-color: rgba(166, 227, 161, 22);
                border: 1px solid rgba(166, 227, 161, 100);
                border-radius: 10px;
            }
            QFrame#updateSecurityWarning {
                background-color: rgba(249, 226, 175, 18);
                border: 1px solid rgba(249, 226, 175, 90);
                border-radius: 10px;
            }
            QLabel#updateSecurityBadge {
                color: #181825;
                background-color: #a6e3a1;
                border-radius: 12px;
                font-weight: 800;
            }
            QFrame#updateSecurityWarning QLabel#updateSecurityBadge {
                background-color: #f9e2af;
            }
            QLabel#updateSecurityText { color: #bac2de; font-size: 11px; }
            QLabel#updateNotesTitle { color: #f5e0dc; font-size: 14px; font-weight: 700; }
            QTextBrowser#updateNotes {
                color: #cdd6f4;
                background-color: #11111b;
                border: 1px solid #313244;
                border-radius: 10px;
                padding: 10px 12px;
                selection-background-color: ACCENT;
            }
            QTextBrowser#updateNotes a, QLabel#updateReleaseLink a { color: ACCENT; }
            QLabel#updateReleaseLink { color: ACCENT; font-size: 11px; }
            QPushButton {
                min-height: 38px;
                padding: 0 18px;
                border-radius: 9px;
                font-weight: 600;
            }
            QPushButton#updateSecondary {
                color: #cdd6f4;
                background-color: #313244;
                border: 1px solid #45475a;
            }
            QPushButton#updateSecondary:hover { background-color: #45475a; }
            QPushButton#updatePrimary {
                color: #181825;
                background-color: ACCENT;
                border: 1px solid ACCENT;
            }
            QPushButton#updatePrimary:hover { background-color: ACCENT_HOVER; }
        )";
        ThemeManager::applyPaletteTokens(dialogStyle, updateTheme);
        dialogStyle.replace("ACCENT_HOVER", updateTheme.accent.lighter(112).name());
        dialogStyle.replace("ACCENT", updateTheme.accent.name());
        dialog.setStyleSheet(dialogStyle);

        connect(laterButton, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(installButton, &QPushButton::clicked, &dialog, &QDialog::accept);

        dialog.setWindowOpacity(0.0);
        auto *fade = new QPropertyAnimation(&dialog, "windowOpacity", &dialog);
        fade->setDuration(260);
        fade->setStartValue(0.0);
        fade->setEndValue(1.0);
        fade->setEasingCurve(QEasingCurve::OutCubic);
        QTimer::singleShot(0, &dialog, [fade]{ fade->start(QAbstractAnimation::DeleteWhenStopped); });

        if (dialog.exec() == QDialog::Accepted) {
            if (!selectedAsset.url.isEmpty())
                downloadAndInstallUpdate(selectedAsset.url, tag, selectedAsset.name,
                                         selectedAsset.digest, selectedAsset.size);
            else                     QDesktopServices::openUrl(QUrl(url));
        } else if (!manual) {
            m_settings.setValue(skipKey, tag);
        }
    });
}

void MainWindow::downloadAndInstallUpdate(const QString &assetUrl, const QString &tag,
                                          const QString &assetName,
                                          const QString &expectedDigest,
                                          qint64 expectedSize) {
    const TrustedReleaseAsset asset{assetName, assetUrl, expectedDigest, expectedSize};
    if (!isTrustedReleaseAssetUrl(asset, tag)) {
        showCopyableError("Обновление заблокировано",
            "Источник или контрольная сумма обновления не прошли проверку. "
            "Файл не был скачан и запущен.");
        return;
    }

    showLoadingBanner("Скачивание обновления " + tag + "…");
    setLoadingProgress(0);

    const bool installerAsset = assetName.endsWith(".exe", Qt::CaseInsensitive);
    QNetworkRequest request{QUrl(assetUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", "EchoBoxII-SecureUpdater");
    QNetworkReply *reply = m_streamArtNam->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this,
        [this, tag](qint64 received, qint64 total) {
            if (total > 0) {
                const int pct = int(received * 100 / total);
                setLoadingProgress(pct);
                updateLoadingText(QString("Скачивание обновления %1… %2%").arg(tag).arg(pct));
            }
        });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, tag, assetName, expectedDigest, expectedSize, installerAsset]{
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            hideLoadingBanner();
            statusBar()->showMessage("Не удалось скачать обновление", 6000);
            return;
        }
        const QByteArray data = reply->readAll();
        const QUrl finalUrl = reply->url();
        const QString finalHost = finalUrl.host().toLower();
        const bool trustedFinalLocation =
            finalUrl.scheme().compare("https", Qt::CaseInsensitive) == 0 &&
            (finalHost == "github.com" ||
             finalHost == "release-assets.githubusercontent.com");
        const QByteArray actualHash =
            QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
        const QByteArray expectedHash = expectedDigest.mid(7).toLatin1().toLower();
        if (!trustedFinalLocation || data.size() != expectedSize ||
            actualHash != expectedHash) {
            hideLoadingBanner();
            showCopyableError("Обновление заблокировано",
                QString("Проверка безопасности не пройдена. Файл не будет запущен.\n\n"
                        "Файл: %1\nОжидался SHA-256: %2\nПолучен SHA-256: %3")
                    .arg(assetName, QString::fromLatin1(expectedHash),
                         QString::fromLatin1(actualHash)));
            return;
        }
        updateLoadingText("SHA-256 подтверждён · подготовка установки " + tag + "…");

        const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                               + "/EchoBoxII_update";
        QDir(tempDir).removeRecursively();
        QDir().mkpath(tempDir);

        if (installerAsset) {
            const QString setupPath = tempDir + "/EchoBoxII-Setup.exe";
            QFile setup(setupPath);
            if (!setup.open(QIODevice::WriteOnly) || setup.write(data) != data.size()) {
                hideLoadingBanner();
                statusBar()->showMessage("Не удалось сохранить установщик обновления", 6000);
                return;
            }
            setup.close();

            hideLoadingBanner();
            QMessageBox::information(this, "Безопасное обновление",
                "Файл получен из официального релиза GitHub, размер и SHA-256 совпали. "
                "Сейчас запустится установщик EchoBox II.");
            saveSettings();
            if (!QProcess::startDetached(setupPath,
                    {"/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
                     "/CLOSEAPPLICATIONS"})) {
                hideLoadingBanner();
                statusBar()->showMessage("Не удалось запустить установщик обновления", 6000);
                return;
            }
            qApp->quit();
            return;
        }

        const QString zipPath     = tempDir + "/update.zip";
        const QString extractDir  = tempDir + "/extracted";

        QFile zf(zipPath);
        if (!zf.open(QIODevice::WriteOnly) || zf.write(data) < 0) {
            hideLoadingBanner();
            statusBar()->showMessage("Не удалось сохранить файл обновления", 6000);
            return;
        }
        zf.close();
        QDir().mkpath(extractDir);

        updateLoadingText("Распаковка обновления " + tag + "…");
        setLoadingProgress(-1);

        QProcess listProc;
        listProc.start("tar", {"-tf", QDir::toNativeSeparators(zipPath)});
        listProc.waitForFinished(30000);
        bool archivePathsSafe = listProc.exitStatus() == QProcess::NormalExit &&
                                listProc.exitCode() == 0;
        const QStringList archiveEntries = QString::fromUtf8(
            listProc.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
        for (QString entry : archiveEntries) {
            entry = entry.trimmed();
            entry.replace('\\', '/');
            const QString clean = QDir::cleanPath(entry);
            if (entry.startsWith('/') || entry.contains(':') || clean == ".." ||
                clean.startsWith("../")) {
                archivePathsSafe = false;
                break;
            }
        }
        if (!archivePathsSafe || archiveEntries.isEmpty()) {
            hideLoadingBanner();
            showCopyableError("Обновление заблокировано",
                "Архив содержит небезопасные пути или не может быть проверен.");
            return;
        }

        QProcess extractProc;
        extractProc.start("tar", {"-xf", QDir::toNativeSeparators(zipPath),
                                   "-C", QDir::toNativeSeparators(extractDir)});
        extractProc.waitForFinished(60000);
        if (extractProc.exitStatus() != QProcess::NormalExit ||
            extractProc.exitCode() != 0) {
            hideLoadingBanner();
            statusBar()->showMessage("Не удалось распаковать обновление", 6000);
            return;
        }

        const QString exeDir  = QCoreApplication::applicationDirPath();
        const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
        QString packageRoot = extractDir;
        if (!QFileInfo::exists(packageRoot + "/" + exeName)) {
            const QFileInfoList subdirs = QDir(packageRoot).entryInfoList(
                QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            if (!subdirs.isEmpty()) packageRoot = subdirs.constFirst().absoluteFilePath();
        }
        const QString packagedUpdater = packageRoot + "/EchoBoxUpdater.exe";
        const QString temporaryUpdater = tempDir + "/EchoBoxUpdater.exe";
        if (!QFileInfo::exists(packageRoot + "/" + exeName) ||
            !QFileInfo::exists(packagedUpdater)) {
            hideLoadingBanner();
            showCopyableError("Обновление заблокировано",
                "В проверенном архиве отсутствуют обязательные файлы EchoBox II.");
            return;
        }
        QFile::remove(temporaryUpdater);
        if (!QFile::copy(packagedUpdater, temporaryUpdater)) {
            hideLoadingBanner();
            statusBar()->showMessage("Не удалось подготовить безопасный updater", 6000);
            return;
        }

        hideLoadingBanner();
        QMessageBox::information(this, "Безопасное обновление",
            "Архив получен из официального релиза GitHub, размер и SHA-256 совпали. "
            "EchoBox II сейчас закроется, обновится и запустится снова.");
        saveSettings();
        if (!QProcess::startDetached(temporaryUpdater,
                {QString::number(QCoreApplication::applicationPid()), packageRoot,
                 exeDir, exeName})) {
            statusBar()->showMessage("Не удалось запустить безопасный updater", 6000);
            return;
        }
        qApp->quit();
    });
}


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


void MainWindow::keyPressEvent(QKeyEvent *e) {
    const bool ctrl  = e->modifiers() & Qt::ControlModifier;
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    switch (e->key()) {
    case Qt::Key_Space: togglePlayPause(); break;
    case Qt::Key_S:     stop();            break;
    case Qt::Key_M:     toggleMute();      break;
    case Qt::Key_F11:   toggleMiniPlayer(); break;
    case Qt::Key_Left: {
        const int stepMs = m_cfg.seekStepSecs * 1000;
        if (ctrl)  previous();
        else       playerSeek(m_player->position() - (shift ? stepMs * 6 : stepMs));
        break;
    }
    case Qt::Key_Right: {
        const int stepMs = m_cfg.seekStepSecs * 1000;
        if (ctrl)  next();
        else       playerSeek(m_player->position() + (shift ? stepMs * 6 : stepMs));
        break;
    }
    case Qt::Key_Up:
        m_volumeSlider->setValue(qMin(m_volumeSlider->value() + m_cfg.volumeStep, 100)); break;
    case Qt::Key_Down:
        m_volumeSlider->setValue(qMax(m_volumeSlider->value() - m_cfg.volumeStep, 0));   break;
    case Qt::Key_Delete:
        removeSelectedTracks(); break;
    case Qt::Key_F:
        if (ctrl) m_searchEdit->setFocus(); break;
    case Qt::Key_A:
        if (ctrl) m_playlistWidget->selectAll(); break;
    default: QMainWindow::keyPressEvent(e);
    }
}


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


void MainWindow::saveCurrentPlaylistState() {
    if (m_activePl < 0 || m_activePl >= m_playlists.size()) return;
    m_playlists[m_activePl].tracks       = m_playlist;
    m_playlists[m_activePl].currentTrack = m_currentIndex;
}

void MainWindow::loadPlaylistState(int index) {
    if (index < 0 || index >= m_playlists.size()) return;

    m_player->stop();
    stopEqEngine();
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
    if (!m_searchEdit->text().trimmed().isEmpty())
        onSearchChanged(m_searchEdit->text());
    resetWaveformUi();
    m_coverPixmap = QPixmap();
    updateAlbumArt();

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
    const int index = m_tabBar->addTab(name);
    m_tabBar->setTabIcon(index, playlistIcon(index, 18));
    m_tabBar->setCurrentIndex(m_tabBar->count() - 1);
    savePlaylistsToFile();
}

void MainWindow::onTabChanged(int index) {
    if (index == m_activePl || index < 0) return;
    showPlaylistTracks();
    saveCurrentPlaylistState();
    m_activePl = index;
    loadPlaylistState(index);
}

void MainWindow::onTabDoubleClicked(int index) {
    editPlaylistDetails(index);
}

void MainWindow::onTabContextMenu(const QPoint &pos) {
    const int index = m_tabBar->tabAt(pos);
    if (index < 0) return;
    QMenu menu(this);
    menu.addAction("Обложка и описание...", [this, index]{ editPlaylistDetails(index); });
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

    if (m_cfg.confirmDelete &&
        QMessageBox::question(this, "Удалить плейлист", msg,
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

QIcon MainWindow::playlistIcon(int index, int size) const {
    if (index < 0 || index >= m_playlists.size()) return {};

    const ThemePalette theme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
    if (size <= 24) {
        return m_cfg.theme == "liquid"
            ? MaterialIco::icon("library_music", theme.accent, size)
            : Ico::music(theme.accent, size);
    }

    QString sourcePath = m_playlists[index].iconPath;
    if (!QFile::exists(sourcePath) && !m_playlists[index].tracks.isEmpty())
        sourcePath = trackIconPath(m_playlists[index].tracks.first());

    QPixmap source(sourcePath);
    if (!source.isNull()) {
        const int side = qMin(source.width(), source.height());
        const QRect crop((source.width() - side) / 2, (source.height() - side) / 2,
                         side, side);
        return QIcon(source.copy(crop).scaled(size, size, Qt::IgnoreAspectRatio,
                                               Qt::SmoothTransformation));
    }

    QPixmap fallback(size, size);
    fallback.fill(Qt::transparent);
    QPainter painter(&fallback);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient gradient(0, 0, size, size);
    gradient.setColorAt(0, theme.surface2);
    gradient.setColorAt(1, theme.mantle);
    painter.setPen(QPen(theme.overlay0, 1));
    painter.setBrush(gradient);
    painter.drawRoundedRect(QRectF(0.5, 0.5, size - 1.0, size - 1.0),
                            size * 0.22, size * 0.22);
    const int glyphSize = qMax(14, int(size * 0.48));
    const QIcon glyph = m_cfg.theme == "liquid"
        ? MaterialIco::icon("library_music", theme.accent, glyphSize)
        : Ico::music(theme.accent, glyphSize);
    glyph.paint(&painter, QRect((size - glyphSize) / 2, (size - glyphSize) / 2,
                                glyphSize, glyphSize));
    return QIcon(fallback);
}

void MainWindow::editPlaylistDetails(int index) {
    if (index < 0 || index >= m_playlists.size()) return;

    QDialog dialog(this);
    dialog.setWindowTitle("Оформление плейлиста");
    dialog.setObjectName("playlistDetailsDialog");
    dialog.setMinimumWidth(440);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(12);

    auto *title = new QLabel("Оформление плейлиста", &dialog);
    title->setObjectName("playlistBrowserTitle");
    auto *subtitle = new QLabel("Название, короткое описание и своя обложка", &dialog);
    subtitle->setObjectName("playlistBrowserSubtitle");
    layout->addWidget(title);
    layout->addWidget(subtitle);

    auto *nameEdit = new QLineEdit(m_playlists[index].name, &dialog);
    nameEdit->setPlaceholderText("Название плейлиста");
    auto *descriptionEdit = new QTextEdit(&dialog);
    descriptionEdit->setPlaceholderText("Например: музыка для вечерних прогулок");
    descriptionEdit->setPlainText(m_playlists[index].description);
    descriptionEdit->setFixedHeight(82);
    layout->addWidget(new QLabel("Название", &dialog));
    layout->addWidget(nameEdit);
    layout->addWidget(new QLabel("Описание", &dialog));
    layout->addWidget(descriptionEdit);

    auto *coverRow = new QHBoxLayout;
    auto *preview = new QLabel(&dialog);
    preview->setObjectName("playlistCoverPreview");
    preview->setFixedSize(88, 88);
    preview->setAlignment(Qt::AlignCenter);
    preview->setPixmap(playlistIcon(index, 78).pixmap(78, 78));
    auto *coverButtons = new QVBoxLayout;
    auto *chooseCover = new QPushButton("Выбрать обложку...", &dialog);
    auto *clearCover = new QPushButton("Сбросить", &dialog);
    coverButtons->addWidget(chooseCover);
    coverButtons->addWidget(clearCover);
    coverButtons->addStretch();
    coverRow->addWidget(preview);
    coverRow->addLayout(coverButtons);
    coverRow->addStretch();
    layout->addLayout(coverRow);

    QString selectedCover = m_playlists[index].iconPath;
    connect(chooseCover, &QPushButton::clicked, &dialog, [&] {
        const QString path = QFileDialog::getOpenFileName(
            &dialog, "Выбрать обложку", QString(),
            "Изображения (*.png *.jpg *.jpeg *.webp *.bmp)");
        if (path.isEmpty()) return;
        QPixmap image(path);
        if (image.isNull()) return;
        selectedCover = path;
        const int side = qMin(image.width(), image.height());
        const QRect crop((image.width() - side) / 2, (image.height() - side) / 2,
                         side, side);
        preview->setPixmap(image.copy(crop).scaled(78, 78, Qt::IgnoreAspectRatio,
                                                    Qt::SmoothTransformation));
    });
    connect(clearCover, &QPushButton::clicked, &dialog, [&] {
        selectedCover.clear();
        const QString previous = m_playlists[index].iconPath;
        m_playlists[index].iconPath.clear();
        preview->setPixmap(playlistIcon(index, 78).pixmap(78, 78));
        m_playlists[index].iconPath = previous;
    });

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText("Сохранить");
    buttons->button(QDialogButtonBox::Cancel)->setText("Отмена");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted || nameEdit->text().trimmed().isEmpty()) return;

    QString storedCover;
    if (!selectedCover.isEmpty()) {
        QPixmap image(selectedCover);
        if (!image.isNull()) {
            const int side = qMin(image.width(), image.height());
            const QRect crop((image.width() - side) / 2, (image.height() - side) / 2,
                             side, side);
            const QPixmap normalized = image.copy(crop).scaled(
                512, 512, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            const QString directory = QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation) + "/playlist-icons";
            QDir().mkpath(directory);
            const QByteArray key = (selectedCover + QString::number(
                QFileInfo(selectedCover).lastModified().toMSecsSinceEpoch())).toUtf8();
            storedCover = directory + "/" + QCryptographicHash::hash(
                key, QCryptographicHash::Sha1).toHex() + ".png";
            if (!normalized.save(storedCover, "PNG")) storedCover.clear();
        }
    }

    m_playlists[index].name = nameEdit->text().trimmed();
    m_playlists[index].description = descriptionEdit->toPlainText().trimmed();
    m_playlists[index].iconPath = storedCover;
    m_tabBar->setTabText(index, m_playlists[index].name);
    m_tabBar->setTabIcon(index, playlistIcon(index, 18));
    savePlaylistsToFile();
}

void MainWindow::showPlaylistBrowser() {
    saveCurrentPlaylistState();

    if (!m_playlistBrowserPage) {
        m_playlistBrowserPage = new QWidget(m_playlistPanel);
        m_playlistBrowserPage->setObjectName("playlistBrowserPage");
        auto *layout = new QVBoxLayout(m_playlistBrowserPage);
        layout->setContentsMargins(8, 8, 8, 6);
        layout->setSpacing(10);

        auto *header = new QHBoxLayout;
        auto *headerText = new QVBoxLayout;
        auto *title = new QLabel("Мои плейлисты", m_playlistBrowserPage);
        title->setObjectName("playlistBrowserTitle");
        auto *subtitle = new QLabel(
            "Открой плейлист или настрой его обложку и описание",
            m_playlistBrowserPage);
        subtitle->setObjectName("playlistBrowserSubtitle");
        headerText->addWidget(title);
        headerText->addWidget(subtitle);
        auto *backButton = new QPushButton("К трекам", m_playlistBrowserPage);
        header->addLayout(headerText);
        header->addStretch();
        header->addWidget(backButton);
        layout->addLayout(header);

        m_playlistBrowserGrid = new QListWidget(m_playlistBrowserPage);
        m_playlistBrowserGrid->setObjectName("playlistBrowserGrid");
        m_playlistBrowserGrid->setViewMode(QListView::IconMode);
        m_playlistBrowserGrid->setFlow(QListView::LeftToRight);
        m_playlistBrowserGrid->setWrapping(true);
        m_playlistBrowserGrid->setResizeMode(QListView::Adjust);
        m_playlistBrowserGrid->setMovement(QListView::Static);
        m_playlistBrowserGrid->setSelectionMode(QAbstractItemView::SingleSelection);
        m_playlistBrowserGrid->setIconSize({112, 112});
        m_playlistBrowserGrid->setGridSize({178, 174});
        m_playlistBrowserGrid->setSpacing(4);
        layout->addWidget(m_playlistBrowserGrid, 1);

        auto *buttonRow = new QHBoxLayout;
        auto *newButton = new QPushButton("Новый плейлист", m_playlistBrowserPage);
        auto *editButton = new QPushButton("Обложка и описание", m_playlistBrowserPage);
        auto *openButton = new QPushButton("Открыть", m_playlistBrowserPage);
        openButton->setObjectName("playlistBrowserOpen");
        buttonRow->addWidget(newButton);
        buttonRow->addWidget(editButton);
        buttonRow->addStretch();
        buttonRow->addWidget(openButton);
        layout->addLayout(buttonRow);

        auto openSelected = [this] {
            if (!m_playlistBrowserGrid || !m_playlistBrowserGrid->currentItem()) return;
            const int index = m_playlistBrowserGrid->currentItem()
                                  ->data(Qt::UserRole).toInt();
            if (index < 0 || index >= m_playlists.size()) return;
            m_tabBar->setCurrentIndex(index);
            showPlaylistTracks();
        };
        connect(m_playlistBrowserGrid, &QListWidget::itemDoubleClicked,
                m_playlistBrowserPage, [openSelected](QListWidgetItem *) {
                    openSelected();
                });
        connect(openButton, &QPushButton::clicked,
                m_playlistBrowserPage, openSelected);
        connect(backButton, &QPushButton::clicked,
                this, &MainWindow::showPlaylistTracks);
        connect(newButton, &QPushButton::clicked, m_playlistBrowserPage, [this] {
            newPlaylist();
            showPlaylistBrowser();
        });
        connect(editButton, &QPushButton::clicked, m_playlistBrowserPage, [this] {
            if (!m_playlistBrowserGrid || !m_playlistBrowserGrid->currentItem()) return;
            editPlaylistDetails(m_playlistBrowserGrid->currentItem()
                                    ->data(Qt::UserRole).toInt());
            refreshPlaylistBrowser();
        });

        auto *panelLayout = qobject_cast<QVBoxLayout *>(m_playlistPanel->layout());
        if (panelLayout) panelLayout->addWidget(m_playlistBrowserPage, 1);
    }

    refreshPlaylistBrowser();
    m_searchEdit->hide();
    m_playlistInfo->hide();
    m_playlistUpBtn->hide();
    m_playlistDownBtn->hide();
    m_playlistRemoveBtn->hide();
    m_playlistClearBtn->hide();
    m_playlistWidget->hide();
    m_playlistBrowserPage->show();
    m_modernHomeBtn->setChecked(false);
}

void MainWindow::showPlaylistTracks() {
    if (m_playlistBrowserPage) m_playlistBrowserPage->hide();
    m_searchEdit->show();
    m_playlistInfo->show();
    m_playlistUpBtn->show();
    m_playlistDownBtn->show();
    m_playlistRemoveBtn->show();
    m_playlistClearBtn->show();
    m_playlistWidget->show();
    m_modernHomeBtn->setChecked(true);
}

void MainWindow::refreshPlaylistBrowser() {
    if (!m_playlistBrowserGrid) return;
    const int selected = m_playlistBrowserGrid->currentItem()
        ? m_playlistBrowserGrid->currentItem()->data(Qt::UserRole).toInt()
        : m_activePl;
    m_playlistBrowserGrid->clear();
    for (int i = 0; i < m_playlists.size(); ++i) {
        QString description = m_playlists[i].description.simplified();
        if (description.isEmpty()) description = "Без описания";
        if (description.size() > 24)
            description = description.left(23) + QString::fromUtf8("…");
        auto *item = new QListWidgetItem(
            playlistIcon(i, 112),
            QString("%1\n%2\n%3 треков")
                .arg(m_playlists[i].name, description)
                .arg(m_playlists[i].tracks.size()));
        item->setData(Qt::UserRole, i);
        item->setTextAlignment(Qt::AlignHCenter);
        item->setToolTip(m_playlists[i].description);
        m_playlistBrowserGrid->addItem(item);
        if (i == selected) m_playlistBrowserGrid->setCurrentItem(item);
    }
    if (!m_playlistBrowserGrid->currentItem() && m_playlistBrowserGrid->count() > 0)
        m_playlistBrowserGrid->setCurrentRow(0);
}

void MainWindow::showSearchOverlay() {
    if (m_cfg.theme != "liquid" || !m_cfg.modernLayout) {
        showPlaylistTracks();
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
        return;
    }

    showPlaylistTracks();
    if (!m_searchPopup) {
        m_searchDimmer = new QPushButton(m_contentShell);
        m_searchDimmer->setObjectName("searchDimmer");
        m_searchDimmer->setCursor(Qt::ArrowCursor);
        m_searchDimmer->setStyleSheet(
            "QPushButton#searchDimmer{background:rgba(2,8,18,168);border:none;border-radius:0;}"
            "QPushButton#searchDimmer:hover{background:rgba(2,8,18,168);}");
        connect(m_searchDimmer, &QPushButton::clicked,
                this, &MainWindow::hideSearchOverlay);

        m_searchPopup = new LiquidGlassWidget(m_contentShell);
        m_searchPopup->setObjectName("searchPopup");
        auto *popupLayout = new QHBoxLayout(m_searchPopup);
        popupLayout->setContentsMargins(18, 12, 18, 12);
        popupLayout->setSpacing(10);
        auto *searchIcon = new QLabel(m_searchPopup);
        searchIcon->setPixmap(MaterialIco::icon(
            "search", ThemeManager::palette("liquid").accent, 24).pixmap(24, 24));
        searchIcon->setFixedSize(24, 24);
        m_searchPopupEdit = new QLineEdit(m_searchPopup);
        m_searchPopupEdit->setObjectName("searchPopupEdit");
        m_searchPopupEdit->setPlaceholderText(
            "Найти по названию, исполнителю или альбому");
        m_searchPopupEdit->setClearButtonEnabled(true);
        m_searchPopupEdit->installEventFilter(this);
        popupLayout->addWidget(searchIcon);
        popupLayout->addWidget(m_searchPopupEdit, 1);
        connect(m_searchPopupEdit, &QLineEdit::textChanged,
                m_searchEdit, &QLineEdit::setText);
        connect(m_searchEdit, &QLineEdit::textChanged,
                m_searchPopupEdit, &QLineEdit::setText);
        connect(m_searchPopupEdit, &QLineEdit::returnPressed,
                this, &MainWindow::hideSearchOverlay);
    }

    const ThemePalette theme = ThemeManager::palette(m_cfg.theme, m_cfg.accentColor);
    if (auto *glass = dynamic_cast<LiquidGlassWidget *>(m_searchPopup))
        glass->setGlassEnabled(true, theme.accent);

    m_searchDimmer->setGeometry(m_contentShell->rect());
    m_searchDimmer->show();
    m_searchDimmer->raise();

    const QPoint buttonPos = m_modernSearchBtn->mapTo(m_contentShell, QPoint(0, 0));
    const QRect startRect(buttonPos.x(), buttonPos.y(),
                          qMax(120, m_modernSearchBtn->width()),
                          m_modernSearchBtn->height());
    const int targetWidth = qMin(680, qMax(360, m_contentShell->width() - 100));
    const QRect targetRect((m_contentShell->width() - targetWidth) / 2,
                           qMax(72, m_contentShell->height() / 7),
                           targetWidth, 68);
    m_searchPopup->setGeometry(startRect);
    m_searchPopup->show();
    m_searchPopup->raise();

    auto *geometryAnimation = new QPropertyAnimation(m_searchPopup, "geometry", this);
    geometryAnimation->setDuration(300);
    geometryAnimation->setStartValue(startRect);
    geometryAnimation->setEndValue(targetRect);
    geometryAnimation->setEasingCurve(QEasingCurve::OutCubic);
    geometryAnimation->start(QAbstractAnimation::DeleteWhenStopped);

    auto *dimmerEffect = new QGraphicsOpacityEffect(m_searchDimmer);
    m_searchDimmer->setGraphicsEffect(dimmerEffect);
    auto *fade = new QPropertyAnimation(dimmerEffect, "opacity", this);
    fade->setDuration(220);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);

    m_searchPopupEdit->setText(m_searchEdit->text());
    m_searchPopupEdit->setFocus();
    m_searchPopupEdit->selectAll();
}

void MainWindow::hideSearchOverlay() {
    if (!m_searchPopup || !m_searchPopup->isVisible()) return;
    auto *fade = new QPropertyAnimation(m_searchPopup, "windowOpacity", this);
    fade->setDuration(150);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    fade->setEasingCurve(QEasingCurve::InCubic);
    connect(fade, &QPropertyAnimation::finished, this, [this] {
        m_searchPopup->hide();
        m_searchPopup->setWindowOpacity(1.0);
        m_searchDimmer->hide();
        m_searchEdit->setFocus();
    });
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}


void MainWindow::applyVolume() {
    const float vol = m_volumeSlider->value() / 100.0f * m_fadeFactor;
    if (m_eqActive) {
        m_audioOutput->setVolume(0.0f);
        m_eqEngine->setVolume(vol);
    } else {
        m_audioOutput->setVolume(vol);
    }
}

void MainWindow::playerSeek(qint64 ms) {
    m_player->setPosition(ms);
    if (m_eqActive) m_eqEngine->setPosition(ms);
}

void MainWindow::stopEqEngine() {
    m_eqPending = false;
    m_eqActive = false;
    m_eqSource = QUrl();
    m_eqEngine->stop();
    applyVolume();
}

void MainWindow::syncEqEngineToCurrentTrack() {
    const QUrl src = m_player->source();
    const bool wantEq = m_cfg.eqEnabled && !src.isEmpty() &&
                         src.isLocalFile();
    if (!wantEq) {
        const bool wasRunning = m_eqActive || m_eqPending;
        if (m_eqActive || m_eqPending) stopEqEngine();
        if (m_cfg.eqEnabled && !src.isEmpty()) {
            if (wasRunning)
                statusBar()->showMessage("Эквалайзер недоступен для этого трека", 3500);
        } else {
            if (wasRunning && !m_cfg.eqEnabled)
                statusBar()->showMessage("Эквалайзер выключен", 2000);
        }
        return;
    }

    if (m_eqSource == src && (m_eqActive || m_eqPending)) return;

    stopEqEngine();
    m_eqSource = src;
    m_eqPending = true;
    statusBar()->showMessage("Эквалайзер включается — подготовка аудио…");
    m_eqEngine->setSource(src);
}


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


void MainWindow::scheduleScan(const QList<QUrl> &urls) {
    for (const QUrl &u : urls)
        if (!m_metaScanQueue.contains(u))
            m_metaScanQueue.append(u);
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

    const QString icoFile = trackIconPath(url);
    if (!QFile::exists(icoFile)) {
        QImage img = meta.value(QMediaMetaData::CoverArtImage).value<QImage>();
        if (img.isNull()) img = meta.value(QMediaMetaData::ThumbnailImage).value<QImage>();
        if (!img.isNull()) {
            QDir().mkpath(QFileInfo(icoFile).absolutePath());
            const QPixmap full = QPixmap::fromImage(img);
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
        obj["description"]  = pl.description;
        obj["iconPath"]     = pl.iconPath;
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
        entry.description  = obj["description"].toString();
        entry.iconPath     = obj["iconPath"].toString();
        for (const QJsonValue &t : obj["tracks"].toArray())
            entry.tracks.append(QUrl(t.toString()));
        m_playlists.append(entry);
        const int index = m_tabBar->addTab(entry.name);
        m_tabBar->setTabIcon(index, playlistIcon(index, 18));
    }

    m_activePl = qBound(0, root["activePl"].toInt(0), m_playlists.size() - 1);
    m_tabBar->setCurrentIndex(m_activePl);
    m_tabBar->blockSignals(false);
    loadPlaylistState(m_activePl);
}

void MainWindow::saveStreamTracksToFile() {
    const QString dir = m_cfg.playlistsFolder.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : m_cfg.playlistsFolder;
    QDir().mkpath(dir);

    QJsonObject root;
    for (auto it = m_streamTracks.constBegin(); it != m_streamTracks.constEnd(); ++it) {
        QJsonObject obj;
        obj["title"]        = it->title;
        obj["artist"]       = it->artist;
        obj["thumbnailUrl"] = it->thumbnailUrl;
        obj["localPath"]    = it->localPath;
        obj["isDirectUrl"]  = it->isDirectUrl;
        root[it.key().toString()] = obj;
    }

    QFile f(dir + "/streamtracks.json");
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(root).toJson());
}

void MainWindow::loadStreamTracksFromFile() {
    const QString dir = m_cfg.playlistsFolder.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : m_cfg.playlistsFolder;
    QFile f(dir + "/streamtracks.json");
    if (!f.open(QIODevice::ReadOnly)) return;

    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        const QJsonObject obj = it.value().toObject();
        StreamTrackInfo info;
        info.title        = obj["title"].toString();
        info.artist        = obj["artist"].toString();
        info.thumbnailUrl  = obj["thumbnailUrl"].toString();
        info.localPath      = obj["localPath"].toString();
        info.isDirectUrl    = obj["isDirectUrl"].toBool();
        m_streamTracks[QUrl(it.key())] = info;
    }
}


void MainWindow::openSettings() {
    const AppSettings savedCfg = m_cfg;

    auto applyEqSettings = [this]{
        m_eqEngine->setEqEnabled(m_cfg.eqEnabled);
        for (int i = 0; i < kEqBandCount; ++i)
            m_eqEngine->setEqBandGain(i, m_cfg.eqBands[i]);
        syncEqEngineToCurrentTrack();
    };

    SettingsDialog dlg(m_cfg, this);
    connect(&dlg, &SettingsDialog::clearStreamCacheRequested, this, [this, &dlg] {
        const QString cacheDir = streamCacheDir();
        const QString normalizedCache = QDir::cleanPath(cacheDir) + "/";
        const QString currentPath = m_player->source().isLocalFile()
            ? QDir::cleanPath(m_player->source().toLocalFile()) : QString();
        const bool currentUsesCache = !currentPath.isEmpty()
            && currentPath.startsWith(normalizedCache, Qt::CaseInsensitive);

        if (currentUsesCache) {
            m_player->stop();
            stopEqEngine();
            m_player->setSource(QUrl());
            resetWaveformUi();
        }

        QPointer<SettingsDialog> dialog(&dlg);
        QTimer::singleShot(currentUsesCache ? 300 : 0, this,
                           [this, dialog, cacheDir, normalizedCache, currentUsesCache] {
            QDir cache(cacheDir);
            const bool removed = !cache.exists() || cache.removeRecursively();
            const bool recreated = QDir().mkpath(cacheDir);
            const bool success = removed && recreated;

            if (success) {
                for (auto it = m_streamTracks.begin(); it != m_streamTracks.end(); ++it) {
                    const QString path = QDir::cleanPath(it->localPath);
                    if (!path.isEmpty() && path.startsWith(normalizedCache, Qt::CaseInsensitive))
                        it->localPath.clear();
                }
                saveStreamTracksToFile();
            }

            if (!dialog) return;
            if (success) {
                dialog->finishCacheClear(true, currentUsesCache
                    ? "Кэш удалён. Трек, использовавший файл из кэша, остановлен."
                    : "Все скачанные по ссылкам файлы удалены.");
            } else {
                dialog->finishCacheClear(false,
                    "Некоторые файлы остались заняты другим процессом. Закрой другие "
                    "экземпляры EchoBox и попробуй ещё раз.");
            }
        });
    });
    connect(&dlg, &SettingsDialog::applied, this, [this, applyEqSettings](const AppSettings &s) {
        const AppSettings prev = m_cfg;
        const bool appIconChanged = s.appIconStyle != prev.appIconStyle;
        const bool appearanceChanged = s.theme != prev.theme
            || s.accentColor != prev.accentColor
            || s.fontSizeIdx != prev.fontSizeIdx
            || s.fontFamily != prev.fontFamily
            || s.fontFilePath != prev.fontFilePath
            || s.artShape != prev.artShape
            || s.appIconStyle != prev.appIconStyle
            || s.showVisualizer != prev.showVisualizer
            || s.showStatusBar != prev.showStatusBar
            || s.modernLayout != prev.modernLayout;
        m_cfg = s;
        if (appearanceChanged) applyTheme();
        if (appIconChanged) syncShellShortcutIcon();
        applyEqSettings();
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

    if (dlg.exec() == QDialog::Accepted) {
        const AppSettings prev = m_cfg;
        m_cfg = dlg.result();
        applyTheme();
        if (m_cfg.appIconStyle != prev.appIconStyle)
            syncShellShortcutIcon();
        applyEqSettings();
        applyIconsIfNeeded(prev);
        if (!m_cfg.discordEnabled && m_discord) m_discord->clearActivity();
        if (m_cfg.launchOnStartup != savedCfg.launchOnStartup)
            setLaunchOnStartup(m_cfg.launchOnStartup);
        saveSettings();
        statusBar()->showMessage("Настройки сохранены", 2000);
    } else {
        const AppSettings prev = m_cfg;
        m_cfg = savedCfg;
        applyTheme();
        if (m_cfg.appIconStyle != prev.appIconStyle)
            syncShellShortcutIcon();
        applyEqSettings();
        applyIconsIfNeeded(prev);
    }
}


void MainWindow::scanLibrary()
{
    const QString folder = m_cfg.libraryFolder;
    if (folder.isEmpty() || !QDir(folder).exists()) {
        statusBar()->showMessage(
            "Папка библиотеки не задана. Укажи её в Настройки → Файлы.", 5000);
        return;
    }

    if (m_libraryThread && m_libraryThread->isRunning()) {
        if (m_libraryScanner) m_libraryScanner->cancel();
        m_libraryThread->quit();
        m_libraryThread->wait(2000);
    }

    if (m_libraryPlIdx < 0 || m_libraryPlIdx >= m_playlists.size()) {
        saveCurrentPlaylistState();
    const QString name = "Библиотека";
        m_playlists.append({name, {}, -1});
        const int index = m_tabBar->addTab(name);
        m_tabBar->setTabIcon(index, playlistIcon(index, 18));
        m_libraryPlIdx = m_playlists.size() - 1;
        m_tabBar->setCurrentIndex(m_libraryPlIdx);
    } else {
        saveCurrentPlaylistState();
        m_playlists[m_libraryPlIdx].tracks.clear();
        m_playlists[m_libraryPlIdx].currentTrack = -1;
        m_tabBar->setCurrentIndex(m_libraryPlIdx);
        loadPlaylistState(m_libraryPlIdx);
    }

    statusBar()->showMessage("Сканирование библиотеки...");

    if (!m_libraryWatcher) {
        m_libraryWatcher = new QFileSystemWatcher(this);
        connect(m_libraryWatcher, &QFileSystemWatcher::directoryChanged,
                this, &MainWindow::onLibraryDirChanged);
    }
    m_libraryWatcher->removePaths(m_libraryWatcher->directories());
    m_libraryWatcher->addPath(folder);

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

    const bool isActive = (m_activePl == m_libraryPlIdx);

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

    if (m_activePl == m_libraryPlIdx)
        updatePlaylistInfo();
}

void MainWindow::onLibraryDirChanged(const QString &/*path*/)
{
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
        if (!path.isEmpty()) pathCount[path]++;
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

    if (!m_apoOpenTimer) {
        m_apoOpenTimer = new QTimer(this);
        m_apoOpenTimer->setInterval(500);
        connect(m_apoOpenTimer, &QTimer::timeout, this, &MainWindow::apoTryOpenRing);
    }
    apoTryOpenRing();
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
    if (!h) return;

    auto *ring = static_cast<EchoBoxRing *>(
        MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (!ring) { CloseHandle(h); return; }

    if (ring->magic != kEchoBoxRingMagic) { UnmapViewOfFile(ring); CloseHandle(h); return; }

    ring->sampleRate = 48000;
    m_apoWritePos    = ring->writePos;

    m_apoMapping = h;
    m_apoRing    = ring;
    apoPushControls();
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
    ring->writePos  = wp;
    ring->playerAlive = 1;
}
