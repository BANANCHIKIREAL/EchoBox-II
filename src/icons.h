#pragma once

#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

namespace Ico {

inline QPixmap makePixmap(int size) {
    const qreal dpr = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(qRound(size * dpr), qRound(size * dpr));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

inline void setup(QPainter &p, QColor color, int size, qreal width = 2.0) {
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0, size / 24.0);
    p.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
}

inline QIcon play(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.setPen(Qt::NoPen); p.setBrush(c);
    QPainterPath path; path.moveTo(7, 4.6); path.quadTo(6, 4, 6, 5.8);
    path.lineTo(6, 18.2); path.quadTo(6, 20, 7.5, 19.1);
    path.lineTo(18, 12.9); path.quadTo(19.5, 12, 18, 11.1); path.closeSubpath();
    p.drawPath(path); return QIcon(pm);
}

inline QIcon pause(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.setPen(Qt::NoPen); p.setBrush(c);
    p.drawRoundedRect(QRectF(6, 4, 4, 16), 1.4, 1.4);
    p.drawRoundedRect(QRectF(14, 4, 4, 16), 1.4, 1.4); return QIcon(pm);
}

inline QIcon stop(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.setPen(Qt::NoPen); p.setBrush(c); p.drawRoundedRect(QRectF(5, 5, 14, 14), 2, 2);
    return QIcon(pm);
}

inline QIcon prev(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.setPen(Qt::NoPen); p.setBrush(c); p.drawRoundedRect(QRectF(5, 5, 2.8, 14), 1, 1);
    QPolygonF triangle; triangle << QPointF(18.5, 5) << QPointF(8.2, 12) << QPointF(18.5, 19);
    p.drawPolygon(triangle); return QIcon(pm);
}

inline QIcon next(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.setPen(Qt::NoPen); p.setBrush(c); p.drawRoundedRect(QRectF(16.2, 5, 2.8, 14), 1, 1);
    QPolygonF triangle; triangle << QPointF(5.5, 5) << QPointF(15.8, 12) << QPointF(5.5, 19);
    p.drawPolygon(triangle); return QIcon(pm);
}

inline QIcon volume(int level, QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    QPainterPath speaker; speaker.moveTo(5, 9); speaker.lineTo(8, 9); speaker.lineTo(12, 5.5);
    speaker.lineTo(12, 18.5); speaker.lineTo(8, 15); speaker.lineTo(5, 15); speaker.closeSubpath();
    p.drawPath(speaker);
    if (level <= 0) {
        p.drawLine(QPointF(16, 9), QPointF(21, 14)); p.drawLine(QPointF(21, 9), QPointF(16, 14));
    } else {
        if (level >= 1) p.drawArc(QRectF(11, 8, 5, 8), -55 * 16, 110 * 16);
        if (level >= 2) p.drawArc(QRectF(10.5, 5, 9, 14), -52 * 16, 104 * 16);
        if (level >= 3) p.drawArc(QRectF(10, 2.5, 12, 19), -48 * 16, 96 * 16);
    }
    return QIcon(pm);
}

inline QIcon shuffle(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    QPainterPath upper; upper.moveTo(4, 7); upper.lineTo(7, 7); upper.cubicTo(11, 7, 13, 17, 17, 17); upper.lineTo(20, 17);
    QPainterPath lower; lower.moveTo(4, 17); lower.lineTo(7, 17); lower.cubicTo(11, 17, 13, 7, 17, 7); lower.lineTo(20, 7);
    p.drawPath(upper); p.drawPath(lower);
    p.drawLine(QPointF(17, 4), QPointF(20, 7)); p.drawLine(QPointF(20, 7), QPointF(17, 10));
    p.drawLine(QPointF(17, 14), QPointF(20, 17)); p.drawLine(QPointF(20, 17), QPointF(17, 20));
    return QIcon(pm);
}

inline void drawRepeat(QPainter &p) {
    p.save();
    const QColor color = p.pen().color();
    p.setPen(QPen(color, 2.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    QPainterPath upper;
    upper.moveTo(5, 13.5);
    upper.cubicTo(5, 9.3, 7.3, 7, 11.5, 7);
    upper.lineTo(18, 7);
    p.drawPath(upper);

    QPainterPath lower;
    lower.moveTo(19, 10.5);
    lower.cubicTo(19, 14.7, 16.7, 17, 12.5, 17);
    lower.lineTo(6, 17);
    p.drawPath(lower);

    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPolygon(QPolygonF() << QPointF(17, 3.8) << QPointF(21, 7)
                              << QPointF(17, 10.2));
    p.drawPolygon(QPolygonF() << QPointF(7, 13.8) << QPointF(3, 17)
                              << QPointF(7, 20.2));
    p.restore();
}

inline QIcon repeatAll(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size); drawRepeat(p); return QIcon(pm);
}

inline QIcon repeatOne(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size); drawRepeat(p);
    p.drawLine(QPointF(11, 11), QPointF(13, 9.5)); p.drawLine(QPointF(13, 9.5), QPointF(13, 14.5));
    return QIcon(pm);
}

inline QIcon microphone(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.drawRoundedRect(QRectF(8, 3, 8, 12), 4, 4);
    QPainterPath arc; arc.moveTo(5, 11); arc.cubicTo(5, 16, 8, 19, 12, 19); arc.cubicTo(16, 19, 19, 16, 19, 11); p.drawPath(arc);
    p.drawLine(QPointF(12, 19), QPointF(12, 22)); p.drawLine(QPointF(9, 22), QPointF(15, 22)); return QIcon(pm);
}

inline QIcon expand(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.drawLine(4, 9, 4, 4); p.drawLine(4, 4, 9, 4); p.drawLine(15, 4, 20, 4); p.drawLine(20, 4, 20, 9);
    p.drawLine(20, 15, 20, 20); p.drawLine(20, 20, 15, 20); p.drawLine(9, 20, 4, 20); p.drawLine(4, 20, 4, 15);
    return QIcon(pm);
}

inline QIcon minimize(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size); p.drawLine(5, 12, 19, 12); return QIcon(pm);
}

