#pragma once
#include <QWidget>
#include <QTimer>
#include <QImage>
#include <QColor>
#include <QVector>
#include <vector>

struct BGParticle {
    float x, y;
    float vy, wobble, wobbleAmp, phase;
    float size, alpha;
    int   colorIdx;
};

class AuroraWidget : public QWidget {
    Q_OBJECT
public:
    explicit AuroraWidget(QWidget *parent = nullptr);
    void setLightMode(bool) {}
    void setAmplitude(float amp);
    void setThemeColors(const QColor &top, const QColor &middle, const QColor &bottom,
                        const QVector<QColor> &accents);

protected:
    void paintEvent(QPaintEvent *) override;

private slots:
    void tick();

private:
    void initParticles();

    QTimer  *m_timer     = nullptr;
    float    m_phase     = 0.f;
    float    m_amp       = 0.f;
    float    m_prevAmp   = 0.f;
    float    m_beat      = 0.f;

    std::vector<BGParticle> m_particles;
    bool     m_particlesInited = false;
    QImage   m_grain;
    int      m_grainTick = 0;
    QColor   m_baseTop {0x10, 0x10, 0x1c};
    QColor   m_baseMiddle {0x18, 0x18, 0x25};
    QColor   m_baseBottom {0x0c, 0x0c, 0x16};
    QVector<QColor> m_themeColors;
};
