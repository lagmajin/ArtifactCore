module;
#include <algorithm>
#include <cmath>
#include <vector>

module ImageProcessing:MotionTrail;

import Particle;
import Image.ImageF32x4_RGBA;

namespace ArtifactCore {

class MotionTrail::Impl {
public:
    ImageF32x4_RGBA history;
    bool has_history = false;

    void reset() {
        has_history = false;
    }
};

MotionTrail::MotionTrail() : impl_(std::make_unique<Impl>()) {}
MotionTrail::~MotionTrail() = default;

void MotionTrail::reset() {
    impl_->reset();
}

void MotionTrail::process(float4* buffer, int width, int height, const MotionTrailSettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;

    size_t total_pixels = static_cast<size_t>(width * height);
    float* current_raw = reinterpret_cast<float*>(buffer);

    // 1. Initialize or resize history buffer if dimensions changed
    if (!impl_->has_history || impl_->history.width() != width || impl_->history.height() != height) {
        impl_->history.resize(width, height);
        
        // Copy current frame to initialize history
        float* history_raw = impl_->history.rgba32fData();
        if (history_raw) {
            std::copy(current_raw, current_raw + total_pixels * 4, history_raw);
        }
        impl_->has_history = true;
        return; // First frame just populates the history
    }

    float* history_raw = impl_->history.rgba32fData();
    if (!history_raw) return;

    float decay = std::clamp(settings.decay, 0.0f, 1.0f);
    float intensity = std::clamp(settings.intensity, 0.0f, 1.0f);

    // 2. Perform temporal blending
    for (size_t i = 0; i < total_pixels; ++i) {
        size_t offset = i * 4;
        
        float r_cur = current_raw[offset + 0];
        float g_cur = current_raw[offset + 1];
        float b_cur = current_raw[offset + 2];
        float a_cur = current_raw[offset + 3];

        float r_his = history_raw[offset + 0];
        float g_his = history_raw[offset + 1];
        float b_his = history_raw[offset + 2];
        float a_his = history_raw[offset + 3];

        float r_out = r_cur;
        float g_out = g_cur;
        float b_out = b_cur;
        float a_out = a_cur;

        // Apply decay to historical pixel
        float r_his_decayed = r_his * decay;
        float g_his_decayed = g_his * decay;
        float b_his_decayed = b_his * decay;
        float a_his_decayed = a_his * decay;

        switch (settings.mode) {
            case MotionTrailMode::Blend:
                // Normal interpolation (Blend)
                // Lerp between current and decayed history based on intensity
                r_out = r_cur * (1.0f - intensity) + std::lerp(r_cur, r_his_decayed, intensity);
                g_out = g_cur * (1.0f - intensity) + std::lerp(g_cur, g_his_decayed, intensity);
                b_out = b_cur * (1.0f - intensity) + std::lerp(b_cur, b_his_decayed, intensity);
                a_out = a_cur * (1.0f - intensity) + std::lerp(a_cur, a_his_decayed, intensity);
                break;

            case MotionTrailMode::Additive:
                // Additive trails (Glowing trails)
                r_out = std::clamp(r_cur + r_his_decayed * intensity, 0.0f, 1.0f);
                g_out = std::clamp(g_cur + g_his_decayed * intensity, 0.0f, 1.0f);
                b_out = std::clamp(b_cur + b_his_decayed * intensity, 0.0f, 1.0f);
                a_out = std::clamp(a_cur + a_his_decayed * intensity, 0.0f, 1.0f);
                break;

            case MotionTrailMode::Maximum:
                // Keep the brightest pixels (Highlights)
                r_out = std::max(r_cur, r_his_decayed * intensity);
                g_out = std::max(g_cur, g_his_decayed * intensity);
                b_out = std::max(b_cur, b_his_decayed * intensity);
                a_out = std::max(a_cur, a_his_decayed * intensity);
                break;
        }

        // 3. Write output back to current buffer
        current_raw[offset + 0] = r_out;
        current_raw[offset + 1] = g_out;
        current_raw[offset + 2] = b_out;
        current_raw[offset + 3] = a_out;

        // 4. Update history with the output of this frame (to accumulate decay)
        history_raw[offset + 0] = r_out;
        history_raw[offset + 1] = g_out;
        history_raw[offset + 2] = b_out;
        history_raw[offset + 3] = a_out;
    }
}

void MotionTrail::process(ImageF32x4_RGBA& image, const MotionTrailSettings& settings) {
    if (image.isEmpty()) return;
    float* raw_data = image.rgba32fData();
    if (!raw_data) return;

    process(reinterpret_cast<float4*>(raw_data), image.width(), image.height(), settings);
}

} // namespace ArtifactCore
