#pragma once
#include <QObject>
#include <QUrl>
#include <QVector>
#include <QIODevice>
#include <QAudioFormat>
#include <atomic>

class QAudioDecoder;
class QAudioSink;

constexpr int kEqBandCount = 8;
extern const int kEqBandFreqs[kEqBandCount];

struct BiquadState {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void setPeaking(float sampleRate, float freq, float gainDb, float q);
    float process(float in);
    void reset();
};

class EqPlaybackDevice : public QIODevice {
    Q_OBJECT
public:
    explicit EqPlaybackDevice(QObject *parent = nullptr);

    void setPcm(QVector<float> interleavedStereo, int sampleRate);
    void configureOutput(const QAudioFormat &format);
    void setFramePosition(qint64 frame);
    qint64 totalFrames() const { return m_totalFrames.load(); }

    void setVolume(float v) { m_volume.store(v); }
    void setRate(double r) { m_rate.store(qBound(0.25, r, 4.0)); }
    void setEqEnabled(bool on) { m_eqEnabled.store(on); }
    void setBandGain(int band, float dB);

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    QVector<float> m_pcm;
    int m_sourceSampleRate = 44100;
    int m_outputSampleRate = 44100;
    int m_outputChannels = 2;
    QAudioFormat::SampleFormat m_outputSampleFormat = QAudioFormat::Float;
    std::atomic<qint64>  m_totalFrames{0};
    std::atomic<double>  m_frameCursor{0.0};

    std::atomic<float>  m_volume{1.0f};
    std::atomic<double> m_rate{1.0};
    std::atomic<bool>   m_eqEnabled{false};
    std::atomic<float>  m_bandGains[kEqBandCount];

    float m_appliedGains[kEqBandCount] = {0};
    BiquadState m_filters[2][kEqBandCount];
    bool m_filtersInited = false;

    void rebuildFiltersIfNeeded();
    float sampleAt(int channel, double frame) const;
};

class AudioEngine : public QObject {
    Q_OBJECT
public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    void setSource(const QUrl &url);
    void play();
    void pause();
    void stop();
    void setPosition(qint64 ms);

    void setVolume(float v);
    void setPlaybackRate(double rate);

    void setEqEnabled(bool on);
    void setEqBandGain(int band, float dB);

signals:
    void ready();
    void decodeError(QString msg);

private slots:
    void onBufferReady();
    void onDecodeFinished();
    void onDecodeError();

private:
    QAudioDecoder *m_decoder = nullptr;
    QAudioSink    *m_sink    = nullptr;
    EqPlaybackDevice *m_device = nullptr;

    QVector<float> m_pcm;
    int m_sampleRate = 44100;
    QUrl m_source;
    bool m_decodedReady = false;

    bool   m_pendingPlay   = false;
    qint64 m_pendingSeekMs = -1;

    bool ensureSink(int sampleRate);
};
