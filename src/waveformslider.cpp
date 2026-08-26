#include "waveformslider.h"
#include <QPainter>
#include <QMouseEvent>
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <cmath>

static constexpr int TARGET_BINS = 500;
static constexpr int H_PAD       = 3;
static constexpr int ANALYSIS_STRIDE = 4;

WaveformSlider::WaveformSlider(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

WaveformSlider::~WaveformSlider() {
    ++m_decodeGeneration;
    if (m_decoder) {
        m_decoder->stop();
        m_decoder = nullptr;
    }
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
    clearWaveform();
    if (!url.isValid() || durationMs <= 0) return;

    m_waveformUrl   = url;
    m_durationMs    = durationMs;
    m_bins          = TARGET_BINS;
    m_peaks.assign(m_bins, 0.f);
    m_binsFilled    = 0;
    m_binAccum      = 0.f;
    m_binCount      = 0;
    m_lastSharedBin = 0;
    m_decodeFinalized = false;
    m_samplesPerBin = 0;

    // A decoder is intentionally created per request. Reusing a decoder while
    // stop() is still settling can make a queued finished signal from the
    // previous track finalize the next track and leave its waveform frozen.
    auto *decoder = new QAudioDecoder(this);
    m_decoder = decoder;
    const quint64 generation = m_decodeGeneration;
    connect(decoder, &QAudioDecoder::bufferReady, this,
            [this, decoder, generation] {
        if (decoder != m_decoder || generation != m_decodeGeneration) return;
        onBufferReady();
    });
    connect(decoder, &QAudioDecoder::finished, this,
            [this, decoder, generation] {
        if (decoder != m_decoder || generation != m_decodeGeneration) return;
        onDecodeFinished();
    });

    decoder->setSource(url);
    decoder->start();
}

void WaveformSlider::setPeaks(const QVector<float> &peaks) {
    ++m_decodeGeneration;
    if (m_decoder) {
        QAudioDecoder *decoder = m_decoder;
        m_decoder = nullptr;
        decoder->stop();
        decoder->deleteLater();
    }
    m_peaks.assign(peaks.begin(), peaks.end());
    m_bins       = int(m_peaks.size());
    m_binsFilled = int(m_peaks.size());
    m_decodeFinalized = true;
    update();
}

void WaveformSlider::clearWaveform() {
    ++m_decodeGeneration;
    if (m_decoder) {
        QAudioDecoder *decoder = m_decoder;
        m_decoder = nullptr;
        decoder->stop();
        decoder->deleteLater();
    }
    m_waveformUrl = QUrl();
    m_peaks.clear();
    m_binsFilled    = 0;
    m_binAccum      = 0.f;
    m_binCount      = 0;
    m_lastSharedBin = 0;
    m_decodeFinalized = false;
    m_samplesPerBin = 0;
    m_durationMs    = 0;
    update();
}

// ── Theme ────────────────────────────────────────────────────────────────────

void WaveformSlider::setAccentColor(const QColor &c) { m_accent = c; update(); }
void WaveformSlider::setTrackColor (const QColor &c) { m_track  = c; update(); }
void WaveformSlider::setBackgroundColor(const QColor &c) { m_background = c; update(); }

// ── Background decoder slots ─────────────────────────────────────────────────

void WaveformSlider::onBufferReady() {
    if (!m_decoder) return;
    while (m_decoder->bufferAvailable() && m_binsFilled < m_bins) {
        const QAudioBuffer buf = m_decoder->read();
        if (!buf.isValid()) continue;

        const QAudioFormat fmt = buf.format();
        const int sr  = fmt.sampleRate();
        const int ch  = fmt.channelCount();
        const int frames = buf.frameCount();
        if (ch <= 0 || frames <= 0 || fmt.sampleFormat() == QAudioFormat::Unknown)
            continue;

        if (m_samplesPerBin == 0 && sr > 0 && m_durationMs > 0) {
            const qint64 totalFrames = qint64(sr) * m_durationMs / 1000;
            m_samplesPerBin = qMax<qint64>(1, totalFrames / (m_bins * ANALYSIS_STRIDE));
        }
        if (m_samplesPerBin == 0) continue;

        auto sampleAt = [&buf, &fmt](int index) -> float {
            switch (fmt.sampleFormat()) {
            case QAudioFormat::Float:
                return buf.constData<float>()[index];
            case QAudioFormat::Int16:
                return buf.constData<qint16>()[index] / 32768.0f;
            case QAudioFormat::Int32:
                return float(buf.constData<qint32>()[index] / 2147483648.0);
            case QAudioFormat::UInt8:
                return (int(buf.constData<quint8>()[index]) - 128) / 128.0f;
            default:
                return 0.f;
            }
        };
        // Four-times decimation is far above the visual resolution of 500
        // bars and cuts amplitude-analysis work substantially.
        for (int i = 0; i < frames && m_binsFilled < m_bins; i += ANALYSIS_STRIDE) {
            float v = 0.f;
            for (int c = 0; c < ch; ++c)
                v += std::abs(sampleAt(i * ch + c));
            m_binAccum += v / ch;
            ++m_binCount;
            if (m_binCount >= m_samplesPerBin) {
                m_peaks[m_binsFilled++] = m_binAccum / m_binCount;
                m_binAccum = 0.f;
                m_binCount = 0;
            }
        }
    }

    // Once all visual bins are ready there is no reason to decode the rest of
    // the track. Finalize here while the decoder is known to exist.
    if (m_bins > 0 && m_binsFilled >= m_bins) {
        m_decoder->stop();
        onDecodeFinished();
        return;
    }
    update();

    // Update the mini player progressively without copying on every buffer.
    if (m_binsFilled - m_lastSharedBin >= 10 || m_binsFilled == m_bins) {
        m_lastSharedBin = m_binsFilled;
        emit peaksReady(m_waveformUrl,
                        QVector<float>(m_peaks.begin(), m_peaks.begin() + m_binsFilled));
    }
}

void WaveformSlider::onDecodeFinished() {
    if (m_decodeFinalized) return;
    m_decodeFinalized = true;
    if (m_binCount > 0 && m_binsFilled < m_bins)
        m_peaks[m_binsFilled++] = m_binAccum / m_binCount;
    if (m_binsFilled <= 0) return;

    float peak = 0.f;
    for (int i = 0; i < m_binsFilled; ++i) peak = std::max(peak, m_peaks[i]);
    if (peak > 0.f)
        for (int i = 0; i < m_binsFilled; ++i) m_peaks[i] /= peak;
    m_peaks.resize(m_binsFilled);
    update();
    const QVector<float> completed(m_peaks.begin(), m_peaks.end());
    emit peaksReady(m_waveformUrl, completed);
    emit waveformReady(m_waveformUrl, m_durationMs, completed);
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
    p.setBrush(m_background);
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
