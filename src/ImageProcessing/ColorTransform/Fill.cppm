module;
#include <algorithm>
#include <QColor>
#include <QImage>

module ImageProcessing.ColorTransform.Fill;

namespace ArtifactCore {

namespace {
void applyFillPixel(const SolidFillSettings& settings,
                    float& r, float& g, float& b, float& a) {
    const float fillR = settings.color.redF();
    const float fillG = settings.color.greenF();
    const float fillB = settings.color.blueF();
    const float opacity = std::clamp(settings.opacity, 0.0f, 1.0f);
    r = r * (1.0f - opacity) + fillR * opacity;
    g = g * (1.0f - opacity) + fillG * opacity;
    b = b * (1.0f - opacity) + fillB * opacity;
    if (!settings.preserveAlpha) {
        a = a * (1.0f - opacity) + opacity;
    }
}
} // namespace

SolidFillSettings SolidFillSettings::white() {
    SolidFillSettings s;
    s.color = QColor(255, 255, 255);
    return s;
}

SolidFillSettings SolidFillSettings::black() {
    SolidFillSettings s;
    s.color = QColor(0, 0, 0);
    return s;
}

SolidFillSettings SolidFillSettings::red() {
    SolidFillSettings s;
    s.color = QColor(255, 64, 48);
    return s;
}

SolidFillSettings SolidFillSettings::blue() {
    SolidFillSettings s;
    s.color = QColor(56, 128, 255);
    return s;
}

SolidFillSettings SolidFillSettings::green() {
    SolidFillSettings s;
    s.color = QColor(48, 196, 112);
    return s;
}

SolidFillProcessor::SolidFillProcessor() = default;
SolidFillProcessor::~SolidFillProcessor() = default;

void SolidFillProcessor::setSettings(const SolidFillSettings& settings) {
    settings_ = settings;
}

const SolidFillSettings& SolidFillProcessor::settings() const {
    return settings_;
}

QImage SolidFillProcessor::apply(const QImage& source) const {
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
            applyFillPixel(settings_, r, g, b, a);
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

void SolidFillProcessor::apply(float* pixels, int width, int height) const {
    if (!pixels || width <= 0 || height <= 0) {
        return;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) +
                                     static_cast<size_t>(x)) * 4u;
            applyFillPixel(settings_, pixel[0], pixel[1], pixel[2], pixel[3]);
        }
    }
}

} // namespace ArtifactCore
