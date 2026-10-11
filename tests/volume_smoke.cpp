#include "audioengine.h"
#include "volumerange.h"

#include <cmath>
#include <cstring>

class TestPlaybackDevice final : public EqPlaybackDevice {
public:
    using EqPlaybackDevice::readData;
};

int main()
{
    if (playerVolumeFromSlider(140) != 70) return 1;
    if (playerVolumeFromSlider(200) != 100) return 2;
    if (playerVolumeFromSlider(300) != 1000) return 3;
    if (playerVolumeToSlider(1000) != 300) return 4;
    if (std::abs(clampAudioSample(0.05f, 10.0f) - 0.5f) > 0.001f) return 5;
    if (clampAudioSample(0.2f, 10.0f) != 1.0f) return 6;

    TestPlaybackDevice device;
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    device.configureOutput(format);
    device.setPcm({0.05f, -0.05f, 0.2f, -0.2f}, 48000);
    device.setVolume(10.0f);
    char output[4 * sizeof(float)] {};
    if (device.readData(output, sizeof(output)) != sizeof(output)) return 7;
    float samples[4] {};
    std::memcpy(samples, output, sizeof(samples));
    if (std::abs(samples[0] - 0.5f) > 0.001f) return 8;
    if (std::abs(samples[1] + 0.5f) > 0.001f) return 9;
    if (samples[2] != 1.0f || samples[3] != -1.0f) return 10;
    return 0;
}
