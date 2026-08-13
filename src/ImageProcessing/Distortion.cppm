module;
#include <utility>
#include <cmath>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <vector>

module ImageProcessing.Distortion;


import FloatRGBA;
import Image.ImageF32x4_RGBA;
import Core.Parallel;

namespace ArtifactCore
{

void morphImages(const ImageF32x4_RGBA& source,
                 const ImageF32x4_RGBA& target,
                 ImageF32x4_RGBA& output,
                 const std::vector<MorphControlPoint>& controlPoints,
                 float amount,
                 bool bilinear)
{
    const int width = std::min(source.width(), target.width());
    const int height = std::min(source.height(), target.height());
    if (width <= 0 || height <= 0) {
        output = ImageF32x4_RGBA();
        return;
    }

    const float t = std::clamp(amount, 0.0f, 1.0f);
    output.resize(width, height);
    float* dst = output.rgba32fData();

    auto sample = [bilinear](const ImageF32x4_RGBA& image, float x, float y) {
        return bilinear ? sampleBilinear(image, x, y)
                        : sampleNearest(image, x, y);
    };

    Parallel::For(0, height, width * height, [&](int y) {
        for (int x = 0; x < width; ++x) {
            float sourceX = static_cast<float>(x);
            float sourceY = static_cast<float>(y);
            float targetX = static_cast<float>(x);
            float targetY = static_cast<float>(y);
            float sourceWeight = 0.0f;
            float targetWeight = 0.0f;

            for (const MorphControlPoint& point : controlPoints) {
                const float dx = static_cast<float>(x) -
                                 (point.sourceX * (1.0f - t) + point.targetX * t);
                const float dy = static_cast<float>(y) -
                                 (point.sourceY * (1.0f - t) + point.targetY * t);
                const float radius2 = dx * dx + dy * dy;
                const float weight = std::max(0.0f, point.weight) /
                                     std::max(1.0f, radius2);
                sourceX += weight * (point.sourceX - point.targetX) * t;
                sourceY += weight * (point.sourceY - point.targetY) * t;
                targetX += weight * (point.targetX - point.sourceX) * (1.0f - t);
                targetY += weight * (point.targetY - point.sourceY) * (1.0f - t);
                sourceWeight += weight;
                targetWeight += weight;
            }

            if (sourceWeight > 0.0f) {
                sourceX = static_cast<float>(x) +
                          (sourceX - static_cast<float>(x)) / sourceWeight;
                sourceY = static_cast<float>(y) +
                          (sourceY - static_cast<float>(y)) / sourceWeight;
            }
            if (targetWeight > 0.0f) {
                targetX = static_cast<float>(x) +
                          (targetX - static_cast<float>(x)) / targetWeight;
                targetY = static_cast<float>(y) +
                          (targetY - static_cast<float>(y)) / targetWeight;
            }

            const FloatRGBA a = sample(source, sourceX, sourceY);
            const FloatRGBA b = sample(target, targetX, targetY);
            float* pixel = dst + (static_cast<size_t>(y) * width + x) * 4u;
            pixel[0] = a.r() * (1.0f - t) + b.r() * t;
            pixel[1] = a.g() * (1.0f - t) + b.g() * t;
            pixel[2] = a.b() * (1.0f - t) + b.b() * t;
            pixel[3] = a.a() * (1.0f - t) + b.a() * t;
        }
    });
}

namespace {

 constexpr float kPi = 3.14159265358979323846f;
 constexpr float kTwoPi = kPi * 2.0f;

 // Hash-based pseudo-random [0, 1]
 float hash2D(float x, float y, int seed)
 {
  float h = std::fmod(std::sin(x * 127.1f + y * 311.7f + seed * 73.13f) * 43758.5453f, 1.0f);
  return h < 0.0f ? h + 1.0f : h;
 }

 float lerpF(float a, float b, float t) { return a + t * (b - a); }

