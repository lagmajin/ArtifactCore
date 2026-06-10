module;
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
module ImageProcessing:AnamorphicFlare;
#include <algorithm>
#include <cmath>
#include <vector>


import Particle;
import Image.ImageF32x4_RGBA;

namespace ArtifactCore {

void AnamorphicFlare::process(float4* buffer, int width, int height, const AnamorphicFlareSettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;

    size_t total_pixels = static_cast<size_t>(width * height);
    std::vector<float4> original(buffer, buffer + total_pixels);
    std::vector<float4> highlights(total_pixels, float4{0.0f, 0.0f, 0.0f, 0.0f});

    float threshold = std::clamp(settings.threshold, 0.0f, 1.0f);
    float decay = std::clamp(settings.flareLength * 0.95f + 0.04f, 0.0f, 0.99f); // Scaled for aesthetic falloff
    float intensity = std::max(settings.intensity, 0.0f);
    float4 tint = settings.tint;

    // 1. Extract highlights exceeding threshold
    tbb::parallel_for(tbb::blocked_range<size_t>(0, total_pixels),
        [&](const tbb::blocked_range<size_t>& range) {
            for (size_t i = range.begin(); i < range.end(); ++i) {
                float4 pixel = original[i];
                float luminance = pixel.x * 0.299f + pixel.y * 0.587f + pixel.z * 0.114f;
                if (luminance > threshold) {
                    // High luminance generates a streak
                    float scale = (luminance - threshold) / (1.0f - threshold + 0.001f);
                    highlights[i] = float4{pixel.x * scale, pixel.y * scale, pixel.z * scale, pixel.w};
                }
            }
        });

    std::vector<float4> streaks(total_pixels, float4{0.0f, 0.0f, 0.0f, 0.0f});

    // 2. Horizontal streak propagation pass (O(N) left-to-right & right-to-left decay sweep)
    // Rows are independent - each row processes its own scanline
    tbb::parallel_for(tbb::blocked_range<int>(0, height),
        [&](const tbb::blocked_range<int>& rows) {
            for (int y = rows.begin(); y < rows.end(); ++y) {
                int row_offset = y * width;

                // Left-to-right sweep
                float4 streak{0.0f, 0.0f, 0.0f, 0.0f};
                for (int x = 0; x < width; ++x) {
                    int idx = row_offset + x;
                    float4 highlight_val = highlights[idx];
                    
                    // Additive combination with exponential decay
                    streak.x = highlight_val.x + streak.x * decay;
                    streak.y = highlight_val.y + streak.y * decay;
                    streak.z = highlight_val.z + streak.z * decay;
                    
                    streaks[idx] = streak;
                }

                // Right-to-left sweep (taking the max/additive contribution to spread both directions)
                streak = float4{0.0f, 0.0f, 0.0f, 0.0f};
                for (int x = width - 1; x >= 0; --x) {
                    int idx = row_offset + x;
                    float4 highlight_val = highlights[idx];

                    streak.x = highlight_val.x + streak.x * decay;
                    streak.y = highlight_val.y + streak.y * decay;
                    streak.z = highlight_val.z + streak.z * decay;

                    // Merge back using max / additive
                    streaks[idx].x = std::max(streaks[idx].x, streak.x);
                    streaks[idx].y = std::max(streaks[idx].y, streak.y);
                    streaks[idx].z = std::max(streaks[idx].z, streak.z);
                }
            }
        });

    // 3. Composite the anamorphic flares onto the original image (Additive Blend)
    tbb::parallel_for(tbb::blocked_range<size_t>(0, total_pixels),
        [&](const tbb::blocked_range<size_t>& range) {
            for (size_t i = range.begin(); i < range.end(); ++i) {
                float4 orig_pixel = original[i];
                float4 streak_pixel = streaks[i];

                // Apply tint and intensity to the horizontal flare
                float r_flare = streak_pixel.x * tint.x * intensity;
                float g_flare = streak_pixel.y * tint.y * intensity;
                float b_flare = streak_pixel.z * tint.z * intensity;

                // Additive blend with clamping to prevent HDR blow-outs
                buffer[i].x = std::clamp(orig_pixel.x + r_flare, 0.0f, 1.0f);
                buffer[i].y = std::clamp(orig_pixel.y + g_flare, 0.0f, 1.0f);
                buffer[i].z = std::clamp(orig_pixel.z + b_flare, 0.0f, 1.0f);
                buffer[i].w = orig_pixel.w; // Preserve original alpha
            }
        });
}

void AnamorphicFlare::process(ImageF32x4_RGBA& image, const AnamorphicFlareSettings& settings) {
    if (image.isEmpty()) return;
    float* raw_data = image.rgba32fData();
    if (!raw_data) return;

    process(reinterpret_cast<float4*>(raw_data), image.width(), image.height(), settings);
}

} // namespace ArtifactCore
