#pragma once
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QGuiApplication>

namespace Ico {

// Creates a high-DPI aware pixmap for the given logical size
inline QPixmap makePixmap(int sz) {
    const qreal dpr = qGuiApp->devicePixelRatio();
    QPixmap pm(qRound(sz * dpr), qRound(sz * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    return pm;
}

inline QIcon play(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(c);
    QPolygonF t;
    t << QPointF(sz*0.22, sz*0.12) << QPointF(sz*0.87, sz*0.50) << QPointF(sz*0.22, sz*0.88);
    p.drawPolygon(t);
    return QIcon(pm);
}

inline QIcon pause(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(c);
    const int bw = qRound(sz*0.25), bh = qRound(sz*0.68), by = qRound(sz*0.16);
    p.drawRoundedRect(qRound(sz*0.14), by, bw, bh, 3, 3);
    p.drawRoundedRect(qRound(sz*0.61), by, bw, bh, 3, 3);
    return QIcon(pm);
}

inline QIcon stop(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(c);
    const int m = qRound(sz * 0.20);
    p.drawRect(m, m, sz - 2*m, sz - 2*m); // строгий квадрат — стандартный символ стоп
    return QIcon(pm);
}

inline QIcon prev(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(c);
    p.drawRoundedRect(qRound(sz*0.10), qRound(sz*0.15), qRound(sz*0.17), qRound(sz*0.70), 2, 2);
    QPolygonF t;
    t << QPointF(sz*0.88, sz*0.14) << QPointF(sz*0.33, sz*0.50) << QPointF(sz*0.88, sz*0.86);
    p.drawPolygon(t);
    return QIcon(pm);
}

inline QIcon next(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(c);
    QPolygonF t;
    t << QPointF(sz*0.12, sz*0.14) << QPointF(sz*0.67, sz*0.50) << QPointF(sz*0.12, sz*0.86);
    p.drawPolygon(t);
    p.drawRoundedRect(qRound(sz*0.73), qRound(sz*0.15), qRound(sz*0.17), qRound(sz*0.70), 2, 2);
    return QIcon(pm);
}

inline QIcon volume(int level, QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(c);
    QPolygonF cone;
    cone << QPointF(sz*0.08, sz*0.37) << QPointF(sz*0.32, sz*0.37)
         << QPointF(sz*0.52, sz*0.16) << QPointF(sz*0.52, sz*0.84)
         << QPointF(sz*0.32, sz*0.63) << QPointF(sz*0.08, sz*0.63);
    p.drawPolygon(cone);
    if (level == 0) {
        QPen xp(c, sz*0.10, Qt::SolidLine, Qt::RoundCap);
        p.setPen(xp); p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(sz*0.62, sz*0.36), QPointF(sz*0.90, sz*0.64));
        p.drawLine(QPointF(sz*0.90, sz*0.36), QPointF(sz*0.62, sz*0.64));
    } else {
        QPen wp(c, sz*0.09, Qt::SolidLine, Qt::RoundCap);
        p.setPen(wp); p.setBrush(Qt::NoBrush);
        if (level >= 1) p.drawArc(QRectF(sz*0.55, sz*0.31, sz*0.17, sz*0.38), -50*16, 100*16);
        if (level >= 2) p.drawArc(QRectF(sz*0.62, sz*0.21, sz*0.22, sz*0.58), -50*16, 100*16);
        if (level >= 3) p.drawArc(QRectF(sz*0.70, sz*0.11, sz*0.26, sz*0.78), -50*16, 100*16);
    }
    return QIcon(pm);
}

inline QIcon shuffle(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.09f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    // верхняя линия ↗
    p.drawLine(QPointF(sz*0.08f, sz*0.68f), QPointF(sz*0.38f, sz*0.68f));
    p.drawLine(QPointF(sz*0.38f, sz*0.68f), QPointF(sz*0.80f, sz*0.26f));
    // нижняя линия ↘
    p.drawLine(QPointF(sz*0.08f, sz*0.32f), QPointF(sz*0.38f, sz*0.32f));
    p.drawLine(QPointF(sz*0.38f, sz*0.32f), QPointF(sz*0.80f, sz*0.74f));
    // стрелки вправо
    p.setPen(Qt::NoPen); p.setBrush(c);
    const float aw = sz*0.10f;
    QPolygonF a1; a1 << QPointF(sz*0.96f, sz*0.26f) << QPointF(sz*0.78f, sz*0.16f) << QPointF(sz*0.78f, sz*0.36f); p.drawPolygon(a1);
    QPolygonF a2; a2 << QPointF(sz*0.96f, sz*0.74f) << QPointF(sz*0.78f, sz*0.64f) << QPointF(sz*0.78f, sz*0.84f); p.drawPolygon(a2);
    return QIcon(pm);
}

inline QIcon repeatAll(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.09f, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(sz*0.10f, sz*0.10f, sz*0.80f, sz*0.80f), 40*16, 280*16);
    // arrowhead
    p.setPen(Qt::NoPen); p.setBrush(c);
    QPolygonF a;
    a << QPointF(sz*0.88f, sz*0.20f) << QPointF(sz*0.72f, sz*0.10f) << QPointF(sz*0.72f, sz*0.30f);
    p.drawPolygon(a);
    return QIcon(pm);
}

inline QIcon repeatOne(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.09f, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(sz*0.10f, sz*0.10f, sz*0.80f, sz*0.80f), 40*16, 280*16);
    p.setPen(Qt::NoPen); p.setBrush(c);
    QPolygonF a;
    a << QPointF(sz*0.88f, sz*0.20f) << QPointF(sz*0.72f, sz*0.10f) << QPointF(sz*0.72f, sz*0.30f);
    p.drawPolygon(a);
    // "1"
    p.setPen(QPen(c, 1)); p.setBrush(Qt::NoBrush);
    QFont f; f.setPixelSize(qRound(sz * 0.32f)); f.setBold(true); p.setFont(f);
    p.drawText(QRectF(sz*0.34f, sz*0.32f, sz*0.32f, sz*0.36f), Qt::AlignCenter, "1");
    return QIcon(pm);
}

