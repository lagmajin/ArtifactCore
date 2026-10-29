module;
#include <algorithm>
#include <cmath>
#include <limits>

module ImageProcessing;
import :PosterizeTime;

namespace ArtifactCore {

PosterizeTime::PosterizeTime() : frameCounter_(0), heldBuffer_(nullptr), heldW_(0), heldH_(0) {}

PosterizeTime::~PosterizeTime() { delete[] heldBuffer_; }

void PosterizeTime::reset() {
    frameCounter_ = 0;
    delete[] heldBuffer_;
    heldBuffer_ = nullptr;
    heldW_ = heldH_ = 0;
}

void PosterizeTime::process(float4* buffer, int width, int height, const PosterizeTimeSettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;

    int step = 1;
    if (std::isfinite(settings.frameRate) && settings.frameRate > 0.0f) {
        const double roundedStep = std::round(30.0 / static_cast<double>(settings.frameRate));
        step = static_cast<int>(std::clamp(roundedStep, 1.0,
            static_cast<double>(std::numeric_limits<int>::max())));
    }
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (heldW_ != width || heldH_ != height) {
        delete[] heldBuffer_;
        heldBuffer_ = new float4[pixelCount];
        heldW_ = width; heldH_ = height;
        std::copy_n(buffer, pixelCount, heldBuffer_);
    }
    if (frameCounter_ % step == 0)
        std::copy_n(buffer, pixelCount, heldBuffer_);
    std::copy_n(heldBuffer_, pixelCount, buffer);
    ++frameCounter_;
}

void PosterizeTime::process(ImageF32x4_RGBA& image, const PosterizeTimeSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
