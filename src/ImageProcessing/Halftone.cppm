module;
#include <algorithm>
#include <cmath>
#include <vector>

module ImageProcessing:Halftone;

import Particle;
import Image.ImageF32x4_RGBA;

namespace ArtifactCore {

namespace {
    constexpr float kPi = 3.14159265358979323846f;
}

void Halftone::process(float4* buffer, int width, int height, const HalftoneSettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;

    // Local copy of original image so we can sample original luminance dynamically
    std::vector<float4> original(buffer, buffer + width * height);

    float angle_rad = settings.angle * kPi / 180.0f;
    float cos_a = std::cos(angle_rad);
    float sin_a = std::sin(angle_rad);
    
    // Reverse rotation parameters to go back to original space
    float cos_neg_a = std::cos(-angle_rad);
    float sin_neg_a = std::sin(-angle_rad);

    float dot_size = std::max(settings.dotSize, 2.0f);
    float half_dot = dot_size * 0.5f;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float px = static_cast<float>(x);
            float py = static_cast<float>(y);

            // 1. Rotate to grid space
            float rx = px * cos_a - py * sin_a;
            float ry = px * sin_a + py * cos_a;

            // 2. Identify grid cell
            float cell_x = std::floor(rx / dot_size);
            float cell_y = std::floor(ry / dot_size);

            // 3. Local coordinates inside the cell center
            float center_rx = (cell_x + 0.5f) * dot_size;
            float center_ry = (cell_y + 0.5f) * dot_size;

            float dist_to_center = std::sqrt((rx - center_rx) * (rx - center_rx) + (ry - center_ry) * (ry - center_ry));

            // 4. Translate cell center back to original pixel coordinates
            float orig_cx = center_rx * cos_neg_a - center_ry * sin_neg_a;
            float orig_cy = center_rx * sin_neg_a + center_ry * cos_neg_a;

            // Clamp sample coordinate to boundary
            int sample_x = std::clamp(static_cast<int>(orig_cx + 0.5f), 0, width - 1);
            int sample_y = std::clamp(static_cast<int>(orig_cy + 0.5f), 0, height - 1);

            // 5. Sample luminance at the cell center
            float4 sample_color = original[sample_y * width + sample_x];
            float luminance = sample_color.x * 0.299f + sample_color.y * 0.587f + sample_color.z * 0.114f;

            // 6. Dot size based on luminance (darker = larger black dot)
            float contrast_lum = std::clamp(luminance * settings.contrast, 0.0f, 1.0f);
            float dot_radius = half_dot * (1.0f - contrast_lum);

            // 7. Calculate dot coverage factor (rough alias-free step or simple threshold)
            float factor = 1.0f;
            if (dist_to_center < dot_radius) {
                factor = 0.0f; // Inside black dot
            }

            // 8. Output color
            int idx = y * width + x;
            float4 original_pixel = original[idx];

            if (settings.isColor) {
                // Apply dot factor to original pixel colors
                buffer[idx].x = original_pixel.x * factor;
                buffer[idx].y = original_pixel.y * factor;
                buffer[idx].z = original_pixel.z * factor;
                buffer[idx].w = original_pixel.w;
            } else {
                // Black and white halftone screen
                buffer[idx].x = factor;
                buffer[idx].y = factor;
                buffer[idx].z = factor;
                buffer[idx].w = original_pixel.w;
            }
        }
    }
}

void Halftone::process(ImageF32x4_RGBA& image, const HalftoneSettings& settings) {
    if (image.isEmpty()) return;
    float* raw_data = image.rgba32fData();
    if (!raw_data) return;

    process(reinterpret_cast<float4*>(raw_data), image.width(), image.height(), settings);
}

} // namespace ArtifactCore
