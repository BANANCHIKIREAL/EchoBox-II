#pragma once
#include <QWidget>
#include <QTimer>
#include <QVector>
#include <QAudioBuffer>
#include <QColor>
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
    void setThemeColors(const QColor &background, const QVector<QColor> &colors);

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
    QColor         m_background {0x18, 0x18, 0x25};
    QVector<QColor> m_themeColors;
};
