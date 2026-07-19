module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

export module Image.SurfacePixelConversion;

import Graphics.SurfaceColorContract;

export namespace ArtifactCore {

enum class SurfacePixelTarget : std::uint8_t {
  Rgba8SrgbStraight,
  Rgba32LinearStraight,
};

struct SurfacePixelBuffer {
  std::vector<std::uint8_t> bytes;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint64_t rowStride = 0;
  SurfaceColorDescriptor descriptor = SurfaceColorDescriptor::unknown();

  bool isValid() const noexcept {
    return width > 0 && height > 0 && rowStride > 0 && !bytes.empty();
  }
};

namespace SurfacePixelConversionDetail {

inline float finiteOrZero(float value) noexcept {
  return std::isfinite(value) ? value : 0.0f;
}

inline float decodeLegacySrgbBoundary(float value) noexcept {
  value = finiteOrZero(value);
  if (value < 0.0f || value > 1.0f) {
    return value;
  }
  return ColorTransferFunction::srgbToLinear(value);
}

inline float decodeToLinear(float value,
                            const SurfaceColorDescriptor &descriptor) noexcept {
  value = finiteOrZero(value);
  if (!descriptor.transferKnown) {
    return decodeLegacySrgbBoundary(value);
  }
  if (descriptor.transfer == TransferFunction::Linear) {
    return value;
  }
  return ColorTransferFunction::decode(value, descriptor.transfer);
}

inline bool supportedPrimaries(
    const SurfaceColorDescriptor &descriptor) noexcept {
  return descriptor.primaries == SurfaceColorPrimaries::Unknown ||
         descriptor.primaries == SurfaceColorPrimaries::SRGB_Rec709_D65;
}

} // namespace SurfacePixelConversionDetail

inline SurfacePixelBuffer convertSurfacePixels(
    const float *sourceFloat, const std::uint8_t *sourceByte, int width,
    int height, const SurfaceColorDescriptor &sourceDescriptor,
    SurfacePixelTarget target) {
  SurfacePixelBuffer output;
  if (width <= 0 || height <= 0 || (!sourceFloat && !sourceByte)) {
    return output;
  }

  if (!SurfacePixelConversionDetail::supportedPrimaries(sourceDescriptor)) {
    return output;
  }

  const bool sourceIsRgba =
      sourceDescriptor.channelOrder == SurfaceChannelOrder::RGBA;
  const bool sourceIsBgra =
      sourceDescriptor.channelOrder == SurfaceChannelOrder::BGRA ||
      sourceDescriptor.channelOrder == SurfaceChannelOrder::Unknown;
  if (!sourceIsRgba && !sourceIsBgra) {
    return output;
  }

  output.width = static_cast<std::uint32_t>(width);
  output.height = static_cast<std::uint32_t>(height);
  const bool outputFloat = target == SurfacePixelTarget::Rgba32LinearStraight;
  output.rowStride = static_cast<std::uint64_t>(output.width) * 4u *
                     (outputFloat ? sizeof(float) : sizeof(std::uint8_t));
  output.bytes.resize(static_cast<std::size_t>(output.rowStride) *
                      static_cast<std::size_t>(output.height));
  output.descriptor = outputFloat
                          ? SurfaceColorDescriptor::linearStraightRgba32Float()
                          : SurfaceColorDescriptor::encodedSrgbRgba8Straight();

  const bool sourcePremultiplied =
      sourceDescriptor.alphaMode == SurfaceAlphaMode::Premultiplied;
  const bool sourceOpaque =
      sourceDescriptor.alphaMode == SurfaceAlphaMode::Opaque;
  constexpr float alphaEpsilon = 1.0e-6f;
  const std::size_t pixelCount =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

  for (std::size_t index = 0; index < pixelCount; ++index) {
    float channels[4]{};
    if (sourceFloat) {
      for (int channel = 0; channel < 4; ++channel) {
        channels[channel] = sourceFloat[index * 4u + channel];
      }
    } else {
      for (int channel = 0; channel < 4; ++channel) {
        channels[channel] =
            static_cast<float>(sourceByte[index * 4u + channel]) / 255.0f;
      }
    }

    const float alpha =
        sourceOpaque
            ? 1.0f
            : std::clamp(
                  SurfacePixelConversionDetail::finiteOrZero(channels[3]),
                  0.0f, 1.0f);
    float red = sourceIsRgba ? channels[0] : channels[2];
    float green = channels[1];
    float blue = sourceIsRgba ? channels[2] : channels[0];

    if (sourcePremultiplied) {
      if (alpha > alphaEpsilon) {
        red /= alpha;
        green /= alpha;
        blue /= alpha;
      } else {
        red = 0.0f;
        green = 0.0f;
        blue = 0.0f;
      }
    }

    if (alpha <= alphaEpsilon) {
      red = 0.0f;
      green = 0.0f;
      blue = 0.0f;
    }

    red = SurfacePixelConversionDetail::decodeToLinear(red, sourceDescriptor);
    green =
        SurfacePixelConversionDetail::decodeToLinear(green, sourceDescriptor);
    blue =
        SurfacePixelConversionDetail::decodeToLinear(blue, sourceDescriptor);

    if (outputFloat) {
      const float converted[4] = {red, green, blue, alpha};
      std::memcpy(output.bytes.data() + index * sizeof(converted), converted,
                  sizeof(converted));
    } else {
      const float encoded[4] = {
          ColorTransferFunction::linearToSRGB(red),
          ColorTransferFunction::linearToSRGB(green),
          ColorTransferFunction::linearToSRGB(blue), alpha};
      for (int channel = 0; channel < 4; ++channel) {
        output.bytes[index * 4u + static_cast<std::size_t>(channel)] =
            static_cast<std::uint8_t>(
                std::clamp(encoded[channel], 0.0f, 1.0f) * 255.0f + 0.5f);
      }
    }
  }

  return output;
}

} // namespace ArtifactCore
