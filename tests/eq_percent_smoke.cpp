#include "eqpercent.h"
#include "audioengine.h"

#include <cmath>

int main()
{
    if (eqGainToPercent(0.0f) != 100) return 1;
    if (eqGainToPercent(20.0f) != 1000) return 2;
    if (eqGainToPercent(-40.0f) != 1) return 3;
    if (std::abs(eqPercentToGain(100)) > 0.001f) return 4;
    if (std::abs(eqPercentToGain(1000) - 20.0f) > 0.001f) return 5;
    if (std::abs(eqPercentToGain(1) + 40.0f) > 0.001f) return 6;
    for (float savedDb : {-12.0f, -6.0f, 6.0f, 12.0f}) {
        if (std::abs(eqPercentToGain(eqGainToPercent(savedDb)) - savedDb) > 0.2f)
            return 7;
    }
    BiquadState boosted;
    boosted.setPeaking(48000.0f, 1000.0f, eqPercentToGain(1000), 1.0f);
    double inputEnergy = 0.0;
    double outputEnergy = 0.0;
    for (int frame = 0; frame < 48000; ++frame) {
        const float input = 0.05f * std::sin(2.0 * 3.141592653589793 * frame / 48.0);
        const float output = boosted.process(input);
        if (frame >= 24000) {
            inputEnergy += double(input) * input;
            outputEnergy += double(output) * output;
        }
    }
    const double gain = std::sqrt(outputEnergy / inputEnergy);
    if (std::abs(gain - 10.0) > 0.1) return 8;
    return 0;
}
