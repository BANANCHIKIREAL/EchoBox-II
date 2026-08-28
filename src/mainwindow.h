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
#include <QSet>
#include <functional>
#include "settingsdialog.h"
#include "waveformslider.h"
#include "backgroundwidget.h"
#include "libraryscanner.h"
#include "audioengine.h"
#include "waveformcache.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <QAudioBufferOutput>
#endif

class QLabel;
class QProgressBar;
class QPropertyAnimation;
class QToolButton;
class QPushButton;
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
class QNetworkAccessManager;
class Visualizer;
class DiscordRPC;

enum class RepeatMode { Off, One, All };

struct PlaylistEntry {
    QString      name;
    QList<QUrl>  tracks;
    int          currentTrack = -1;
    QString      description;
    QString      iconPath;
};

struct StreamTrackInfo {
    QString title;
    QString artist;
    QString thumbnailUrl;
    QString localPath;
    bool    isDirectUrl = false;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    bool startsMinimized() const;

protected:
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void closeEvent(QCloseEvent *) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void openFiles();
    void openFolder();
    void openUrlDialog();
    void savePlaylist();
    void loadPlaylist();
    void clearPlaylist();
    void removeSelectedTracks();
    void moveSelectedTracksToPlaylist(int targetIndex);
    void copySelectedTracksToPlaylist(int targetIndex);
    void moveTrackUp();
    void moveTrackDown();
    void onSearchChanged(const QString &text);
    void onTrackActivated(QListWidgetItem *item);
    void onPlaylistContextMenu(const QPoint &pos);
    void rebuildPlaylistFromWidget();
    void togglePlayPause();
    void stop();
    void previous();
    void next();
    void nextAuto();
    void setVolume(int v);
    void toggleMute();
    void onSpeedChanged(int index);
    void toggleShuffle();
    void cycleRepeat();
    void toggleMiniPlayer();
    void toggleMiniDock();
    void toggleAlwaysOnTop();
    void toggleRemainingTime();
    void toggleMicRouting();
    void apoTryOpenRing();
    void onDurationChanged(qint64 ms);
    void onWaveformReady(const QUrl &url, qint64 duration, QVector<float> peaks);
    void onPositionChanged(qint64 ms);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onMetaDataChanged();
    void onError(QMediaPlayer::Error error, const QString &msg);
    void onAudioBuffer(const QAudioBuffer &buffer);
    void newPlaylist();
    void onTabChanged(int index);
    void onTabDoubleClicked(int index);
    void onTabContextMenu(const QPoint &pos);
    void showPlaylistBrowser();
    void showPlaylistTracks();
    void refreshPlaylistBrowser();
    void showSearchOverlay();
    void hideSearchOverlay();
    void editPlaylistDetails(int index);
    void showAbout();
    void checkForUpdates(bool manual);
    void downloadAndInstallUpdate(const QString &assetUrl, const QString &tag,
                                  const QString &assetName,
                                  const QString &expectedDigest,
                                  qint64 expectedSize);
    void openRecentFile(const QString &path);
    void openSettings();
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
    void applyModernLayout();
    void showMiniMoreMenu();
    QIcon uiIcon(const QString &symbol, const QColor &color, int size,
                 const QIcon &fallback) const;
    void syncShellShortcutIcon();
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
    QIcon playlistIcon(int index, int size) const;

    void openStreamUrl(const QString &link);
    bool looksLikeDirectMediaUrl(const QUrl &url) const;
    void addDirectStreamUrl(const QUrl &url);
    int  insertStreamPlaceholder(const QUrl &pageUrl);
    void updateStreamPlaceholder(const QUrl &pageUrl, bool ok, const QString &localPath,
                                  const QString &title, const QString &artist,
                                  const QString &thumbnailUrl, const QString &errorMsg);
    void downloadStreamTrack(const QUrl &pageUrl,
                              std::function<void(bool ok, QString localPath, QString title,
                                                  QString artist, QString thumbnailUrl,
                                                  QString errorMsg)> callback);
    void beginStreamPlayback(int index, const QUrl &pageUrl);
    void commitStreamPlayback(int index, const QUrl &pageUrl, const QUrl &mediaSource);
    void fetchStreamThumbnail(const QUrl &pageUrl, const QString &thumbnailUrl);
    QString ytDlpPath() const;
    QString streamCacheDir() const;
    QString trackDisplayTitle(const QUrl &url) const;
    QString trackDisplayArtist(const QUrl &url) const;
    QString playlistRowLabel(const QUrl &url) const;

    QString positionKey(const QUrl &url) const;
    void    saveTrackPosition();

    void applyVolume();

    void updateRepeatButton();
    void updateAlbumArt();
    int  artRadius() const;
    void updatePlaylistInfo();
    void updateTimeDisplay(qint64 pos, qint64 dur);
    void setCurrentTrackVisual(int index);
    void resetWaveformUi();
    void applyWaveformPeaks(const QVector<float> &peaks);

