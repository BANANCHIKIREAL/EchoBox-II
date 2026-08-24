#include "audioengine.h"
#include <QAudioDecoder>
#include <QAudioSink>
#include <QAudioBuffer>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaDevices>
#include <cmath>
#include <cstring>

const int kEqBandFreqs[kEqBandCount] = {60, 150, 400, 1000, 2400, 6000, 12000, 16000};

// ─── BiquadState ─────────────────────────────────────────────────────────────

void BiquadState::setPeaking(float sampleRate, float freq, float gainDb, float q) {
    if (gainDb == 0.0f || sampleRate <= 0.0f || freq >= sampleRate * 0.49f) {
        // Идентичный фильтр — вообще не красит звук (быстрый путь для 0 дБ)
        b0 = 1; b1 = 0; b2 = 0; a1 = 0; a2 = 0;
        return;
    }
    const float A     = std::pow(10.0f, gainDb / 40.0f);
    const float w0     = 2.0f * float(M_PI) * freq / sampleRate;
    const float alpha  = std::sin(w0) / (2.0f * q);
    const float cosw0  = std::cos(w0);

    const float b0n = 1 + alpha * A;
    const float b1n = -2 * cosw0;
    const float b2n = 1 - alpha * A;
    const float a0n = 1 + alpha / A;
    const float a1n = -2 * cosw0;
    const float a2n = 1 - alpha / A;

    b0 = b0n / a0n;
    b1 = b1n / a0n;
    b2 = b2n / a0n;
    a1 = a1n / a0n;
    a2 = a2n / a0n;
}

float BiquadState::process(float in) {
    const float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = in;
    y2 = y1; y1 = out;
    return out;
}

void BiquadState::reset() { x1 = x2 = y1 = y2 = 0; }

// ─── EqPlaybackDevice ──────────────────────────────────────────────────────

EqPlaybackDevice::EqPlaybackDevice(QObject *parent) : QIODevice(parent) {
    for (int i = 0; i < kEqBandCount; ++i) m_bandGains[i].store(0.0f);
    open(QIODevice::ReadOnly);
}

void EqPlaybackDevice::setPcm(const QVector<float> &interleavedStereo, int sampleRate) {
    m_pcm = interleavedStereo;
    m_sourceSampleRate = sampleRate;
    m_totalFrames.store(m_pcm.size() / 2);
    m_frameCursor.store(0.0);
    m_filtersInited = false;   // частота дискретизации могла смениться — пересчитать
}

void EqPlaybackDevice::configureOutput(const QAudioFormat &format) {
    m_outputSampleRate = format.sampleRate();
    m_outputChannels = format.channelCount();
    m_outputSampleFormat = format.sampleFormat();
    m_filtersInited = false;
}

void EqPlaybackDevice::setFramePosition(qint64 frame) {
    frame = qBound<qint64>(0, frame, m_totalFrames.load());
    m_frameCursor.store(double(frame));
    for (int ch = 0; ch < 2; ++ch)
        for (int b = 0; b < kEqBandCount; ++b)
            m_filters[ch][b].reset();
}

void EqPlaybackDevice::setBandGain(int band, float dB) {
    if (band < 0 || band >= kEqBandCount) return;
    m_bandGains[band].store(dB);
}

qint64 EqPlaybackDevice::bytesAvailable() const {
    // "Данные" тут генерируются по требованию (сэмплы или тишина в хвосте),
    // так что честного deferred-EOF не нужно — всегда говорим, что есть что читать
    return 64 * 1024 + QIODevice::bytesAvailable();
}

void EqPlaybackDevice::rebuildFiltersIfNeeded() {
    bool changed = !m_filtersInited;
    for (int b = 0; b < kEqBandCount; ++b) {
        const float g = m_bandGains[b].load();
        if (g != m_appliedGains[b]) changed = true;
        m_appliedGains[b] = g;
    }
    if (!changed) return;
    m_filtersInited = true;
    for (int ch = 0; ch < 2; ++ch)
        for (int b = 0; b < kEqBandCount; ++b)
            m_filters[ch][b].setPeaking(float(m_outputSampleRate), float(kEqBandFreqs[b]),
                                         m_appliedGains[b], 1.0f);
}

float EqPlaybackDevice::sampleAt(int channel, double frame) const {
    const qint64 total = m_totalFrames.load();
    if (total <= 0) return 0.0f;
    qint64 i0 = qint64(frame);
    if (i0 >= total - 1) return m_pcm[(total - 1) * 2 + channel];
    if (i0 < 0) i0 = 0;
    const double frac = frame - double(i0);
    const float s0 = m_pcm[i0 * 2 + channel];
    const float s1 = m_pcm[(i0 + 1) * 2 + channel];
    return float(s0 + (s1 - s0) * frac);
}