inline QIcon microphone(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(c);
    p.drawRoundedRect(QRectF(sz*0.34f, sz*0.05f, sz*0.32f, sz*0.50f), sz*0.16f, sz*0.16f);
    QPen ap(c, sz*0.09f, Qt::SolidLine, Qt::RoundCap);
    p.setPen(ap); p.setBrush(Qt::NoBrush);
    p.drawArc(QRectF(sz*0.17f, sz*0.30f, sz*0.66f, sz*0.44f), 0, -180*16);
    p.drawLine(QPointF(sz*0.50f, sz*0.74f), QPointF(sz*0.50f, sz*0.89f));
    p.drawLine(QPointF(sz*0.28f, sz*0.89f), QPointF(sz*0.72f, sz*0.89f));
    return QIcon(pm);
}

inline QIcon expand(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.11f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    // top-left corner
    p.drawLine(QPointF(sz*0.16f, sz*0.43f), QPointF(sz*0.16f, sz*0.16f));
    p.drawLine(QPointF(sz*0.16f, sz*0.16f), QPointF(sz*0.43f, sz*0.16f));
    // top-right corner
    p.drawLine(QPointF(sz*0.57f, sz*0.16f), QPointF(sz*0.84f, sz*0.16f));
    p.drawLine(QPointF(sz*0.84f, sz*0.16f), QPointF(sz*0.84f, sz*0.43f));
    // bottom-left corner
    p.drawLine(QPointF(sz*0.16f, sz*0.57f), QPointF(sz*0.16f, sz*0.84f));
    p.drawLine(QPointF(sz*0.16f, sz*0.84f), QPointF(sz*0.43f, sz*0.84f));
    // bottom-right corner
    p.drawLine(QPointF(sz*0.57f, sz*0.84f), QPointF(sz*0.84f, sz*0.84f));
    p.drawLine(QPointF(sz*0.84f, sz*0.84f), QPointF(sz*0.84f, sz*0.57f));
    return QIcon(pm);
}

} // namespace Ico
