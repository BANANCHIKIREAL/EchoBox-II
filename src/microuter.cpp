#include "microuter.h"

#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QAudioSink>
#include <QAudioSource>
#include <QFileInfo>
#include <QIODevice>
#include <QMediaDevices>
#include <QTimer>
#include <QtMath>

MicRouter::MicRouter(QMediaPlayer *sourcePlayer, QObject *parent)
    : QObject(parent), m_sourcePlayer(sourcePlayer)
{
    m_soundPlayer = new QMediaPlayer(this);
    m_soundOutput = new QAudioOutput(this);
    m_soundPlayer->setAudioOutput(m_soundOutput);
    m_soundBufferOutput = new QAudioBufferOutput(this);
    m_soundPlayer->setAudioBufferOutput(m_soundBufferOutput);
    connect(m_soundBufferOutput, &QAudioBufferOutput::audioBufferReceived,
            this, &MicRouter::handleAudioBuffer);

    m_soundMonitorPlayer = new QMediaPlayer(this);
    m_soundMonitorOutput = new QAudioOutput(this);
    m_soundMonitorPlayer->setAudioOutput(m_soundMonitorOutput);

    m_musicWriteTimer = new QTimer(this);
    m_musicWriteTimer->setInterval(5);
    m_musicWriteTimer->setTimerType(Qt::PreciseTimer);
    connect(m_musicWriteTimer, &QTimer::timeout,
            this, &MicRouter::flushMusicBuffer);

    connect(m_sourcePlayer, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState state) {
                if (m_active && state == QMediaPlayer::PausedState)
                    resetMusicStream();
            });
    connect(m_soundPlayer, &QMediaPlayer::mediaStatusChanged,
            this, [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia) stopSound();
            });
    connect(m_soundPlayer, &QMediaPlayer::errorOccurred,
            this, [this](QMediaPlayer::Error, const QString &message) {
                if (!message.trimmed().isEmpty())
                    emit routingError("Не удалось воспроизвести звук: " + message);
                stopSound();
            });
}

MicRouter::~MicRouter() { stop(); }

QAudioDevice MicRouter::findOutput(const QByteArray &id) const
{
    const auto devices = QMediaDevices::audioOutputs();
    for (const QAudioDevice &device : devices)
        if (device.id() == id) return device;
    return {};
}

QAudioDevice MicRouter::findInput(const QByteArray &id) const
{
    const auto devices = QMediaDevices::audioInputs();
    for (const QAudioDevice &device : devices)
        if (device.id() == id) return device;
    return {};
}

QAudioFormat MicRouter::commonFormat(const QAudioDevice &input,
                                     const QAudioDevice &output) const
{
    QList<QAudioFormat> candidates;
    candidates << input.preferredFormat() << output.preferredFormat();
    for (int rate : {48000, 44100}) {
        for (int channels : {1, 2}) {
            for (QAudioFormat::SampleFormat sample :
                 {QAudioFormat::Float, QAudioFormat::Int16, QAudioFormat::Int32}) {
                QAudioFormat format;
                format.setSampleRate(rate);
                format.setChannelCount(channels);
                format.setSampleFormat(sample);
                candidates << format;
            }
        }
    }
    for (const QAudioFormat &format : candidates)
        if (format.isValid() && input.isFormatSupported(format) &&
            output.isFormatSupported(format))
            return format;
    return {};
}

