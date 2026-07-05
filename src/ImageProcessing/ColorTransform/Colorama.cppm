module;
#include <algorithm>
#include <array>
#include <cmath>
#include <QColor>
#include <QImage>

module ImageProcessing.ColorTransform.Colorama;

namespace ArtifactCore {

ColoramaSettings ColoramaSettings::rainbow() {
    return ColoramaSettings{};
}

ColoramaSettings ColoramaSettings::fire() {
    ColoramaSettings settings;
    settings.palette = ColoramaPalette::Fire;
    settings.saturationBoost = 1.15f;
    settings.contrast = 1.1f;
    return settings;
}

ColoramaSettings ColoramaSettings::ocean() {
    ColoramaSettings settings;
    settings.palette = ColoramaPalette::Ocean;
    settings.saturationBoost = 1.05f;
    settings.contrast = 0.95f;
    return settings;
}

ColoramaProcessor::ColoramaProcessor() = default;
ColoramaProcessor::~ColoramaProcessor() = default;

void ColoramaProcessor::setSettings(const ColoramaSettings& settings) {
    settings_ = settings;
}

const ColoramaSettings& ColoramaProcessor::settings() const {
    return settings_;
}

QImage ColoramaProcessor::apply(const QImage& source) const {
    if (source.isNull() || settings_.strength <= 0.0f) {
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

void ColoramaProcessor::applyPixel(float& r, float& g, float& b) const {
    const float sourceLuma = luma(r, g, b);
    float h = 0.0f;
    float s = 0.0f;
    float l = 0.0f;
    rgbToHsl(r, g, b, h, s, l);

    float key = 0.0f;
    if (settings_.sourceMode == ColoramaSourceMode::Hue) {
        key = h / 360.0f;
    } else {
        key = sourceLuma;
    }

    key = std::fmod(key * settings_.spread + settings_.phase, 1.0f);
    if (key < 0.0f) {
        key += 1.0f;
    }
    ColoramaColor mapped = samplePalette(settings_.palette, key);

    if (settings_.contrast != 1.0f) {
        mapped.r = std::clamp((mapped.r - 0.5f) * settings_.contrast + 0.5f, 0.0f, 1.0f);
        mapped.g = std::clamp((mapped.g - 0.5f) * settings_.contrast + 0.5f, 0.0f, 1.0f);
        mapped.b = std::clamp((mapped.b - 0.5f) * settings_.contrast + 0.5f, 0.0f, 1.0f);
    }

    if (settings_.saturationBoost != 1.0f) {
        float mh = 0.0f;
        float ms = 0.0f;
        float ml = 0.0f;
        rgbToHsl(mapped.r, mapped.g, mapped.b, mh, ms, ml);
        ms = std::clamp(ms * settings_.saturationBoost, 0.0f, 1.0f);
        hslToRgb(mh, ms, ml, mapped.r, mapped.g, mapped.b);
    }

    r = r * (1.0f - settings_.strength) + mapped.r * settings_.strength;
    g = g * (1.0f - settings_.strength) + mapped.g * settings_.strength;
    b = b * (1.0f - settings_.strength) + mapped.b * settings_.strength;

    preserveLuma(sourceLuma, r, g, b, settings_.preserveLuma);

    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
}

float ColoramaProcessor::luma(float r, float g, float b) {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

void ColoramaProcessor::rgbToHsl(float r, float g, float b, float& h, float& s, float& l) {
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

void ColoramaProcessor::hslToRgb(float h, float s, float l, float& r, float& g, float& b) {
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

ColoramaColor ColoramaProcessor::interpolate(const ColoramaColor& a, const ColoramaColor& b, float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return {
        a.r * (1.0f - clamped) + b.r * clamped,
        a.g * (1.0f - clamped) + b.g * clamped,
        a.b * (1.0f - clamped) + b.b * clamped,
    };
}

ColoramaColor ColoramaProcessor::samplePalette(ColoramaPalette palette, float t) {
    static constexpr std::array<ColoramaColor, 5> rainbow {
        ColoramaColor{1.00f, 0.20f, 0.20f},
        ColoramaColor{1.00f, 0.80f, 0.20f},
        ColoramaColor{0.20f, 1.00f, 0.35f},
        ColoramaColor{0.20f, 0.70f, 1.00f},
        ColoramaColor{0.85f, 0.20f, 1.00f},
    };
    static constexpr std::array<ColoramaColor, 5> fire {
        ColoramaColor{0.10f, 0.00f, 0.00f},
        ColoramaColor{0.55f, 0.10f, 0.00f},
        ColoramaColor{0.90f, 0.35f, 0.00f},
        ColoramaColor{1.00f, 0.74f, 0.20f},
        ColoramaColor{1.00f, 0.95f, 0.75f},
    };
    static constexpr std::array<ColoramaColor, 5> ocean {
        ColoramaColor{0.03f, 0.05f, 0.18f},
        ColoramaColor{0.00f, 0.30f, 0.45f},
        ColoramaColor{0.10f, 0.65f, 0.75f},
        ColoramaColor{0.30f, 0.85f, 0.95f},
        ColoramaColor{0.80f, 0.98f, 1.00f},
    };
    static constexpr std::array<ColoramaColor, 5> neon {
        ColoramaColor{0.00f, 0.95f, 0.75f},
        ColoramaColor{0.85f, 0.10f, 1.00f},
        ColoramaColor{0.10f, 0.90f, 1.00f},
        ColoramaColor{1.00f, 0.15f, 0.45f},
        ColoramaColor{0.90f, 1.00f, 0.20f},
    };
    static constexpr std::array<ColoramaColor, 5> sunset {
        ColoramaColor{0.05f, 0.04f, 0.16f},
        ColoramaColor{0.35f, 0.09f, 0.40f},
        ColoramaColor{0.80f, 0.20f, 0.30f},
        ColoramaColor{0.98f, 0.45f, 0.12f},
        ColoramaColor{1.00f, 0.86f, 0.58f},
    };

    const auto sample = [&](const auto& ramp, float value) -> ColoramaColor {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        const float scaled = clamped * static_cast<float>(ramp.size() - 1);
        const int idx = static_cast<int>(std::floor(scaled));
        const int next = std::min(idx + 1, static_cast<int>(ramp.size() - 1));
        const float frac = scaled - static_cast<float>(idx);
        return interpolate(ramp[static_cast<size_t>(idx)], ramp[static_cast<size_t>(next)], frac);
    };

    switch (palette) {
    case ColoramaPalette::Fire:
        return sample(fire, t);
    case ColoramaPalette::Ocean:
        return sample(ocean, t);
    case ColoramaPalette::Neon:
        return sample(neon, t);
    case ColoramaPalette::Sunset:
        return sample(sunset, t);
    case ColoramaPalette::Rainbow:
    default:
        return sample(rainbow, t);
    }
}

void ColoramaProcessor::preserveLuma(float sourceLuma, float& r, float& g, float& b, bool enabled) {
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
