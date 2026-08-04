module;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QImage>

module ImageProcessing.ColorTransform.SelectiveColor;

namespace ArtifactCore {

namespace {
constexpr std::array<float, 9> kHueCenters = {
    0.0f,        // Reds
    1.0f / 6.0f,  // Yellows
    1.0f / 3.0f,  // Greens
    0.5f,         // Cyans
    2.0f / 3.0f,  // Blues
    5.0f / 6.0f,  // Magentas
    0.0f,
    0.0f,
    0.0f,
};
}

SelectiveColorSettings SelectiveColorSettings::neutral() {
    return SelectiveColorSettings{};
}

SelectiveColorSettings SelectiveColorSettings::warm() {
    SelectiveColorSettings settings;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Yellows)].yellow = 0.10f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Yellows)].black = -0.03f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Reds)].yellow = 0.08f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Whites)].yellow = 0.04f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Neutrals)].yellow = 0.03f;
    return settings;
}

SelectiveColorSettings SelectiveColorSettings::cool() {
    SelectiveColorSettings settings;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Cyans)].cyan = 0.08f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Blues)].cyan = 0.10f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Blues)].magenta = -0.03f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Neutrals)].cyan = 0.03f;
    return settings;
}

SelectiveColorSettings SelectiveColorSettings::vivid() {
    SelectiveColorSettings settings;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Reds)].magenta = 0.04f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Reds)].yellow = 0.06f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Greens)].cyan = 0.04f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Blues)].cyan = 0.06f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Magentas)].magenta = 0.08f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Whites)].black = -0.04f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Neutrals)].black = -0.02f;
    return settings;
}

SelectiveColorSettings SelectiveColorSettings::film() {
    SelectiveColorSettings settings;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Reds)].yellow = 0.08f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Yellows)].cyan = -0.03f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Greens)].magenta = 0.02f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Blues)].yellow = -0.04f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Magentas)].cyan = 0.04f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Whites)].black = -0.03f;
    settings.groups[static_cast<size_t>(SelectiveColorGroup::Blacks)].black = 0.04f;
    return settings;
}

SelectiveColorProcessor::SelectiveColorProcessor() = default;
SelectiveColorProcessor::~SelectiveColorProcessor() = default;

void SelectiveColorProcessor::setSettings(const SelectiveColorSettings& settings) {
    settings_ = settings;
}

const SelectiveColorSettings& SelectiveColorProcessor::settings() const {
    return settings_;
}