bool MicRouter::start(const QByteArray &outputId, const QByteArray &microphoneId,
                      bool includeVoice, const QByteArray &monitorId, bool monitorEnabled,
                      float musicVolume, float voiceVolume, float monitorVolume,
                      QString *errorMessage)
{
    stop();
    const QAudioDevice output = findOutput(outputId);
    if (output.isNull()) {
        if (errorMessage) *errorMessage = "Выбранный виртуальный выход больше не доступен.";
        return false;
    }

    m_soundOutput->setDevice(output);
    setMusicVolume(musicVolume);
    setVoiceVolume(voiceVolume);
    setMonitorVolume(monitorVolume);

    m_monitorEnabled = false;
    if (monitorEnabled) {
        const QAudioDevice monitor = findOutput(monitorId);
        if (monitor.isNull()) {
            if (errorMessage) *errorMessage = "Выбранное устройство мониторинга больше не доступно.";
            return false;
        }
        if (monitor.id() != output.id()) {
            m_soundMonitorOutput->setDevice(monitor);
            m_monitorEnabled = true;
        }
    }

    m_musicFormat.setSampleRate(48000);
    m_musicFormat.setChannelCount(2);
    m_musicFormat.setSampleFormat(QAudioFormat::Float);
    if (!output.isFormatSupported(m_musicFormat)) {
        if (errorMessage)
            *errorMessage = "Виртуальный выход не поддерживает формат 48 кГц, стерео Float.";
        return false;
    }
    m_musicSink = new QAudioSink(output, m_musicFormat, this);
    applyMusicVolume();
    // Keep enough audio queued to absorb FFmpeg/WASAPI scheduling jitter.
    // The old duplicate-player path skipped fragments whenever either clock
    // stalled; a 600 ms device buffer with a 250 ms pre-roll remains stable
    // without making the virtual-microphone delay excessive.
    m_musicSink->setBufferSize(m_musicFormat.bytesForDuration(600000));
    m_musicStream = m_musicSink->start();
    if (!m_musicStream) {
        if (errorMessage) *errorMessage = "Windows не разрешила открыть виртуальный аудиовыход.";
        stop();
        return false;
    }
    connect(m_musicSink, &QAudioSink::stateChanged,
            this, [this](QAudio::State state) {
                if (m_active && state == QAudio::StoppedState && m_musicSink &&
                    m_musicSink->error() != QAudio::NoError)
                    emit routingError("Поток музыки в виртуальный микрофон был остановлен Windows.");
            });

    if (includeVoice) {
        const QAudioDevice input = findInput(microphoneId);
        if (input.isNull()) {
            if (errorMessage) *errorMessage = "Выбранный микрофон больше не доступен.";
            return false;
        }
        const QAudioFormat format = commonFormat(input, output);
        if (!format.isValid()) {
            if (errorMessage)
                *errorMessage = "У микрофона и виртуального выхода нет общего аудиоформата.";
            return false;
        }

        m_microphoneSource = new QAudioSource(input, format, this);
        m_microphoneSource->setVolume(m_voiceVolume);
        m_microphoneSink = new QAudioSink(output, format, this);
        m_microphoneStream = m_microphoneSource->start();
        if (!m_microphoneStream) {
            if (errorMessage) *errorMessage = "Windows не разрешила открыть выбранный микрофон.";
            stop();
            return false;
        }
        m_microphoneSink->start(m_microphoneStream);
        connect(m_microphoneSource, &QAudioSource::stateChanged,
                this, [this](QAudio::State state) {
                    if (m_active && state == QAudio::StoppedState &&
                        m_microphoneSource && m_microphoneSource->error() != QAudio::NoError)
                        emit routingError("Поток физического микрофона был остановлен Windows.");
                });
    }

    m_active = true;
    m_musicWriteTimer->start();
    emit activeChanged(true);
    return true;
}

void MicRouter::stop()
{
    const bool wasActive = m_active;
    m_active = false;
    stopSound();
    if (m_musicWriteTimer) m_musicWriteTimer->stop();
    if (m_musicSink) m_musicSink->stop();
    if (m_microphoneSink) m_microphoneSink->stop();
    if (m_microphoneSource) m_microphoneSource->stop();
    delete m_musicSink;
    delete m_microphoneSink;
    delete m_microphoneSource;
    m_musicSink = nullptr;
    m_musicStream = nullptr;
    m_pendingMusic.clear();
    m_pendingMusicOffset = 0;
    m_musicPrimed = false;
    m_reportedMusicOverflow = false;
    m_microphoneSink = nullptr;
    m_microphoneSource = nullptr;
    m_microphoneStream = nullptr;
    m_monitorEnabled = false;
    emit levelChanged(0.0f);
    if (wasActive) emit activeChanged(false);
}

bool MicRouter::playSound(const QString &path, QString *errorMessage)
{
    if (!m_active) {
        if (errorMessage) *errorMessage = "Сначала запустите микшер.";
        return false;
    }
    if (!QFileInfo::exists(path)) {
        if (errorMessage) *errorMessage = "Аудиофайл не найден: " + path;
        return false;
    }

    stopSound();
    m_activeSound = path;
    const QUrl source = QUrl::fromLocalFile(path);
    m_soundPlayer->setSource(source);
    m_soundPlayer->setPosition(0);
    m_soundPlayer->play();
    if (m_monitorEnabled) {
        m_soundMonitorPlayer->setSource(source);
        m_soundMonitorPlayer->setPosition(0);
        m_soundMonitorPlayer->play();
    }
    emit soundStateChanged(path, true);
    return true;
}

void MicRouter::stopSound()
{
    const QString previous = m_activeSound;
    m_activeSound.clear();
    if (m_soundPlayer) m_soundPlayer->stop();
    if (m_soundMonitorPlayer) m_soundMonitorPlayer->stop();
    if (!previous.isEmpty()) emit soundStateChanged(previous, false);
}

void MicRouter::setMusicVolume(float volume)
{
    m_musicVolume = qBound(0.0f, volume, 1.25f);
    applyMusicVolume();
}

void MicRouter::setPlayerVolume(float volume)
{
    m_playerVolume = qBound(0.0f, volume, 1.0f);
    applyMusicVolume();
}

void MicRouter::applyMusicVolume()
{
    const float effective = qBound(0.0f, m_musicVolume * m_playerVolume, 1.0f);
    if (m_musicSink) m_musicSink->setVolume(effective);
    if (m_soundOutput) m_soundOutput->setVolume(effective);
}