    void fadeInWidget(QWidget *w, int durationMs = 260);
    void fadeOutWidget(QWidget *w, int durationMs = 220);
    void popButtonIcon(QToolButton *btn);
    void animateTrackHighlight(const QUrl &url);

    void showLoadingBanner(const QString &text);
    void updateLoadingText(const QString &text);
    void setLoadingProgress(int percent);
    void hideLoadingBanner();

    void showCopyableError(const QString &title, const QString &message);
    void addRecentFile(const QString &path);
    void refreshRecentMenu();
    bool isVideoFile(const QUrl &url) const;
    QString formatTime(qint64 ms);

    void savePlaylistsToFile();
    void loadPlaylistsFromFile();
    void saveStreamTracksToFile();
    void loadStreamTracksFromFile();
    QString trackIconPath(const QUrl &url) const;
    void applyTrackIcon(QListWidgetItem *item, const QUrl &url);
    void reloadTrackIcons();
    void updateDuplicateHighlights();
    void scheduleScan(const QList<QUrl> &urls);
    void advanceMetaScan();
    void handleMetaReaderUpdate();

    QMediaPlayer   *m_player      = nullptr;
    QAudioOutput   *m_audioOutput = nullptr;
    QMediaPlayer   *m_metaReader  = nullptr;
    QList<QUrl>     m_metaScanQueue;
    bool            m_scanInProgress = false;

    AudioEngine *m_eqEngine = nullptr;
    bool         m_eqActive = false;
    bool         m_eqPending = false;
    QUrl         m_eqSource;
    void syncEqEngineToCurrentTrack();
    void stopEqEngine();
    void playerSeek(qint64 ms);

    QList<QUrl>    m_playlist;
    int            m_currentIndex  = -1;
    bool           m_shuffle       = false;
    RepeatMode     m_repeat        = RepeatMode::Off;
    bool           m_showRemaining = false;
    bool           m_muted         = false;
    int            m_lastVolume    = 70;
    bool           m_seeking       = false;
    bool           m_miniPlayer    = false;
    bool           m_miniTransitioning = false;
    bool           m_miniDragging  = false;
    QPoint         m_miniDragOffset;
    bool           m_miniDocked    = false;
    QRect          m_miniUndockedGeometry;
    QRect          m_fullPlayerGeometry;

    AuroraWidget   *m_aurora      = nullptr;
    QFrame         *m_separator   = nullptr;
    QWidget        *m_contentShell = nullptr;
    QWidget        *m_mainColumn   = nullptr;
    QWidget        *m_modernSidebar = nullptr;
    QLabel         *m_modernBrandIcon = nullptr;
    QToolButton    *m_modernHomeBtn = nullptr;
    QToolButton    *m_modernSearchBtn = nullptr;
    QToolButton    *m_modernPlaylistsBtn = nullptr;
    QToolButton    *m_modernLibraryBtn = nullptr;
    QToolButton    *m_modernOpenBtn = nullptr;
    QToolButton    *m_modernFolderBtn = nullptr;
    QToolButton    *m_modernLinkBtn = nullptr;
    QToolButton    *m_modernSettingsBtn = nullptr;
    class QVBoxLayout *m_mainColumnLayout = nullptr;

    QWidget        *m_topWidget   = nullptr;
    QStackedWidget *m_mediaStack  = nullptr;
    QLabel         *m_albumArt   = nullptr;
    QVideoWidget   *m_videoWidget = nullptr;

    QLabel  *m_titleLabel  = nullptr;
    QLabel  *m_artistLabel = nullptr;
    QLabel  *m_albumLabel  = nullptr;

    QWidget     *m_loadingBanner = nullptr;
    QProgressBar *m_loadingBar   = nullptr;
    QLabel      *m_loadingText   = nullptr;
    QLabel      *m_loadingIcon   = nullptr;
    QLabel      *m_loadingPercent = nullptr;
    QPropertyAnimation *m_loadingAnim = nullptr;
    QWidget     *m_miniLoadingPanel = nullptr;
    QLabel      *m_miniLoadingIcon = nullptr;
    QLabel      *m_miniLoadingText = nullptr;
    QProgressBar *m_miniLoadingBar = nullptr;
    QLabel      *m_miniLoadingPercent = nullptr;

    Visualizer *m_visualizer  = nullptr;
    QPixmap     m_coverPixmap;

    WaveformSlider *m_seekSlider = nullptr;
    QLabel         *m_timeLabel  = nullptr;
    WaveformCache m_waveformCache;

    QToolButton *m_prevBtn      = nullptr;
    QToolButton *m_playPauseBtn = nullptr;
    QToolButton *m_stopBtn      = nullptr;
    QToolButton *m_nextBtn      = nullptr;
    QToolButton *m_shuffleBtn   = nullptr;
    QToolButton *m_repeatBtn    = nullptr;
    QComboBox   *m_speedCombo   = nullptr;

