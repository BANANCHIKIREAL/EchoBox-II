#pragma once
#include <QDialog>
#include <QColor>
#include <QLabel>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QStackedWidget;
class QShowEvent;

struct AppSettings {
    QString theme        = "mocha";
    QColor  accentColor  = QColor(0xcb, 0xa6, 0xf7);
    int     fontSizeIdx  = 1;
    QString fontFamily   = "";   // empty = system default
    QString fontFilePath = "";   // path to custom font file (loaded via QFontDatabase)
    QString artShape     = "rounded";

    bool autoPlay       = false;
    bool showVisualizer = true;
    int  crossfadeSecs  = 0;

    QString libraryFolder;
    QString playlistsFolder;
    QString iconsFolder;

    bool showTrackIcons = true;
    bool showStatusBar  = true;
    bool closeToTray    = true;

    bool discordEnabled = true;

    // Куки браузера для yt-dlp (сайты, требующие вход, напр. Яндекс.Музыка) —
    // "" значит не использовать; иначе имя браузера для --cookies-from-browser
    QString ytDlpCookiesBrowser = "";

    // Качество аудио при скачивании по ссылке: "best" | "medium" | "low"
    QString streamAudioQuality = "best";

    // Запуск
    bool launchOnStartup  = false;   // добавлять в автозагрузку Windows
    bool startMinimized   = false;   // не показывать окно при запуске (сразу в трей)
    bool autoCheckUpdates = true;    // тихая проверка обновлений при старте

    // Подтверждения
    bool confirmDelete = true;       // спрашивать перед удалением треков/плейлиста

    // Управление
    int seekStepSecs = 5;   // шаг перемотки ←/→ (Shift+←/→ всегда ×6)
    int volumeStep   = 5;   // шаг громкости ↑/↓
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const AppSettings &current, QWidget *parent = nullptr);
    AppSettings result() const { return m_result; }

public slots:
    void browseFolder(QLineEdit *edit);

signals:
    void applied(const AppSettings &s);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void pickAccentColor();
    void setAccentPreset(const QColor &c);
    void liveApply();
    void animateToPage(int index);
    void clearStreamCache();
    void refreshCacheSize();

private:
    void buildAppearanceTab(class QWidget *tab);
    void buildPlayerTab(QWidget *tab);
    void buildFilesTab(QWidget *tab);
    void buildInterfaceTab(QWidget *tab);
    void buildIntegrationsTab(QWidget *tab);
    void collectResult();
    void connectLive();
    void refreshPresetSwatches();

    AppSettings m_result;

    QStackedWidget *m_stack = nullptr;

    QLabel        *m_accentSwatch    = nullptr;
    QList<class QPushButton*> m_presetBtns;
    QList<QColor>              m_presetColors;
    QButtonGroup  *m_fontGroup       = nullptr;
    QFontComboBox *m_fontFamilyCombo = nullptr;
    QComboBox     *m_artShapeCombo   = nullptr;

    QCheckBox *m_autoPlayChk    = nullptr;
    QCheckBox *m_vizChk         = nullptr;
    QComboBox *m_crossfadeCombo = nullptr;

    QLineEdit *m_libraryEdit    = nullptr;
    QLineEdit *m_playlistsEdit  = nullptr;
    QLineEdit *m_iconsEdit      = nullptr;

    QCheckBox *m_iconsChk      = nullptr;
    QCheckBox *m_statusBarChk  = nullptr;
    QCheckBox *m_trayChk       = nullptr;

    QCheckBox *m_discordChk    = nullptr;
    QComboBox *m_cookiesBrowserCombo = nullptr;
    QComboBox *m_audioQualityCombo   = nullptr;

    QCheckBox *m_launchOnStartupChk = nullptr;
    QCheckBox *m_startMinimizedChk  = nullptr;
    QCheckBox *m_autoUpdatesChk     = nullptr;
    QCheckBox *m_confirmDeleteChk   = nullptr;
    QComboBox *m_seekStepCombo      = nullptr;
    QComboBox *m_volumeStepCombo    = nullptr;

    QLabel    *m_cacheSizeLabel = nullptr;

    // collect all live-connectable widgets
    QList<QObject*> m_liveWidgets;
};
