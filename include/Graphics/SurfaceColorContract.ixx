module;

#include <cstdint>

export module Graphics.SurfaceColorContract;

// SurfaceColorDescriptor exposes TransferFunction in its public contract, so
// consumers must receive the defining module as part of this interface.
export import Color.TransferFunction;

export namespace ArtifactCore {

enum class SurfacePixelStorage : std::uint8_t {
  Unknown,
  RGBA8UNorm,
  RGBA8UNormSrgb,
  RGBA16Float,
  RGBA32Float,
};

enum class SurfaceChannelOrder : std::uint8_t {
  Unknown,
  RGBA,
  BGRA,
  RGB,
  BGR,
  Gray,
};

enum class SurfaceColorPrimaries : std::uint8_t {
  Unknown,
  SRGB_Rec709_D65,
  DisplayP3_D65,
  Rec2020_D65,
  ACES_AP0,
  ACES_AP1,
};

enum class SurfaceAlphaMode : std::uint8_t {
  Unknown,
  Opaque,
  Straight,
  Premultiplied,
  Coverage,
};

enum class SurfaceColorRange : std::uint8_t {
  Unknown,
  SceneReferred,
  DisplayReferred,
  Data,
};

struct SurfaceColorDescriptor {
  SurfacePixelStorage storage = SurfacePixelStorage::Unknown;
  SurfaceChannelOrder channelOrder = SurfaceChannelOrder::Unknown;
  SurfaceColorPrimaries primaries = SurfaceColorPrimaries::Unknown;
  TransferFunction transfer = TransferFunction::Linear;
  SurfaceAlphaMode alphaMode = SurfaceAlphaMode::Unknown;
  SurfaceColorRange range = SurfaceColorRange::Unknown;
  bool transferKnown = false;

  static constexpr SurfaceColorDescriptor unknown() noexcept {
    return {};
  }

  static constexpr SurfaceColorDescriptor unknownRgba32Float() noexcept {
    SurfaceColorDescriptor descriptor;
    descriptor.storage = SurfacePixelStorage::RGBA32Float;
    descriptor.channelOrder = SurfaceChannelOrder::RGBA;
    return descriptor;
  }

  static constexpr SurfaceColorDescriptor canonicalLinearPremultiplied(
      SurfaceColorPrimaries workingPrimaries =
          SurfaceColorPrimaries::SRGB_Rec709_D65) noexcept {
    SurfaceColorDescriptor descriptor;
    descriptor.storage = SurfacePixelStorage::RGBA32Float;
    descriptor.channelOrder = SurfaceChannelOrder::RGBA;
    descriptor.primaries = workingPrimaries;
    descriptor.transfer = TransferFunction::Linear;
    descriptor.alphaMode = SurfaceAlphaMode::Premultiplied;
    descriptor.range = SurfaceColorRange::SceneReferred;
    descriptor.transferKnown = true;
    return descriptor;
  }

  static constexpr SurfaceColorDescriptor encodedSrgbRgba8Premultiplied()
      noexcept {
    SurfaceColorDescriptor descriptor;
    descriptor.storage = SurfacePixelStorage::RGBA8UNormSrgb;
    descriptor.channelOrder = SurfaceChannelOrder::RGBA;
    descriptor.primaries = SurfaceColorPrimaries::SRGB_Rec709_D65;
    descriptor.transfer = TransferFunction::sRGB;
    descriptor.alphaMode = SurfaceAlphaMode::Premultiplied;
    descriptor.range = SurfaceColorRange::DisplayReferred;
    descriptor.transferKnown = true;
    return descriptor;
  }

  static constexpr SurfaceColorDescriptor encodedSrgbRgba8Straight() noexcept {
    SurfaceColorDescriptor descriptor = encodedSrgbRgba8Premultiplied();
    descriptor.alphaMode = SurfaceAlphaMode::Straight;
    return descriptor;
  }

  static constexpr SurfaceColorDescriptor linearStraightRgba32Float(
      SurfaceColorPrimaries workingPrimaries =
          SurfaceColorPrimaries::SRGB_Rec709_D65) noexcept {
    SurfaceColorDescriptor descriptor =
        canonicalLinearPremultiplied(workingPrimaries);
    descriptor.alphaMode = SurfaceAlphaMode::Straight;
    return descriptor;
  }

  static constexpr SurfaceColorDescriptor legacyOpenCvBgra32Float(
      TransferFunction sourceTransfer = TransferFunction::sRGB,
      SurfaceAlphaMode sourceAlpha = SurfaceAlphaMode::Straight) noexcept {
    SurfaceColorDescriptor descriptor;
    descriptor.storage = SurfacePixelStorage::RGBA32Float;
    descriptor.channelOrder = SurfaceChannelOrder::BGRA;
    descriptor.primaries = SurfaceColorPrimaries::SRGB_Rec709_D65;
    descriptor.transfer = sourceTransfer;
    descriptor.alphaMode = sourceAlpha;
    descriptor.range = SurfaceColorRange::DisplayReferred;
    descriptor.transferKnown = true;
    return descriptor;
  }

  constexpr bool isFullySpecified() const noexcept {
    return storage != SurfacePixelStorage::Unknown &&
           channelOrder != SurfaceChannelOrder::Unknown &&
           primaries != SurfaceColorPrimaries::Unknown && transferKnown &&
           alphaMode != SurfaceAlphaMode::Unknown &&
           range != SurfaceColorRange::Unknown;
  }

  constexpr bool isCanonical() const noexcept {
    return storage == SurfacePixelStorage::RGBA32Float &&
           channelOrder == SurfaceChannelOrder::RGBA &&
           primaries != SurfaceColorPrimaries::Unknown && transferKnown &&
           transfer == TransferFunction::Linear &&
           alphaMode == SurfaceAlphaMode::Premultiplied &&
           range == SurfaceColorRange::SceneReferred;
  }

  friend constexpr bool operator==(const SurfaceColorDescriptor &,
                                   const SurfaceColorDescriptor &) = default;
};

static_assert(
    SurfaceColorDescriptor::canonicalLinearPremultiplied().isCanonical());
static_assert(!SurfaceColorDescriptor::unknown().isFullySpecified());

} // namespace ArtifactCore
