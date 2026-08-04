module;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QImage>

module ImageProcessing.ColorTransform.Tritone;

namespace ArtifactCore {

TritoneSettings TritoneSettings::cinematic() {
    TritoneSettings settings;
    settings.shadowColor = {0.09f, 0.18f, 0.28f};
    settings.midtoneColor = {0.54f, 0.48f, 0.40f};
    settings.highlightColor = {0.96f, 0.84f, 0.60f};
    settings.balance = 0.48f;
    settings.softness = 0.62f;
    settings.colorMix = 0.9f;
    return settings;
}

TritoneSettings TritoneSettings::tealAndOrange() {
    TritoneSettings settings;
    settings.shadowColor = {0.10f, 0.35f, 0.42f};
    settings.midtoneColor = {0.53f, 0.52f, 0.50f};
    settings.highlightColor = {0.96f, 0.56f, 0.24f};
    settings.balance = 0.46f;
    settings.softness = 0.58f;
    settings.colorMix = 0.88f;
    return settings;
}

TritoneProcessor::TritoneProcessor() = default;
TritoneProcessor::~TritoneProcessor() = default;

void TritoneProcessor::setSettings(const TritoneSettings& settings) {
    settings_ = settings;
}

const TritoneSettings& TritoneProcessor::settings() const {
    return settings_;
}

QImage TritoneProcessor::apply(const QImage& source) const {
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

void TritoneProcessor::applyPixel(float& r, float& g, float& b) const {
    const float sourceLuma = luma(r, g, b);
    const float shadowWeight = 1.0f - smoothstep(settings_.balance - settings_.softness, settings_.balance, sourceLuma);
    const float highlightWeight = smoothstep(settings_.balance, settings_.balance + settings_.softness, sourceLuma);
    const float midWeight = std::clamp(1.0f - shadowWeight - highlightWeight, 0.0f, 1.0f);

    float targetR =
        settings_.shadowColor.r * shadowWeight * settings_.shadowStrength +
        settings_.midtoneColor.r * midWeight * settings_.midtoneStrength +
        settings_.highlightColor.r * highlightWeight * settings_.highlightStrength;
    float targetG =
        settings_.shadowColor.g * shadowWeight * settings_.shadowStrength +
        settings_.midtoneColor.g * midWeight * settings_.midtoneStrength +
        settings_.highlightColor.g * highlightWeight * settings_.highlightStrength;
    float targetB =
        settings_.shadowColor.b * shadowWeight * settings_.shadowStrength +
        settings_.midtoneColor.b * midWeight * settings_.midtoneStrength +
        settings_.highlightColor.b * highlightWeight * settings_.highlightStrength;

    targetR = std::clamp(targetR, 0.0f, 1.0f);
    targetG = std::clamp(targetG, 0.0f, 1.0f);
    targetB = std::clamp(targetB, 0.0f, 1.0f);

    const float mixAmount = std::clamp(settings_.masterStrength * settings_.colorMix, 0.0f, 1.0f);
    r = r * (1.0f - mixAmount) + targetR * mixAmount;
    g = g * (1.0f - mixAmount) + targetG * mixAmount;
    b = b * (1.0f - mixAmount) + targetB * mixAmount;

    if (settings_.preserveLuma) {
        preserveLuma(sourceLuma, r, g, b, std::clamp(settings_.masterStrength, 0.0f, 1.0f));
    }

    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
}

float TritoneProcessor::luma(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

float TritoneProcessor::smoothstep(float edge0, float edge1, float x) {
    if (edge0 == edge1) {
        return x < edge0 ? 0.0f : 1.0f;
    }
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void TritoneProcessor::preserveLuma(float sourceLuma, float& r, float& g, float& b, float preserveAmount) {
    const float targetLuma = luma(r, g, b);
    if (targetLuma <= 1e-6f) {
        return;
    }

    const float scale = sourceLuma / targetLuma;
    const float preservedR = std::clamp(r * scale, 0.0f, 1.0f);
    const float preservedG = std::clamp(g * scale, 0.0f, 1.0f);
    const float preservedB = std::clamp(b * scale, 0.0f, 1.0f);

    r = r * (1.0f - preserveAmount) + preservedR * preserveAmount;
    g = g * (1.0f - preserveAmount) + preservedG * preserveAmount;
    b = b * (1.0f - preserveAmount) + preservedB * preserveAmount;
}

} // namespace ArtifactCore
