module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QJsonObject>
#include <QString>

module Layer.Matte;

import std;
import Serialization.JsonAdapter;
import Serialization.SchemaMigration;

namespace ArtifactCore {

namespace {
const bool kMatteSerializationRegistered = [] {
    Serialization::registerJsonSerializableType<MatteNode>(QStringLiteral("MatteNode"), 1);
    Serialization::registerJsonSerializableType<MatteStack>(QStringLiteral("MatteStack"), 1);
    auto& migrations = Serialization::SchemaMigrationRegistry::instance();
    migrations.registerMigration(QStringLiteral("MatteNode"), 0, 1,
                                  [](const QJsonObject& object) { return object; });
    migrations.registerMigration(QStringLiteral("MatteStack"), 0, 1,
                                  [](const QJsonObject& object) { return object; });
    return true;
}();
}

namespace {
inline float clamp01(float value) {
    if (!std::isfinite(value)) return 0.0f;
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}
} // namespace

float MatteEvaluator::sample(const FloatRGBA& pixel,
                             MatteMode mode,
                             LuminanceStandard standard)
{
    const float alpha = clamp01(pixel.a());
    const float luminance = clamp01(
        ColorLuminance::calculate(pixel.r(), pixel.g(), pixel.b(), standard));

    switch (mode) {
        case MatteMode::None:
            return 1.0f;
        case MatteMode::Alpha:
            return alpha;
        case MatteMode::AlphaInverted:
            return 1.0f - alpha;
        case MatteMode::Luminance:
            return luminance;
        case MatteMode::LuminanceInverted:
            return 1.0f - luminance;
        default:
            return alpha;
    }
}

float MatteEvaluator::sample(const ImageF32x4_RGBA& image,
                             int x,
                             int y,
                             MatteMode mode,
                             LuminanceStandard standard)
{
    if (image.isEmpty()) {
        return 1.0f;
    }
    return sample(image.getPixel(x, y), mode, standard);
}

float MatteEvaluator::combine(float current, float next, MatteStackMode mode)
{
    current = clamp01(current);
    next = clamp01(next);

    switch (mode) {
        case MatteStackMode::Add:
            return clamp01(current + next);
        case MatteStackMode::Common:
            return std::min(current, next);
        case MatteStackMode::Subtract:
            return std::max(0.0f, current - next);
        default:
            return current;
    }
}

float MatteEvaluator::evaluate(const std::vector<float>& matteFactors,
                               MatteStackMode stackMode)
{
    if (matteFactors.empty()) {
        return 1.0f;
    }

    float result = clamp01(matteFactors.front());
    for (size_t i = 1; i < matteFactors.size(); ++i) {
        result = combine(result, matteFactors[i], stackMode);
    }
    return clamp01(result);
}

FloatRGBA MatteEvaluator::apply(const FloatRGBA& source, float matteFactor)
{
    const float factor = clamp01(matteFactor);
    return FloatRGBA(
        source.r() * factor,
        source.g() * factor,
        source.b() * factor,
        source.a() * factor);
}

ImageF32x4_RGBA MatteEvaluator::apply(const ImageF32x4_RGBA& source,
                                      const ImageF32x4_RGBA& matte,
                                      const MatteEvaluationSettings& settings)
{
    if (source.isEmpty()) {
        return ImageF32x4_RGBA();
    }
    if (settings.mode == MatteMode::None) {
        return source.DeepCopy();
    }

    ImageF32x4_RGBA matteSample = matte;
    if (!matteSample.isEmpty() &&
        (matteSample.width() != source.width() ||
         matteSample.height() != source.height())) {
        matteSample = matte.DeepCopy();
        matteSample.resize(source.width(), source.height());
    }

    ImageF32x4_RGBA out = source.DeepCopy();
    if (matteSample.isEmpty()) {
        return out;
    }

    const int width = source.width();
    const int height = source.height();
    const float* sourcePixels = source.rgba32fData();
    const float* mattePixels = matteSample.rgba32fData();
    float* outputPixels = out.rgba32fData();
    if (sourcePixels && mattePixels && outputPixels) {
        for (int y = 0; y < height; ++y) {
            const float* sourceRow = sourcePixels +
                static_cast<std::size_t>(y) * width * 4u;
            const float* matteRow = mattePixels +
                static_cast<std::size_t>(y) * width * 4u;
            float* outputRow = outputPixels +
                static_cast<std::size_t>(y) * width * 4u;
            for (int x = 0; x < width; ++x) {
                const FloatRGBA mattePixel(
                    matteRow[x * 4 + 0], matteRow[x * 4 + 1],
                    matteRow[x * 4 + 2], matteRow[x * 4 + 3]);
                const float matteFactor =
                    sample(mattePixel, settings.mode,
                           settings.luminanceStandard);
                const float combined =
                    clamp01(matteFactor * settings.opacity);
                outputRow[x * 4 + 0] = sourceRow[x * 4 + 0] * combined;
                outputRow[x * 4 + 1] = sourceRow[x * 4 + 1] * combined;
                outputRow[x * 4 + 2] = sourceRow[x * 4 + 2] * combined;
                outputRow[x * 4 + 3] = sourceRow[x * 4 + 3] * combined;
            }
        }
        return out;
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float matteFactor = sample(matteSample, x, y, settings.mode,
                                             settings.luminanceStandard);
            const float combined = clamp01(matteFactor * settings.opacity);
            out.setPixel(x, y, apply(source.getPixel(x, y), combined));
        }
    }

    return out;
}

} // namespace ArtifactCore
