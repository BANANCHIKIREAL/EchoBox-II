#include "waveformslider.h"
#include <QPainter>
#include <QMouseEvent>
#include <QAudioBuffer>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <cmath>

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <QAudioBufferOutput>
#endif

static constexpr int TARGET_BINS = 500;
static constexpr int H_PAD       = 3;

WaveformSlider::WaveformSlider(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

WaveformSlider::~WaveformSlider() {
    if (m_loader) m_loader->stop();
}

QSize WaveformSlider::sizeHint() const { return {400, 36}; }

// ── Slider API ───────────────────────────────────────────────────────────────

void WaveformSlider::setRange(int min, int max) {
    m_min = min; m_max = max;
    m_value = qBound(min, m_value, max);
    update();
}

void WaveformSlider::setValue(int v) {
    v = qBound(m_min, v, m_max);
    if (v == m_value) return;
    m_value = v;
    update();
}

// ── Waveform loading ─────────────────────────────────────────────────────────

void WaveformSlider::loadWaveform(const QUrl &url, qint64 durationMs) {
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
    Q_UNUSED(url) Q_UNUSED(durationMs) return;
#else
    clearWaveform();
    if (!url.isValid() || durationMs <= 0) return;

    m_durationMs    = durationMs;
    m_bins          = TARGET_BINS;
    m_peaks.assign(m_bins, 0.f);
    m_binsFilled    = 0;
    m_binAccum      = 0.f;
    m_binCount      = 0;
    m_samplesPerBin = 0;

    if (!m_loader) {
        m_loader  = new QMediaPlayer(this);

        // Muted output keeps the pipeline active so QAudioBufferOutput gets data
        m_silence = new QAudioOutput(this);
        m_silence->setVolume(0.f);
        m_loader->setAudioOutput(m_silence);

        // Same format as main player — guaranteed to work with the backend
        QAudioFormat fmt;
        fmt.setSampleRate(48000);
        fmt.setChannelCount(2);
        fmt.setSampleFormat(QAudioFormat::Float);
        m_loaderBuf = new QAudioBufferOutput(fmt, this);
        m_loader->setAudioBufferOutput(m_loaderBuf);

        connect(m_loaderBuf, &QAudioBufferOutput::audioBufferReceived,
                this, &WaveformSlider::onAudioBuffer);

        connect(m_loader, &QMediaPlayer::mediaStatusChanged,
                this, &WaveformSlider::onLoaderStatus);
    }

    m_loader->setSource(url);
    m_loader->play();
#endif
}

void WaveformSlider::setPeaks(const QVector<float> &peaks) {
    m_peaks.assign(peaks.begin(), peaks.end());
    m_bins       = int(m_peaks.size());
    m_binsFilled = int(m_peaks.size());
    update();
}

void WaveformSlider::clearWaveform() {
    if (m_loader) m_loader->stop();
    m_peaks.clear();
    m_binsFilled    = 0;
    m_binAccum      = 0.f;
    m_binCount      = 0;
    m_samplesPerBin = 0;
    m_durationMs    = 0;
    update();
}

// ── Theme ────────────────────────────────────────────────────────────────────

void WaveformSlider::setAccentColor(const QColor &c) { m_accent = c; update(); }
void WaveformSlider::setTrackColor (const QColor &c) { m_track  = c; update(); }

// ── Background decoder slots ─────────────────────────────────────────────────

void WaveformSlider::onAudioBuffer(const QAudioBuffer &buf) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if (m_binsFilled >= m_bins || m_bins == 0) return;

    const QAudioFormat fmt = buf.format();
    const int sr  = fmt.sampleRate();
    const int ch  = fmt.channelCount();
    const int frames = buf.frameCount();

    // Compute samplesPerBin once we know the actual sample rate
    if (m_samplesPerBin == 0 && sr > 0 && m_durationMs > 0) {
        const qint64 totalFrames = sr * m_durationMs / 1000;
        m_samplesPerBin = qMax(1, int(totalFrames / m_bins));
    }
    if (m_samplesPerBin == 0) return;

    const float *data = buf.constData<float>();
    const int prevFilled = m_binsFilled;
    for (int i = 0; i < frames && m_binsFilled < m_bins; ++i) {
        float v = 0.f;
        for (int c = 0; c < ch; ++c)
            v += std::abs(data[i * ch + c]);
        m_binAccum += v / ch;
        ++m_binCount;
        if (m_binCount >= m_samplesPerBin) {
            m_peaks[m_binsFilled++] = m_binAccum / m_binCount;
            m_binAccum = 0.f;
            m_binCount = 0;
        }
    }
    update();

    // Send partial peaks to mini slider every 25 bins so it updates progressively
    if (m_binsFilled != prevFilled && (m_binsFilled % 25 == 0 || m_binsFilled == m_bins))
        emit peaksReady(QVector<float>(m_peaks.begin(), m_peaks.begin() + m_binsFilled));
#else
    Q_UNUSED(buf)
#endif
}

