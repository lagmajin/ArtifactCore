module;
#include <cmath>
#include <algorithm>
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
module GenerateTestImage;

namespace ArtifactCore {

static void setPixel(float* pixels, int width, int x, int y,
                     float r, float g, float b, float a = 1.0f) {
    int idx = (y * width + x) * 4;
    pixels[idx + 0] = r;
    pixels[idx + 1] = g;
    pixels[idx + 2] = b;
    pixels[idx + 3] = a;
}

// HSV -> RGB (H: 0-360, S: 0-1, V: 0-1)
static void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    if (s < 0.001f) { r = g = b = v; return; }
    float hh = std::fmod(h, 360.0f) / 60.0f;
    int i = static_cast<int>(hh);
    float f = hh - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    switch (i) {
        case 0:  r = v; g = t; b = p; break;
        case 1:  r = q; g = v; b = p; break;
        case 2:  r = p; g = v; b = t; break;
        case 3:  r = p; g = q; b = v; break;
        case 4:  r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}

void TestImageGenerator::colorBars(float* pixels, int width, int height) {
    // SMPTE HD 75% color bars: White, Yellow, Cyan, Green, Magenta, Red, Blue
    const float bars[][3] = {
        {0.75f, 0.75f, 0.75f},  // White 75%
        {0.75f, 0.75f, 0.0f},   // Yellow
        {0.0f,  0.75f, 0.75f},  // Cyan
        {0.0f,  0.75f, 0.0f},   // Green
        {0.75f, 0.0f,  0.75f},  // Magenta
        {0.75f, 0.0f,  0.0f},   // Red
        {0.0f,  0.0f,  0.75f},  // Blue
    };
    int barCount = 7;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int barIdx = x * barCount / width;
            barIdx = std::clamp(barIdx, 0, barCount - 1);
            setPixel(pixels, width, x, y, bars[barIdx][0], bars[barIdx][1], bars[barIdx][2]);
        }
    }
}

void TestImageGenerator::colorBars100(float* pixels, int width, int height) {
    const float bars[][3] = {
        {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f},
        {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
    };
    int barCount = 7;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int barIdx = std::clamp(x * barCount / width, 0, barCount - 1);
            setPixel(pixels, width, x, y, bars[barIdx][0], bars[barIdx][1], bars[barIdx][2]);
        }
    }
}

void TestImageGenerator::horizontalGradient(float* pixels, int width, int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float v = static_cast<float>(x) / std::max(1, width - 1);
            setPixel(pixels, width, x, y, v, v, v);
        }
    }
}

void TestImageGenerator::verticalGradient(float* pixels, int width, int height) {
    for (int y = 0; y < height; ++y) {
        float v = static_cast<float>(y) / std::max(1, height - 1);
        for (int x = 0; x < width; ++x) {
            setPixel(pixels, width, x, y, v, v, v);
        }
    }
}

void TestImageGenerator::rgbGradient(float* pixels, int width, int height) {
    int third = width / 3;
    for (int y = 0; y < height; ++y) {
        float bright = 1.0f - static_cast<float>(y) / std::max(1, height - 1);
        for (int x = 0; x < width; ++x) {
            if (x < third)
                setPixel(pixels, width, x, y, bright, 0.0f, 0.0f);
            else if (x < third * 2)
                setPixel(pixels, width, x, y, 0.0f, bright, 0.0f);
            else
                setPixel(pixels, width, x, y, 0.0f, 0.0f, bright);
        }
    }
}

void TestImageGenerator::checkerboard(float* pixels, int width, int height, int cellSize) {
    if (cellSize < 1) cellSize = 1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool isWhite = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            float v = isWhite ? 0.8f : 0.2f;
            setPixel(pixels, width, x, y, v, v, v);
        }
    }
}

void TestImageGenerator::zoneSystem(float* pixels, int width, int height) {
    const int zones = 11; // Zone 0 (black) to Zone X (white)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int zone = x * zones / width;
            zone = std::clamp(zone, 0, zones - 1);
            float v = static_cast<float>(zone) / static_cast<float>(zones - 1);
            setPixel(pixels, width, x, y, v, v, v);
        }
    }
}

void TestImageGenerator::solidColor(float* pixels, int width, int height,
                                     float r, float g, float b, float a) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            setPixel(pixels, width, x, y, r, g, b, a);
        }
    }
}

void TestImageGenerator::radialGradient(float* pixels, int width, int height) {
    float cx = width * 0.5f;
    float cy = height * 0.5f;
    float maxR = std::sqrt(cx * cx + cy * cy);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy);
            float v = 1.0f - std::clamp(dist / maxR, 0.0f, 1.0f);
            setPixel(pixels, width, x, y, v, v, v);
        }
    }
}

void TestImageGenerator::colorWheel(float* pixels, int width, int height) {
    float cx = width * 0.5f;
    float cy = height * 0.5f;
    float maxR = std::min(cx, cy);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = std::sqrt(dx * dx + dy * dy);
            float norm = dist / maxR;
            if (norm > 1.0f) {
                setPixel(pixels, width, x, y, 0.1f, 0.1f, 0.1f);
            } else {
                float hue = std::fmod(std::atan2(dy, dx) * 180.0f / 3.14159265f + 360.0f, 360.0f);
                float r, g, b;
                hsvToRgb(hue, norm, 1.0f, r, g, b);
                setPixel(pixels, width, x, y, r, g, b);
            }
        }
    }
}

} // namespace ArtifactCore
