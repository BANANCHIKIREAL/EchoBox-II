#include "backgroundwidget.h"
#include <QPainter>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QtMath>
#include <cstdlib>

// Catppuccin Mocha accent palette
static const QColor kPalette[] = {
    { 0x89, 0xb4, 0xfa },  // Sapphire
    { 0xcb, 0xa6, 0xf7 },  // Mauve
    { 0x94, 0xe2, 0xd5 },  // Teal
    { 0xf3, 0x8b, 0xa8 },  // Pink
    { 0xfa, 0xb3, 0x87 },  // Peach
    { 0x89, 0xdc, 0xeb },  // Sky
    { 0xa6, 0xe3, 0xa1 },  // Green
};
static constexpr int kPaletteSize = 7;

AuroraWidget::AuroraWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);
    std::srand(12345);
    initParticles();
    m_timer = new QTimer(this);
    m_timer->setInterval(20);
    connect(m_timer, &QTimer::timeout, this, &AuroraWidget::tick);
    m_timer->start();
}

void AuroraWidget::setAmplitude(float amp)
{
    m_amp = m_amp * 0.72f + amp * 0.28f;
    if (amp > 0.38f && amp > m_prevAmp * 1.25f)
        m_beat = qMin(1.0f, m_beat + 0.55f);
    m_prevAmp = amp;
}

void AuroraWidget::initParticles()
{
    m_particles.resize(100);
    for (auto &p : m_particles) {
        p.x         = float(std::rand()) / RAND_MAX;
        p.y         = float(std::rand()) / RAND_MAX;
        p.vy        = 0.00025f + float(std::rand()) / RAND_MAX * 0.00070f;
        p.wobble    = 0.4f + float(std::rand()) / RAND_MAX * 2.2f;
        p.wobbleAmp =        float(std::rand()) / RAND_MAX;
        p.phase     = float(std::rand()) / RAND_MAX * 6.28f;
        p.size      = 1.2f + float(std::rand()) / RAND_MAX * 3.2f;
        p.alpha     = 0.35f + float(std::rand()) / RAND_MAX * 0.55f;
        p.colorIdx  = std::rand() % kPaletteSize;
    }
}

void AuroraWidget::tick()
{
    m_phase += 0.0038f;
    m_beat  *= 0.91f;

    for (auto &p : m_particles) {
        p.phase += 0.022f;
        p.y -= p.vy * (1.f + m_amp * 2.5f);
        p.x += p.wobbleAmp * std::sin(p.phase * p.wobble) * 0.00075f;
        if (p.y < -0.06f) {
            p.y = 1.06f + float(std::rand()) / RAND_MAX * 0.1f;
            p.x = float(std::rand()) / RAND_MAX;
            p.size = 1.2f + float(std::rand()) / RAND_MAX * 3.2f;
        }
        if (p.x < 0.f) p.x += 1.f;
        if (p.x > 1.f) p.x -= 1.f;
    }

    // Regenerate film grain every 3 frames
    if (++m_grainTick % 3 == 0) {
        constexpr int GS = 256;
        m_grain = QImage(GS, GS, QImage::Format_ARGB32);
        for (int gy = 0; gy < GS; ++gy) {
            auto *row = reinterpret_cast<QRgb *>(m_grain.scanLine(gy));
            for (int gx = 0; gx < GS; ++gx) {
                const int v = std::rand() & 0xff;
                row[gx] = qRgba(v, v, v, std::rand() % 14);
            }
        }
    }

    update();
}

void AuroraWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const float W = float(width());
    const float H = float(height());
    const float t = m_phase;

    // ── Deep gradient base ────────────────────────────────────────────────────
    QLinearGradient base(0, 0, 0, H);
    base.setColorAt(0.0, QColor(0x10, 0x10, 0x1c));
    base.setColorAt(0.5, QColor(0x18, 0x18, 0x25));
    base.setColorAt(1.0, QColor(0x0c, 0x0c, 0x16));
    p.fillRect(rect(), base);

    // ── Blobs ─────────────────────────────────────────────────────────────────
    struct Blob {
        float bx, by;       // base position (0..1)
        float ax, ay;       // motion amplitude
        float fx, fy;       // motion frequency
        float px, py;       // phase offset
        float radius;       // fraction of W
        int   r, g, b, a;
        float reactivity;   // response to beat/amp
    };

    static const Blob blobs[] = {
        // 4 large slow base blobs
        { 0.18f, 0.28f,  0.13f,0.16f,  0.52f,0.40f,  0.00f,0.00f,  0.58f,  0x89,0xb4,0xfa, 52, 0.7f },
        { 0.76f, 0.58f,  0.11f,0.13f,  0.57f,0.48f,  1.57f,0.80f,  0.54f,  0xcb,0xa6,0xf7, 50, 0.8f },
        { 0.48f, 0.82f,  0.09f,0.11f,  0.65f,0.60f,  3.14f,1.60f,  0.50f,  0x94,0xe2,0xd5, 38, 0.6f },
        { 0.86f, 0.14f,  0.08f,0.10f,  0.48f,0.43f,  4.71f,2.40f,  0.46f,  0xf3,0x8b,0xa8, 32, 0.6f },
        // 5 medium blobs
        { 0.33f, 0.62f,  0.11f,0.13f,  0.88f,0.75f,  2.50f,1.00f,  0.37f,  0xfa,0xb3,0x87, 46, 1.1f },
        { 0.64f, 0.28f,  0.10f,0.14f,  0.73f,0.82f,  0.70f,3.20f,  0.34f,  0x89,0xdc,0xeb, 42, 1.0f },
        { 0.14f, 0.74f,  0.09f,0.11f,  1.05f,0.63f,  1.80f,0.50f,  0.31f,  0xa6,0xe3,0xa1, 34, 0.9f },
        { 0.88f, 0.72f,  0.08f,0.09f,  0.82f,0.70f,  5.50f,1.20f,  0.28f,  0xcb,0xa6,0xf7, 38, 1.0f },
        { 0.44f, 0.14f,  0.10f,0.12f,  0.92f,0.85f,  0.30f,4.00f,  0.26f,  0x89,0xb4,0xfa, 30, 0.8f },
        // 3 small fast accent blobs (most reactive)
        { 0.24f, 0.44f,  0.15f,0.17f,  1.35f,1.18f,  2.10f,0.90f,  0.19f,  0xf3,0x8b,0xa8, 75, 1.6f },
        { 0.68f, 0.78f,  0.13f,0.15f,  1.55f,1.32f,  3.80f,2.70f,  0.17f,  0xfa,0xb3,0x87, 68, 1.5f },
        { 0.54f, 0.50f,  0.12f,0.14f,  1.25f,1.12f,  0.50f,1.40f,  0.20f,  0x94,0xe2,0xd5, 62, 1.4f },
    };

    for (const auto &bl : blobs) {
        const float bx = (bl.bx + bl.ax * qSin(t * bl.fx + bl.px)) * W;
        const float by = (bl.by + bl.ay * qCos(t * bl.fy + bl.py)) * H;
        const float br = bl.radius * W * (1.f + (m_amp * 0.28f + m_beat * 0.13f) * bl.reactivity);
        const int   ba = qMin(255, qRound(bl.a * (1.f + m_amp * 0.4f + m_beat * 0.25f)));

        QRadialGradient g(bx, by, br);
        g.setColorAt(0.00f, QColor(bl.r, bl.g, bl.b, ba));
        g.setColorAt(0.40f, QColor(bl.r, bl.g, bl.b, qMax(0, ba / 3)));
        g.setColorAt(0.75f, QColor(bl.r, bl.g, bl.b, qMax(0, ba / 10)));
        g.setColorAt(1.00f, Qt::transparent);
        p.fillRect(rect(), g);
    }

    // ── Particles ─────────────────────────────────────────────────────────────
    p.setPen(Qt::NoPen);
    for (const auto &pt : m_particles) {
        if (pt.y < 0.f || pt.y > 1.f) continue;
        const float px = pt.x * W;
        const float py = pt.y * H;
        const float sz = pt.size * (1.f + m_amp * 0.9f + m_beat * 0.4f);
        const float a  = pt.alpha * (0.55f + m_amp * 0.35f + m_beat * 0.25f);
        QColor col = kPalette[pt.colorIdx];

        // Outer halo
        col.setAlphaF(qMin(1.f, a * 0.12f));
        p.setBrush(col);
        p.drawEllipse(QPointF(px, py), sz * 3.0f, sz * 3.0f);

        // Mid glow
        col.setAlphaF(qMin(1.f, a * 0.30f));
        p.setBrush(col);
        p.drawEllipse(QPointF(px, py), sz * 1.6f, sz * 1.6f);

        // Bright core
        col.setAlphaF(qMin(1.f, a * 0.88f));
        p.setBrush(col);
        p.drawEllipse(QPointF(px, py), sz, sz);
    }

    // ── Film grain overlay ────────────────────────────────────────────────────
    if (!m_grain.isNull()) {
        p.setOpacity(0.038);
        p.drawImage(rect(), m_grain);
        p.setOpacity(1.0);
    }

    // ── Vignette ──────────────────────────────────────────────────────────────
    QRadialGradient vig(W * 0.5f, H * 0.5f, qMax(W, H) * 0.76f);
    vig.setColorAt(0.20f, Qt::transparent);
    vig.setColorAt(0.62f, QColor(0x08, 0x08, 0x12, 55));
    vig.setColorAt(1.00f, QColor(0x04, 0x04, 0x0c, 210));
    p.fillRect(rect(), vig);

    // ── Beat flash: центральная вспышка в такт музыке ─────────────────────────
    if (m_beat > 0.08f) {
        QRadialGradient flash(W * 0.5f, H * 0.42f, qMax(W, H) * 0.55f);
        flash.setColorAt(0.0f, QColor(0xcb, 0xa6, 0xf7, int(m_beat * 18)));
        flash.setColorAt(0.5f, QColor(0x89, 0xb4, 0xfa, int(m_beat *  8)));
        flash.setColorAt(1.0f, Qt::transparent);
        p.fillRect(rect(), flash);
    }
}
