module;
#include <vector>
#include <cmath>
#include <algorithm>

module ImageProcessing.FluidVisualizer;

namespace ArtifactCore {

float4 FluidVisualizer::sampleGradient(float t, const Style& style) {
    if (!style.useFireGradient) {
        // Simple base to highlight interpolation
        return {
            style.baseColor.x + (style.highlightColor.x - style.baseColor.x) * t,
            style.baseColor.y + (style.highlightColor.y - style.baseColor.y) * t,
            style.baseColor.z + (style.highlightColor.z - style.baseColor.z) * t,
            style.baseColor.w * t
        };
    } else {
        // Fire heatmap: Black -> Red -> Yellow -> White
        if (t < 0.33f) {
            float s = t / 0.33f;
            return {s, 0, 0, s};
        } else if (t < 0.66f) {
            float s = (t - 0.33f) / 0.33f;
            return {1.0f, s, 0, 1.0f};
        } else {
            float s = (t - 0.66f) / 0.33f;
            return {1.0f, 1.0f, s, 1.0f};
        }
    }
}

void FluidVisualizer::render(float4* buffer, int width, int height, const FluidSolver2D& fluid, const Style& style) {
    if (!buffer || width <= 0 || height <= 0) return;

    std::vector<float4> source(buffer, buffer + width * height);
    
    float gx = static_cast<float>(fluid.width()) / width;
    float gy = static_cast<float>(fluid.height()) / height;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int ix = static_cast<int>(x * gx);
            int iy = static_cast<int>(y * gy);
            
            // 1. Refraction (Warping original image based on velocity)
            float vx, vy;
            fluid.getVelocity(ix, iy, vx, vy);
            
            float sx = x - vx * style.refractionStrength * width;
            float sy = y - vy * style.refractionStrength * height;
            
            // Bilinear sampling from source
            int x0 = std::clamp(static_cast<int>(sx), 0, width - 1);
            int y0 = std::clamp(static_cast<int>(sy), 0, height - 1);
            int x1 = std::clamp(x0 + 1, 0, width - 1);
            int y1 = std::clamp(y0 + 1, 0, height - 1);
            
            float fx = sx - std::floor(sx);
            float fy = sy - std::floor(sy);
            
            float4 s00 = source[y0 * width + x0];
            float4 s10 = source[y0 * width + x1];
            float4 s01 = source[y1 * width + x0];
            float4 s11 = source[y1 * width + x1];
            
            float4 warped = {
                (1-fx)*(1-fy)*s00.x + fx*(1-fy)*s10.x + (1-fx)*fy*s01.x + fx*fy*s11.x,
                (1-fx)*(1-fy)*s00.y + fx*(1-fy)*s10.y + (1-fx)*fy*s01.y + fx*fy*s11.y,
                (1-fx)*(1-fy)*s00.z + fx*(1-fy)*s10.z + (1-fx)*fy*s01.z + fx*fy*s11.z,
                (1-fx)*(1-fy)*s00.w + fx*(1-fy)*s10.w + (1-fx)*fy*s01.w + fx*fy*s11.w
            };

            // 2. Additive coloring (Density-based)
            float density = std::clamp(fluid.getDensity(ix, iy) * style.densityMultiplier, 0.0f, 1.0f);
            
            // 3. Density Gradient for Specular Highlights (Surface feel)
            float d_up = fluid.getDensity(ix, std::max(0, iy - 1));
            float d_down = fluid.getDensity(ix, std::min(fluid.height() - 1, iy + 1));
            float d_left = fluid.getDensity(std::max(0, ix - 1), iy);
            float d_right = fluid.getDensity(std::min(fluid.width() - 1, ix + 1), iy);
            
            float gradX = d_right - d_left;
            float gradY = d_down - d_up;
            float gradMag = std::sqrt(gradX*gradX + gradY*gradY);
            
            float specular = std::pow(std::clamp(gradMag * 10.0f, 0.0f, 1.0f), 5.0f);
            
            float4 col = sampleGradient(density, style);
            
            // Blend: Screen/Additive mix logic
            buffer[y * width + x].x = warped.x + col.x * col.w * style.glowIntensity + specular;
            buffer[y * width + x].y = warped.y + col.y * col.w * style.glowIntensity + specular;
            buffer[y * width + x].z = warped.z + col.z * col.w * style.glowIntensity + specular;
            buffer[y * width + x].w = std::max(warped.w, col.w);
        }
    }
}

} // namespace ArtifactCore
