module;
#include <vector>
#include <cmath>
#include <algorithm>

export module ArtifactCore.ImageProcessing.VolumetricShine;

import Particle; // For float2, float3, float4 definitions

namespace ArtifactCore {

/**
 * @brief Volumetric Ray Shine (God Rays)
 * Generates radial light rays from high-luminance points.
 */
export class VolumetricShine {
public:
    struct Settings {
        float2 sourcePos{0.5f, 0.5f}; // Normalized source center (0..1)
        float rayLength = 0.5f;       // Length of the rays
        float intensity = 1.0f;       // Brightness
        float decay = 0.95f;          // Falloff (0..1)
        int samples = 32;             // Number of radial samples
        float4 tint{1.0f, 0.9f, 0.7f, 1.0f}; // Ray color
    };

    /**
     * @brief Apply Shine effect to image buffer
     */
    void process(float4* buffer, int width, int height, const Settings& settings) {
        if (!buffer || width <= 0 || height <= 0) return;
        ScopedPerformanceTimer timer("Volumetric Shine");

        std::vector<float4> original(buffer, buffer + width * height);
        float2 center{settings.sourcePos.x * width, settings.sourcePos.y * height};

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float2 current{static_cast<float>(x), static_cast<float>(y)};
                float2 dir{current.x - center.x, current.y - center.y};
                
                // Scale direction by ray length
                dir.x *= settings.rayLength / settings.samples;
                dir.y *= settings.rayLength / settings.samples;

                float4 shineColor{0, 0, 0, 0};
                float illuminationDecay = 1.0f;
                float2 samplePos = current;

                // Radial accumulation pass
                for (int i = 0; i < settings.samples; ++i) {
                    samplePos.x -= dir.x;
                    samplePos.y -= dir.y;

                    int sx = std::clamp(static_cast<int>(samplePos.x), 0, width - 1);
                    int sy = std::clamp(static_cast<int>(samplePos.y), 0, height - 1);

                    float4 sample = original[sy * width + sx];
                    
                    // Add luminance to shine (High luminosity produces more rays)
                    float lum = sample.x * 0.299f + sample.y * 0.587f + sample.z * 0.114f;
                    shineColor.x += sample.x * illuminationDecay * settings.intensity;
                    shineColor.y += sample.y * illuminationDecay * settings.intensity;
                    shineColor.z += sample.z * illuminationDecay * settings.intensity;

                    illuminationDecay *= settings.decay;
                }

                // Apply tint and blend additive
                buffer[y * width + x].x += (shineColor.x / settings.samples) * settings.tint.x;
                buffer[y * width + x].y += (shineColor.y / settings.samples) * settings.tint.y;
                buffer[y * width + x].z += (shineColor.z / settings.samples) * settings.tint.z;
            }
        }
    }
};

} // namespace ArtifactCore