QImage SelectiveColorProcessor::apply(const QImage& source) const {
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

void SelectiveColorProcessor::applyPixel(float& r, float& g, float& b) const {
    const float sourceR = r;
    const float sourceG = g;
    const float sourceB = b;
    const float sourceLuma = luma(sourceR, sourceG, sourceB);

    const RangeWeights w = computeWeights(sourceR, sourceG, sourceB);
    const auto& groups = settings_.groups;

    auto accumulate = [&](SelectiveColorGroup group, float& outC, float& outM, float& outY, float& outK, float weight) {
        const auto& adj = groups[static_cast<size_t>(group)];
        outC += adj.cyan * weight;
        outM += adj.magenta * weight;
        outY += adj.yellow * weight;
        outK += adj.black * weight;
    };

    float c = 0.0f, m = 0.0f, yv = 0.0f, k = 0.0f;
    accumulate(SelectiveColorGroup::Reds, c, m, yv, k, w.reds);
    accumulate(SelectiveColorGroup::Yellows, c, m, yv, k, w.yellows);
    accumulate(SelectiveColorGroup::Greens, c, m, yv, k, w.greens);
    accumulate(SelectiveColorGroup::Cyans, c, m, yv, k, w.cyans);
    accumulate(SelectiveColorGroup::Blues, c, m, yv, k, w.blues);
    accumulate(SelectiveColorGroup::Magentas, c, m, yv, k, w.magentas);
    accumulate(SelectiveColorGroup::Whites, c, m, yv, k, w.whites);
    accumulate(SelectiveColorGroup::Neutrals, c, m, yv, k, w.neutrals);
    accumulate(SelectiveColorGroup::Blacks, c, m, yv, k, w.blacks);

    const float strength = std::clamp(settings_.strength, 0.0f, 1.0f);
    if (settings_.relativeMode) {
        c *= (1.0f - sourceR);
        m *= (1.0f - sourceG);
        yv *= (1.0f - sourceB);
        k *= (1.0f - sourceLuma);
    }

    r = sourceR - (c + k) * strength;
    g = sourceG - (m + k) * strength;
    b = sourceB - (yv + k) * strength;

    preserveLuma(sourceLuma, r, g, b, settings_.preserveLuma, strength);

    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
}

float SelectiveColorProcessor::luma(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

float SelectiveColorProcessor::smoothstep(float edge0, float edge1, float x) {
    if (edge0 == edge1) {
        return x < edge0 ? 0.0f : 1.0f;
    }
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float SelectiveColorProcessor::hueDistance(float a, float b) {
    float d = std::fabs(a - b);
    return std::min(d, 1.0f - d);
}

SelectiveColorProcessor::RangeWeights SelectiveColorProcessor::computeWeights(float r, float g, float b) {
    RangeWeights weights;
    const float maxv = std::max({r, g, b});
    const float minv = std::min({r, g, b});
    const float chroma = maxv - minv;
    const float value = maxv;

    if (chroma < 1e-4f) {
        weights.whites = smoothstep(0.70f, 0.95f, value);
        weights.blacks = 1.0f - smoothstep(0.05f, 0.35f, value);
        weights.neutrals = std::clamp(1.0f - weights.whites - weights.blacks, 0.0f, 1.0f);
        return weights;
    }

    float hue = 0.0f;
    if (maxv == r) {
        hue = std::fmod((g - b) / chroma, 6.0f) / 6.0f;
    } else if (maxv == g) {
        hue = (((b - r) / chroma) + 2.0f) / 6.0f;
    } else {
        hue = (((r - g) / chroma) + 4.0f) / 6.0f;
    }
    if (hue < 0.0f) {
        hue += 1.0f;
    }

    const float saturation = std::clamp(chroma / std::max(value, 1e-4f), 0.0f, 1.0f);
    const float colorWeight = std::clamp(saturation, 0.0f, 1.0f);
    const float brightWeight = smoothstep(0.65f, 0.95f, value);
    const float darkWeight = 1.0f - smoothstep(0.08f, 0.32f, value);
    const float neutralWeight = std::clamp(1.0f - colorWeight, 0.0f, 1.0f);

    auto sector = [&](float center) {
        const float d = hueDistance(hue, center);
        return std::clamp(1.0f - d / (1.0f / 12.0f), 0.0f, 1.0f);
    };

    weights.reds = sector(kHueCenters[0]) * colorWeight;
    weights.yellows = sector(kHueCenters[1]) * colorWeight;
    weights.greens = sector(kHueCenters[2]) * colorWeight;
    weights.cyans = sector(kHueCenters[3]) * colorWeight;
    weights.blues = sector(kHueCenters[4]) * colorWeight;
    weights.magentas = sector(kHueCenters[5]) * colorWeight;
    weights.whites = brightWeight * neutralWeight;
    weights.blacks = darkWeight * neutralWeight;
    weights.neutrals = std::clamp(1.0f - weights.whites - weights.blacks, 0.0f, 1.0f) * neutralWeight;

    const float sum = weights.reds + weights.yellows + weights.greens + weights.cyans + weights.blues + weights.magentas + weights.whites + weights.neutrals + weights.blacks;
    if (sum > 1e-4f) {
        const float inv = 1.0f / sum;
        weights.reds *= inv;
        weights.yellows *= inv;
        weights.greens *= inv;
        weights.cyans *= inv;
        weights.blues *= inv;
        weights.magentas *= inv;
        weights.whites *= inv;
        weights.neutrals *= inv;
        weights.blacks *= inv;
    }

    return weights;
}

void SelectiveColorProcessor::preserveLuma(float sourceLuma, float& r, float& g, float& b, bool enabled, float strength) {
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
