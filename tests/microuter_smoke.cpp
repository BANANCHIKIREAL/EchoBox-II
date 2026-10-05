#include "microuter.h"

#include <QApplication>
#include <QAudioBufferOutput>
#include <QAudioOutput>
#include <QAudioSource>
#include <QDataStream>
#include <QFile>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QTemporaryDir>
#include <QTimer>
#include <QtEndian>

#include <cmath>
#include <cstdio>
#include <cstring>

static bool writeTone(const QString &path)
{
    constexpr int sampleRate = 48000;
    constexpr int channels = 2;
    constexpr int seconds = 4;
    constexpr int sampleCount = sampleRate * seconds;
    constexpr quint32 dataSize = sampleCount * channels * sizeof(qint16);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + dataSize);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(channels) << quint32(sampleRate)
        << quint32(sampleRate * channels * sizeof(qint16))
        << quint16(channels * sizeof(qint16)) << quint16(16);
    out.writeRawData("data", 4);
    out << dataSize;
    for (int i = 0; i < sampleCount; ++i) {
        const qint16 sample = qint16(std::sin(2.0 * M_PI * 440.0 * i / sampleRate) * 12000.0);
        out << sample << sample;
    }
    return out.status() == QDataStream::Ok;
}

static float peakLevel(const QByteArray &bytes, QAudioFormat::SampleFormat format)
{
    float peak = 0.0f;
    if (format == QAudioFormat::UInt8) {
        for (unsigned char value : bytes)
            peak = qMax(peak, std::abs(int(value) - 128) / 128.0f);
    } else if (format == QAudioFormat::Int16) {
        for (qsizetype i = 0; i + 1 < bytes.size(); i += 2) {
            const auto value = qFromLittleEndian<qint16>(bytes.constData() + i);
            peak = qMax(peak, std::abs(int(value)) / 32768.0f);
        }
    } else if (format == QAudioFormat::Int32) {
        for (qsizetype i = 0; i + 3 < bytes.size(); i += 4) {
            const auto value = qFromLittleEndian<qint32>(bytes.constData() + i);
            peak = qMax(peak, float(std::abs(double(value)) / 2147483648.0));
        }
    } else if (format == QAudioFormat::Float) {
        for (qsizetype i = 0; i + 3 < bytes.size(); i += 4) {
            float value = 0.0f;
            std::memcpy(&value, bytes.constData() + i, sizeof(value));
            peak = qMax(peak, std::abs(value));
        }
    }
    return peak;
}

