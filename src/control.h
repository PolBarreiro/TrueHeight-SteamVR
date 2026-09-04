#pragma once

#include <algorithm>
#include <cmath>

namespace trueheight
{

inline float Clamp(float value, float minimum, float maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

struct ControlSettings
{
    float targetHeightM;
    float smoothingTimeS;
    float deadzoneM;
    float maxSpeedMps;
    float maxOffsetM;
};

inline float ComputeNextOffset(float measuredHeightM,
                               float currentOffsetM,
                               float deltaTimeS,
                               const ControlSettings& settings)
{
    const float errorM = settings.targetHeightM - measuredHeightM;
    if (std::fabs(errorM) <= settings.deadzoneM)
        return currentOffsetM;

    const float safeDeltaTimeS = Clamp(deltaTimeS, 0.0f, 0.1f);
    const float safeSmoothingS = std::max(0.01f, settings.smoothingTimeS);
    const float alpha = 1.0f - std::exp(-safeDeltaTimeS / safeSmoothingS);
    float stepM = errorM * alpha;
    const float maxStepM = settings.maxSpeedMps * safeDeltaTimeS;
    stepM = Clamp(stepM, -maxStepM, maxStepM);

    return Clamp(currentOffsetM + stepM,
                 -settings.maxOffsetM,
                 settings.maxOffsetM);
}

} // namespace trueheight
