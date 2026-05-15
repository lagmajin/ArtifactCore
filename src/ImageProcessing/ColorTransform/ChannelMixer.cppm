module;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QImage>

module ImageProcessing.ColorTransform.ChannelMixer;

import Color.GamutConversion;

namespace ArtifactCore {

ChannelMixerSettings ChannelMixerSettings::identityMix() {
    ChannelMixerSettings settings;
    settings.matrix = identity();
    return settings;
}

ChannelMixerSettings ChannelMixerSettings::warm() {
    ChannelMixerSettings settings;
    settings.matrix = {{{1.08f, 0.02f, -0.04f},
                        {0.03f, 0.98f, -0.02f},
                        {0.00f, -0.05f, 0.95f}}};
    settings.strength = 0.9f;
    return settings;
}

ChannelMixerSettings ChannelMixerSettings::cool() {
    ChannelMixerSettings settings;
    settings.matrix = {{{0.96f, 0.00f, 0.08f},
                        {-0.02f, 1.00f, 0.06f},
                        {0.06f, 0.02f, 1.04f}}};
    settings.strength = 0.9f;
    return settings;
}

ChannelMixerSettings ChannelMixerSettings::crossProcess() {
    ChannelMixerSettings settings;
    settings.matrix = {{{1.02f, 0.08f, -0.04f},
                        {0.02f, 0.98f, 0.06f},
                        {-0.04f, 0.10f, 0.98f}}};
    settings.strength = 1.0f;
    return settings;
}

ChannelMixerSettings ChannelMixerSettings::monochromeMix() {
    ChannelMixerSettings settings;
    settings.matrix = {{{0.299f, 0.587f, 0.114f},
                        {0.299f, 0.587f, 0.114f},
                        {0.299f, 0.587f, 0.114f}}};
    settings.monochrome = true;
    settings.strength = 1.0f;
    return settings;
}

ChannelMixerProcessor::ChannelMixerProcessor() = default;
ChannelMixerProcessor::~ChannelMixerProcessor() = default;

void ChannelMixerProcessor::setSettings(const ChannelMixerSettings& settings) {
    settings_ = settings;
}

const ChannelMixerSettings& ChannelMixerProcessor::settings() const {
    return settings_;
}

QImage ChannelMixerProcessor::apply(const QImage& source) const {
    if (source.isNull()) {
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

void ChannelMixerProcessor::applyPixel(float& r, float& g, float& b) const {
    const float sourceR = r;
    const float sourceG = g;
    const float sourceB = b;
    const float sourceLuma = luma(sourceR, sourceG, sourceB);

    float mixedR = settings_.matrix[0][0] * sourceR + settings_.matrix[0][1] * sourceG + settings_.matrix[0][2] * sourceB;
    float mixedG = settings_.matrix[1][0] * sourceR + settings_.matrix[1][1] * sourceG + settings_.matrix[1][2] * sourceB;
    float mixedB = settings_.matrix[2][0] * sourceR + settings_.matrix[2][1] * sourceG + settings_.matrix[2][2] * sourceB;

    if (settings_.monochrome) {
        const float mono = luma(mixedR, mixedG, mixedB);
        mixedR = mono;
        mixedG = mono;
        mixedB = mono;
    }

    preserveLuma(sourceLuma, mixedR, mixedG, mixedB, settings_.preserveLuma, settings_.strength);

    const float strength = std::clamp(settings_.strength, 0.0f, 1.0f);
    r = std::clamp(sourceR * (1.0f - strength) + mixedR * strength, 0.0f, 1.0f);
    g = std::clamp(sourceG * (1.0f - strength) + mixedG * strength, 0.0f, 1.0f);
    b = std::clamp(sourceB * (1.0f - strength) + mixedB * strength, 0.0f, 1.0f);
}

float ChannelMixerProcessor::luma(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

void ChannelMixerProcessor::preserveLuma(float sourceLuma, float& r, float& g, float& b, bool enabled, float strength) {
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
