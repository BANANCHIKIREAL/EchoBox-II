#pragma once
#include <QDialog>
#include <QColor>
#include <QLabel>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;

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

private slots:
    void pickAccentColor();
    void setAccentPreset(const QColor &c);
    void liveApply();

private:
    void buildAppearanceTab(class QWidget *tab);
    void buildPlayerTab(QWidget *tab);
    void buildFilesTab(QWidget *tab);
    void buildInterfaceTab(QWidget *tab);
    void buildIntegrationsTab(QWidget *tab);
    void collectResult();
    void connectLive();

    AppSettings m_result;

    QLabel        *m_accentSwatch    = nullptr;
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

    // collect all live-connectable widgets
    QList<QObject*> m_liveWidgets;
};
