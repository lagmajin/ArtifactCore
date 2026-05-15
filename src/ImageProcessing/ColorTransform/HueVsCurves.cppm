module;
#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>
#include <QImage>
#include <QColor>

module ImageProcessing.ColorTransform.HueVsCurves;

namespace ArtifactCore {

// ============================================================================
// HueVsCurveProcessor
// ============================================================================

HueVsCurveProcessor::HueVsCurveProcessor() = default;
HueVsCurveProcessor::~HueVsCurveProcessor() = default;

void HueVsCurveProcessor::setSettings(const HueVsCurveSettings& settings) {
    settings_ = settings;
}

auto HueVsCurveProcessor::settings() const -> const HueVsCurveSettings& {
    return settings_;
}

auto HueVsCurveProcessor::apply(const QImage& source) const -> QImage {
    if (source.isNull() || settings_.controlPoints.empty()) {
        return source;
    }

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int width = result.width();
    const int height = result.height();

    for (int y = 0; y < height; ++y) {
        auto* scanLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < width; ++x) {
            float r = qRed(scanLine[x]) / 255.0f;
            float g = qGreen(scanLine[x]) / 255.0f;
            float b = qBlue(scanLine[x]) / 255.0f;
            float a = qAlpha(scanLine[x]) / 255.0f;

            applyPixel(r, g, b);

            scanLine[x] = qRgba(
                static_cast<int>(std::clamp(r * 255.0f, 0.0f, 255.0f)),
                static_cast<int>(std::clamp(g * 255.0f, 0.0f, 255.0f)),
                static_cast<int>(std::clamp(b * 255.0f, 0.0f, 255.0f)),
                static_cast<int>(std::clamp(a * 255.0f, 0.0f, 255.0f))
            );
        }
    }

    return result;
}

void HueVsCurveProcessor::applyPixel(float& r, float& g, float& b) const {
    if (settings_.controlPoints.empty() || settings_.masterStrength <= 0.0f) {
        return;
    }

    // RGB → HSL変換
    float h, s, l;
    rgbToHsl(r, g, b, h, s, l);

    // カーブ値を取得
    float curveValue = getCurveValue(h);
    curveValue *= settings_.masterStrength;

    // カーブの種類に応じて適用
    switch (settings_.curveType) {
        case HueCurveType::HueVsHue:
            // 色相回転
            h = std::fmod(h + curveValue + 360.0f, 360.0f);
            break;

        case HueCurveType::HueVsSat:
            // 彩度調整
            s = clampValue(s + (curveValue / 100.0f), 0.0f, 1.0f);
            break;

        case HueCurveType::HueVsLuma:
            // 明度調整
            l = clampValue(l + (curveValue / 100.0f), 0.0f, 1.0f);
            break;
    }

    // HSL → RGB変換
    hslToRgb(h, s, l, r, g, b);
}

auto HueVsCurveProcessor::getCurveValue(float hue) const -> float {
    if (settings_.controlPoints.empty()) {
        return 0.0f;
    }

    // 最も重みの大きい制御点を探す
    float totalWeight = 0.0f;
    float weightedValue = 0.0f;

    for (const auto& point : settings_.controlPoints) {
        if (!point.enabled) continue;

        float weight = getHueWeight(hue, point);
        totalWeight += weight;
        weightedValue += point.value * weight;
    }

    if (totalWeight <= 0.0f) {
        return 0.0f;
    }

    return weightedValue / totalWeight;
}

auto HueVsCurveProcessor::getHueWeight(float pixelHue, const HueControlPoint& point) const -> float {
    // 色相差を計算（0-360の循環を考慮）
    float hueDiff = std::abs(pixelHue - point.hue);
    if (hueDiff > 180.0f) {
        hueDiff = 360.0f - hueDiff;
    }

    // ガウス関数で重みを計算
    if (hueDiff > point.range * 2.0f) {
        return 0.0f;
    }

    float sigma = point.range / 2.0f;
    return std::exp(-(hueDiff * hueDiff) / (2.0f * sigma * sigma));
}

void HueVsCurveProcessor::rgbToHsl(float r, float g, float b, float& h, float& s, float& l) {
    float maxVal = std::max({r, g, b});
    float minVal = std::min({r, g, b});
    float delta = maxVal - minVal;

    l = (maxVal + minVal) / 2.0f;

    if (delta < 1e-6f) {
        h = 0.0f;
        s = 0.0f;
        return;
    }

    s = (l > 0.5f) ? (delta / (2.0f - maxVal - minVal)) : (delta / (maxVal + minVal));

    if (maxVal == r) {
        h = (g - b) / delta + (g < b ? 6.0f : 0.0f);
    } else if (maxVal == g) {
        h = (b - r) / delta + 2.0f;
    } else {
        h = (r - g) / delta + 4.0f;
    }

    h *= 60.0f;
    if (h < 0.0f) h += 360.0f;
}

void HueVsCurveProcessor::hslToRgb(float h, float s, float l, float& r, float& g, float& b) {
    if (s < 1e-6f) {
        r = g = b = l;
        return;
    }

    auto hueToRgb = [](float p, float q, float t) -> float {
        if (t < 0.0f) t += 1.0f;
        if (t > 1.0f) t -= 1.0f;
        if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
        if (t < 1.0f / 2.0f) return q;
        if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
        return p;
    };

    float q = (l < 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
    float p = 2.0f * l - q;

    float hNorm = h / 360.0f;
    r = hueToRgb(p, q, hNorm + 1.0f / 3.0f);
    g = hueToRgb(p, q, hNorm);
    b = hueToRgb(p, q, hNorm - 1.0f / 3.0f);
}

auto HueVsCurveProcessor::clampValue(float value, float minVal, float maxVal) -> float {
    return std::max(minVal, std::min(maxVal, value));
}

} // namespace ArtifactCore
