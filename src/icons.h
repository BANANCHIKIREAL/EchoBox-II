#pragma once
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QPainterPath>
#include <QLineF>
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

// Полигон со скруглёнными углами (каждый угол — квадратичная кривая к
// точке, отступленной от вершины на radius по обеим сторонам) — тот же
// приём, что и скруглённые прямоугольники у pause(), но для треугольника
inline QPainterPath roundedPolygon(const QVector<QPointF> &pts, qreal radius) {
    QPainterPath path;
    const int n = pts.size();
    for (int i = 0; i < n; ++i) {
        const QPointF prev = pts[(i - 1 + n) % n];
        const QPointF cur  = pts[i];
        const QPointF next = pts[(i + 1) % n];
        const qreal lenPrev = QLineF(cur, prev).length();
        const qreal lenNext = QLineF(cur, next).length();
        const qreal rp = qMin(radius, lenPrev * 0.5);
        const qreal rn = qMin(radius, lenNext * 0.5);
        const QPointF p1 = cur + (prev - cur) * (rp / lenPrev);
        const QPointF p2 = cur + (next - cur) * (rn / lenNext);
        if (i == 0) path.moveTo(p1);
        else        path.lineTo(p1);
        path.quadTo(cur, p2);
    }
    path.closeSubpath();
    return path;
}

inline QIcon play(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(c);
    const QVector<QPointF> t = {
        QPointF(sz*0.20, sz*0.10), QPointF(sz*0.88, sz*0.50), QPointF(sz*0.20, sz*0.90)
    };
    p.drawPath(roundedPolygon(t, sz*0.09));
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

inline QIcon minimize(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.11f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.drawLine(QPointF(sz*0.18f, sz*0.5f), QPointF(sz*0.82f, sz*0.5f));
    return QIcon(pm);
}

inline QIcon closeIcon(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.11f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.drawLine(QPointF(sz*0.22f, sz*0.22f), QPointF(sz*0.78f, sz*0.78f));
    p.drawLine(QPointF(sz*0.78f, sz*0.22f), QPointF(sz*0.22f, sz*0.78f));
    return QIcon(pm);
}

inline QIcon dockTop(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.11f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    // Стрелка вверх — "прижать к верху экрана"
    p.drawLine(QPointF(sz*0.24f, sz*0.62f), QPointF(sz*0.5f, sz*0.30f));
    p.drawLine(QPointF(sz*0.5f,  sz*0.30f), QPointF(sz*0.76f, sz*0.62f));
    // Полоса — "во всю ширину"
    p.drawLine(QPointF(sz*0.16f, sz*0.84f), QPointF(sz*0.84f, sz*0.84f));
    return QIcon(pm);
}

inline QIcon trash(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.09f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    // Крышка
    p.drawLine(QPointF(sz*0.20f, sz*0.30f), QPointF(sz*0.80f, sz*0.30f));
    p.drawLine(QPointF(sz*0.40f, sz*0.30f), QPointF(sz*0.44f, sz*0.18f));
    p.drawLine(QPointF(sz*0.44f, sz*0.18f), QPointF(sz*0.56f, sz*0.18f));
    p.drawLine(QPointF(sz*0.56f, sz*0.18f), QPointF(sz*0.60f, sz*0.30f));
    // Корзина
    QPainterPath body;
    body.moveTo(sz*0.28f, sz*0.34f);
    body.lineTo(sz*0.33f, sz*0.86f);
    body.lineTo(sz*0.67f, sz*0.86f);
    body.lineTo(sz*0.72f, sz*0.34f);
    p.drawPath(body);
    // Полоски
    p.drawLine(QPointF(sz*0.41f, sz*0.44f), QPointF(sz*0.43f, sz*0.76f));
    p.drawLine(QPointF(sz*0.5f,  sz*0.44f), QPointF(sz*0.5f,  sz*0.76f));
    p.drawLine(QPointF(sz*0.59f, sz*0.44f), QPointF(sz*0.57f, sz*0.76f));
    return QIcon(pm);
}

inline QIcon sliders(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.09f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    const float ys[] = {0.28f, 0.5f, 0.72f};
    const float kx[] = {0.62f, 0.36f, 0.58f};
    for (int i = 0; i < 3; ++i) {
        const float y = sz * ys[i];
        p.drawLine(QPointF(sz*0.16f, y), QPointF(sz*0.84f, y));
        p.setBrush(c);
        p.drawEllipse(QPointF(sz * kx[i], y), sz*0.075f, sz*0.075f);
    }
    return QIcon(pm);
}

inline QIcon folder(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.09f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(sz*0.14f, sz*0.30f);
    path.lineTo(sz*0.14f, sz*0.78f);
    path.lineTo(sz*0.86f, sz*0.78f);
    path.lineTo(sz*0.86f, sz*0.38f);
    path.lineTo(sz*0.46f, sz*0.38f);
    path.lineTo(sz*0.38f, sz*0.24f);
    path.lineTo(sz*0.14f, sz*0.24f);
    path.closeSubpath();
    p.drawPath(path);
    return QIcon(pm);
}

inline QIcon windowIcon(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.09f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(sz*0.14f, sz*0.20f, sz*0.72f, sz*0.60f), sz*0.06f, sz*0.06f);
    p.drawLine(QPointF(sz*0.14f, sz*0.36f), QPointF(sz*0.86f, sz*0.36f));
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(sz*0.24f, sz*0.28f), sz*0.028f, sz*0.028f);
    p.drawEllipse(QPointF(sz*0.33f, sz*0.28f), sz*0.028f, sz*0.028f);
    return QIcon(pm);
}

inline QIcon link(QColor c, int sz = 24) {
    QPixmap pm = makePixmap(sz);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, sz * 0.10f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.save();
    p.translate(sz*0.36f, sz*0.64f); p.rotate(-45);
    p.drawRoundedRect(QRectF(-sz*0.20f, -sz*0.13f, sz*0.40f, sz*0.26f), sz*0.13f, sz*0.13f);
    p.restore();
    p.save();
    p.translate(sz*0.64f, sz*0.36f); p.rotate(-45);
    p.drawRoundedRect(QRectF(-sz*0.20f, -sz*0.13f, sz*0.40f, sz*0.26f), sz*0.13f, sz*0.13f);
    p.restore();
    return QIcon(pm);
}

} // namespace Ico
