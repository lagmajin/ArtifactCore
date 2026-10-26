#pragma once

extern "C" void artifactSmokeRuntimeRndObserve(
    int stage, int width, int height, int solverIterations,
    const float* density,
    const float* velocityX, const float* velocityY);