qint64 EqPlaybackDevice::readData(char *data, qint64 maxSize) {
    rebuildFiltersIfNeeded();

    int bytesPerSample = 0;
    switch (m_outputSampleFormat) {
    case QAudioFormat::UInt8: bytesPerSample = 1; break;
    case QAudioFormat::Int16: bytesPerSample = 2; break;
    case QAudioFormat::Int32:
    case QAudioFormat::Float: bytesPerSample = 4; break;
    default: return 0;
    }
    const qint64 bytesPerFrame = qint64(m_outputChannels) * bytesPerSample;
    if (bytesPerFrame <= 0) return 0;
    const qint64 framesRequested = maxSize / bytesPerFrame;
    if (framesRequested <= 0) return 0;
    const int silenceByte = m_outputSampleFormat == QAudioFormat::UInt8 ? 0x80 : 0x00;

    const qint64 total = m_totalFrames.load();
    if (total <= 0) {
        std::memset(data, silenceByte, size_t(framesRequested * bytesPerFrame));
        return framesRequested * bytesPerFrame;
    }

    const float  volume = m_volume.load();
    const double rate = m_rate.load() * double(m_sourceSampleRate) /
                        double(qMax(1, m_outputSampleRate));
    const bool   eqOn   = m_eqEnabled.load();
    double cursor = m_frameCursor.load();

    qint64 framesWritten = 0;
    for (; framesWritten < framesRequested; ++framesWritten) {
        if (cursor >= double(total)) break;   // конец трека — дальше тишина
        float stereo[2] = {0.0f, 0.0f};
        for (int ch = 0; ch < 2; ++ch) {
            float s = sampleAt(ch, cursor);
            if (eqOn)
                for (int b = 0; b < kEqBandCount; ++b)
                    s = m_filters[ch][b].process(s);
            stereo[ch] = qBound(-1.0f, s * volume, 1.0f);
        }
        for (int ch = 0; ch < m_outputChannels; ++ch) {
            const float sample = m_outputChannels == 1
                ? (stereo[0] + stereo[1]) * 0.5f
                : (ch == 0 ? stereo[0] : ch == 1 ? stereo[1] : 0.0f);
            char *dst = data + framesWritten * bytesPerFrame + ch * bytesPerSample;
            switch (m_outputSampleFormat) {
            case QAudioFormat::Float: {
                std::memcpy(dst, &sample, sizeof(sample));
                break;
            }
            case QAudioFormat::Int16: {
                const qint16 value = qint16(std::lround(sample * 32767.0f));
                std::memcpy(dst, &value, sizeof(value));
                break;
            }
            case QAudioFormat::Int32: {
                const qint32 value = qint32(std::llround(double(sample) * 2147483647.0));
                std::memcpy(dst, &value, sizeof(value));
                break;
            }
            case QAudioFormat::UInt8: {
                const quint8 value = quint8(qBound(0, int(std::lround(sample * 127.0f + 128.0f)), 255));
                std::memcpy(dst, &value, sizeof(value));
                break;
            }
            default: break;
            }
        }
        cursor += rate;
    }
    if (framesWritten < framesRequested)
        std::memset(data + framesWritten * bytesPerFrame, silenceByte,
                    size_t(framesRequested - framesWritten) * bytesPerFrame);

    m_frameCursor.store(cursor);
    return framesRequested * bytesPerFrame;
}

qint64 EqPlaybackDevice::writeData(const char *, qint64) { return -1; }

// ─── AudioEngine ─────────────────────────────────────────────────────────────

AudioEngine::AudioEngine(QObject *parent) : QObject(parent) {
    m_device = new EqPlaybackDevice(this);
}

AudioEngine::~AudioEngine() {
    if (m_sink) m_sink->stop();
}

void AudioEngine::setSource(const QUrl &url) {
    if (m_sink) m_sink->stop();
    if (m_decoder) m_decoder->stop();
    m_pcm.clear();
    m_pendingPlay = false;
    m_pendingSeekMs = -1;

    if (!m_decoder) {
        m_decoder = new QAudioDecoder(this);
        connect(m_decoder, &QAudioDecoder::bufferReady, this, &AudioEngine::onBufferReady);
        connect(m_decoder, &QAudioDecoder::finished, this, &AudioEngine::onDecodeFinished);
        connect(m_decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error),
                this, &AudioEngine::onDecodeError);
    }

    // Не навязываем декодеру конкретный формат (Float/44100 и т.п.) —
    // не все бэкенды готовы отдать именно такой, а при отказе декодер молча
    // не даёт ни одного буфера. Вместо этого читаем то, что реально пришло
    // (см. onBufferReady), каким бы ни было исходное качество/частота файла
    m_sampleRate = 0;
    m_decoder->setSource(url);
    m_decoder->start();
}

