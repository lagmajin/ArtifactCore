module;
#include <algorithm>
#include <cmath>

module ImageProcessing:PosterizeTime;

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
    int step = std::max(1, static_cast<int>(std::round(30.0f / settings.frameRate)));
    if (heldW_ != width || heldH_ != height) {
        delete[] heldBuffer_;
        heldBuffer_ = new float4[width * height];
        heldW_ = width; heldH_ = height;
        std::copy_n(buffer, width * height, heldBuffer_);
    }
    if (frameCounter_ % step == 0)
        std::copy_n(buffer, width * height, heldBuffer_);
    std::copy_n(heldBuffer_, width * height, buffer);
    ++frameCounter_;
}

void PosterizeTime::process(ImageF32x4_RGBA& image, const PosterizeTimeSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
