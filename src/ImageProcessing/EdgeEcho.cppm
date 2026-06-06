module;
#include <algorithm>
#include <cmath>
#include <vector>

module ImageProcessing:EdgeEcho;

import Particle;
import Image.ImageF32x4_RGBA;
import Memory.TrackedPtr;

namespace ArtifactCore {

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = kPi * 2.0f;
}

class EdgeEcho::Impl {
public:
    ImageF32x4_RGBA history;
    bool has_history = false;

    void reset() {
        has_history = false;
    }
};

EdgeEcho::EdgeEcho() : impl_(std::make_unique<Impl>()) {}
EdgeEcho::~EdgeEcho() = default;

void EdgeEcho::reset() {
    impl_->reset();
}

void EdgeEcho::process(float4* buffer, int width, int height, const EdgeEchoSettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;

    size_t total_pixels = static_cast<size_t>(width * height);
    std::vector<float4> original(buffer, buffer + total_pixels);

    // 1. Initialize or resize history buffer
    if (!impl_->has_history || impl_->history.width() != width || impl_->history.height() != height) {
        impl_->history.resize(width, height);
        impl_->history.fill(FloatRGBA{0.0f, 0.0f, 0.0f, 0.0f});
        impl_->has_history = true;
    }

    float* history_raw = impl_->history.rgba32fData();
    if (!history_raw) return;

    std::vector<float> current_edges(total_pixels, 0.0f);
    float edge_thresh = std::max(settings.edgeThreshold, 0.001f);

    // 2. Sobel edge detection pass (on luminance of current frame)
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            float g_x = 0.0f;
            float g_y = 0.0f;

            // 3x3 Sobel kernels
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    float4 p = original[(y + dy) * width + (x + dx)];
                    float lum = p.x * 0.299f + p.y * 0.587f + p.z * 0.114f;

                    // Sobel X kernel weight
                    int wx = dx * (dy == 0 ? 2 : 1);
                    // Sobel Y kernel weight
                    int wy = dy * (dx == 0 ? 2 : 1);

                    g_x += lum * wx;
                    g_y += lum * wy;
                }
            }

            float magnitude = std::sqrt(g_x * g_x + g_y * g_y);
            if (magnitude > edge_thresh) {
                current_edges[y * width + x] = std::clamp((magnitude - edge_thresh) / (1.0f - edge_thresh), 0.0f, 1.0f);
            }
        }
    }

    // Temporary buffer to calculate the warped new history
    std::vector<float> next_history(total_pixels, 0.0f);
    float decay = std::clamp(settings.decay, 0.0f, 1.0f);
    float wave_amp = settings.waveAmp;
    float wave_freq = settings.waveFreq;
    float time_val = settings.timeEvolution;

    // 3. Temporal wave warping pass (warp previous history outline)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Apply sinusoidal offset to vertical sampling coordinate
            float nx = static_cast<float>(x);
            float ny = static_cast<float>(y) + wave_amp * std::sin(kTwoPi * wave_freq * (nx / width) + time_val);

            // Bilinear sample from previous history outline (channel 0 stores outline strength)
            int y0 = std::clamp(static_cast<int>(std::floor(ny)), 0, height - 1);
            int y1 = std::clamp(y0 + 1, 0, height - 1);
            float fy = ny - std::floor(ny);

            float prev_his0 = history_raw[(y0 * width + x) * 4];
            float prev_his1 = history_raw[(y1 * width + x) * 4];
            float prev_his_decayed = (prev_his0 * (1.0f - fy) + prev_his1 * fy) * decay;

            // Combine current edge with decayed warped history (additive/max)
            float next_val = std::max(current_edges[y * width + x], prev_his_decayed);
            next_history[y * width + x] = next_val;
        }
    }

    // 4. Update the history buffer for the next frame
    for (size_t i = 0; i < total_pixels; ++i) {
        float val = next_history[i];
        history_raw[i * 4 + 0] = val;
        history_raw[i * 4 + 1] = val;
        history_raw[i * 4 + 2] = val;
        history_raw[i * 4 + 3] = 1.0f;
    }

    // 5. Composite EdgeEcho onto original image
    float4 echo_color = settings.echoColor;
    for (size_t i = 0; i < total_pixels; ++i) {
        float outline_strength = next_history[i];
        if (outline_strength > 0.001f) {
            float4 orig_pixel = original[i];
            
            // Additive combination with neon glow overlay
            buffer[i].x = std::clamp(orig_pixel.x + echo_color.x * outline_strength, 0.0f, 1.0f);
            buffer[i].y = std::clamp(orig_pixel.y + echo_color.y * outline_strength, 0.0f, 1.0f);
            buffer[i].z = std::clamp(orig_pixel.z + echo_color.z * outline_strength, 0.0f, 1.0f);
        }
    }
}

void EdgeEcho::process(ImageF32x4_RGBA& image, const EdgeEchoSettings& settings) {
    if (image.isEmpty()) return;
    float* raw_data = image.rgba32fData();
    if (!raw_data) return;

    process(reinterpret_cast<float4*>(raw_data), image.width(), image.height(), settings);
}

} // namespace ArtifactCore
