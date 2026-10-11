#pragma once

#include <algorithm>
#include <cmath>

constexpr int kMaxPlayerVolumePercent = 1000;
constexpr int kPlayerVolumeSliderMaximum = 300;

// Keep the usual 0-100% range in the first two thirds of the slider.
// The numeric editor provides exact values in the boost range.
inline int playerVolumeFromSlider(int position)
{
    position = std::clamp(position, 0, kPlayerVolumeSliderMaximum);
    return position <= 200 ? (position + 1) / 2 : 100 + (position - 200) * 9;
}

inline int playerVolumeToSlider(int percent)
{
    percent = std::clamp(percent, 0, kMaxPlayerVolumePercent);
    return percent <= 100 ? percent * 2
                          : 200 + int(std::lround(double(percent - 100) / 9.0));
}

inline float clampAudioSample(float sample, float gain)
{
    return std::clamp(sample * gain, -1.0f, 1.0f);
}
