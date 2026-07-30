module;
#include <algorithm>
#include <cmath>
#include <vector>
#include <array>
module ImageProcessing;
import :Halftone;
import Particle;
import Image.ImageF32x4_RGBA;
import Core.Parallel;

namespace ArtifactCore {

namespace {
    constexpr float kPi = 3.14159265358979323846f;

    // Normalised sinc function for anti-aliased dot evaluation.
    inline float boxCross(float x, float half) {
        // smooth step: 1 inside half, ~0 outside with a 0.5px ramp
        float d = std::abs(x) - half;
        if (d <= -0.5f) return 1.0f;
        if (d >=  0.5f) return 0.0f;
        return 0.5f - d;   // linear ramp
    }

    inline float circleCoverage(float dist, float radius) {
        // distance to nearest edge: radius - dist
        float edge = radius - dist;
        if (edge >= 0.5f) return 1.0f;
        if (edge <= -0.5f) return 0.0f;
        return 0.5f + edge;
    }

    inline float dotShapeFactor(
        float dx, float dy,
        float dotSize,
        HalftoneDotShape shape,
        float ellipseAspect)
    {
        float half = dotSize * 0.5f;
        switch (shape) {
        case HalftoneDotShape::Circle: {
            float dist = std::sqrt(dx * dx + dy * dy);
            // radius = half * intensity (applied later); here just shape mask
            return 1.0f - (dist / half); // return normalised distance (used later)
        }
        case HalftoneDotShape::Ellipse: {
            float ax = dx / ellipseAspect;
            float dist = std::sqrt(ax * ax + dy * dy);
            return 1.0f - (dist / half);
        }
        case HalftoneDotShape::Diamond: {
            float dist = std::abs(dx) + std::abs(dy);
            return 1.0f - dist / (half * 1.41421356f);
        }
        case HalftoneDotShape::Line: {
            // vertical line screen
            return 1.0f - std::abs(dx) / half;
        }
        case HalftoneDotShape::Cross: {
            float hx = 1.0f - std::abs(dx) / half;
            float hy = 1.0f - std::abs(dy) / half;
            return std::max(hx, hy);
        }
        }
        return 1.0f - std::sqrt(dx * dx + dy * dy) / half;
    }

