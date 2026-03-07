module;
#include <cmath>
#include <algorithm>
module Generator;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>

import :Noise;


import Math.Noise;

namespace ArtifactCore {

static void setPixelRGBA(float* pixels, int width, int x, int y,
                          float r, float g, float b, float a = 1.0f) {
    int idx = (y * width + x) * 4;
    pixels[idx + 0] = r;  pixels[idx + 1] = g;
    pixels[idx + 2] = b;  pixels[idx + 3] = a;
}

void NoiseImageGenerator::perlinNoise(float* pixels, int width, int height,
                                       float scale, float offsetX, float offsetY) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float nx = static_cast<float>(x) / width * scale + offsetX;
            float ny = static_cast<float>(y) / height * scale + offsetY;
            float val = NoiseGenerator::perlin(nx, ny) * 0.5f + 0.5f;
            val = std::clamp(val, 0.0f, 1.0f);
            setPixelRGBA(pixels, width, x, y, val, val, val);
        }
    }
}

void NoiseImageGenerator::perlinNoiseColor(float* pixels, int width, int height, float scale) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float nx = static_cast<float>(x) / width * scale;
            float ny = static_cast<float>(y) / height * scale;
            float r = NoiseGenerator::perlin(nx, ny, 0.0f) * 0.5f + 0.5f;
            float g = NoiseGenerator::perlin(nx, ny, 100.0f) * 0.5f + 0.5f;
            float b = NoiseGenerator::perlin(nx, ny, 200.0f) * 0.5f + 0.5f;
            setPixelRGBA(pixels, width, x, y,
                         std::clamp(r, 0.0f, 1.0f),
                         std::clamp(g, 0.0f, 1.0f),
                         std::clamp(b, 0.0f, 1.0f));
        }
    }
}

void NoiseImageGenerator::fractalNoise(float* pixels, int width, int height,
                                        float scale, int octaves,
                                        float persistence, float lacunarity) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float nx = static_cast<float>(x) / width * scale;
            float ny = static_cast<float>(y) / height * scale;
            float val = NoiseGenerator::fractal(nx, ny, 0.0f, octaves, persistence, lacunarity);
            val = val * 0.5f + 0.5f;
            val = std::clamp(val, 0.0f, 1.0f);
            setPixelRGBA(pixels, width, x, y, val, val, val);
        }
    }
}

void NoiseImageGenerator::worleyNoise(float* pixels, int width, int height, float scale) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float nx = static_cast<float>(x) / width * scale;
            float ny = static_cast<float>(y) / height * scale;
            float val = NoiseGenerator::worley(nx, ny, 0.0f);
            val = std::clamp(val, 0.0f, 1.0f);
            setPixelRGBA(pixels, width, x, y, val, val, val);
        }
    }
}

void NoiseImageGenerator::turbulence(float* pixels, int width, int height,
                                      float scale, int octaves) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float nx = static_cast<float>(x) / width * scale;
            float ny = static_cast<float>(y) / height * scale;
            float val = 0.0f;
            float amp = 1.0f;
            float freq = 1.0f;
            float totalAmp = 0.0f;
            for (int o = 0; o < octaves; ++o) {
                val += std::abs(NoiseGenerator::perlin(nx * freq, ny * freq)) * amp;
                totalAmp += amp;
                amp *= 0.5f;
                freq *= 2.0f;
            }
            val /= totalAmp;
            val = std::clamp(val, 0.0f, 1.0f);
            setPixelRGBA(pixels, width, x, y, val, val, val);
        }
    }
}

void NoiseImageGenerator::cloudTexture(float* pixels, int width, int height,
                                        float scale, float coverage) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float nx = static_cast<float>(x) / width * scale;
            float ny = static_cast<float>(y) / height * scale;
            float val = NoiseGenerator::fractal(nx, ny, 0.0f, 8, 0.5f, 2.0f);
            val = val * 0.5f + 0.5f;
            // Apply coverage threshold with smooth falloff
            val = (val - (1.0f - coverage)) / coverage;
            val = std::clamp(val, 0.0f, 1.0f);
            // Smooth step
            val = val * val * (3.0f - 2.0f * val);
            setPixelRGBA(pixels, width, x, y, val, val, val);
        }
    }
}

void NoiseImageGenerator::woodGrain(float* pixels, int width, int height,
                                     float scale, float ringFrequency) {
    float cx = width * 0.5f, cy = height * 0.5f;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float nx = static_cast<float>(x) / width * scale;
            float ny = static_cast<float>(y) / height * scale;
            float dx = (x - cx) / width * scale;
            float dy = (y - cy) / height * scale;
            float dist = std::sqrt(dx * dx + dy * dy);
            float noise = NoiseGenerator::perlin(nx * 0.5f, ny * 0.5f) * 2.0f;
            float rings = std::sin((dist + noise) * ringFrequency) * 0.5f + 0.5f;
            // Wood-like brown tones
            float r = 0.4f + rings * 0.35f;
            float g = 0.25f + rings * 0.2f;
            float b = 0.1f + rings * 0.1f;
            setPixelRGBA(pixels, width, x, y, r, g, b);
        }
    }
}

void NoiseImageGenerator::marble(float* pixels, int width, int height,
                                  float scale, float stripeFrequency) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float nx = static_cast<float>(x) / width * scale;
            float ny = static_cast<float>(y) / height * scale;
            float noise = NoiseGenerator::fractal(nx, ny, 0.0f, 6, 0.5f, 2.0f);
            float val = std::sin(nx * stripeFrequency + noise * 5.0f) * 0.5f + 0.5f;
            // Marble-like tones (white to grey-blue veins)
            float r = 0.9f - val * 0.3f;
            float g = 0.9f - val * 0.25f;
            float b = 0.95f - val * 0.15f;
            setPixelRGBA(pixels, width, x, y, r, g, b);
        }
    }
}

} // namespace ArtifactCore
