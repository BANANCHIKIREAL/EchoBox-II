#pragma once
#include <QPixmap>
#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QFont>
#include <QRectF>
#include <cmath>
#include "thememanager.h"

inline QPixmap createLogo(int size, const ThemePalette &baseTheme,
                          const QString &style = "classic") {
    ThemePalette theme = baseTheme;
    const QString iconStyle = style.toLower();
    const bool roundedTile = iconStyle == "cosmic" || iconStyle == "aurora"
        || iconStyle == "sunset" || iconStyle == "ocean"
        || iconStyle == "mono" || iconStyle == "ruby";

    // App-icon variants are intentionally independent from the UI theme.
    // Each keeps the EchoBox equalizer mark while changing its own identity.
    if (iconStyle == "cosmic") {
        theme.mantle = QColor("#100b25"); theme.surface2 = QColor("#392766");
        theme.accent = QColor("#a78bfa"); theme.accent2 = QColor("#38bdf8");
        theme.danger = QColor("#f472b6"); theme.teal = QColor("#67e8f9");
        theme.subtext0 = QColor("#ddd6fe");
    } else if (iconStyle == "aurora") {
        theme.mantle = QColor("#071d22"); theme.surface2 = QColor("#164e63");
        theme.accent = QColor("#4ade80"); theme.accent2 = QColor("#38bdf8");
        theme.danger = QColor("#c084fc"); theme.teal = QColor("#2dd4bf");
        theme.subtext0 = QColor("#ccfbf1");
    } else if (iconStyle == "sunset") {
        theme.mantle = QColor("#2a1020"); theme.surface2 = QColor("#71334d");
        theme.accent = QColor("#fb7185"); theme.accent2 = QColor("#f59e0b");
        theme.danger = QColor("#f43f5e"); theme.teal = QColor("#fbbf24");
        theme.subtext0 = QColor("#ffe4e6");
    } else if (iconStyle == "ocean") {
        theme.mantle = QColor("#06182b"); theme.surface2 = QColor("#164e7a");
        theme.accent = QColor("#38bdf8"); theme.accent2 = QColor("#6366f1");
        theme.danger = QColor("#22d3ee"); theme.teal = QColor("#2dd4bf");
        theme.subtext0 = QColor("#dbeafe");
    } else if (iconStyle == "mono") {
        theme.mantle = QColor("#111318"); theme.surface2 = QColor("#3f4652");
        theme.accent = QColor("#e2e8f0"); theme.accent2 = QColor("#94a3b8");
        theme.danger = QColor("#f8fafc"); theme.teal = QColor("#cbd5e1");
        theme.subtext0 = QColor("#e5e7eb");
    } else if (iconStyle == "ruby") {
        theme.mantle = QColor("#240912"); theme.surface2 = QColor("#641d36");
        theme.accent = QColor("#fb7185"); theme.accent2 = QColor("#a855f7");
        theme.danger = QColor("#f43f5e"); theme.teal = QColor("#f0abfc");
        theme.subtext0 = QColor("#ffe4e6");
    }

    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const float s = float(size);

    // Outer glow
    QRadialGradient glow(s*0.5f, s*0.5f, s*0.5f);
    glow.setColorAt(0.55f, Qt::transparent);
    QColor glowColor = theme.accent;
    glowColor.setAlpha(62);
    glow.setColorAt(0.78f, glowColor);
    glow.setColorAt(1.00f, Qt::transparent);
    p.fillRect(pm.rect(), glow);

    // Background circle
    QRadialGradient bg(s*0.38f, s*0.33f, s*0.58f);
    bg.setColorAt(0.0f, theme.surface2);
    bg.setColorAt(1.0f, theme.mantle);
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    const QRectF bodyRect(s*0.04f, s*0.04f, s*0.92f, s*0.92f);
    if (roundedTile)
        p.drawRoundedRect(bodyRect, s * 0.23f, s * 0.23f);
    else
        p.drawEllipse(bodyRect);

    // Inner ring
    p.setBrush(Qt::NoBrush);
    QColor ring = theme.accent;
    ring.setAlpha(72);
    p.setPen(QPen(ring, s*0.014f));
    const QRectF ringRect(s*0.09f, s*0.09f, s*0.82f, s*0.82f);
    if (roundedTile)
        p.drawRoundedRect(ringRect, s * 0.18f, s * 0.18f);
    else
        p.drawEllipse(ringRect);

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
        barG.setColorAt(0.0f, theme.danger);
        barG.setColorAt(0.4f, theme.accent);
        barG.setColorAt(0.8f, theme.accent2);
        barG.setColorAt(1.0f, theme.teal);

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
        QColor label = theme.subtext0;
        label.setAlpha(210);
        p.setPen(label);
        QFont f("Segoe UI", qMax(5, int(s * 0.12f)), QFont::Bold);
        p.setFont(f);
        p.drawText(QRectF(0, s*0.73f, s, s*0.20f),
                   Qt::AlignHCenter | Qt::AlignVCenter, "II");
    }

    return pm;
}
