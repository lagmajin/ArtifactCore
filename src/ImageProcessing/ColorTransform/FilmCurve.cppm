module;
#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>
#include <QImage>
#include <QColor>

module ImageProcessing.ColorTransform.FilmCurve;

namespace ArtifactCore {

// ============================================================================
// FilmCurveProcessor
// ============================================================================

FilmCurveProcessor::FilmCurveProcessor() = default;
FilmCurveProcessor::~FilmCurveProcessor() = default;

void FilmCurveProcessor::setSettings(const FilmCurveSettings& settings) {
    settings_ = settings;
}

auto FilmCurveProcessor::settings() const -> const FilmCurveSettings& {
    return settings_;
}

auto FilmCurveProcessor::apply(const QImage& source) const -> QImage {
    if (source.isNull()) {
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

            applyToLuma(r, g, b);

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

auto FilmCurveProcessor::applyChannel(float value) const -> float {
    // 1. Toe（シャドウ側）
    value = applyToe(value);

    // 2. Shoulder（ハイライト側）
    value = applyShoulder(value);

    // 3. Midtone contrast
    value = applyMidtone(value);

    // 4. Output levels
    value = settings_.outputBlack + value * (settings_.outputWhite - settings_.outputBlack);

    return std::clamp(value, 0.0f, 1.0f);
}

void FilmCurveProcessor::applyToLuma(float& r, float& g, float& b) const {
    // ルミナンス計算
    float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;

    // カーブ適用
    float newLuma = applyChannel(luma);

    // 比率を維持してRGBに再適用
    if (luma > 1e-6f) {
        float scale = newLuma / luma;
        r = std::clamp(r * scale, 0.0f, 1.0f);
        g = std::clamp(g * scale, 0.0f, 1.0f);
        b = std::clamp(b * scale, 0.0f, 1.0f);
    } else {
        r = g = b = newLuma;
    }
}

auto FilmCurveProcessor::applyToe(float value) const -> float {
    if (settings_.toeStrength <= 0.0f) {
        return value;
    }

    float start = settings_.toeStart;
    float softness = settings_.toeSoftness;

    if (value <= start) {
        // Toe領域: シャドウを圧縮
        float t = value / (start + 1e-6f);
        t = smoothClamp(t, 0.0f, settings_.toeStrength, softness);
        return value * (1.0f - t);
    }

    // Toe範囲外: 滑らかに接続
    float transitionWidth = start * softness;
    if (value < start + transitionWidth) {
        float blend = (value - start) / transitionWidth;
        blend = blend * blend * (3.0f - 2.0f * blend); // smoothstep
        float toeValue = applyToe(value);
        return toeValue * (1.0f - blend) + value * blend;
    }

    return value;
}

auto FilmCurveProcessor::applyShoulder(float value) const -> float {
    if (settings_.shoulderStrength <= 0.0f) {
        return value;
    }

    float start = settings_.shoulderStart;
    float softness = settings_.shoulderSoftness;

    if (value >= start) {
        // Shoulder領域: ハイライトを圧縮
        float t = (value - start) / (1.0f - start + 1e-6f);
        t = smoothClamp(t, 1.0f, -settings_.shoulderStrength, softness);
        return value + t * (1.0f - value);
    }

    // Shoulder範囲外: 滑らかに接続
    float transitionWidth = (1.0f - start) * softness;
    if (value > start - transitionWidth) {
        float blend = (value - (start - transitionWidth)) / transitionWidth;
        blend = blend * blend * (3.0f - 2.0f * blend); // smoothstep
        float shoulderValue = applyShoulder(value);
        return value * (1.0f - blend) + shoulderValue * blend;
    }

    return value;
}

auto FilmCurveProcessor::applyMidtone(float value) const -> float {
    if (std::abs(settings_.midtoneContrast - 1.0f) < 1e-6f) {
        return value;
    }

    float bias = settings_.midtoneBias;

    // コントラスト適用
    float contrast = settings_.midtoneContrast;
    float result = (value - bias) * contrast + bias;

    return std::clamp(result, 0.0f, 1.0f);
}

auto FilmCurveProcessor::smoothClamp(float value, float start, float strength, float softness) const -> float {
    // 滑らかなクリンプ関数
    float t = std::clamp(value, 0.0f, 1.0f);

    // Softnessでカーブを調整
    float k = 1.0f / (softness + 0.01f);
    float sigmoid = 1.0f / (1.0f + std::exp(-k * (t - 0.5f) * 10.0f));

    return sigmoid * strength;
}

} // namespace ArtifactCore
