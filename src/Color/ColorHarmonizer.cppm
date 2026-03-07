module;

#include <cmath>
#include <QList>

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
module Color.Harmonizer;




import Color.Float;
import Color.Conversion;

namespace ArtifactCore {

static FloatColor fromHueShifted(const FloatColor& baseColor, qreal hueShiftDegrees) {
    auto hsv = ColorConversion::RGBToHSV(baseColor.r(), baseColor.g(), baseColor.b());
    hsv.h = std::fmod(hsv.h + hueShiftDegrees + 360.0f, 360.0f);
    auto rgb = ColorConversion::HSVToRGB(hsv);
    return FloatColor(rgb[0], rgb[1], rgb[2], baseColor.a());
}

FloatColor ColorHarmonizer::getComplementary(const FloatColor& color) {
    return fromHueShifted(color, 180.0f);
}

QList<FloatColor> ColorHarmonizer::getAnalogous(const FloatColor& color, float angle) {
    return {
        fromHueShifted(color, angle),
        fromHueShifted(color, -angle)
    };
}

QList<FloatColor> ColorHarmonizer::getTriadic(const FloatColor& color) {
    return {
        fromHueShifted(color, 120.0f),
        fromHueShifted(color, 240.0f)
    };
}

QList<FloatColor> ColorHarmonizer::getSplitComplementary(const FloatColor& color, float offsetAngle) {
    return {
        fromHueShifted(color, 180.0f - offsetAngle),
        fromHueShifted(color, 180.0f + offsetAngle)
    };
}

QList<FloatColor> ColorHarmonizer::getTetradic(const FloatColor& color) {
    return {
        fromHueShifted(color, 90.0f),
        fromHueShifted(color, 180.0f),
        fromHueShifted(color, 270.0f)
    };
}

QList<FloatColor> ColorHarmonizer::getMonochromatic(const FloatColor& color, int count) {
    QList<FloatColor> result;
    auto hsv = ColorConversion::RGBToHSV(color.r(), color.g(), color.b());
    
    // Varying the Value (Lightness)
    float step = 1.0f / (count + 1.0f);
    for (int i = 1; i <= count; ++i) {
        auto hsvTemp = hsv;
        hsvTemp.v = std::fmod(hsv.v + step * i, 1.0f);
        auto rgb = ColorConversion::HSVToRGB(hsvTemp);
        result.append(FloatColor(rgb[0], rgb[1], rgb[2], color.a()));
    }
    return result;
}

} // namespace ArtifactCore