    QToolButton *m_muteBtn      = nullptr;
    QSlider     *m_volumeSlider = nullptr;
    QLabel      *m_volumeLabel  = nullptr;

    QWidget     *m_miniBar        = nullptr;
    QToolButton *m_miniPrevBtn    = nullptr;
    QToolButton *m_miniPlayBtn    = nullptr;
    QToolButton *m_miniNextBtn    = nullptr;
    QLabel      *m_miniTitle      = nullptr;
    QLabel      *m_miniAlbumArt   = nullptr;
    WaveformSlider *m_miniWaveform  = nullptr;
    QToolButton *m_miniShuffleBtn = nullptr;
    QToolButton *m_miniRepeatBtn  = nullptr;
    QToolButton *m_miniMoreBtn    = nullptr;
    QMenu       *m_miniMoreMenu   = nullptr;
    QMenu       *m_miniSpeedMenu  = nullptr;
    QAction     *m_miniStopAct    = nullptr;
    QAction     *m_miniShuffleAct = nullptr;
    QAction     *m_miniRepeatAct  = nullptr;
    QSlider     *m_miniVolSlider  = nullptr;
    QToolButton *m_miniMuteBtn    = nullptr;
    QToolButton *m_miniDockBtn    = nullptr;
    QToolButton *m_miniExpandBtn  = nullptr;
    QToolButton *m_miniMinimizeBtn = nullptr;
    QToolButton *m_miniCloseBtn   = nullptr;

    QWidget     *m_playlistPanel  = nullptr;
    QTabBar     *m_tabBar         = nullptr;
    QLineEdit   *m_searchEdit     = nullptr;
    QLabel      *m_playlistInfo   = nullptr;
    QListWidget *m_playlistWidget = nullptr;
    QWidget     *m_playlistBrowserPage = nullptr;
    QListWidget *m_playlistBrowserGrid = nullptr;
    QPushButton *m_searchDimmer = nullptr;
    QWidget     *m_searchPopup = nullptr;
    QLineEdit   *m_searchPopupEdit = nullptr;
    QToolButton *m_playlistUpBtn  = nullptr;
    QToolButton *m_playlistDownBtn = nullptr;
    QToolButton *m_playlistRemoveBtn = nullptr;
    QToolButton *m_playlistClearBtn = nullptr;
    QToolButton *m_newPlaylistBtn = nullptr;

    QVector<PlaylistEntry> m_playlists;
    int                    m_activePl = 0;

    QHash<QUrl, StreamTrackInfo> m_streamTracks;
    bool                         m_ytDlpMissingWarned = false;
    QNetworkAccessManager       *m_streamArtNam = nullptr;
    QSet<QUrl>                   m_streamResolving;

    QMenu        *m_recentMenu     = nullptr;
    QAction      *m_openUrlAct     = nullptr;
    QAction      *m_scanLibraryAct = nullptr;
    QAction      *m_shuffleAct     = nullptr;
    QAction      *m_alwaysOnTopAct = nullptr;
    QAction      *m_miniPlayerAct  = nullptr;
    QAction      *m_repeatOffAct   = nullptr;
    QAction      *m_repeatOneAct   = nullptr;
    QAction      *m_repeatAllAct   = nullptr;

    QSystemTrayIcon *m_tray        = nullptr;
    QAction         *m_trayPlayAct = nullptr;
    QAction         *m_trayNextAct = nullptr;

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    QAudioBufferOutput *m_audioBufferOut = nullptr;
#endif

    DiscordRPC *m_discord = nullptr;

    LibraryScanner    *m_libraryScanner = nullptr;
    QThread           *m_libraryThread  = nullptr;
    QFileSystemWatcher *m_libraryWatcher = nullptr;
    int                m_libraryPlIdx   = -1;

    int    m_crossfadeSecs = 0;
    bool   m_crossfading   = false;
    float  m_fadeFactor    = 1.0f;
    QTimer *m_fadeInTimer  = nullptr;

    QToolButton *m_micBtn        = nullptr;
    bool         m_micRouting    = false;
    void        *m_apoMapping    = nullptr;
    void        *m_apoRing       = nullptr;
    QTimer      *m_apoOpenTimer  = nullptr;
    unsigned     m_apoWritePos   = 0;
    bool         m_apoBlockVoice = false;
    float        m_apoMusicGain  = 1.5f;
    bool         m_apoNoiseGate  = true;
    float        m_apoGateThresh = 0.02f;
    void         apoCloseRing();
    void         apoFeed(const class QAudioBuffer &buffer);
    void         apoPushControls();
    void         showMicMenu();

    AppSettings m_cfg;

    QSettings   m_settings;
    QStringList m_recentFiles;
    static const int MAX_RECENT = 10;

    static const QStringList VIDEO_EXTS;
    static const QStringList MEDIA_FILTER;
};