static void continuityStats(const QByteArray &bytes, const QAudioFormat &format,
                            int &activeWindows, int &silentInterior,
                            int &longestSilentInterior, float &earlyPeak,
                            float &latePeak)
{
    activeWindows = 0;
    silentInterior = 0;
    longestSilentInterior = 0;
    const int bytesPerFrame = format.bytesPerFrame();
    const int windowBytes = qMax(bytesPerFrame,
        (format.sampleRate() / 50) * bytesPerFrame); // 20 ms
    QVector<bool> active;
    QVector<float> peaks;
    for (qsizetype offset = 0; offset + windowBytes <= bytes.size();
         offset += windowBytes) {
        const QByteArray view = QByteArray::fromRawData(bytes.constData() + offset,
                                                        windowBytes);
        const float peak = peakLevel(view, format.sampleFormat());
        peaks.append(peak);
        active.append(peak > 0.01f);
    }
    // Ignore isolated capture noise before and after the sustained test tone.
    int first = -1;
    int last = -1;
    constexpr int sustainedWindows = 10;
    for (int i = 0; i + sustainedWindows <= active.size(); ++i) {
        bool sustained = true;
        for (int j = 0; j < sustainedWindows; ++j)
            sustained = sustained && active[i + j];
        if (sustained) {
            first = i;
            break;
        }
    }
    for (int i = active.size() - sustainedWindows; i >= 0; --i) {
        bool sustained = true;
        for (int j = 0; j < sustainedWindows; ++j)
            sustained = sustained && active[i + j];
        if (sustained) {
            last = i + sustainedWindows - 1;
            break;
        }
    }
    earlyPeak = 0.0f;
    latePeak = 0.0f;
    if (last - first >= 80) {
        for (int i = 0; i < 40; ++i) {
            earlyPeak += peaks[first + i] / 40.0f;
            latePeak += peaks[last - i] / 40.0f;
        }
    }
    int run = 0;
    std::fprintf(stderr, "continuity_first_window=%d last_window=%d silent_windows=", first, last);
    for (int i = first; i >= 0 && i <= last; ++i) {
        if (!active[i]) {
            std::fprintf(stderr, "%d,", i);
            ++silentInterior;
            longestSilentInterior = qMax(longestSilentInterior, ++run);
        } else {
            ++activeWindows;
            run = 0;
        }
    }
    std::fprintf(stderr, "\n");
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir temporary;
    const QString tonePath = temporary.filePath("route-test.wav");
    if (!temporary.isValid() || !writeTone(tonePath)) return 10;

    QAudioDevice cableInput;
    for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
        if (device.description().startsWith("CABLE Input", Qt::CaseInsensitive)) {
            cableInput = device;
            break;
        }
    }
    QAudioDevice cableOutput;
    QAudioDevice physicalMicrophone;
    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
        if (device.description().startsWith("CABLE Output", Qt::CaseInsensitive)) {
            cableOutput = device;
        } else if (physicalMicrophone.isNull() &&
                   !device.description().contains("CABLE", Qt::CaseInsensitive)) {
            physicalMicrophone = device;
        }
    }
    if (cableInput.isNull() || cableOutput.isNull()) {
        qCritical("VB-CABLE endpoints were not found");
        return 11;
    }

    QMediaPlayer player;
    QAudioOutput mutedMonitor;
    mutedMonitor.setVolume(0.0f);
    player.setAudioOutput(&mutedMonitor);
    QAudioFormat routeFormat;
    routeFormat.setSampleRate(48000);
    routeFormat.setChannelCount(2);
    routeFormat.setSampleFormat(QAudioFormat::Float);
    QAudioBufferOutput decodedAudio(routeFormat);
    player.setAudioBufferOutput(&decodedAudio);
    player.setSource(QUrl::fromLocalFile(tonePath));

    MicRouter router(&player);
    QObject::connect(&decodedAudio, &QAudioBufferOutput::audioBufferReceived,
                     &router, &MicRouter::feedMusicBuffer);
    float routerPeak = 0.0f;
    QObject::connect(&router, &MicRouter::levelChanged, &app,
                     [&routerPeak](float level) { routerPeak = qMax(routerPeak, level); });
    QObject::connect(&router, &MicRouter::routingError, &app,
                     [](const QString &message) {
                         std::fprintf(stderr, "router_error=%s\n", message.toUtf8().constData());
                     });
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &app,
                     [](QMediaPlayer::Error error, const QString &message) {
                         std::fprintf(stderr, "player_error=%d %s\n", int(error),
                                      message.toUtf8().constData());
                     });
    QString error;
    if (!router.start(cableInput.id(), {}, false, {}, false,
                      1.0f, 0.0f, 0.0f, &error)) {
        qCritical().noquote() << error;
        return 12;
    }
    router.setPlayerVolume(1.0f);
    bool volumeReduced = false;
    QObject::connect(&player, &QMediaPlayer::positionChanged, &app,
                     [&router, &volumeReduced](qint64 position) {
                         if (!volumeReduced && position >= 2000) {
                             router.setPlayerVolume(0.25f);
                             volumeReduced = true;
                         }
                     });

    const QAudioFormat captureFormat = cableOutput.preferredFormat();
    QAudioSource capture(cableOutput, captureFormat);
    QIODevice *stream = capture.start();
    if (!stream) return 13;
    QByteArray captured;
    QObject::connect(stream, &QIODevice::readyRead, &app,
                     [&captured, stream] { captured += stream->readAll(); });

    QTimer::singleShot(250, &player, &QMediaPlayer::play);
    // On a cold FFmpeg start the first local track can take a few seconds to
    // open, so leave enough time for the complete four-second signal.
    QTimer::singleShot(9000, &app, [&] {
        captured += stream->readAll();
        const float peak = peakLevel(captured, captureFormat.sampleFormat());
        int activeWindows = 0;
        int silentInterior = 0;
        int longestSilentInterior = 0;
        float earlyPeak = 0.0f;
        float latePeak = 0.0f;
        continuityStats(captured, captureFormat, activeWindows,
                        silentInterior, longestSilentInterior, earlyPeak, latePeak);
        const float volumeRatio = earlyPeak > 0.0f ? latePeak / earlyPeak : 0.0f;
        std::fprintf(stderr, "volume_early_peak=%.4f late_peak=%.4f ratio=%.4f reduced=%d\n",
                     earlyPeak, latePeak, volumeRatio, int(volumeReduced));
        std::fprintf(stderr, "captured_bytes=%lld peak=%.4f format=%d rate=%d channels=%d\n",
                     static_cast<long long>(captured.size()), peak,
                     int(captureFormat.sampleFormat()), captureFormat.sampleRate(),
                     captureFormat.channelCount());
        std::fprintf(stderr, "player_state=%d position=%lld error=%d router_active=%d\n",
                     int(player.playbackState()), static_cast<long long>(player.position()),
                     int(player.error()), int(router.isActive()));
        std::fprintf(stderr, "router_peak=%.4f\n", routerPeak);
        std::fprintf(stderr,
                     "continuity_active_windows=%d silent_interior=%d longest_silent=%d window_ms=20\n",
                     activeWindows, silentInterior, longestSilentInterior);
        player.stop();
        capture.stop();
        router.stop();
        QString voiceError;
        const bool physicalMicOpened = !physicalMicrophone.isNull() &&
            router.start(cableInput.id(), physicalMicrophone.id(), true, {}, false,
                         0.0f, 0.0f, 0.0f, &voiceError);
        std::fprintf(stderr, "physical_mic_opened=%d device=%s error=%s\n",
                     int(physicalMicOpened),
                     physicalMicrophone.description().toUtf8().constData(),
                     voiceError.toUtf8().constData());
        router.stop();
        QCoreApplication::exit(captured.size() > 4096 && peak > 0.02f &&
                               activeWindows >= 150 && longestSilentInterior <= 3 &&
                               volumeReduced && volumeRatio >= 0.10f &&
                               volumeRatio <= 0.45f && physicalMicOpened ? 0 : 14);
    });
    return app.exec();
}
