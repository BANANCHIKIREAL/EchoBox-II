#pragma once

#include <QWidget>
#include <QList>

class MicRouter;
class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QSettings;
class QSlider;
class QTimer;

class MicDeckDialog final : public QWidget {
    Q_OBJECT
public:
    explicit MicDeckDialog(MicRouter *router, QSettings *settings,
                           QWidget *parent = nullptr, bool settingsOnly = false);

    void setTracks(const QStringList &paths, const QStringList &titles,
                   const QString &playlistName);

signals:
    void clipsChanged();
    void requestPlayerPage();
    void trackFilesAdded(const QStringList &paths);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Clip {
        QString title;
        QString path;
        qint64 durationMs = 0;
    };

    void buildUi();
    void refreshDevices();
    void loadSettings();
    void saveRoutingSettings();
    bool startRouting();
    void loadClips();
    void saveClips();
    void migrateLegacyMicDeck();
    void rebuildCards();
    void relayoutCards();
    void addFiles(const QStringList &files);
    void playClip(int index);
    void removeClip(int index);
    void updateRunningUi();
    void updateVoiceUi();
    static qint64 probeDuration(const QString &path);
    static QString formatDuration(qint64 durationMs);

    MicRouter *m_router = nullptr;
    QSettings *m_settings = nullptr;
    bool m_settingsOnly = false;
    QList<Clip> m_clips;
    QString m_playingPath;
    QString m_playlistName;

    QComboBox *m_outputBox = nullptr;
    QComboBox *m_microphoneBox = nullptr;
    QComboBox *m_monitorBox = nullptr;
    QCheckBox *m_mixVoiceCheck = nullptr;
    QCheckBox *m_monitorCheck = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
    QLabel *m_voiceHint = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_footerLabel = nullptr;
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_stopMixerButton = nullptr;
    QSlider *m_musicVolume = nullptr;
    QSlider *m_voiceVolume = nullptr;
    QSlider *m_monitorVolume = nullptr;
    QProgressBar *m_level = nullptr;
    QGridLayout *m_cards = nullptr;
    QWidget *m_cardsHost = nullptr;
    QLabel *m_emptyState = nullptr;
    QList<QWidget *> m_cardWidgets;
    int m_cardColumns = -1;
    QTimer *m_levelDecay = nullptr;
};
