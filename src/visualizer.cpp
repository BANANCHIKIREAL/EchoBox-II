#include "visualizer.h"

#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QtMath>
#include <complex>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>


void Visualizer::fft(std::vector<std::complex<float>> &data) {
    const int n = int(data.size());

    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }

    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * float(M_PI) / float(len);
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int k = 0; k < len / 2; ++k) {
                auto u = data[i + k];
                auto v = data[i + k + len/2] * w;
                data[i + k]         = u + v;
                data[i + k + len/2] = u - v;
                w *= wlen;
            }
        }
    }
}


QVector<float> Visualizer::computeFFTBands(const float *mono, int count) {
    constexpr int N = 2048;

    std::vector<std::complex<float>> data(N, {0, 0});
    int fill = std::min(count, N);
    for (int i = 0; i < fill; ++i) {
        float w = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * i / (fill - 1)));
        data[i] = {mono[i] * w, 0.0f};
    }

    fft(data);

    std::vector<float> mag(N / 2);
    for (int i = 0; i < N / 2; ++i)
        mag[i] = std::abs(data[i]) / (N / 4.0f);

    QVector<float> bands(BARS, 0.0f);
    const float logMin = std::log10(2.0f);
    const float logMax = std::log10(460.0f);

    for (int b = 0; b < BARS; ++b) {
        float lo = std::pow(10.0f, logMin + (logMax - logMin) * float(b)     / BARS);
        float hi = std::pow(10.0f, logMin + (logMax - logMin) * float(b + 1) / BARS);
        int iLo = std::max(0, int(lo));
        int iHi = std::min(int(mag.size()) - 1, int(hi) + 1);

        float peak = 0.0f;
        for (int i = iLo; i <= iHi; ++i)
            peak = std::max(peak, mag[i]);

        bands[b] = std::min(peak * 5.5f, 1.0f);
    }
    return bands;
}


Visualizer::Visualizer(QWidget *parent) : QWidget(parent) {
    m_h.fill(0.0f, BARS);
    m_t.fill(0.0f, BARS);
    m_themeColors = {
        QColor(0x94,0xe2,0xd5), QColor(0x89,0xb4,0xfa),
        QColor(0xcb,0xa6,0xf7), QColor(0xf3,0x8b,0xa8)
    };

    m_timer = new QTimer(this);
    m_timer->setInterval(30);
    connect(m_timer, &QTimer::timeout, this, &Visualizer::tick);
    m_timer->start();

    setMinimumHeight(62);
    setMaximumHeight(80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void Visualizer::setThemeColors(const QColor &background,
                                const QVector<QColor> &colors) {
    m_background = background;
    if (!colors.isEmpty()) m_themeColors = colors;
    update();
}

QSize Visualizer::sizeHint() const { return {400, 70}; }

void Visualizer::setActive(bool active) {
    m_active   = active;
    m_hasBands = false;
    m_t.fill(0.0f);
}

void Visualizer::feedAudioBuffer(const QAudioBuffer &buffer) {
    if (!buffer.isValid() || !m_active) return;

    const auto fmt      = buffer.format();
    const int sampleCount = buffer.sampleCount();
    const int channels  = fmt.channelCount();
    if (channels < 1 || sampleCount < 1) return;

    const int monoCount = sampleCount / channels;
    std::vector<float> mono(monoCount, 0.0f);

    if (fmt.sampleFormat() == QAudioFormat::Int16) {
        const auto *raw = buffer.constData<qint16>();
        for (int i = 0; i < monoCount; ++i) {
            float s = 0;
            for (int c = 0; c < channels; ++c)
                s += raw[i * channels + c] / 32768.0f;
            mono[i] = s / channels;
        }
    } else if (fmt.sampleFormat() == QAudioFormat::Float) {
        const auto *raw = buffer.constData<float>();
        for (int i = 0; i < monoCount; ++i) {
            float s = 0;
            for (int c = 0; c < channels; ++c)
                s += raw[i * channels + c];
            mono[i] = s / channels;
        }
    } else {
        return;
    }

    m_t      = computeFFTBands(mono.data(), monoCount);
    m_hasBands = true;
}


void Visualizer::tick() {
    m_auroraPhase += 0.012f;
    bool changed = false;

    if (!m_hasBands && m_active) {
        auto *rng = QRandomGenerator::global();
        for (int i = 0; i < BARS; ++i)
            if (rng->bounded(7) == 0)
                m_t[i] = rng->bounded(100) / 100.0f;
    }

    for (int i = 0; i < BARS; ++i) {
        float d = m_t[i] - m_h[i];
        float speed = (d > 0) ? 0.28f : 0.18f;
        if (qAbs(d) > 0.002f) { m_h[i] += d * speed; changed = true; }
        else m_h[i] = m_t[i];

    }

    if (changed || m_active) update();
}


void Visualizer::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const float W  = float(width());
    const float H  = float(height());
    const float bw = W / BARS;
    const float gap = bw * 0.26f;
    const float t  = m_auroraPhase;

    p.fillRect(rect(), m_background);
    {
        QRadialGradient g(W * (0.5f + 0.2f * qSin(t)),
                          H * (0.5f + 0.2f * qCos(t * 0.7f)),
                          W * 0.55f);
        const QColor glowA = m_themeColors.value(2, QColor(0xcb,0xa6,0xf7));
        const QColor glowB = m_themeColors.value(1, QColor(0x89,0xb4,0xfa));
        g.setColorAt(0.0f, QColor(glowA.red(), glowA.green(), glowA.blue(), 18));
        g.setColorAt(0.5f, QColor(glowB.red(), glowB.green(), glowB.blue(), 10));
        g.setColorAt(1.0f, Qt::transparent);
        p.fillRect(rect(), g);
    }

    for (int i = 0; i < BARS; ++i) {
        const float bh  = qMax(m_h[i] * (H - 8.0f), 2.0f);
        const float x   = i * bw + gap * 0.5f;
        const float y   = H - bh;
        const float fw  = bw - gap;

        QLinearGradient grad(x, y + bh, x, y);
        grad.setColorAt(0.00f, m_themeColors.value(0, QColor(0x94,0xe2,0xd5)));
        grad.setColorAt(0.45f, m_themeColors.value(1, QColor(0x89,0xb4,0xfa)));
        grad.setColorAt(0.80f, m_themeColors.value(2, QColor(0xcb,0xa6,0xf7)));
        grad.setColorAt(1.00f, m_themeColors.value(3, QColor(0xf3,0x8b,0xa8)));

        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(x, y, fw, bh), 2.5f, 2.5f);

        QLinearGradient topGlow(x, y, x, y + bh * 0.18f);
        topGlow.setColorAt(0.0f, QColor(0xff, 0xff, 0xff, 55));
        topGlow.setColorAt(1.0f, Qt::transparent);
        p.fillRect(QRectF(x, y, fw, bh * 0.18f), topGlow);

    }

    p.setOpacity(0.10f);
    for (int i = 0; i < BARS; ++i) {
        const float bh  = qMax(m_h[i] * (H - 8.0f), 2.0f) * 0.3f;
        const float x   = i * bw + gap * 0.5f;
        const float fw  = bw - gap;
        QLinearGradient ref(x, H, x, H - bh);
        const QColor reflection = m_themeColors.value(1, QColor(0x89,0xb4,0xfa));
        ref.setColorAt(0.0f, QColor(reflection.red(), reflection.green(), reflection.blue(), 120));
        ref.setColorAt(1.0f, Qt::transparent);
        p.fillRect(QRectF(x, H - bh, fw, bh), ref);
    }
    p.setOpacity(1.0f);
}
