#include "backgroundwidget.h"
#include <QPainter>
#include <QRadialGradient>
#include <QtMath>

AuroraWidget::AuroraWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, false);
    setAutoFillBackground(false);
    m_timer = new QTimer(this);
    m_timer->setInterval(20);
    connect(m_timer, &QTimer::timeout, [this] { m_phase += 0.0038f; update(); });
    m_timer->start();
}

void AuroraWidget::setLightMode(bool light) {
    if (m_lightMode != light) { m_lightMode = light; update(); }
}

void AuroraWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const float W = float(width());
    const float H = float(height());
    const float t = m_phase;

    if (m_lightMode) {
        // Light base (Catppuccin Latte)
        p.fillRect(rect(), QColor(0xef, 0xf1, 0xf5));

        struct Blob { float bx, by, br; int r, g, b, a; };
        const Blob blobs[] = {
            { W*(0.22f + 0.13f*qSin(t*0.70f)), H*(0.32f + 0.17f*qCos(t*0.50f)), W*0.55f, 0x7c, 0x7f, 0xff, 22 },
            { W*(0.74f + 0.11f*qCos(t*0.60f+1.57f)), H*(0.55f+0.14f*qSin(t*0.75f+0.80f)), W*0.48f, 0x88, 0x39, 0xef, 18 },
            { W*(0.50f + 0.09f*qSin(t*1.10f+2.50f)), H*(0.17f+0.12f*qCos(t*0.85f+1.00f)), W*0.38f, 0x04, 0xa5, 0xe5, 14 },
            { W*(0.82f + 0.08f*qSin(t*0.55f+3.20f)), H*(0.82f+0.09f*qCos(t*0.65f+2.10f)), W*0.34f, 0xea, 0x76, 0xcb, 12 },
            { W*(0.13f + 0.07f*qCos(t*0.45f+0.50f)), H*(0.76f+0.10f*qSin(t*0.80f+1.80f)), W*0.30f, 0xfe, 0x64, 0x0b, 10 },
            { W*(0.48f + 0.10f*qCos(t*0.95f+4.00f)), H*(0.50f+0.08f*qSin(t*1.20f+3.00f)), W*0.26f, 0x17, 0x92, 0x99, 10 },
        };
        for (const auto &bl : blobs) {
            QRadialGradient g(bl.bx, bl.by, bl.br);
            g.setColorAt(0.0f, QColor(bl.r, bl.g, bl.b, bl.a));
            g.setColorAt(0.6f, QColor(bl.r, bl.g, bl.b, bl.a / 4));
            g.setColorAt(1.0f, Qt::transparent);
            p.fillRect(rect(), g);
        }
        // Soft light vignette
        QRadialGradient vig(W*0.5f, H*0.5f, qMax(W, H)*0.75f);
        vig.setColorAt(0.35f, Qt::transparent);
        vig.setColorAt(1.00f, QColor(0xdc, 0xe0, 0xe8, 60));
        p.fillRect(rect(), vig);

    } else {
        // Dark base (Catppuccin Mocha)
        p.fillRect(rect(), QColor(0x18, 0x18, 0x25));

        struct Blob { float bx, by, br; int r, g, b, a; };
        const Blob blobs[] = {
            { W*(0.22f+0.13f*qSin(t*0.70f)), H*(0.32f+0.17f*qCos(t*0.50f)), W*0.50f, 0x89,0xb4,0xfa, 58 },
            { W*(0.74f+0.11f*qCos(t*0.60f+1.57f)), H*(0.55f+0.14f*qSin(t*0.75f+0.80f)), W*0.44f, 0xcb,0xa6,0xf7, 48 },
            { W*(0.50f+0.09f*qSin(t*1.10f+2.50f)), H*(0.17f+0.12f*qCos(t*0.85f+1.00f)), W*0.36f, 0x94,0xe2,0xd5, 32 },
            { W*(0.82f+0.08f*qSin(t*0.55f+3.20f)), H*(0.82f+0.09f*qCos(t*0.65f+2.10f)), W*0.32f, 0xf3,0x8b,0xa8, 24 },
            { W*(0.13f+0.07f*qCos(t*0.45f+0.50f)), H*(0.76f+0.10f*qSin(t*0.80f+1.80f)), W*0.30f, 0xfa,0xb3,0x87, 20 },
            { W*(0.48f+0.10f*qCos(t*0.95f+4.00f)), H*(0.50f+0.08f*qSin(t*1.20f+3.00f)), W*0.26f, 0x89,0xdc,0xeb, 18 },
        };
        for (const auto &bl : blobs) {
            QRadialGradient g(bl.bx, bl.by, bl.br);
            g.setColorAt(0.0f, QColor(bl.r, bl.g, bl.b, bl.a));
            g.setColorAt(0.5f, QColor(bl.r, bl.g, bl.b, bl.a / 3));
            g.setColorAt(1.0f, Qt::transparent);
            p.fillRect(rect(), g);
        }
        QRadialGradient vig(W*0.5f, H*0.5f, qMax(W, H)*0.72f);
        vig.setColorAt(0.40f, Qt::transparent);
        vig.setColorAt(1.00f, QColor(0x0d, 0x0d, 0x18, 160));
        p.fillRect(rect(), vig);
    }
}
