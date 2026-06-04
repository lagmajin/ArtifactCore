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

struct Chromaticity {
    float x;
    float y;
};

constexpr Chromaticity kD65{0.3127f, 0.3290f};
constexpr Chromaticity kD60{0.32168f, 0.33767f};
constexpr Chromaticity kSRGBRed{0.64f, 0.33f};
constexpr Chromaticity kSRGBGreen{0.30f, 0.60f};
constexpr Chromaticity kSRGBBlue{0.15f, 0.06f};
constexpr Chromaticity kRec2020Red{0.708f, 0.292f};
constexpr Chromaticity kRec2020Green{0.170f, 0.797f};
constexpr Chromaticity kRec2020Blue{0.131f, 0.046f};
constexpr Chromaticity kP3Red{0.68f, 0.32f};
constexpr Chromaticity kP3Green{0.265f, 0.690f};
constexpr Chromaticity kP3Blue{0.150f, 0.060f};
constexpr Chromaticity kACESAP0Red{0.7347f, 0.2653f};
constexpr Chromaticity kACESAP0Green{0.0f, 1.0f};
constexpr Chromaticity kACESAP0Blue{0.0001f, -0.0770f};
constexpr Chromaticity kACESAP1Red{0.713f, 0.293f};
constexpr Chromaticity kACESAP1Green{0.165f, 0.830f};
constexpr Chromaticity kACESAP1Blue{0.128f, 0.044f};

using Mat3 = std::array<float, 9>;

constexpr Mat3 makeIdentity3()
{
    return {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };
}

std::array<float, 3> xyToXyz(Chromaticity c)
{
    return {c.x / c.y, 1.0f, (1.0f - c.x - c.y) / c.y};
}

Mat3 multiply3(const Mat3 &a, const Mat3 &b)
{
    Mat3 out{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            out[row * 3 + col] = a[row * 3 + 0] * b[0 * 3 + col] +
                                 a[row * 3 + 1] * b[1 * 3 + col] +
                                 a[row * 3 + 2] * b[2 * 3 + col];
        }
    }
    return out;
}

std::array<float, 3> multiply3Vec(const Mat3 &m, const std::array<float, 3> &v)
{
    return {
        m[0] * v[0] + m[1] * v[1] + m[2] * v[2],
        m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
        m[6] * v[0] + m[7] * v[1] + m[8] * v[2],
    };
}

Mat3 invert3(const Mat3 &m)
{
    const float a = m[0];
    const float b = m[1];
    const float c = m[2];
    const float d = m[3];
    const float e = m[4];
    const float f = m[5];
    const float g = m[6];
    const float h = m[7];
    const float i = m[8];

    const float A = e * i - f * h;
    const float B = c * h - b * i;
    const float C = b * f - c * e;
    const float D = f * g - d * i;
    const float E = a * i - c * g;
    const float F = c * d - a * f;
    const float G = d * h - e * g;
    const float H = b * g - a * h;
    const float I = a * e - b * d;

    const float det = a * A + b * D + c * G;
    if (std::abs(det) < 1e-8f) {
        return makeIdentity3();
    }

    const float invDet = 1.0f / det;
    return {
        A * invDet, B * invDet, C * invDet,
        D * invDet, E * invDet, F * invDet,
        G * invDet, H * invDet, I * invDet,
    };
}

Mat3 chromaticAdaptationBradford(Chromaticity sourceWhite, Chromaticity targetWhite)
{
    constexpr Mat3 kBradford = {
        0.8951f, 0.2664f, -0.1614f,
        -0.7502f, 1.7135f, 0.0367f,
        0.0389f, -0.0685f, 1.0296f,
    };
    const Mat3 kBradfordInv = invert3(kBradford);

    const auto sourceXyz = xyToXyz(sourceWhite);
    const auto targetXyz = xyToXyz(targetWhite);
    const auto sourceCone = multiply3Vec(kBradford, sourceXyz);
    const auto targetCone = multiply3Vec(kBradford, targetXyz);

    const Mat3 coneScale = {
        targetCone[0] / sourceCone[0], 0.0f, 0.0f,
        0.0f, targetCone[1] / sourceCone[1], 0.0f,
        0.0f, 0.0f, targetCone[2] / sourceCone[2],
    };

    return multiply3(kBradfordInv, multiply3(coneScale, kBradford));
}

Mat3 rgbToXyzMatrix(Chromaticity red, Chromaticity green, Chromaticity blue, Chromaticity white)
{
    const auto xr = xyToXyz(red);
    const auto xg = xyToXyz(green);
    const auto xb = xyToXyz(blue);
    const Mat3 primaries = {
        xr[0], xg[0], xb[0],
        xr[1], xg[1], xb[1],
        xr[2], xg[2], xb[2],
    };

    const auto whiteXyz = xyToXyz(white);
    const auto scale = multiply3Vec(invert3(primaries), whiteXyz);

    return {
        primaries[0] * scale[0], primaries[1] * scale[1], primaries[2] * scale[2],
        primaries[3] * scale[0], primaries[4] * scale[1], primaries[5] * scale[2],
        primaries[6] * scale[0], primaries[7] * scale[1], primaries[8] * scale[2],
    };
}

struct ColorSpaceDefinition {
    Chromaticity red;
    Chromaticity green;
    Chromaticity blue;
    Chromaticity white;
};

ColorSpaceDefinition definitionFor(ColorSpace space)
{
    switch (space) {
    case ColorSpace::Linear:
    case ColorSpace::sRGB:
    case ColorSpace::Rec709:
        return {kSRGBRed, kSRGBGreen, kSRGBBlue, kD65};
    case ColorSpace::Rec2020:
        return {kRec2020Red, kRec2020Green, kRec2020Blue, kD65};
    case ColorSpace::P3:
        return {kP3Red, kP3Green, kP3Blue, kD65};
    case ColorSpace::ACES_AP0:
        return {kACESAP0Red, kACESAP0Green, kACESAP0Blue, kD60};
    case ColorSpace::ACES_AP1:
        return {kACESAP1Red, kACESAP1Green, kACESAP1Blue, kD60};
    default:
        return {kSRGBRed, kSRGBGreen, kSRGBBlue, kD65};
    }
}

std::array<float, 16> to4x4(const Mat3 &m)
{
    return {
        m[0], m[1], m[2], 0.0f,
        m[3], m[4], m[5], 0.0f,
        m[6], m[7], m[8], 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}
} // namespace

std::array<float, 16> ColorSpaceConverter::getConversionMatrix(ColorSpace from, ColorSpace to)
{
    if (from == to) {
        return makeIdentityMatrix();
    }

    const auto src = definitionFor(from);
    const auto dst = definitionFor(to);

    Mat3 srcToXyz = rgbToXyzMatrix(src.red, src.green, src.blue, src.white);
    if (src.white.x != dst.white.x || src.white.y != dst.white.y) {
        srcToXyz = multiply3(chromaticAdaptationBradford(src.white, dst.white), srcToXyz);
    }

    const Mat3 xyzToDst = invert3(rgbToXyzMatrix(dst.red, dst.green, dst.blue, dst.white));
    return to4x4(multiply3(xyzToDst, srcToXyz));
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
