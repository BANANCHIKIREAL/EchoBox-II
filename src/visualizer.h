#pragma once
#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QAudioBuffer>
#include <complex>
#include <vector>

class Visualizer : public QWidget {
    Q_OBJECT
public:
    static constexpr int BARS = 22;

    explicit Visualizer(QWidget *parent = nullptr);
    QSize sizeHint() const override;

    void setActive(bool active);
    void feedAudioBuffer(const QAudioBuffer &buffer);  // реальный звук

protected:
    void paintEvent(QPaintEvent *) override;

private slots:
    void tick();

private:
    static void   fft(std::vector<std::complex<float>> &data);
    QVector<float> computeFFTBands(const float *mono, int count);

    QTimer        *m_timer;
    QVector<float> m_h;
    QVector<float> m_t;
    bool           m_active   = false;
    bool           m_hasBands = false;
    float          m_auroraPhase = 0.0f;
};
