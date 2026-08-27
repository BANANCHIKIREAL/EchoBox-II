#pragma once
#include <QDialog>
#include <QColor>
#include <QLabel>
#include "audioengine.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QShowEvent;

struct AppSettings {
    QString theme        = "mocha";
    QColor  accentColor  = QColor(0xcb, 0xa6, 0xf7);
    int     fontSizeIdx  = 1;
    QString fontFamily   = "";
    QString fontFilePath = "";
    QString artShape     = "rounded";
    QString appIconStyle = "classic";

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

    QString ytDlpCookiesBrowser = "";

    QString streamAudioQuality = "best";

    bool launchOnStartup  = false;
    bool startMinimized   = false;
    bool autoCheckUpdates = true;

    bool confirmDelete = true;

    int seekStepSecs = 5;
    int volumeStep   = 5;

    bool  eqEnabled = false;
    float eqBands[kEqBandCount] = {0,0,0,0,0,0,0,0};
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
    void buildEqualizerTab(QWidget *tab);
    void collectResult();
    void connectLive();
    void refreshSidebarIcons();
    void refreshPresetSwatches();
    void refreshThemePreview();
    void setEqPreset(const float (&gains)[kEqBandCount]);

    AppSettings m_result;

    QStackedWidget *m_stack = nullptr;
    QListWidget    *m_sidebar = nullptr;

    QLabel        *m_accentSwatch    = nullptr;
    QComboBox     *m_themeCombo      = nullptr;
    QLabel        *m_themePreview    = nullptr;
    QList<class QPushButton*> m_presetBtns;
    QList<QColor>              m_presetColors;
    QButtonGroup  *m_fontGroup       = nullptr;
    QFontComboBox *m_fontFamilyCombo = nullptr;
    QComboBox     *m_artShapeCombo   = nullptr;
    QListWidget   *m_appIconList     = nullptr;

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

    QCheckBox *m_eqEnabledChk = nullptr;
    class QSlider *m_eqSliders[kEqBandCount] = { nullptr };
    QLabel    *m_eqValueLabels[kEqBandCount] = { nullptr };

    QList<QObject*> m_liveWidgets;
};
