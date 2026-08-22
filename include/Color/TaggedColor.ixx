module;
#include <algorithm>
#include <cstdint>
#include <optional>
export module Color.Tagged;

import Color.Float;
import FloatRGBA;
import Graphics.SurfaceColorContract;
import Color.TransferFunction;
import Color.GamutConversion;

export namespace ArtifactCore {

// Canonical bridge between the surface-contract primaries vocabulary and the
// gamut conversion matrices. AdobeRGB / DaVinciWideGamut / XYZ have no
// SurfaceColorPrimaries counterpart and map to nothing.
[[nodiscard]] inline std::optional<Gamut> gamutForPrimaries(
    const SurfaceColorPrimaries primaries) noexcept
{
    switch (primaries) {
    case SurfaceColorPrimaries::SRGB_Rec709_D65: return Gamut::Rec709;
    case SurfaceColorPrimaries::DisplayP3_D65:   return Gamut::DisplayP3;
    case SurfaceColorPrimaries::Rec2020_D65:     return Gamut::Rec2020;
    case SurfaceColorPrimaries::ACES_AP0:        return Gamut::ACES_AP0;
    case SurfaceColorPrimaries::ACES_AP1:        return Gamut::ACES_AP1;
    case SurfaceColorPrimaries::Unknown:         break;
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<SurfaceColorPrimaries> primariesForGamut(
    const Gamut gamut) noexcept
{
    switch (gamut) {
    case Gamut::sRGB:
    case Gamut::Rec709:            return SurfaceColorPrimaries::SRGB_Rec709_D65;
    case Gamut::DisplayP3:         return SurfaceColorPrimaries::DisplayP3_D65;
    case Gamut::Rec2020:           return SurfaceColorPrimaries::Rec2020_D65;
    case Gamut::ACES_AP0:          return SurfaceColorPrimaries::ACES_AP0;
    case Gamut::ACES_AP1:          return SurfaceColorPrimaries::ACES_AP1;
    case Gamut::DCI_P3:
    case Gamut::AdobeRGB:
    case Gamut::XYZ_D65:
    case Gamut::XYZ_D60:
    case Gamut::DaVinciWideGamut:  break;
    }
    return std::nullopt;
}

// A float RGBA value carrying the interpretation that SurfaceColorDescriptor
// assigns to surfaces. This is the value-side counterpart: single colors can
// travel between layers, effects, and NLE data without losing their meaning.
// Gamut (primaries) conversion is intentionally not implemented here yet;
// transfer and alpha-mode conversions are exact.
struct TaggedColor {
    FloatRGBA rgba{};
    SurfaceColorPrimaries primaries = SurfaceColorPrimaries::SRGB_Rec709_D65;
    TransferFunction transfer = TransferFunction::sRGB;
    bool transferKnown = true;
    SurfaceAlphaMode alphaMode = SurfaceAlphaMode::Straight;

    [[nodiscard]] static constexpr TaggedColor srgbEncoded(
        const float r, const float g, const float b, const float a = 1.0f) noexcept
    {
        TaggedColor color;
        color.rgba = FloatRGBA(r, g, b, a);
        color.primaries = SurfaceColorPrimaries::SRGB_Rec709_D65;
        color.transfer = TransferFunction::sRGB;
        color.transferKnown = true;
        color.alphaMode = SurfaceAlphaMode::Straight;
        return color;
    }

    [[nodiscard]] static constexpr TaggedColor sceneLinear(
        const float r, const float g, const float b, const float a = 1.0f,
        const SurfaceColorPrimaries primaries =
            SurfaceColorPrimaries::SRGB_Rec709_D65) noexcept
    {
        TaggedColor color;
        color.rgba = FloatRGBA(r, g, b, a);
        color.primaries = primaries;
        color.transfer = TransferFunction::Linear;
        color.transferKnown = true;
        color.alphaMode = SurfaceAlphaMode::Straight;
        return color;
    }

    // Re-encodes RGB channels to `target` via scene-linear. Unknown transfers
    // pass through untouched so bad metadata cannot corrupt values.
    [[nodiscard]] TaggedColor toTransfer(const TransferFunction target) const
    {
        if (!transferKnown || transfer == target) {
            return *this;
        }
        TaggedColor out = *this;
        out.rgba.setRed(ColorTransferFunction::encode(
            ColorTransferFunction::decode(rgba.r(), transfer), target));
        out.rgba.setGreen(ColorTransferFunction::encode(
            ColorTransferFunction::decode(rgba.g(), transfer), target));
        out.rgba.setBlue(ColorTransferFunction::encode(
            ColorTransferFunction::decode(rgba.b(), transfer), target));
        out.transfer = target;
        out.transferKnown = true;
        return out;
    }

    [[nodiscard]] TaggedColor premultiplied() const noexcept
    {
        if (alphaMode == SurfaceAlphaMode::Premultiplied ||
            alphaMode == SurfaceAlphaMode::Opaque) {
            return *this;
        }
        TaggedColor out = *this;
        const float alpha = std::clamp(rgba.a(), 0.0f, 1.0f);
        out.rgba.setRed(rgba.r() * alpha);
        out.rgba.setGreen(rgba.g() * alpha);
        out.rgba.setBlue(rgba.b() * alpha);
        out.alphaMode = SurfaceAlphaMode::Premultiplied;
        return out;
    }

    [[nodiscard]] TaggedColor straight() const noexcept
    {
        if (alphaMode != SurfaceAlphaMode::Premultiplied) {
            return *this;
        }
        TaggedColor out = *this;
        const float alpha = std::clamp(rgba.a(), 0.0f, 1.0f);
        if (alpha > 1e-6f) {
            out.rgba.setRed(rgba.r() / alpha);
            out.rgba.setGreen(rgba.g() / alpha);
            out.rgba.setBlue(rgba.b() / alpha);
        } else {
            out.rgba.setRed(0.0f);
            out.rgba.setGreen(0.0f);
            out.rgba.setBlue(0.0f);
        }
        out.alphaMode = SurfaceAlphaMode::Straight;
        return out;
    }

    // Converts primaries via scene-linear using the gamut conversion
    // matrices (Bradford white-point adaptation included). Transfer metadata
    // is preserved; unknown transfers pass through untouched.
    [[nodiscard]] TaggedColor toPrimaries(const SurfaceColorPrimaries target) const
    {
        if (primaries == target || !transferKnown) {
            return *this;
        }
        const auto sourceGamut = gamutForPrimaries(primaries);
        const auto targetGamut = gamutForPrimaries(target);
        if (!sourceGamut || !targetGamut) {
            return *this;
        }
        const TaggedColor linear = toTransfer(TransferFunction::Linear);
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        ColorGamutConversion::convert(linear.rgba.r(), linear.rgba.g(),
                                      linear.rgba.b(), *sourceGamut,
                                      *targetGamut, r, g, b);
        TaggedColor out = linear;
        out.rgba.setRed(r);
        out.rgba.setGreen(g);
        out.rgba.setBlue(b);
        out.primaries = target;
        return out.toTransfer(transfer);
    }

    // The equivalent surface descriptor for this value when stored as f32 RGBA.
    [[nodiscard]] constexpr SurfaceColorDescriptor surfaceDescriptor() const noexcept
    {
        SurfaceColorDescriptor descriptor =
            SurfaceColorDescriptor::unknownRgba32Float();
        descriptor.primaries = primaries;
        descriptor.transfer = transfer;
        descriptor.transferKnown = transferKnown;
        descriptor.alphaMode = alphaMode;
        descriptor.range = transfer == TransferFunction::Linear
                               ? SurfaceColorRange::SceneReferred
                               : SurfaceColorRange::DisplayReferred;
        return descriptor;
    }

    [[nodiscard]] friend bool operator==(const TaggedColor& a,
                                         const TaggedColor& b) noexcept
    {
        return a.rgba == b.rgba && a.primaries == b.primaries &&
               a.transfer == b.transfer && a.transferKnown == b.transferKnown &&
               a.alphaMode == b.alphaMode;
    }

    [[nodiscard]] friend bool operator!=(const TaggedColor& a,
                                        const TaggedColor& b) noexcept
    {
        return !(a == b);
    }
};

} // namespace ArtifactCore
