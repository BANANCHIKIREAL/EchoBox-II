#pragma once
#include <QMainWindow>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QAudioBuffer>
#include <QFrame>
#include <QUrl>
#include <QList>
#include <QVector>
#include <QSettings>
#include <QStringList>
#include <QtGlobal>
#include <QTabBar>
#include <QTimer>
#include <QHash>
#include <QPixmap>
#include <QColor>
#include <QCryptographicHash>
#include <QAudioDevice>
#include <QSlider>
#include <QThread>
#include <QFileSystemWatcher>
#include "settingsdialog.h"
#include "waveformslider.h"
#include "backgroundwidget.h"
#include "libraryscanner.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <QAudioBufferOutput>
#endif

class QLabel;
class QToolButton;
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QComboBox;
class QSystemTrayIcon;
class QMenu;
class QAction;
class QActionGroup;
class QStackedWidget;
class QVideoWidget;
class Visualizer;
class DiscordRPC;

enum class RepeatMode { Off, One, All };

struct PlaylistEntry {
    QString      name;
    QList<QUrl>  tracks;
    int          currentTrack = -1;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void closeEvent(QCloseEvent *) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    // File
    void openFiles();
    void openFolder();
    void savePlaylist();
    void loadPlaylist();
    // Playlist
    void clearPlaylist();
    void removeSelectedTracks();
    void moveTrackUp();
    void moveTrackDown();
    void onSearchChanged(const QString &text);
    void onTrackActivated(QListWidgetItem *item);
    void onPlaylistContextMenu(const QPoint &pos);
    void rebuildPlaylistFromWidget();
    // Transport
    void togglePlayPause();
    void stop();
    void previous();
    void next();
    void nextAuto(); // called by EndOfMedia — respects repeat mode
    // Audio
    void setVolume(int v);
    void toggleMute();
    void onSpeedChanged(int index);
    // Modes
    void toggleShuffle();
    void cycleRepeat();
    void toggleMiniPlayer();
    void toggleMicRouting();
    void micTimerTick();
    void toggleAlwaysOnTop();
    void toggleRemainingTime();
    // Player signals
    void onDurationChanged(qint64 ms);
    void onPositionChanged(qint64 ms);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onMetaDataChanged();
    void onError(QMediaPlayer::Error error, const QString &msg);
    void onAudioBuffer(const QAudioBuffer &buffer);
    // Playlists
    void newPlaylist();
    void onTabChanged(int index);
    void onTabDoubleClicked(int index);
    void onTabContextMenu(const QPoint &pos);
    // Other
    void showAbout();
    void openRecentFile(const QString &path);
    void openSettings();
    // Library scanner
    void scanLibrary();
    void onLibraryBatch(QList<QUrl> batch);
    void onLibraryProgress(int found, int scanned);
    void onLibraryFinished(int total);
    void onLibraryDirChanged(const QString &path);

private:
    void setupMenuBar();
    void setupUi();
    void setupTray();
    void setupConnections();
    void applyTheme();
    void loadSettings();
    void saveSettings();

    void addFiles(const QList<QUrl> &urls);
    void addFolder(const QString &dir);
    void playTrack(int index);
    void playNext();
    void playNext(bool respectRepeat);
    void saveCurrentPlaylistState();
    void loadPlaylistState(int index);
    void deletePlaylist(int index);

    // Feature 8 — position memory
    QString positionKey(const QUrl &url) const;
    void    saveTrackPosition();

    // Feature 13 — crossfade
    void applyVolume();  // applies m_lastVolume * m_fadeFactor to audio output

    void updateRepeatButton();
    void updateAlbumArt();
    int  artRadius() const;
    void updatePlaylistInfo();
    void updateTimeDisplay(qint64 pos, qint64 dur);
    void setCurrentTrackVisual(int index);
    void addRecentFile(const QString &path);
    void refreshRecentMenu();
    bool isVideoFile(const QUrl &url) const;
    QString formatTime(qint64 ms);

    void savePlaylistsToFile();
    void loadPlaylistsFromFile();
    QString trackIconPath(const QUrl &url) const;
    void applyTrackIcon(QListWidgetItem *item, const QUrl &url);
    void reloadTrackIcons();
    void updateDuplicateHighlights();
    void scheduleScan(const QList<QUrl> &urls);
    void advanceMetaScan();
    void handleMetaReaderUpdate();

    // ── Player ───────────────────────────────────────────────────────────────
    QMediaPlayer   *m_player      = nullptr;
    QAudioOutput   *m_audioOutput = nullptr;
    QMediaPlayer   *m_metaReader  = nullptr;  // metadata-only, no audio output
    QList<QUrl>     m_metaScanQueue;
    bool            m_scanInProgress = false;

    // ── State ────────────────────────────────────────────────────────────────
    QList<QUrl>    m_playlist;
    int            m_currentIndex  = -1;
    bool           m_shuffle       = false;
    RepeatMode     m_repeat        = RepeatMode::Off;
    bool           m_showRemaining = false;
    bool           m_muted         = false;
    int            m_lastVolume    = 70;
    bool           m_seeking       = false;
    bool           m_miniPlayer    = false;

    // ── Layout helpers ───────────────────────────────────────────────────────
    AuroraWidget   *m_aurora      = nullptr;
    QFrame         *m_separator   = nullptr;

