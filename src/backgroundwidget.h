#pragma once
#include <QWidget>
#include <QTimer>
#include <QImage>
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
};
