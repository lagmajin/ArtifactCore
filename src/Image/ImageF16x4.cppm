module;
#include <algorithm>
#include <cmath>
#include <cstring>

module Image.ImageF16x4;

namespace ArtifactCore {
namespace {

float halfToFloat(std::uint16_t value) noexcept {
    const std::uint32_t sign = (value & 0x8000u) << 16u;
    const std::uint32_t exponent = (value >> 10u) & 0x1fu;
    const std::uint32_t mantissa = value & 0x3ffu;
    std::uint32_t bits = sign;
    if (exponent == 0) {
        if (mantissa != 0) {
            float result = std::ldexp(static_cast<float>(mantissa), -24);
            return (sign != 0) ? -result : result;
        }
    } else if (exponent == 0x1fu) {
        bits |= 0x7f800000u | (mantissa << 13u);
    } else {
        bits |= ((exponent + 112u) << 23u) | (mantissa << 13u);
    }
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

std::uint16_t floatToHalf(float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const std::uint32_t sign = (bits >> 16u) & 0x8000u;
    const std::uint32_t exponent = (bits >> 23u) & 0xffu;
    const std::uint32_t mantissa = bits & 0x7fffffu;
    if (exponent == 0xffu) return static_cast<std::uint16_t>(sign | 0x7c00u);
    const int halfExponent = static_cast<int>(exponent) - 127 + 15;
    if (halfExponent >= 31) return static_cast<std::uint16_t>(sign | 0x7c00u);
    if (halfExponent <= 0) {
        if (halfExponent < -10) return static_cast<std::uint16_t>(sign);
        const auto shifted = (mantissa | 0x800000u) >> (1 - halfExponent);
        return static_cast<std::uint16_t>(sign | ((shifted + 0x1000u) >> 13u));
    }
    return static_cast<std::uint16_t>(sign |
        (static_cast<std::uint32_t>(halfExponent) << 10u) |
        ((mantissa + 0x1000u) >> 13u));
}

}

ImageF16x4::ImageF16x4(int width, int height, SurfaceColorDescriptor descriptor)
    : width_(std::max(0, width)), height_(std::max(0, height)),
      descriptor_(descriptor), pixels_(static_cast<std::size_t>(width_) *
                                        static_cast<std::size_t>(height_) * 4u) {}

ImageF16x4 ImageF16x4::fromF32(const ImageF32x4_RGBA& source) {
    ImageF16x4 result(source.width(), source.height(), source.colorDescriptor());
    const float* input = source.rgba32fData();
    if (!input) return result;
    for (std::size_t i = 0; i < result.pixels_.size(); ++i)
        result.pixels_[i] = floatToHalf(input[i]);
    return result;
}

ImageF32x4_RGBA ImageF16x4::toF32() const {
    ImageF32x4_RGBA result;
    if (width_ <= 0 || height_ <= 0) return result;
    std::vector<float> pixels(pixels_.size());
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = halfToFloat(pixels_[i]);
    result.setFromRGBA32F(pixels.data(), width_, height_, descriptor_);
    return result;
}

ImageF16x4 ImageF16x4::toCanonicalRGBA16FC4() const {
    if (descriptor_.channelOrder != SurfaceChannelOrder::BGRA) return *this;
    ImageF16x4 result(width_, height_, descriptor_);
    result.descriptor_.channelOrder = SurfaceChannelOrder::RGBA;
    for (std::size_t i = 0; i < pixels_.size(); i += 4) {
        result.pixels_[i + 0] = pixels_[i + 2];
        result.pixels_[i + 1] = pixels_[i + 1];
        result.pixels_[i + 2] = pixels_[i + 0];
        result.pixels_[i + 3] = pixels_[i + 3];
    }
    return result;
}

ImageF16x4 ImageF16x4::toCanonicalBGRA16FC4() const {
    if (descriptor_.channelOrder == SurfaceChannelOrder::BGRA) return *this;
    ImageF16x4 result(width_, height_, descriptor_);
    result.descriptor_.channelOrder = SurfaceChannelOrder::BGRA;
    for (std::size_t i = 0; i < pixels_.size(); i += 4) {
        result.pixels_[i + 0] = pixels_[i + 2];
        result.pixels_[i + 1] = pixels_[i + 1];
        result.pixels_[i + 2] = pixels_[i + 0];
        result.pixels_[i + 3] = pixels_[i + 3];
    }
    return result;
}

ImageSurfaceView ImageF16x4::surfaceView() const noexcept {
    return {pixels_.empty() ? nullptr : pixels_.data(), width_, height_,
            static_cast<std::size_t>(width_) * 4u * sizeof(std::uint16_t),
            SurfacePrecision::Float16, descriptor_};
}

}
