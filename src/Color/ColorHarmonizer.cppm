module;

#include <cmath>
#include <algorithm>
#include <vector>

module Color.Harmonizer;

import std;
import Color.Conversion;

namespace ArtifactCore {

std::array<float, 3> ColorHarmonizer::getComplementary(float r, float g, float b) {
    auto hsv = ColorConversion::RGBToHSV(r, g, b);
    hsv.h = std::fmod(hsv.h + 180.0f, 360.0f);
    return ColorConversion::HSVToRGB(hsv);
}

std::vector<std::array<float, 3>> ColorHarmonizer::getAnalogous(float r, float g, float b, float angle) {
    auto hsv = ColorConversion::RGBToHSV(r, g, b);
    auto h1 = hsv; h1.h = std::fmod(h1.h + angle + 360.0f, 360.0f);
    auto h2 = hsv; h2.h = std::fmod(h2.h - angle + 360.0f, 360.0f);
    return { ColorConversion::HSVToRGB(h1), ColorConversion::HSVToRGB(h2) };
}

std::vector<std::array<float, 3>> ColorHarmonizer::getTriadic(float r, float g, float b) {
    auto hsv = ColorConversion::RGBToHSV(r, g, b);
    auto h1 = hsv; h1.h = std::fmod(h1.h + 120.0f, 360.0f);
    auto h2 = hsv; h2.h = std::fmod(h2.h + 240.0f, 360.0f);
    return { ColorConversion::HSVToRGB(h1), ColorConversion::HSVToRGB(h2) };
}

std::vector<std::array<float, 3>> ColorHarmonizer::getSplitComplementary(float r, float g, float b, float offsetAngle) {
    auto hsv = ColorConversion::RGBToHSV(r, g, b);
    auto h1 = hsv; h1.h = std::fmod(h1.h + 180.0f - offsetAngle + 360.0f, 360.0f);
    auto h2 = hsv; h2.h = std::fmod(h2.h + 180.0f + offsetAngle + 360.0f, 360.0f);
    return { ColorConversion::HSVToRGB(h1), ColorConversion::HSVToRGB(h2) };
}

std::vector<std::array<float, 3>> ColorHarmonizer::getTetradic(float r, float g, float b) {
    auto hsv = ColorConversion::RGBToHSV(r, g, b);
    auto h1 = hsv; h1.h = std::fmod(h1.h + 90.0f, 360.0f);
    auto h2 = hsv; h2.h = std::fmod(h2.h + 180.0f, 360.0f);
    auto h3 = hsv; h3.h = std::fmod(h3.h + 270.0f, 360.0f);
    return { ColorConversion::HSVToRGB(h1), ColorConversion::HSVToRGB(h2), ColorConversion::HSVToRGB(h3) };
}

} // namespace ArtifactCore
