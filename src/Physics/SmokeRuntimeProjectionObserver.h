#pragma once

extern "C" void artifactSmokeRuntimeRndObserve(
    int stage, int width, int height, int solverIterations,
    const float* density,
    const float* velocityX, const float* velocityY);

extern "C" void artifactSmokeRuntimeRndApplyPreUpdate(
    int width, int height, float dt, const float* density,
    float* velocityX, float* velocityY);
