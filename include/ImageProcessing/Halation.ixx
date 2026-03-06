module;
#include <vector>
#include <cmath>
#include <algorithm>

export module ArtifactCore.ImageProcessing.Halation;

import Particle; // For float3, float4 definitions

namespace ArtifactCore {

/**
 * @brief Physical Film Halation Effect
 * Simulates light scattering in film emulsion layers.
 */
export class PhysicalHalation {
public:
    struct Settings {
        float threshold = 0.8f;      // Halation trigger luminance
        float spread = 15.0f;       // Diffusion radius
        float intensity = 0.5f;     // Overall strength
        float redDiffusion = 2.0f;  // Red layer scatters further in film
        float softness = 0.5f;      // Transition softness
    };

    /**
     * @brief Process an RGBA image buffer (float32x4)
     * Note: In a real implementation, this would be a GPU compute shader.
     * This C++ implementation provides the physical logic.
     */
    void process(float4* buffer, int width, int height, const Settings& settings) {
        if (!buffer || width <= 0 || height <= 0) return;
        ScopedPerformanceTimer timer("Film Halation");

        std::vector<float4> source(buffer, buffer + width * height);
        
        // 1. Threshold & Extract Highlights with Red bias
        std::vector<float4> highlights(width * height);
        for (int i = 0; i < width * height; ++i) {
            float lum = source[i].x * 0.299f + source[i].y * 0.587f + source[i].z * 0.114f;
            if (lum > settings.threshold) {
                float weight = std::clamp((lum - settings.threshold) / (1.0f - settings.threshold + 0.001f), 0.0f, 1.0f);
                // Halation is physically red-shifted due to scattering
                highlights[i] = {source[i].x * weight, source[i].y * weight * 0.2f, source[i].z * weight * 0.1f, 0.0f};
            } else {
                highlights[i] = {0, 0, 0, 0};
            }
        }

        // 2. Dual-Layer Diffusion (Simulating film layers)
        // Red scatters much wider than Green/Blue
        diffuse(highlights.data(), width, height, settings.spread * settings.redDiffusion, 1.0f, 0.0f, 0.0f); // Red layer
        diffuse(highlights.data(), width, height, settings.spread, 0.0f, 0.1f, 0.05f); // Minor Green/Blue scattering

        // 3. Composite back with Bloom-like additive blending
        for (int i = 0; i < width * height; ++i) {
            buffer[i].x += highlights[i].x * settings.intensity;
            buffer[i].y += highlights[i].y * settings.intensity;
            buffer[i].z += highlights[i].z * settings.intensity;
        }
    }

private:
    // Simple box-blur based diffusion (for physical demonstration)
    void diffuse(float4* data, int w, int h, float radius, float rM, float gM, float bM) {
        int r = static_cast<int>(radius);
        if (r <= 0) return;
        
        std::vector<float4> temp(w * h);
        // Horizontal Pass
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                float4 sum{0,0,0,0};
                int count = 0;
                for (int dx = -r; dx <= r; ++dx) {
                    int nx = std::clamp(x + dx, 0, w - 1);
                    sum.x += data[y * w + nx].x;
                    sum.y += data[y * w + nx].y;
                    sum.z += data[y * w + nx].z;
                    count++;
                }
                temp[y * w + x] = {sum.x / count, sum.y / count, sum.z / count, 0};
            }
        }
        // Vertical Pass + Channel Multiply
        for (int x = 0; x < w; ++x) {
            for (int y = 0; y < h; ++y) {
                float4 sum{0,0,0,0};
                int count = 0;
                for (int dy = -r; dy <= r; ++dy) {
                    int ny = std::clamp(y + dy, 0, h - 1);
                    sum.x += temp[ny * w + x].x;
                    sum.y += temp[ny * w + x].y;
                    sum.z += temp[ny * w + x].z;
                    count++;
                }
                data[y * w + x].x = (sum.x / count) * rM;
                data[y * w + x].y = (sum.y / count) * gM;
                data[y * w + x].z = (sum.z / count) * bM;
            }
        }
    }
};

} // namespace ArtifactCore
