module;
#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>
#include <QImage>
#include <QColor>

module ImageProcessing.ColorTransform.TonalSplit;

namespace ArtifactCore {

// ============================================================================
// TonalSplitProcessor
// ============================================================================

TonalSplitProcessor::TonalSplitProcessor() = default;
TonalSplitProcessor::~TonalSplitProcessor() = default;

void TonalSplitProcessor::setSettings(const TonalSplitSettings& settings) {
    settings_ = settings;
}

auto TonalSplitProcessor::settings() const -> const TonalSplitSettings& {
    return settings_;
}

auto TonalSplitProcessor::apply(const QImage& source) const -> QImage {
    if (source.isNull() || settings_.masterStrength <= 0.0f) {
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

void TonalSplitProcessor::applyPixel(float& r, float& g, float& b) const {
    // ルミナンス計算
    float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;

    // マスク取得
    float shadowMask = getShadowMask(luma);
    float highlightMask = getHighlightMask(luma);

    // HSL変換
    float h, s, l;
    rgbToHsl(r, g, b, h, s, l);

    // シャドウ側のカラータスク
    if (shadowMask > 0.0f && settings_.shadowStrength > 0.0f) {
        float strength = shadowMask * settings_.shadowStrength * settings_.masterStrength;

        // 色相をシャドウカラーに近づける
        float hueDiff = std::fmod(settings_.shadowHue - h + 540.0f, 360.0f) - 180.0f;
        h = std::fmod(h + hueDiff * strength * 0.3f + 360.0f, 360.0f);

        // 彩度調整
        s = clampValue(s + (settings_.shadowSaturation / 100.0f) * strength, 0.0f, 1.0f);
    }

    // ハイライト側のカラータスク
    if (highlightMask > 0.0f && settings_.highlightStrength > 0.0f) {
        float strength = highlightMask * settings_.highlightStrength * settings_.masterStrength;

        // 色相をハイライトカラーに近づける
        float hueDiff = std::fmod(settings_.highlightHue - h + 540.0f, 360.0f) - 180.0f;
        h = std::fmod(h + hueDiff * strength * 0.3f + 360.0f, 360.0f);

        // 彩度調整
        s = clampValue(s + (settings_.highlightSaturation / 100.0f) * strength, 0.0f, 1.0f);
    }

    // RGB変換
    hslToRgb(h, s, l, r, g, b);
}

auto TonalSplitProcessor::getShadowMask(float luma) const -> float {
    if (luma >= settings_.splitPoint) {
        return 0.0f;
    }

    float softness = settings_.splitSoftness;
    float transitionWidth = settings_.splitPoint * softness;
    float start = settings_.splitPoint - transitionWidth;

    if (luma <= start) {
        return 1.0f;
    }

    // Smoothstepで滑らかに
    float t = (luma - start) / transitionWidth;
    t = t * t * (3.0f - 2.0f * t);
    return 1.0f - t;
}

auto TonalSplitProcessor::getHighlightMask(float luma) const -> float {
    if (luma <= settings_.splitPoint) {
        return 0.0f;
    }

    float softness = settings_.splitSoftness;
    float transitionWidth = (1.0f - settings_.splitPoint) * softness;
    float end = settings_.splitPoint + transitionWidth;

    if (luma >= end) {
        return 1.0f;
    }

    // Smoothstepで滑らかに
    float t = (luma - settings_.splitPoint) / transitionWidth;
    return t * t * (3.0f - 2.0f * t);
}

void TonalSplitProcessor::rgbToHsl(float r, float g, float b, float& h, float& s, float& l) {
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

void TonalSplitProcessor::hslToRgb(float h, float s, float l, float& r, float& g, float& b) {
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

auto TonalSplitProcessor::clampValue(float value, float minVal, float maxVal) -> float {
    return std::max(minVal, std::min(maxVal, value));
}

} // namespace ArtifactCore
