module;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QImage>

module ImageProcessing.ColorTransform.ColorBalance;

namespace ArtifactCore {

ColorBalanceSettings ColorBalanceSettings::neutral() {
    return ColorBalanceSettings{};
}

ColorBalanceSettings ColorBalanceSettings::coolShadows() {
    ColorBalanceSettings settings;
    settings.shadowB = 0.10f;
    settings.shadowG = 0.04f;
    settings.highlightR = 0.02f;
    settings.highlightG = 0.01f;
    settings.highlightB = -0.02f;
    return settings;
}

ColorBalanceSettings ColorBalanceSettings::warmHighlights() {
    ColorBalanceSettings settings;
    settings.highlightR = 0.10f;
    settings.highlightG = 0.04f;
    settings.shadowB = -0.02f;
    return settings;
}

ColorBalanceSettings ColorBalanceSettings::cinematic() {
    ColorBalanceSettings settings;
    settings.shadowR = -0.01f;
    settings.shadowG = 0.03f;
    settings.shadowB = 0.10f;
    settings.midtoneR = 0.02f;
    settings.midtoneG = 0.01f;
    settings.midtoneB = -0.01f;
    settings.highlightR = 0.10f;
    settings.highlightG = 0.04f;
    settings.highlightB = -0.01f;
    settings.shadowRange = 0.30f;
    settings.highlightRange = 0.70f;
    settings.preserveLuma = true;
    return settings;
}

ColorBalanceProcessor::ColorBalanceProcessor() = default;
ColorBalanceProcessor::~ColorBalanceProcessor() = default;

void ColorBalanceProcessor::setSettings(const ColorBalanceSettings& settings) {
    settings_ = settings;
}

const ColorBalanceSettings& ColorBalanceProcessor::settings() const {
    return settings_;
}

QImage ColorBalanceProcessor::apply(const QImage& source) const {
    if (source.isNull() || settings_.masterStrength <= 0.0f) {
        return source;
    }

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < result.height(); ++y) {
        auto* scanLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
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

void ColorBalanceProcessor::applyPixel(float& r, float& g, float& b) const {
    const float sourceLuma = luma(r, g, b);
    const float shadowW = 1.0f - smoothstep(settings_.shadowRange - 0.1f, settings_.shadowRange + 0.1f, sourceLuma);
    const float highlightW = smoothstep(settings_.highlightRange - 0.1f, settings_.highlightRange + 0.1f, sourceLuma);
    const float midtoneW = std::clamp(1.0f - shadowW - highlightW, 0.0f, 1.0f);

    const float strength = std::clamp(settings_.masterStrength, 0.0f, 1.0f);
    r += (settings_.shadowR * shadowW + settings_.midtoneR * midtoneW + settings_.highlightR * highlightW) * strength;
    g += (settings_.shadowG * shadowW + settings_.midtoneG * midtoneW + settings_.highlightG * highlightW) * strength;
    b += (settings_.shadowB * shadowW + settings_.midtoneB * midtoneW + settings_.highlightB * highlightW) * strength;

    preserveLuma(sourceLuma, r, g, b, settings_.preserveLuma, strength);

    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
}

float ColorBalanceProcessor::luma(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

float ColorBalanceProcessor::smoothstep(float edge0, float edge1, float x) {
    if (edge0 == edge1) {
        return x < edge0 ? 0.0f : 1.0f;
    }
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void ColorBalanceProcessor::preserveLuma(float sourceLuma, float& r, float& g, float& b, bool enabled, float strength) {
    if (!enabled || strength <= 0.0f) {
        return;
    }
    const float targetLuma = luma(r, g, b);
    if (targetLuma <= 1e-6f) {
        return;
    }
    const float scale = sourceLuma / targetLuma;
    const float clampedStrength = std::clamp(strength, 0.0f, 1.0f);
    const float preservedR = std::clamp(r * scale, 0.0f, 1.0f);
    const float preservedG = std::clamp(g * scale, 0.0f, 1.0f);
    const float preservedB = std::clamp(b * scale, 0.0f, 1.0f);
    r = r * (1.0f - clampedStrength) + preservedR * clampedStrength;
    g = g * (1.0f - clampedStrength) + preservedG * clampedStrength;
    b = b * (1.0f - clampedStrength) + preservedB * clampedStrength;
}

} // namespace ArtifactCore