void WaveformSlider::onLoaderStatus(QMediaPlayer::MediaStatus status) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if (status == QMediaPlayer::LoadedMedia) {
        // Speed up decoding after the media is ready
        m_loader->setPlaybackRate(4.0);
    } else if (status == QMediaPlayer::EndOfMedia && m_binsFilled > 0) {
        // Normalise peaks
        float peak = 0.f;
        for (float v : m_peaks) peak = std::max(peak, v);
        if (peak > 0.f)
            for (float &v : m_peaks) v /= peak;
        m_peaks.resize(m_binsFilled);
        m_loader->stop();
        update();
        emit peaksReady(QVector<float>(m_peaks.begin(), m_peaks.end()));
    }
#else
    Q_UNUSED(status)
#endif
}

// ── Painting ─────────────────────────────────────────────────────────────────

void WaveformSlider::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int W   = width();
    const int H   = height();
    const int mid = H / 2;

    // Background
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x1e, 0x1e, 0x2e));
    p.drawRoundedRect(rect(), 6, 6);

    const float progress = (m_max > m_min)
        ? float(m_value - m_min) / float(m_max - m_min)
        : 0.f;
    const int cursorX = int(progress * (W - 1));

    if (m_peaks.empty()) {
        // Fallback — simple progress line
        p.setBrush(m_track);   p.drawRect(0,     mid-1, W,        2);
        p.setBrush(m_accent);  p.drawRect(0,     mid-1, cursorX,  2);
    } else {
        const int   n     = int(m_peaks.size());
        const float barW  = float(W) / n;
        const int   drawn = qMin(n, m_binsFilled);

        for (int i = 0; i < n; ++i) {
            const float x  = i * barW;
            const float bw = qMax(1.0f, barW - 1.0f);

            float pk = (i < drawn) ? m_peaks[i] : 0.04f;
            pk = qMax(pk, 0.03f);
            const float bh = pk * (mid - H_PAD);

            const bool played = (x + bw * 0.5f) <= float(cursorX);
            QColor col = played ? m_accent : m_track;
            col.setAlphaF(played ? 0.90f : 0.55f);
            p.setBrush(col);
            p.drawRect(QRectF(x, mid - bh, bw, bh * 2.f));
        }
    }

    // Cursor line
    if (m_max > m_min) {
        p.setPen(QPen(QColor(255, 255, 255, 220), 1.5f));
        p.drawLine(cursorX, H_PAD, cursorX, H - H_PAD);
    }

    // Hover ghost line
    if (m_hovered && m_hoverX >= 0 && m_hoverX != cursorX) {
        p.setPen(QPen(QColor(255, 255, 255, 50), 1));
        p.drawLine(m_hoverX, H_PAD, m_hoverX, H - H_PAD);
    }
}

// ── Mouse ────────────────────────────────────────────────────────────────────

int WaveformSlider::valueToX(int v) const {
    if (m_max == m_min) return 0;
    return int(float(v - m_min) / float(m_max - m_min) * (width() - 1));
}

int WaveformSlider::xToValue(int x) const {
    if (width() <= 1) return m_min;
    return m_min + int(float(qBound(0, x, width()-1)) / float(width()-1) * float(m_max - m_min));
}

void WaveformSlider::applyDrag(int x) {
    const int v = xToValue(x);
    if (v != m_value) { m_value = v; emit valueChanged(v); update(); }
}

void WaveformSlider::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        applyDrag(e->pos().x());
        emit sliderPressed();
    }
}

void WaveformSlider::mouseMoveEvent(QMouseEvent *e) {
    m_hoverX = e->pos().x();
    if (m_dragging) applyDrag(e->pos().x());
    update();
}

void WaveformSlider::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && m_dragging) {
        applyDrag(e->pos().x());
        m_dragging = false;
        emit sliderReleased();
    }
}

void WaveformSlider::enterEvent(QEnterEvent *) { m_hovered = true;  update(); }
void WaveformSlider::leaveEvent(QEvent *)       { m_hovered = false; m_hoverX = -1; update(); }
