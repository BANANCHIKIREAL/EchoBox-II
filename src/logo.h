#pragma once
#include <QPixmap>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QFont>
#include <QRectF>
#include <cmath>

inline QPixmap createLogo(int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const float s = float(size);

    // Outer glow
    QRadialGradient glow(s*0.5f, s*0.5f, s*0.5f);
    glow.setColorAt(0.55f, Qt::transparent);
    glow.setColorAt(0.78f, QColor(0xcb, 0xa6, 0xf7, 50));
    glow.setColorAt(1.00f, Qt::transparent);
    p.fillRect(pm.rect(), glow);

    // Background circle
    QRadialGradient bg(s*0.38f, s*0.33f, s*0.58f);
    bg.setColorAt(0.0f, QColor(0x45, 0x47, 0x5a));
    bg.setColorAt(1.0f, QColor(0x18, 0x18, 0x25));
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(s*0.04f, s*0.04f, s*0.92f, s*0.92f));

    // Inner ring
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0xcb, 0xa6, 0xf7, 55), s*0.014f));
    p.drawEllipse(QRectF(s*0.09f, s*0.09f, s*0.82f, s*0.82f));

    // EQ bars
    const int   bars    = 5;
    const float hgt[]   = {0.38f, 0.62f, 0.92f, 0.62f, 0.38f};
    const float bw      = s * 0.092f;
    const float gap     = s * 0.033f;
    const float totalW  = bars * bw + (bars - 1) * gap;
    const float startX  = (s - totalW) * 0.5f;
    const float cy      = s * 0.50f;

    for (int i = 0; i < bars; ++i) {
        float x = startX + i * (bw + gap);
        float h = s * hgt[i] * 0.58f;
        float y = cy - h * 0.5f;

        QLinearGradient barG(x, y, x, y + h);
        barG.setColorAt(0.0f, QColor(0xf3, 0x8b, 0xa8)); // pink top
        barG.setColorAt(0.4f, QColor(0xcb, 0xa6, 0xf7)); // mauve
        barG.setColorAt(0.8f, QColor(0x89, 0xb4, 0xfa)); // blue
        barG.setColorAt(1.0f, QColor(0x94, 0xe2, 0xd5)); // teal bottom

        p.setBrush(barG);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(x, y, bw, h), bw * 0.4f, bw * 0.4f);

        // Bar top glow cap
        QRadialGradient cap(x + bw*0.5f, y, bw*0.7f);
        cap.setColorAt(0.0f, QColor(0xff, 0xff, 0xff, 60));
        cap.setColorAt(1.0f, Qt::transparent);
        p.fillRect(QRectF(x, y - bw*0.3f, bw, bw*0.6f), cap);
    }

    // "II" text
    if (size >= 28) {
        p.setPen(QColor(0xa6, 0xad, 0xc8, 190));
        QFont f("Segoe UI", qMax(5, int(s * 0.12f)), QFont::Bold);
        p.setFont(f);
        p.drawText(QRectF(0, s*0.73f, s, s*0.20f),
                   Qt::AlignHCenter | Qt::AlignVCenter, "II");
    }

    return pm;
}
