module;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QImage>

module ImageProcessing.ColorTransform.GradientRamp;

namespace ArtifactCore {

namespace {
void applyGradientPixel(const GradientRampSettings& settings,
                        float x, float y,
                        int width, int height,
                        float& r, float& g, float& b, float& a) {
    const float sx = settings.startX * std::max(1, width - 1);
    const float sy = settings.startY * std::max(1, height - 1);
    const float ex = settings.endX * std::max(1, width - 1);
    const float ey = settings.endY * std::max(1, height - 1);
    const float dx = ex - sx;
    const float dy = ey - sy;
    const float lenSq = std::max(1e-6f, dx * dx + dy * dy);
    const float invLenSq = 1.0f / lenSq;
    const float t = std::clamp(((x - sx) * dx + (y - sy) * dy) * invLenSq, 0.0f, 1.0f);

    const float sr = settings.startColor.redF();
    const float sg = settings.startColor.greenF();
    const float sb = settings.startColor.blueF();
    const float er = settings.endColor.redF();
    const float eg = settings.endColor.greenF();
    const float eb = settings.endColor.blueF();
    const float opacity = std::clamp(settings.opacity, 0.0f, 1.0f);
    const float outR = r * (1.0f - opacity) + (sr * (1.0f - t) + er * t) * opacity;
    const float outG = g * (1.0f - opacity) + (sg * (1.0f - t) + eg * t) * opacity;
    const float outB = b * (1.0f - opacity) + (sb * (1.0f - t) + eb * t) * opacity;

    r = outR;
    g = outG;
    b = outB;
    if (!settings.preserveAlpha) {
        a = a * (1.0f - opacity) + opacity;
    }
}
}

GradientRampSettings GradientRampSettings::sunrise() {
    GradientRampSettings s;
    s.startColor = QColor(22, 36, 78);
    s.endColor = QColor(255, 165, 94);
    s.startX = 0.1f;
    s.startY = 0.15f;
    s.endX = 0.9f;
    s.endY = 0.85f;
    s.opacity = 1.0f;
    return s;
}

GradientRampSettings GradientRampSettings::ocean() {
    GradientRampSettings s;
    s.startColor = QColor(5, 22, 46);
    s.endColor = QColor(52, 186, 230);
    s.startX = 0.0f;
    s.startY = 1.0f;
    s.endX = 1.0f;
    s.endY = 0.0f;
    return s;
}

GradientRampSettings GradientRampSettings::neon() {
    GradientRampSettings s;
    s.startColor = QColor(18, 10, 42);
    s.endColor = QColor(255, 71, 201);
    s.startX = 0.2f;
    s.startY = 0.0f;
    s.endX = 0.8f;
    s.endY = 1.0f;
    return s;
}

GradientRampSettings GradientRampSettings::mono() {
    GradientRampSettings s;
    s.startColor = QColor(0, 0, 0);
    s.endColor = QColor(255, 255, 255);
    s.startX = 0.0f;
    s.startY = 0.0f;
    s.endX = 1.0f;
    s.endY = 0.0f;
    return s;
}

GradientRampProcessor::GradientRampProcessor() = default;
GradientRampProcessor::~GradientRampProcessor() = default;

void GradientRampProcessor::setSettings(const GradientRampSettings& settings) {
    settings_ = settings;
}

const GradientRampSettings& GradientRampProcessor::settings() const {
    return settings_;
}

void GradientRampProcessor::applyPreset(Preset preset) {
    switch (preset) {
    case Preset::Sunrise:
        settings_ = GradientRampSettings::sunrise();
        break;
    case Preset::Ocean:
        settings_ = GradientRampSettings::ocean();
        break;
    case Preset::Neon:
        settings_ = GradientRampSettings::neon();
        break;
    case Preset::Mono:
        settings_ = GradientRampSettings::mono();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

QImage GradientRampProcessor::apply(const QImage& source) const {
    if (source.isNull()) {
        return source;
    }

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int width = result.width();
    const int height = result.height();

    for (int y = 0; y < height; ++y) {
        auto* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < width; ++x) {
            float r = qRed(line[x]) / 255.0f;
            float g = qGreen(line[x]) / 255.0f;
            float b = qBlue(line[x]) / 255.0f;
            float a = qAlpha(line[x]) / 255.0f;
            applyGradientPixel(settings_, static_cast<float>(x), static_cast<float>(y),
                               width, height, r, g, b, a);
            line[x] = qRgba(
                static_cast<int>(std::clamp(r * 255.0f, 0.0f, 255.0f)),
                static_cast<int>(std::clamp(g * 255.0f, 0.0f, 255.0f)),
                static_cast<int>(std::clamp(b * 255.0f, 0.0f, 255.0f)),
                static_cast<int>(std::clamp(a * 255.0f, 0.0f, 255.0f))
            );
        }
    }

    return result;
}

void GradientRampProcessor::apply(float* pixels, int width, int height) const {
    if (!pixels || width <= 0 || height <= 0) {
        return;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) +
                                     static_cast<size_t>(x)) * 4u;
            applyGradientPixel(settings_, static_cast<float>(x), static_cast<float>(y),
                               width, height, pixel[0], pixel[1], pixel[2], pixel[3]);
        }
    }
}

} // namespace ArtifactCore