void AudioEngine::onBufferReady() {
    while (m_decoder->bufferAvailable()) {
        const QAudioBuffer buf = m_decoder->read();
        if (!buf.isValid()) continue;
        const QAudioFormat fmt = buf.format();
        const int channels = fmt.channelCount();
        const int frames = buf.frameCount();
        if (channels < 1 || frames <= 0) continue;
        m_sampleRate = fmt.sampleRate();

        const int n = m_pcm.size();
        m_pcm.resize(n + frames * 2);
        float *dst = m_pcm.data() + n;

        switch (fmt.sampleFormat()) {
        case QAudioFormat::Float: {
            const float *src = buf.constData<float>();
            for (int i = 0; i < frames; ++i) {
                dst[i*2]   = src[i*channels + 0];
                dst[i*2+1] = (channels > 1) ? src[i*channels + 1] : dst[i*2];
            }
            break;
        }
        case QAudioFormat::Int16: {
            const qint16 *src = buf.constData<qint16>();
            for (int i = 0; i < frames; ++i) {
                dst[i*2]   = src[i*channels + 0] / 32768.0f;
                dst[i*2+1] = (channels > 1) ? src[i*channels + 1] / 32768.0f : dst[i*2];
            }
            break;
        }
        case QAudioFormat::Int32: {
            const qint32 *src = buf.constData<qint32>();
            for (int i = 0; i < frames; ++i) {
                dst[i*2]   = float(src[i*channels + 0] / 2147483648.0);
                dst[i*2+1] = (channels > 1) ? float(src[i*channels + 1] / 2147483648.0) : dst[i*2];
            }
            break;
        }
        case QAudioFormat::UInt8: {
            const quint8 *src = buf.constData<quint8>();
            for (int i = 0; i < frames; ++i) {
                dst[i*2]   = (int(src[i*channels + 0]) - 128) / 128.0f;
                dst[i*2+1] = (channels > 1) ? (int(src[i*channels + 1]) - 128) / 128.0f : dst[i*2];
            }
            break;
        }
        default:
            // Неизвестный формат сэмплов — этот буфер прочитать не можем
            m_pcm.resize(n);
            break;
        }
    }
}

void AudioEngine::onDecodeFinished() {
    if (m_pcm.isEmpty() || m_sampleRate <= 0) {
        m_pendingPlay = false;
        m_pendingSeekMs = -1;
        emit decodeError("Декодер не вернул звуковые данные для этого файла.");
        return;
    }
    m_device->setPcm(m_pcm, m_sampleRate > 0 ? m_sampleRate : 44100);
    if (!ensureSink(m_sampleRate)) {
        m_pendingPlay = false;
        m_pendingSeekMs = -1;
        emit decodeError("Устройство вывода звука не поддерживает доступный аудиоформат.");
        return;
    }
    emit ready();

    if (m_pendingSeekMs >= 0) {
        setPosition(m_pendingSeekMs);
        m_pendingSeekMs = -1;
    }
    if (m_pendingPlay) {
        m_pendingPlay = false;
        play();
    }
}

void AudioEngine::onDecodeError() {
    const QString details = m_decoder->errorString().trimmed();
    emit decodeError(details.isEmpty() ? "Неизвестная ошибка декодирования." : details);
}

bool AudioEngine::ensureSink(int sampleRate) {
    const QAudioDevice output = QMediaDevices::defaultAudioOutput();
    if (output.isNull()) return false;

    QAudioFormat fmt;
    fmt.setSampleRate(sampleRate);
    fmt.setChannelCount(2);
    fmt.setSampleFormat(QAudioFormat::Float);
    if (!output.isFormatSupported(fmt)) fmt = output.preferredFormat();
    if (!fmt.isValid() || fmt.channelCount() < 1 || fmt.sampleRate() < 1 ||
        fmt.sampleFormat() == QAudioFormat::Unknown)
        return false;

    if (m_sink && m_sink->format() == fmt) {
        m_device->configureOutput(fmt);
        return true;
    }
    if (m_sink) { m_sink->stop(); m_sink->deleteLater(); m_sink = nullptr; }
    m_device->configureOutput(fmt);
    m_sink = new QAudioSink(output, fmt, this);
    return true;
}

void AudioEngine::play() {
    if (m_device->totalFrames() <= 0) { m_pendingPlay = true; return; }
    if (!ensureSink(m_sampleRate > 0 ? m_sampleRate : 44100)) {
        emit decodeError("Устройство вывода звука недоступно.");
        return;
    }
    switch (m_sink->state()) {
    case QAudio::ActiveState: break;
    case QAudio::SuspendedState: m_sink->resume(); break;
    default: m_sink->start(m_device); break;
    }
}

void AudioEngine::pause() {
    if (m_sink && m_sink->state() == QAudio::ActiveState) m_sink->suspend();
}

void AudioEngine::stop() {
    if (m_sink) m_sink->stop();
    if (m_decoder) m_decoder->stop();
    m_pendingPlay = false;
    m_pendingSeekMs = -1;
    m_device->setFramePosition(0);
}

void AudioEngine::setPosition(qint64 ms) {
    if (m_device->totalFrames() <= 0) { m_pendingSeekMs = ms; return; }
    const qint64 frame = qint64(double(ms) / 1000.0 * m_sampleRate);
    m_device->setFramePosition(frame);
}

void AudioEngine::setVolume(float v) { m_device->setVolume(v); }
void AudioEngine::setPlaybackRate(double rate) { m_device->setRate(rate); }
void AudioEngine::setEqEnabled(bool on) { m_device->setEqEnabled(on); }
void AudioEngine::setEqBandGain(int band, float dB) { m_device->setBandGain(band, dB); }
