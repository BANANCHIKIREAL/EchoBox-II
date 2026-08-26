#pragma once
#include <QWidget>
#include <QVector>
#include <QUrl>
#include <QtGlobal>
#include <vector>

class QAudioBuffer;
class QAudioDecoder;

class WaveformSlider : public QWidget {
    Q_OBJECT
public:
    explicit WaveformSlider(QWidget *parent = nullptr);
    ~WaveformSlider() override;

    QSize sizeHint() const override;

    // QSlider-compatible API
    void setRange(int min, int max);
    void setValue(int v);
    int  value() const { return m_value; }

    // Waveform
    void loadWaveform(const QUrl &url, qint64 durationMs);
    void clearWaveform();
    void setPeaks(const QVector<float> &peaks);

    // Theme
    void setAccentColor(const QColor &c);
    void setTrackColor (const QColor &c);
    void setBackgroundColor(const QColor &c);

signals:
    void sliderPressed();
    void sliderReleased();
    void valueChanged(int v);
    void peaksReady(QUrl url, QVector<float> peaks);
    void waveformReady(QUrl url, qint64 durationMs, QVector<float> peaks);

protected:
    void paintEvent(QPaintEvent *)    override;
    void mousePressEvent(QMouseEvent *)   override;
    void mouseMoveEvent(QMouseEvent *)    override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *)      override;

private:
    void onBufferReady();
    void onDecodeFinished();

    int  m_min = 0, m_max = 0, m_value = 0;
    bool m_dragging = false;
    bool m_hovered  = false;
    int  m_hoverX   = -1;

    std::vector<float> m_peaks;
    int    m_bins          = 0;
    int    m_samplesPerBin = 0;
    float  m_binAccum      = 0.f;
    int    m_binCount      = 0;
    int    m_binsFilled    = 0;
    int    m_lastSharedBin = 0;
    bool   m_decodeFinalized = false;
    quint64 m_decodeGeneration = 0;
    qint64 m_durationMs    = 0;
    QUrl   m_waveformUrl;

    QAudioDecoder *m_decoder = nullptr;

    QColor m_accent { 0xcb, 0xa6, 0xf7 };
    QColor m_track  { 0x45, 0x47, 0x5a };
    QColor m_background { 0x1e, 0x1e, 0x2e };

    int  valueToX(int v)  const;
    int  xToValue(int x)  const;
    void applyDrag(int x);
};
