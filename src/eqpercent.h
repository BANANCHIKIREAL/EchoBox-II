#pragma once

#include <algorithm>
#include <cmath>

// Persisted equalizer gains remain in dB for compatibility with older settings.
// 100% is neutral; 1000% is +20 dB; 1% is -40 dB.
inline int eqGainToPercent(float gainDb)
{
    if (!std::isfinite(gainDb)) return 100;
    const double boundedDb = std::clamp(double(gainDb), -40.0, 20.0);
    return std::clamp(int(std::lround(100.0 * std::pow(10.0, boundedDb / 20.0))),
                      1, 1000);
}

inline float eqPercentToGain(int percent)
{
    return 20.0f * std::log10(float(std::clamp(percent, 1, 1000)) / 100.0f);
}
