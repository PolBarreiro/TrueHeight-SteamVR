#include "control.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{

void ExpectNear(float actual, float expected, float tolerance, const char* name)
{
    if (std::fabs(actual - expected) <= tolerance)
        return;

    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    std::exit(EXIT_FAILURE);
}

} // namespace

int main()
{
    using trueheight::ComputeNextOffset;
    using trueheight::ControlSettings;

    const ControlSettings normal { 1.80f, 0.30f, 0.02f, 1.0f, 3.0f };

    ExpectNear(ComputeNextOffset(1.79f, 0.25f, 0.016f, normal),
               0.25f, 0.00001f, "deadzone preserves offset");
    ExpectNear(ComputeNextOffset(1.20f, 0.0f, 0.10f, normal),
               0.10f, 0.00001f, "positive correction obeys speed limit");
    ExpectNear(ComputeNextOffset(2.40f, 0.0f, 0.10f, normal),
               -0.10f, 0.00001f, "negative correction obeys speed limit");

    const ControlSettings clampHigh { 10.0f, 0.01f, 0.0f, 100.0f, 3.0f };
    ExpectNear(ComputeNextOffset(0.0f, 2.9f, 0.10f, clampHigh),
               3.0f, 0.00001f, "positive offset clamp");

    const ControlSettings clampLow { -10.0f, 0.01f, 0.0f, 100.0f, 3.0f };
    ExpectNear(ComputeNextOffset(0.0f, -2.9f, 0.10f, clampLow),
               -3.0f, 0.00001f, "negative offset clamp");

    ExpectNear(ComputeNextOffset(1.20f, 0.0f, -1.0f, normal),
               0.0f, 0.00001f, "negative elapsed time is harmless");

    return EXIT_SUCCESS;
}