inline QIcon closeIcon(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size); p.drawLine(6, 6, 18, 18); p.drawLine(18, 6, 6, 18); return QIcon(pm);
}

inline QIcon dockTop(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.drawRoundedRect(QRectF(3, 4, 18, 16), 2, 2); p.drawLine(3, 9, 21, 9);
    p.drawLine(9, 16, 12, 13); p.drawLine(12, 13, 15, 16); return QIcon(pm);
}

inline QIcon trash(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.drawLine(4, 7, 20, 7); p.drawLine(9, 7, 9, 4); p.drawLine(9, 4, 15, 4); p.drawLine(15, 4, 15, 7);
    QPainterPath body; body.moveTo(6, 7); body.lineTo(7, 20); body.lineTo(17, 20); body.lineTo(18, 7); p.drawPath(body);
    p.drawLine(10, 11, 10, 16); p.drawLine(14, 11, 14, 16); return QIcon(pm);
}

inline QIcon sliders(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.drawLine(4, 7, 20, 7); p.drawLine(4, 12, 20, 12); p.drawLine(4, 17, 20, 17);
    p.setBrush(c); p.drawEllipse(QPointF(9, 7), 2, 2); p.drawEllipse(QPointF(15, 12), 2, 2); p.drawEllipse(QPointF(11, 17), 2, 2);
    return QIcon(pm);
}

inline QIcon folder(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    QPainterPath path; path.moveTo(3, 6); path.lineTo(9, 6); path.lineTo(11, 8); path.lineTo(21, 8);
    path.lineTo(21, 19); path.quadTo(21, 21, 19, 21); path.lineTo(5, 21); path.quadTo(3, 21, 3, 19); path.closeSubpath();
    p.drawPath(path); return QIcon(pm);
}

inline QIcon windowIcon(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.drawRoundedRect(QRectF(3, 4, 18, 16), 2, 2); p.drawLine(3, 9, 21, 9);
    p.setPen(Qt::NoPen); p.setBrush(c); p.drawEllipse(QPointF(6.5, 6.5), .8, .8); p.drawEllipse(QPointF(9.5, 6.5), .8, .8);
    return QIcon(pm);
}

inline QIcon link(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    QPainterPath a; a.moveTo(10, 13); a.lineTo(8.5, 14.5); a.cubicTo(6.5, 16.5, 3.5, 13.5, 5.5, 11.5); a.lineTo(8, 9);
    QPainterPath b; b.moveTo(14, 11); b.lineTo(15.5, 9.5); b.cubicTo(17.5, 7.5, 20.5, 10.5, 18.5, 12.5); b.lineTo(16, 15);
    p.drawPath(a); p.drawPath(b); p.drawLine(9, 15, 15, 9); return QIcon(pm);
}

inline QIcon equalizer(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.drawLine(6, 4, 6, 20); p.drawLine(12, 4, 12, 20); p.drawLine(18, 4, 18, 20);
    p.setBrush(c); p.drawRoundedRect(QRectF(4, 7, 4, 4), 1, 1); p.drawRoundedRect(QRectF(10, 13, 4, 4), 1, 1); p.drawRoundedRect(QRectF(16, 6, 4, 4), 1, 1);
    return QIcon(pm);
}

inline QIcon music(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(QRectF(7.8, 4.5, 11.6, 3.0), 1.1, 1.1);
    p.drawRoundedRect(QRectF(7.8, 5.7, 2.7, 11.8), 1.1, 1.1);
    p.drawRoundedRect(QRectF(16.7, 5.7, 2.7, 11.8), 1.1, 1.1);
    p.drawEllipse(QPointF(6.4, 17.5), 3.4, 2.5);
    p.drawEllipse(QPointF(15.3, 17.5), 3.4, 2.5);
    return QIcon(pm);
}

inline QIcon arrowUp(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size); p.drawLine(12, 20, 12, 5); p.drawLine(6, 11, 12, 5); p.drawLine(18, 11, 12, 5); return QIcon(pm);
}

inline QIcon arrowDown(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size); p.drawLine(12, 4, 12, 19); p.drawLine(6, 13, 12, 19); p.drawLine(18, 13, 12, 19); return QIcon(pm);
}

inline QIcon download(QColor c, int size = 24) {
    QPixmap pm = makePixmap(size); QPainter p(&pm); setup(p, c, size, 2.2);
    p.drawLine(QPointF(12, 3.5), QPointF(12, 14.5));
    p.drawLine(QPointF(7.5, 10.5), QPointF(12, 15));
    p.drawLine(QPointF(16.5, 10.5), QPointF(12, 15));
    QPainterPath tray;
    tray.moveTo(5, 16.5);
    tray.lineTo(5, 18.5);
    tray.quadTo(5, 20.5, 7, 20.5);
    tray.lineTo(17, 20.5);
    tray.quadTo(19, 20.5, 19, 18.5);
    tray.lineTo(19, 16.5);
    p.drawPath(tray);
    return QIcon(pm);
}

}
