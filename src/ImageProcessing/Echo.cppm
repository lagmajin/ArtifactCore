module;
#include <algorithm>
#include <cmath>

module ImageProcessing:Echo;

namespace ArtifactCore {

Echo::Echo() : writePos_(0), frameCount_(0), bufW_(0), bufH_(0) {}

Echo::~Echo() {}

void Echo::reset() {
    writePos_ = 0;
    frameCount_ = 0;
    ringBuffer_.clear();
    ringCapacity_ = 0;
    bufW_ = bufH_ = 0;
}

void Echo::process(float4* buffer, int width, int height, const EchoSettings& s) {
    int cap = std::max(1, s.echoCount);
    if (bufW_ != width || bufH_ != height || ringCapacity_ != cap) {
        ringBuffer_.resize(static_cast<size_t>(width) * height * cap);
        ringCapacity_ = cap;
        bufW_ = width; bufH_ = height;
        writePos_ = 0; frameCount_ = 0;
        std::fill(ringBuffer_.begin(), ringBuffer_.end(), float4{0,0,0,0});
    }

    size_t frameSize = static_cast<size_t>(width) * height;
    float4* slot = ringBuffer_.data() + writePos_ * frameSize;
    std::copy_n(buffer, frameSize, slot);
    writePos_ = (writePos_ + 1) % ringCapacity_;
    if (frameCount_ < ringCapacity_) ++frameCount_;

    std::fill_n(buffer, frameSize, float4{0,0,0,0});
    for (int i = 0; i < frameCount_; ++i) {
        int idx = (writePos_ - 1 - i + ringCapacity_) % ringCapacity_;
        float weight = (i == 0) ? 1.0f : s.startingIntensity * std::pow(s.decay, static_cast<float>(i - 1));
        float4* src = ringBuffer_.data() + idx * frameSize;
        for (size_t j = 0; j < frameSize; ++j) {
            buffer[j].x += src[j].x * weight;
            buffer[j].y += src[j].y * weight;
            buffer[j].z += src[j].z * weight;
            buffer[j].w += src[j].w * weight;
        }
    }

    float inv = 1.0f / (1.0f + s.startingIntensity * (1.0f - std::pow(s.decay, static_cast<float>(frameCount_ - 1))) / std::max(1.0f - s.decay, 0.001f));
    for (size_t j = 0; j < frameSize; ++j) {
        buffer[j].x = std::clamp(buffer[j].x * inv, 0.0f, 1.0f);
        buffer[j].y = std::clamp(buffer[j].y * inv, 0.0f, 1.0f);
        buffer[j].z = std::clamp(buffer[j].z * inv, 0.0f, 1.0f);
        buffer[j].w = std::clamp(buffer[j].w * inv, 0.0f, 1.0f);
    }
}

void Echo::process(ImageF32x4_RGBA& image, const EchoSettings& settings) {
    process(reinterpret_cast<float4*>(image.rgba32fData()),
            static_cast<int>(image.width()),
            static_cast<int>(image.height()), settings);
}

}
