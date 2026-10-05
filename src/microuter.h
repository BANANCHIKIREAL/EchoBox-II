#pragma once

#include <QObject>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaPlayer>

class QAudioBuffer;
class QAudioBufferOutput;
class QAudioOutput;
class QAudioSource;
class QAudioSink;
class QIODevice;
class QTimer;

class MicRouter final : public QObject {
    Q_OBJECT
public:
    explicit MicRouter(QMediaPlayer *sourcePlayer, QObject *parent = nullptr);
    ~MicRouter() override;

    bool start(const QByteArray &outputId, const QByteArray &microphoneId,
               bool includeVoice, const QByteArray &monitorId, bool monitorEnabled,
               float musicVolume, float voiceVolume, float monitorVolume,
               QString *errorMessage = nullptr);
    void stop();
    bool isActive() const { return m_active; }
    bool playSound(const QString &path, QString *errorMessage = nullptr);
    void stopSound();
    void setMusicVolume(float volume);
    void setPlayerVolume(float volume);
    void setVoiceVolume(float volume);
    void setMonitorVolume(float volume);
    void feedMusicBuffer(const QAudioBuffer &buffer);
    void resetMusicStream();
    QString activeSound() const { return m_activeSound; }

signals:
    void activeChanged(bool active);
    void routingError(const QString &message);
    void soundStateChanged(const QString &path, bool playing);
    void levelChanged(float level);

private:
    QAudioDevice findOutput(const QByteArray &id) const;
    QAudioDevice findInput(const QByteArray &id) const;
    QAudioFormat commonFormat(const QAudioDevice &input,
                              const QAudioDevice &output) const;
    void handleAudioBuffer(const QAudioBuffer &buffer);
    void flushMusicBuffer();
    void applyMusicVolume();

    QMediaPlayer *m_sourcePlayer = nullptr;
    QAudioSink *m_musicSink = nullptr;
    QIODevice *m_musicStream = nullptr;
    QAudioFormat m_musicFormat;
    QByteArray m_pendingMusic;
    qsizetype m_pendingMusicOffset = 0;
    bool m_musicPrimed = false;
    bool m_reportedMusicOverflow = false;
    QMediaPlayer *m_soundPlayer = nullptr;
    QAudioOutput *m_soundOutput = nullptr;
    QAudioBufferOutput *m_soundBufferOutput = nullptr;
    QMediaPlayer *m_soundMonitorPlayer = nullptr;
    QAudioOutput *m_soundMonitorOutput = nullptr;
    QAudioSource *m_microphoneSource = nullptr;
    QAudioSink *m_microphoneSink = nullptr;
    QIODevice *m_microphoneStream = nullptr;
    QTimer *m_musicWriteTimer = nullptr;
    QString m_activeSound;
    float m_musicVolume = 0.9f;
    float m_playerVolume = 1.0f;
    float m_voiceVolume = 1.0f;
    float m_monitorVolume = 0.7f;
    bool m_monitorEnabled = false;
    bool m_active = false;
};
