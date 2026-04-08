module;
#include <utility>

#include <array>
#include <cmath>
#include <compare>

class tst_QList;

module Color.ColorSpace;

namespace ArtifactCore {

namespace {
constexpr std::array<float, 16> makeIdentityMatrix()
{
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}
} // namespace

std::array<float, 16> ColorSpaceConverter::getConversionMatrix(ColorSpace from, ColorSpace to)
{
    if (from == to) {
        return makeIdentityMatrix();
    }

    // Keep the first implementation conservative: provide a stable matrix path
    // for the node graph while color management is still being iterated on.
    // Non-linear conversions are handled elsewhere in the pipeline.
    return makeIdentityMatrix();
}

float ColorSpaceConverter::applyGamma(float value, GammaFunction gamma)
{
    switch (gamma) {
    case GammaFunction::Linear:
        return value;
    case GammaFunction::sRGB:
        return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    case GammaFunction::Gamma22:
        return std::pow(value, 1.0f / 2.2f);
    case GammaFunction::Gamma24:
        return std::pow(value, 1.0f / 2.4f);
    default:
        return value;
    }
}

float ColorSpaceConverter::removeGamma(float value, GammaFunction gamma)
{
    switch (gamma) {
    case GammaFunction::Linear:
        return value;
    case GammaFunction::sRGB:
        return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
    case GammaFunction::Gamma22:
        return std::pow(value, 2.2f);
    case GammaFunction::Gamma24:
        return std::pow(value, 2.4f);
    default:
        return value;
    }
}

float ColorSpaceConverter::getWhitePointX(ColorSpace space)
{
    switch (space) {
    case ColorSpace::Linear:
    case ColorSpace::sRGB:
    case ColorSpace::Rec709:
        return 0.3127f;
    case ColorSpace::Rec2020:
        return 0.3127f;
    case ColorSpace::P3:
        return 0.3140f;
    case ColorSpace::ACES_AP0:
    case ColorSpace::ACES_AP1:
        return 0.32168f;
    default:
        return 0.3127f;
    }
}

float ColorSpaceConverter::getWhitePointY(ColorSpace space)
{
    switch (space) {
    case ColorSpace::Linear:
    case ColorSpace::sRGB:
    case ColorSpace::Rec709:
        return 0.3290f;
    case ColorSpace::Rec2020:
        return 0.3290f;
    case ColorSpace::P3:
        return 0.3377f;
    case ColorSpace::ACES_AP0:
    case ColorSpace::ACES_AP1:
        return 0.33767f;
    default:
        return 0.3290f;
    }
}

float ColorSpaceConverter::getGammaExponent(ColorSpace space)
{
    switch (space) {
    case ColorSpace::Linear:
        return 1.0f;
    case ColorSpace::sRGB:
    case ColorSpace::Rec709:
        return 2.2f;
    case ColorSpace::Rec2020:
        return 2.4f;
    case ColorSpace::P3:
        return 2.2f;
    case ColorSpace::ACES_AP0:
    case ColorSpace::ACES_AP1:
        return 1.0f;
    default:
        return 2.2f;
    }
}

} // namespace ArtifactCore
