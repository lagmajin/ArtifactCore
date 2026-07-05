module;
#include <algorithm>
#include <array>
#include <cmath>
#include <QColor>
#include <QImage>

module ImageProcessing.ColorTransform.PhotoFilter;

namespace ArtifactCore {

PhotoFilterSettings PhotoFilterSettings::warm() {
    PhotoFilterSettings settings;
    settings.color = {1.0f, 0.90f, 0.72f};
    settings.density = 0.34f;
    settings.saturationBoost = 1.06f;
    return settings;
}

PhotoFilterSettings PhotoFilterSettings::cool() {
    PhotoFilterSettings settings;
    settings.color = {0.74f, 0.88f, 1.0f};
    settings.density = 0.30f;
    settings.saturationBoost = 1.02f;
    return settings;
}

PhotoFilterSettings PhotoFilterSettings::sepia() {
    PhotoFilterSettings settings;
    settings.color = {1.0f, 0.87f, 0.63f};
    settings.density = 0.42f;
    settings.saturationBoost = 0.94f;
    settings.contrast = 1.02f;
    return settings;
}

PhotoFilterSettings PhotoFilterSettings::cyan() {
    PhotoFilterSettings settings;
    settings.color = {0.70f, 0.95f, 1.0f};
    settings.density = 0.32f;
    settings.saturationBoost = 1.08f;
    return settings;
}

PhotoFilterSettings PhotoFilterSettings::rose() {
    PhotoFilterSettings settings;
    settings.color = {1.0f, 0.78f, 0.88f};
    settings.density = 0.28f;
    settings.saturationBoost = 1.04f;
    return settings;
}

PhotoFilterProcessor::PhotoFilterProcessor() = default;
PhotoFilterProcessor::~PhotoFilterProcessor() = default;

void PhotoFilterProcessor::setSettings(const PhotoFilterSettings& settings) {
    settings_ = settings;
}

const PhotoFilterSettings& PhotoFilterProcessor::settings() const {
    return settings_;
}

QImage PhotoFilterProcessor::apply(const QImage& source) const {
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

void PhotoFilterProcessor::applyPixel(float& r, float& g, float& b) const {
    const float sourceLuma = luma(r, g, b);
    float filteredR = r * settings_.color.r;
    float filteredG = g * settings_.color.g;
    float filteredB = b * settings_.color.b;

    r = r * (1.0f - settings_.density) + filteredR * settings_.density;
    g = g * (1.0f - settings_.density) + filteredG * settings_.density;
    b = b * (1.0f - settings_.density) + filteredB * settings_.density;

    r = (r - 0.5f) * settings_.contrast + 0.5f + settings_.brightness;
    g = (g - 0.5f) * settings_.contrast + 0.5f + settings_.brightness;
    b = (b - 0.5f) * settings_.contrast + 0.5f + settings_.brightness;

    if (settings_.saturationBoost != 1.0f) {
        float h = 0.0f;
        float s = 0.0f;
        float l = 0.0f;
        rgbToHsl(r, g, b, h, s, l);
        s = std::clamp(s * settings_.saturationBoost, 0.0f, 1.0f);
        hslToRgb(h, s, l, r, g, b);
    }

    preserveLuma(sourceLuma, r, g, b, settings_.preserveLuma);

    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
}

float PhotoFilterProcessor::luma(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

void PhotoFilterProcessor::rgbToHsl(float r, float g, float b, float& h, float& s, float& l) {
    const float maxVal = std::max({r, g, b});
    const float minVal = std::min({r, g, b});
    const float delta = maxVal - minVal;
    l = (maxVal + minVal) * 0.5f;

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
    if (h < 0.0f) {
        h += 360.0f;
    }
}

void PhotoFilterProcessor::hslToRgb(float h, float s, float l, float& r, float& g, float& b) {
    if (s <= 1e-6f) {
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

    const float q = (l < 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
    const float p = 2.0f * l - q;
    const float hn = h / 360.0f;
    r = hueToRgb(p, q, hn + 1.0f / 3.0f);
    g = hueToRgb(p, q, hn);
    b = hueToRgb(p, q, hn - 1.0f / 3.0f);
}

void PhotoFilterProcessor::preserveLuma(float sourceLuma, float& r, float& g, float& b, bool enabled) {
    if (!enabled) {
        return;
    }
    const float targetLuma = luma(r, g, b);
    if (targetLuma <= 1e-6f) {
        return;
    }
    const float scale = sourceLuma / targetLuma;
    r = std::clamp(r * scale, 0.0f, 1.0f);
    g = std::clamp(g * scale, 0.0f, 1.0f);
    b = std::clamp(b * scale, 0.0f, 1.0f);
}

} // namespace ArtifactCore