    // Evaluate coverage for one channel given a rotated grid.
    // Returns 0 (ink) to 1 (paper).
    inline float channelCoverage(
        float rx, float ry,
        float dotSize,
        float halfDot,
        float intensity,
        HalftoneDotShape shape,
        float ellipseAspect)
    {
        float cellX = std::floor(rx / dotSize);
        float cellY = std::floor(ry / dotSize);
        float cx = (cellX + 0.5f) * dotSize;
        float cy = (cellY + 0.5f) * dotSize;
        float dx = rx - cx;
        float dy = ry - cy;

        float raw = dotShapeFactor(dx, dy, dotSize, shape, ellipseAspect);
        // raw: 1 at centre, 0 at edge, negative outside.
        float threshold = 1.0f - intensity;  // intensity 1 → always ink
        float edge = raw - threshold;
        if (edge >= 0.5f) return 0.0f;       // fully inked
        if (edge <= -0.5f) return 1.0f;      // fully paper
        return 0.5f - edge;                  // anti-aliased transition
    }
}

void Halftone::process(float4* buffer, int width, int height, const HalftoneSettings& settings) {
    if (!buffer || width <= 0 || height <= 0) return;

    std::vector<float4> original(buffer, buffer + width * height);
    float dotSize = std::max(settings.dotSize, 2.0f);
    float halfDot = dotSize * 0.5f;

    if (settings.colorMode == HalftoneColorMode::Monochrome ||
        settings.colorMode == HalftoneColorMode::Color) {
        const float angleRad = settings.angle * kPi / 180.0f;
        const float cosA = std::cos(angleRad);
        const float sinA = std::sin(angleRad);

        for (int y = 0; y < height; ++y) {
            const float py = static_cast<float>(y) + 0.5f;
            for (int x = 0; x < width; ++x) {
                const float px = static_cast<float>(x) + 0.5f;
                const float rx = px * cosA - py * sinA;
                const float ry = px * sinA + py * cosA;
                const int idx = y * width + x;
                const auto& src = original[idx];

                if (settings.colorMode == HalftoneColorMode::Monochrome) {
                    const float lum = src.x * 0.299f + src.y * 0.587f + src.z * 0.114f;
                    const float intensity = std::clamp(lum * settings.contrast, 0.0f, 1.0f);
                    const float ink = 1.0f - channelCoverage(rx, ry, dotSize, halfDot, intensity,
                                                             settings.dotShape, settings.ellipseAspect);
                    buffer[idx].x = ink;
                    buffer[idx].y = ink;
                    buffer[idx].z = ink;
                    buffer[idx].w = src.w;
                } else {
                    const float r = std::clamp(src.x * settings.contrast, 0.0f, 1.0f);
                    const float g = std::clamp(src.y * settings.contrast, 0.0f, 1.0f);
                    const float b = std::clamp(src.z * settings.contrast, 0.0f, 1.0f);
                    buffer[idx].x = 1.0f - channelCoverage(rx, ry, dotSize, halfDot, r,
                                                           settings.dotShape, settings.ellipseAspect);
                    buffer[idx].y = 1.0f - channelCoverage(rx, ry, dotSize, halfDot, g,
                                                           settings.dotShape, settings.ellipseAspect);
                    buffer[idx].z = 1.0f - channelCoverage(rx, ry, dotSize, halfDot, b,
                                                           settings.dotShape, settings.ellipseAspect);
                    buffer[idx].w = src.w;
                }
            }
        }
    } else {
        // CMYK: 4 separations, each with its own angle
        // Convert RGB → CMYK (naive inverse)
        // Assume src is sRGB 0-1
        std::vector<std::array<float, 4>> cmyk(width * height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int i = y * width + x;
                const auto& s = original[i];
                const float k = 1.0f - std::max({s.x, s.y, s.z});
                const float c = k < 1.0f ? (1.0f - s.x - k) / (1.0f - k) : 0.0f;
                const float m = k < 1.0f ? (1.0f - s.y - k) / (1.0f - k) : 0.0f;
                const float y_ = k < 1.0f ? (1.0f - s.z - k) / (1.0f - k) : 0.0f;
                cmyk[i] = {c, m, y_, k};
            }
        }

        std::fill(buffer, buffer + width * height, float4{1.0f, 1.0f, 1.0f, 1.0f});

        // Simplified CMYK screen: treat as 4-channel monochrome screens composited.
        for (int ch = 0; ch < 4; ++ch) {
            const float angleRad = settings.cmykAngles[ch] * kPi / 180.0f;
            const float cosA = std::cos(angleRad);
            const float sinA = std::sin(angleRad);

            for (int y = 0; y < height; ++y) {
                const float py = static_cast<float>(y) + 0.5f;
                for (int x = 0; x < width; ++x) {
                    const float px = static_cast<float>(x) + 0.5f;
                    const float rx = px * cosA - py * sinA;
                    const float ry = px * sinA + py * cosA;
                    const int idx = y * width + x;
                    const float intensity = std::clamp(cmyk[idx][ch] * settings.contrast, 0.0f, 1.0f);
                    const float ink = channelCoverage(rx, ry, dotSize, halfDot, intensity,
                                                      settings.dotShape, settings.ellipseAspect);
                    buffer[idx].x *= ink;
                    buffer[idx].y *= ink;
                    buffer[idx].z *= ink;
                }
            }
        }
    }
}

void Halftone::process(ImageF32x4_RGBA& image, const HalftoneSettings& settings) {
    if (image.isEmpty()) return;
    float* raw = image.rgba32fData();
    if (!raw) return;
    process(reinterpret_cast<float4*>(raw), image.width(), image.height(), settings);
}

}
