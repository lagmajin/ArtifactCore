module;
#include <utility>
#include <algorithm>

module Layer.Matte;

import std;

namespace ArtifactCore {

namespace {
constexpr float clamp01(float value) {
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
            return clamp01(current * next);
        case MatteStackMode::Subtract:
            return clamp01(current * (1.0f - next));
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

    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            const float matteFactor = sample(matteSample, x, y, settings.mode,
                                             settings.luminanceStandard);
            const float combined = clamp01(matteFactor * settings.opacity);
            out.setPixel(x, y, apply(source.getPixel(x, y), combined));
        }
    }

    return out;
}

} // namespace ArtifactCore
