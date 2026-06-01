module;
#include <algorithm>
#include <cmath>
#include <vector>

module ImageProcessing:TiltShift;

import Particle;
import Image.ImageF32x4_RGBA;

namespace ArtifactCore {

namespace {
    // A fast horizontal and vertical box-blur pass to create the blurred reference image
    void fastBlur(const std::vector<float4>& src, std::vector<float4>& dst, int w, int h, int radius) {
        if (radius <= 0) {
            dst = src;
            return;
        }

        std::vector<float4> temp(w * h);

        // Horizontal pass
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float4 sum{0.0f, 0.0f, 0.0f, 0.0f};
                int count = 0;
                for (int dx = -radius; dx <= radius; ++dx) {
                    int kx = std::clamp(x + dx, 0, w - 1);
                    float4 pixel = src[y * w + kx];
                    sum.x += pixel.x;
                    sum.y += pixel.y;
                    sum.z += pixel.z;
                    sum.w += pixel.w;
                    count++;
                }
                temp[y * w + x] = float4{sum.x / count, sum.y / count, sum.z / count, sum.w / count};
            }
        }

        // Vertical pass
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float4 sum{0.0f, 0.0f, 0.0f, 0.0f};
                int count = 0;
                for (int dy = -radius; dy <= radius; ++dy) {
                    int ky = std::clamp(y + dy, 0, h - 1);
                    float4 pixel = temp[ky * w + x];
                    sum.x += pixel.x;
                    sum.y += pixel.y;
                    sum.z += pixel.z;
                    sum.w += pixel.w;
                    count++;
                }
                dst[y * w + x] = float4{sum.x / count, sum.y / count, sum.z / count, sum.w / count};
            }
        }
    }
}

void TiltShift::process(float4* buffer, int width, int height, const TiltShiftSettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;

    // 1. Create blurred copy
    std::vector<float4> original(buffer, buffer + width * height);
    std::vector<float4> blurred(width * height);

    // Box blur radius depends on image size for aesthetic balance, clamped to a reasonable range
    int blur_radius = std::clamp(std::max(width, height) / 100, 3, 20);
    fastBlur(original, blurred, width, height, blur_radius);

    float focus_pos = std::clamp(settings.focusPos, 0.0f, 1.0f);
    float focus_width = std::clamp(settings.focusWidth, 0.0f, 1.0f);
    float falloff = std::max(settings.blurFalloff, 0.001f);
    float max_blur = std::clamp(settings.maxBlur, 0.0f, 1.0f);

    // 2. Linear-interpolate based on distance to horizontal focus line
    for (int y = 0; y < height; ++y) {
        float normalized_y = static_cast<float>(y) / height;
        float distance = std::abs(normalized_y - focus_pos);

        // Calculate blend factor W
        float w_factor = 0.0f;
        if (distance > focus_width * 0.5f) {
            w_factor = std::clamp((distance - focus_width * 0.5f) / falloff, 0.0f, 1.0f) * max_blur;
        }

        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            float4 orig_pixel = original[idx];
            float4 blur_pixel = blurred[idx];

            buffer[idx].x = orig_pixel.x * (1.0f - w_factor) + blur_pixel.x * w_factor;
            buffer[idx].y = orig_pixel.y * (1.0f - w_factor) + blur_pixel.y * w_factor;
            buffer[idx].z = orig_pixel.z * (1.0f - w_factor) + blur_pixel.z * w_factor;
            buffer[idx].w = orig_pixel.w; // Preserve alpha
        }
    }
}

void TiltShift::process(ImageF32x4_RGBA& image, const TiltShiftSettings& settings) {
    if (image.isEmpty()) return;
    float* raw_data = image.rgba32fData();
    if (!raw_data) return;

    process(reinterpret_cast<float4*>(raw_data), image.width(), image.height(), settings);
}

} // namespace ArtifactCore