void MicRouter::setVoiceVolume(float volume)
{
    m_voiceVolume = qBound(0.0f, volume, 1.0f);
    if (m_microphoneSource) m_microphoneSource->setVolume(m_voiceVolume);
}

void MicRouter::setMonitorVolume(float volume)
{
    m_monitorVolume = qBound(0.0f, volume, 1.0f);
    if (m_soundMonitorOutput) m_soundMonitorOutput->setVolume(m_monitorVolume);
}

void MicRouter::feedMusicBuffer(const QAudioBuffer &buffer)
{
    if (!m_active || !m_musicSink || !m_musicStream) return;
    if (!buffer.isValid() || buffer.byteCount() <= 0) {
        emit levelChanged(0.0f);
        return;
    }
    if (buffer.format() != m_musicFormat) {
        emit routingError("Формат потока плеера изменился; перезапустите микшер.");
        return;
    }

    const char *data = buffer.constData<char>();
    m_pendingMusic.append(data, buffer.byteCount());
    handleAudioBuffer(buffer);

    const qsizetype queued = m_pendingMusic.size() - m_pendingMusicOffset;
    const qsizetype primeBytes = m_musicFormat.bytesForDuration(250000);
    if (!m_musicPrimed && queued >= primeBytes)
        m_musicPrimed = true;

    const qsizetype maximumBytes = m_musicFormat.bytesForDuration(1000000);
    if (queued > maximumBytes) {
        // A one-second backlog is stale audio, so discard it and re-prime instead
        // of playing delayed fragments later.
        m_pendingMusic.clear();
        m_pendingMusicOffset = 0;
        m_musicPrimed = false;
        if (!m_reportedMusicOverflow) {
            m_reportedMusicOverflow = true;
            emit routingError("Аудиовыход не успевает принимать музыку; поток пересинхронизирован.");
        }
        return;
    }
    flushMusicBuffer();
}

void MicRouter::resetMusicStream()
{
    m_pendingMusic.clear();
    m_pendingMusicOffset = 0;
    m_musicPrimed = false;
    m_reportedMusicOverflow = false;
    if (!m_active || !m_musicSink) return;
    m_musicSink->reset();
    m_musicStream = m_musicSink->start();
    if (!m_musicStream)
        emit routingError("Не удалось перезапустить поток виртуального микрофона.");
}

void MicRouter::flushMusicBuffer()
{
    if (!m_active || !m_musicPrimed || !m_musicSink || !m_musicStream) return;
    while (m_pendingMusicOffset < m_pendingMusic.size()) {
        const qint64 available = m_musicSink->bytesFree();
        if (available <= 0) break;
        const qsizetype remaining = m_pendingMusic.size() - m_pendingMusicOffset;
        const qsizetype amount = qMin<qsizetype>(remaining, available);
        const qint64 written = m_musicStream->write(
            m_pendingMusic.constData() + m_pendingMusicOffset, amount);
        if (written <= 0) break;
        m_pendingMusicOffset += written;
    }
    if (m_pendingMusicOffset == m_pendingMusic.size()) {
        m_pendingMusic.clear();
        m_pendingMusicOffset = 0;
    } else if (m_pendingMusicOffset > 262144) {
        m_pendingMusic.remove(0, m_pendingMusicOffset);
        m_pendingMusicOffset = 0;
    }
}

void MicRouter::handleAudioBuffer(const QAudioBuffer &buffer)
{
    if (!buffer.isValid() || buffer.sampleCount() <= 0) {
        emit levelChanged(0.0f);
        return;
    }
    float peak = 0.0f;
    const qsizetype count = buffer.sampleCount();
    switch (buffer.format().sampleFormat()) {
    case QAudioFormat::UInt8: {
        const auto *samples = buffer.constData<quint8>();
        for (qsizetype i = 0; i < count; ++i)
            peak = qMax(peak, qAbs(int(samples[i]) - 128) / 128.0f);
        break;
    }
    case QAudioFormat::Int16: {
        const auto *samples = buffer.constData<qint16>();
        for (qsizetype i = 0; i < count; ++i)
            peak = qMax(peak, qAbs(int(samples[i])) / 32768.0f);
        break;
    }
    case QAudioFormat::Int32: {
        const auto *samples = buffer.constData<qint32>();
        for (qsizetype i = 0; i < count; ++i)
            peak = qMax(peak, float(qAbs(double(samples[i])) / 2147483648.0));
        break;
    }
    case QAudioFormat::Float: {
        const auto *samples = buffer.constData<float>();
        for (qsizetype i = 0; i < count; ++i)
            peak = qMax(peak, qAbs(samples[i]));
        break;
    }
    default:
        break;
    }
    emit levelChanged(qBound(0.0f, peak, 1.0f));
}
