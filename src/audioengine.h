#pragma once
#include <QObject>
#include <QUrl>
#include <QVector>
#include <QIODevice>
#include <atomic>

class QAudioDecoder;
class QAudioSink;

// Число полос графического эквалайзера и их центральные частоты (Гц) —
// стандартный набор для 8-полосного графического EQ
constexpr int kEqBandCount = 8;
extern const int kEqBandFreqs[kEqBandCount];

// Один biquad-фильтр второго порядка (Direct Form I), настраиваемый как
// пиковый ("peaking") EQ по формулам из Audio EQ Cookbook (RBJ)
struct BiquadState {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void setPeaking(float sampleRate, float freq, float gainDb, float q);
    float process(float in);
    void reset();
};

// QIODevice, из которого QAudioSink в pull-режиме читает уже
// эквализированные и обработанные по громкости/скорости сэмплы. PCM
// заполняется целиком ДО начала воспроизведения (см. AudioEngine), поэтому
// readData() — единственное место, которое трогает m_pcm, и потокобезопасно
// без мьютексов: параметры, которые может поменять GUI-поток на лету
// (громкость, скорость, гейны полос), хранятся как atomic
class EqPlaybackDevice : public QIODevice {
    Q_OBJECT
public:
    explicit EqPlaybackDevice(QObject *parent = nullptr);

    void setPcm(const QVector<float> &interleavedStereo, int sampleRate);
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
    QVector<float> m_pcm;              // interleaved, 2 канала
    int m_sampleRate = 44100;
    std::atomic<qint64>  m_totalFrames{0};
    std::atomic<double>  m_frameCursor{0.0};   // точная позиция чтения (дробная — из-за ресемплинга при смене скорости)

    std::atomic<float>  m_volume{1.0f};
    std::atomic<double> m_rate{1.0};
    std::atomic<bool>   m_eqEnabled{false};
    std::atomic<float>  m_bandGains[kEqBandCount];

    float m_appliedGains[kEqBandCount] = {0};
    BiquadState m_filters[2][kEqBandCount];   // [канал][полоса]
    bool m_filtersInited = false;

    void rebuildFiltersIfNeeded();
    float sampleAt(int channel, double frame) const;   // линейная интерполяция между соседними сэмплами
};

// Декодирует локальный файл целиком в память (QAudioDecoder) и проигрывает
// его через QAudioSink с применением графического эквалайзера. Используется
// как "теневой" движок параллельно с обычным QMediaPlayer — см. mainwindow.cpp:
// пока эквалайзер выключен, ничего не меняется; как только он включён (для
// аудиофайлов), обычный плеер приглушается, а слышимый звук идёт отсюда.
// Такая схема почти не трогает уже отлаженную логику позиции/кроссфейда/
// сохранения состояния — она вся продолжает жить на QMediaPlayer как раньше.
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

    void setVolume(float v);           // 0..1
    void setPlaybackRate(double rate); // 0.5 .. 2.0 — простой ресемплинг (меняется и высота тона)

    void setEqEnabled(bool on);
    void setEqBandGain(int band, float dB);

signals:
    void ready();                 // декодирование завершено, можно играть без задержки
    void decodeError(QString msg);

private slots:
    void onBufferReady();
    void onDecodeFinished();
    void onDecodeError();

private:
    QAudioDecoder *m_decoder = nullptr;
    QAudioSink    *m_sink    = nullptr;
    EqPlaybackDevice *m_device = nullptr;

    QVector<float> m_pcm;      // накопитель во время декодирования
    int m_sampleRate = 44100;

    bool   m_pendingPlay   = false;
    qint64 m_pendingSeekMs = -1;

    void ensureSink(int sampleRate);
};
