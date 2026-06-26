#pragma once
#include <QWidget>
#include <QTimer>

class AuroraWidget : public QWidget {
    Q_OBJECT
public:
    explicit AuroraWidget(QWidget *parent = nullptr);
    void setLightMode(bool light);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QTimer *m_timer;
    float   m_phase     = 0.0f;
    bool    m_lightMode = false;
};