    // ── Media panel ──────────────────────────────────────────────────────────
    QWidget        *m_topWidget   = nullptr;
    QStackedWidget *m_mediaStack  = nullptr;
    QLabel         *m_albumArt   = nullptr;
    QVideoWidget   *m_videoWidget = nullptr;

    // ── Track info ───────────────────────────────────────────────────────────
    QLabel  *m_titleLabel  = nullptr;
    QLabel  *m_artistLabel = nullptr;
    QLabel  *m_albumLabel  = nullptr;

    // ── Visualizer ───────────────────────────────────────────────────────────
    Visualizer *m_visualizer  = nullptr;
    QPixmap     m_coverPixmap;          // raw cover from metadata (unscaled)

    // ── Seek ─────────────────────────────────────────────────────────────────
    WaveformSlider *m_seekSlider = nullptr;
    QLabel         *m_timeLabel  = nullptr;

    // ── Transport controls ───────────────────────────────────────────────────
    QToolButton *m_prevBtn      = nullptr;
    QToolButton *m_playPauseBtn = nullptr;
    QToolButton *m_stopBtn      = nullptr;
    QToolButton *m_nextBtn      = nullptr;
    QToolButton *m_shuffleBtn   = nullptr;
    QToolButton *m_repeatBtn    = nullptr;
    QComboBox   *m_speedCombo   = nullptr;

    // ── Volume ───────────────────────────────────────────────────────────────
    QToolButton *m_muteBtn      = nullptr;
    QSlider     *m_volumeSlider = nullptr;
    QLabel      *m_volumeLabel  = nullptr;

    // ── Mini player ──────────────────────────────────────────────────────────
    QWidget     *m_miniBar        = nullptr;
    QToolButton *m_miniPlayBtn    = nullptr;
    QLabel      *m_miniTitle      = nullptr;
    QLabel      *m_miniAlbumArt   = nullptr;
    WaveformSlider *m_miniWaveform  = nullptr;
    QToolButton *m_miniShuffleBtn = nullptr;
    QToolButton *m_miniRepeatBtn  = nullptr;
    QSlider     *m_miniVolSlider  = nullptr;
    QToolButton *m_miniMuteBtn    = nullptr;

    // ── Playlist panel ───────────────────────────────────────────────────────
    QWidget     *m_playlistPanel  = nullptr;
    QTabBar     *m_tabBar         = nullptr;
    QLineEdit   *m_searchEdit     = nullptr;
    QLabel      *m_playlistInfo   = nullptr;
    QListWidget *m_playlistWidget = nullptr;

    // ── Multi-playlist data ───────────────────────────────────────────────────
    QVector<PlaylistEntry> m_playlists;
    int                    m_activePl = 0;

    // ── Menu actions ─────────────────────────────────────────────────────────
    QMenu        *m_recentMenu     = nullptr;
    QAction      *m_shuffleAct     = nullptr;
    QAction      *m_alwaysOnTopAct = nullptr;
    QAction      *m_miniPlayerAct  = nullptr;
    QAction      *m_repeatOffAct   = nullptr;
    QAction      *m_repeatOneAct   = nullptr;
    QAction      *m_repeatAllAct   = nullptr;

    // ── Tray ─────────────────────────────────────────────────────────────────
    QSystemTrayIcon *m_tray        = nullptr;
    QAction         *m_trayPlayAct = nullptr;

    // ── Audio analysis (Qt 6.6+) ─────────────────────────────────────────────
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    QAudioBufferOutput *m_audioBufferOut = nullptr;
#endif

    // ── Discord RPC ──────────────────────────────────────────────────────────
    DiscordRPC *m_discord = nullptr;

    // ── Library scanner ──────────────────────────────────────────────────────
    LibraryScanner    *m_libraryScanner = nullptr;
    QThread           *m_libraryThread  = nullptr;
    QFileSystemWatcher *m_libraryWatcher = nullptr;
    int                m_libraryPlIdx   = -1;   // index of "Библиотека" tab

    // ── Mic routing ──────────────────────────────────────────────────────────
    class QAudioSink   *m_micSink    = nullptr;
    class QAudioSource *m_micSource  = nullptr;
    QIODevice          *m_micOutput  = nullptr;
    QIODevice          *m_micCapture = nullptr;
    QTimer             *m_micTimer   = nullptr;
    QByteArray          m_musicMixBuf;
    bool                m_micRouting = false;
    QAudioDevice        m_micDevice;
    QToolButton        *m_micBtn     = nullptr;
    void                stopMicRouting();

    // ── Crossfade ─────────────────────────────────────────────────────────────
    int    m_crossfadeSecs = 0;
    bool   m_crossfading   = false;
    float  m_fadeFactor    = 1.0f;
    QTimer *m_fadeInTimer  = nullptr;

    // ── Настройки ─────────────────────────────────────────────────────────────
    AppSettings m_cfg;

    // ── Persistence ──────────────────────────────────────────────────────────
    QSettings   m_settings;
    QStringList m_recentFiles;
    static const int MAX_RECENT = 10;

    static const QStringList VIDEO_EXTS;
    static const QStringList MEDIA_FILTER;
};