 float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

} // anonymous

// === Sampling ===

FloatRGBA sampleBilinear(const ImageF32x4_RGBA& src, float sx, float sy)
{
 int w = src.width();
 int h = src.height();
 if (w <= 0 || h <= 0) return FloatRGBA{ 0, 0, 0, 0 };

 // Wrap coordinates
 sx = std::fmod(sx, static_cast<float>(w));
 if (sx < 0) sx += w;
 sy = std::fmod(sy, static_cast<float>(h));
 if (sy < 0) sy += h;

 int x0 = static_cast<int>(sx);
 int y0 = static_cast<int>(sy);
 int x1 = (x0 + 1) % w;
 int y1 = (y0 + 1) % h;

 float fx = sx - x0;
 float fy = sy - y0;

 const float* pixels = src.rgba32fData();
 const float* c00 = pixels + (static_cast<size_t>(y0) * w + x0) * 4u;
 const float* c10 = pixels + (static_cast<size_t>(y0) * w + x1) * 4u;
 const float* c01 = pixels + (static_cast<size_t>(y1) * w + x0) * 4u;
 const float* c11 = pixels + (static_cast<size_t>(y1) * w + x1) * 4u;

 float r = lerpF(lerpF(c00[0], c10[0], fx), lerpF(c01[0], c11[0], fx), fy);
 float g = lerpF(lerpF(c00[1], c10[1], fx), lerpF(c01[1], c11[1], fx), fy);
 float b = lerpF(lerpF(c00[2], c10[2], fx), lerpF(c01[2], c11[2], fx), fy);
 float a = lerpF(lerpF(c00[3], c10[3], fx), lerpF(c01[3], c11[3], fx), fy);

 return FloatRGBA{ r, g, b, a };
}

FloatRGBA sampleNearest(const ImageF32x4_RGBA& src, float sx, float sy)
{
 int w = src.width();
 int h = src.height();
 if (w <= 0 || h <= 0) return FloatRGBA{ 0, 0, 0, 0 };

 int x = static_cast<int>(sx + 0.5f) % w;
 int y = static_cast<int>(sy + 0.5f) % h;
 if (x < 0) x += w;
 if (y < 0) y += h;

 const float* pixel = src.rgba32fData() +
     (static_cast<size_t>(y) * w + x) * 4u;
 return FloatRGBA(pixel[0], pixel[1], pixel[2], pixel[3]);
}

// === Apply displacement ===

void applyDisplacement(const ImageF32x4_RGBA& src,
                        ImageF32x4_RGBA& dst,
                        DisplacementFunc func,
                        bool bilinear)
{
 int w = src.width();
 int h = src.height();
 if (w <= 0 || h <= 0) return;

 dst.resize(w, h);

 const float* sourcePixels = src.rgba32fData();
 float* destinationPixels = dst.rgba32fData();
 auto sampleRaw = [&](float sx, float sy) {
    if (bilinear) {
        sx = std::fmod(sx, static_cast<float>(w));
        if (sx < 0.0f) sx += w;
        sy = std::fmod(sy, static_cast<float>(h));
        if (sy < 0.0f) sy += h;

        const int x0 = static_cast<int>(sx);
        const int y0 = static_cast<int>(sy);
        const int x1 = (x0 + 1) % w;
        const int y1 = (y0 + 1) % h;
        const float fx = sx - x0;
        const float fy = sy - y0;
        const float* c00 = sourcePixels +
            (static_cast<size_t>(y0) * w + x0) * 4u;
        const float* c10 = sourcePixels +
            (static_cast<size_t>(y0) * w + x1) * 4u;
        const float* c01 = sourcePixels +
            (static_cast<size_t>(y1) * w + x0) * 4u;
        const float* c11 = sourcePixels +
            (static_cast<size_t>(y1) * w + x1) * 4u;
        return FloatRGBA(
            lerpF(lerpF(c00[0], c10[0], fx), lerpF(c01[0], c11[0], fx), fy),
            lerpF(lerpF(c00[1], c10[1], fx), lerpF(c01[1], c11[1], fx), fy),
            lerpF(lerpF(c00[2], c10[2], fx), lerpF(c01[2], c11[2], fx), fy),
            lerpF(lerpF(c00[3], c10[3], fx), lerpF(c01[3], c11[3], fx), fy));
    }

    int x = static_cast<int>(sx + 0.5f) % w;
    int y = static_cast<int>(sy + 0.5f) % h;
    if (x < 0) x += w;
    if (y < 0) y += h;
    const float* pixel = sourcePixels +
        (static_cast<size_t>(y) * w + x) * 4u;
    return FloatRGBA(pixel[0], pixel[1], pixel[2], pixel[3]);
 };

 Parallel::For(0, h, w * h, [&](int y) {
    float sy = static_cast<float>(y);
    for (int x = 0; x < w; ++x) {
     float sx = static_cast<float>(x);
     float outSX = sx, outSY = sy;
     func(sx, sy, static_cast<float>(w), static_cast<float>(h), outSX, outSY);
     const FloatRGBA pixel = sampleRaw(outSX, outSY);
     float* destination = destinationPixels +
         (static_cast<size_t>(y) * w + x) * 4u;
     destination[0] = pixel.r();
     destination[1] = pixel.g();
     destination[2] = pixel.b();
     destination[3] = pixel.a();
    }
 });
}

// === Value noise ===

float valueNoise2D(float x, float y, int seed)
{
 int ix = static_cast<int>(std::floor(x));
 int iy = static_cast<int>(std::floor(y));
 float fx = x - ix;
 float fy = y - iy;

 float u = smoothstep(fx);
 float v = smoothstep(fy);

 float n00 = hash2D(static_cast<float>(ix),     static_cast<float>(iy),     seed);
 float n10 = hash2D(static_cast<float>(ix + 1), static_cast<float>(iy),     seed);
 float n01 = hash2D(static_cast<float>(ix),     static_cast<float>(iy + 1), seed);
 float n11 = hash2D(static_cast<float>(ix + 1), static_cast<float>(iy + 1), seed);

 return lerpF(lerpF(n00, n10, u), lerpF(n01, n11, u), v);
}

float fbmNoise2D(float x, float y, int octaves, float lacunarity, float gain, int seed)
{
 float sum = 0.0f;
 float amp = 0.5f;
 float freq = 1.0f;
 for (int i = 0; i < octaves; ++i) {
  sum += amp * valueNoise2D(x * freq, y * freq, seed + i * 31);
  freq *= lacunarity;
  amp *= gain;
 }
 return sum;
}

// === Displacement mappers ===

DisplacementFunc makePinchBulge(float cx, float cy, float radius, float amount)
{
 return [cx, cy, radius, amount](float sx, float sy, float, float, float& ox, float& oy) {
  float dx = sx - cx;
  float dy = sy - cy;
  float dist = std::sqrt(dx * dx + dy * dy);
  if (dist < 0.001f || dist > radius) { ox = sx; oy = sy; return; }
  float t = dist / radius;
  float r = std::pow(t, 1.0f + amount * 0.01f) * radius;
  float scale = r / dist;
  ox = cx + dx * scale;
  oy = cy + dy * scale;
 };
}

DisplacementFunc makeSpherize(float cx, float cy, float radius, float amount)
{
 return [cx, cy, radius, amount](float sx, float sy, float, float, float& ox, float& oy) {
  float dx = sx - cx;
  float dy = sy - cy;
  float dist = std::sqrt(dx * dx + dy * dy);
  if (dist < 0.001f || dist > radius) { ox = sx; oy = sy; return; }
  float t = dist / radius;
  float factor = amount * 0.01f;
  float s = 1.0f + factor * (1.0f - std::sqrt(1.0f - t * t)) / (t + 0.001f);
  ox = cx + dx * s;
  oy = cy + dy * s;
 };
}

DisplacementFunc makeTwirl(float cx, float cy, float radius, float angleDeg)
{
 return [cx, cy, radius, angleDeg](float sx, float sy, float, float, float& ox, float& oy) {
  float dx = sx - cx;
  float dy = sy - cy;
  float dist = std::sqrt(dx * dx + dy * dy);
  if (dist < 0.001f || dist > radius) { ox = sx; oy = sy; return; }
  float falloff = 1.0f - dist / radius;
  float angle = angleDeg * falloff * kPi / 180.0f;
  float cosA = std::cos(angle);
  float sinA = std::sin(angle);
  ox = cx + dx * cosA - dy * sinA;
  oy = cy + dx * sinA + dy * cosA;
 };
}

DisplacementFunc makeWave(float ampX, float ampY, float freqX, float freqY,
                           float phaseX, float phaseY)
{
 return [ampX, ampY, freqX, freqY, phaseX, phaseY]
        (float sx, float sy, float, float, float& ox, float& oy) {
  ox = sx + ampX * std::sin(kTwoPi * freqY * sy + phaseY);
  oy = sy + ampY * std::sin(kTwoPi * freqX * sx + phaseX);
 };
}

DisplacementFunc makeTurbulentDisplace(float amount, float size, float complexity,
                                        int octaves, float evolution)
{
 return [amount, size, complexity, octaves, evolution]
        (float sx, float sy, float, float, float& ox, float& oy) {
  float scale = 1.0f / std::max(size, 0.001f);
  float nx = fbmNoise2D(sx * scale + evolution, sy * scale,
                         octaves, complexity, 0.5f, 42);
  float ny = fbmNoise2D(sx * scale, sy * scale + evolution,
                         octaves, complexity, 0.5f, 97);
  ox = sx + (nx - 0.5f) * 2.0f * amount;
  oy = sy + (ny - 0.5f) * 2.0f * amount;
 };
}

DisplacementFunc makeOpticsCompensation(float cx, float cy, float fov, int direction)
{
 return [cx, cy, fov, direction](float sx, float sy, float, float, float& ox, float& oy) {
  float dx = (sx - cx) / cx;
  float dy = (sy - cy) / cy;
  float r2 = dx * dx + dy * dy;
  float k = fov * 0.001f * static_cast<float>(direction);
  float denom = 1.0f - k * r2;
  if (std::abs(denom) < 0.001f) { ox = sx; oy = sy; return; }
  float invD = 1.0f / denom;
  ox = cx + dx * cx * invD;
  oy = cy + dy * cy * invD;
 };
}

DisplacementFunc makeBilinearWarp(float tlX, float tlY, float trX, float trY,
                                   float blX, float blY, float brX, float brY)
{
 return [tlX,tlY,trX,trY,blX,blY,brX,brY]
        (float sx, float sy, float imgW, float imgH, float& ox, float& oy) {
  float u = sx / imgW;
  float v = sy / imgH;
  float oneMinusU = 1.0f - u;
  float oneMinusV = 1.0f - v;
  ox = oneMinusU * oneMinusV * tlX + u * oneMinusV * trX +
       oneMinusU * v * blX + u * v * brX;
  oy = oneMinusU * oneMinusV * tlY + u * oneMinusV * trY +
       oneMinusU * v * blY + u * v * brY;
 };
}

DisplacementFunc makeOffset(float dx, float dy)
{
 return [dx, dy](float sx, float sy, float, float, float& ox, float& oy) {
  ox = sx + dx;
  oy = sy + dy;
 };
}

DisplacementFunc makeScale(float cx, float cy, float scaleX, float scaleY)
{
 return [cx, cy, scaleX, scaleY](float sx, float sy, float, float, float& ox, float& oy) {
  ox = cx + (sx - cx) / scaleX;
  oy = cy + (sy - cy) / scaleY;
 };
}

DisplacementFunc makeRotate(float cx, float cy, float angleDeg)
{
 float angle = -angleDeg * kPi / 180.0f;
 float cosA = std::cos(angle);
 float sinA = std::sin(angle);
 return [cx, cy, cosA, sinA](float sx, float sy, float, float, float& ox, float& oy) {
  float dx = sx - cx;
  float dy = sy - cy;
  ox = cx + dx * cosA - dy * sinA;
  oy = cy + dx * sinA + dy * cosA;
 };
}

DisplacementFunc makeMirror(float axisX, float axisY, bool horizontal, bool vertical)
{
 return [axisX, axisY, horizontal, vertical](float sx, float sy, float, float, float& ox, float& oy) {
  ox = horizontal ? 2.0f * axisX - sx : sx;
  oy = vertical   ? 2.0f * axisY - sy : sy;
 };
}

DisplacementFunc makeNoiseDisplace(float amount, float size, int seed, float evolution)
{
 return [amount, size, seed, evolution](float sx, float sy, float, float, float& ox, float& oy) {
  float scale = 1.0f / std::max(size, 0.001f);
  float nx = valueNoise2D(sx * scale + evolution, sy * scale, seed);
  float ny = valueNoise2D(sx * scale, sy * scale + evolution, seed + 31);
  ox = sx + (nx - 0.5f) * 2.0f * amount;
  oy = sy + (ny - 0.5f) * 2.0f * amount;
 };
}

DisplacementFunc makeKaleidoscope(float cx, float cy, int segments, float angleDegrees)
{
 float phi = kTwoPi / std::max(segments, 1);
 float angleRad = angleDegrees * kPi / 180.0f;
 return [cx, cy, phi, angleRad](float sx, float sy, float, float, float& ox, float& oy) {
  float dx = sx - cx;
  float dy = sy - cy;
  float dist = std::sqrt(dx * dx + dy * dy);
  if (dist < 0.001f) {
   ox = sx;
   oy = sy;
   return;
  }

  // Polar coordinates conversion
  float theta = std::atan2(dy, dx);

  // Apply rotation
  float theta_prime = theta - angleRad;

  // Segment modulo wrapping
  float theta_local = std::fmod(theta_prime, phi);
  if (theta_local < 0.0f) {
   theta_local += phi;
  }

  // Reflection (Mirror symmetry within the sector)
  if (theta_local > phi * 0.5f) {
   theta_local = phi - theta_local;
  }

  // Convert back to Cartesian
  ox = cx + dist * std::cos(theta_local + angleRad);
  oy = cy + dist * std::sin(theta_local + angleRad);
 };
}

}
