module;
#include <cstring>
#include <cstdint>

module Image.GpuImageUpload;

import Core.Parallel;

namespace ArtifactCore {
namespace {

void swapRgbaBgra(std::uint8_t* dst, const std::uint8_t* src,
                  std::size_t pixelCount, std::size_t channelBytes) {
    for (std::size_t i = 0; i < pixelCount; ++i) {
        const auto* in = src + i * channelBytes * 4u;
        auto* out = dst + i * channelBytes * 4u;
        std::memcpy(out + channelBytes * 0u, in + channelBytes * 2u, channelBytes);
        std::memcpy(out + channelBytes * 1u, in + channelBytes * 1u, channelBytes);
        std::memcpy(out + channelBytes * 2u, in + channelBytes * 0u, channelBytes);
        std::memcpy(out + channelBytes * 3u, in + channelBytes * 3u, channelBytes);
    }
}

}

GpuImageUploadBuffer makeGpuImageUploadBuffer(const ImageSurfaceView& view) {
    GpuImageUploadBuffer result;
    const auto minimumRowStride = view.precision == SurfacePrecision::Float16
        ? static_cast<std::size_t>(view.width) * 8u
        : static_cast<std::size_t>(view.width) * 16u;
    if (!view.isValid() || view.rowStride < minimumRowStride) {
        return result;
    }

    const std::size_t channelBytes =
        view.precision == SurfacePrecision::Float16 ? sizeof(std::uint16_t)
                                                    : sizeof(float);
    const std::size_t rowBytes = static_cast<std::size_t>(view.width) * 4u * channelBytes;
    const std::size_t pixelCount = static_cast<std::size_t>(view.width) *
                                   static_cast<std::size_t>(view.height);
    result.width = view.width;
    result.height = view.height;
    result.rowStride = rowBytes;
    result.format = view.precision == SurfacePrecision::Float16
                        ? GpuImageFormat::Rgba16Float
                        : GpuImageFormat::Rgba32Float;
    result.descriptor = view.descriptor;
    result.descriptor.channelOrder = SurfaceChannelOrder::RGBA;
    result.bytes.resize(rowBytes * static_cast<std::size_t>(view.height));

    const auto* source = static_cast<const std::uint8_t*>(view.data);
    if (view.descriptor.channelOrder == SurfaceChannelOrder::BGRA) {
        Parallel::For(0, view.height, view.width * view.height, [&](int y) {
            swapRgbaBgra(result.bytes.data() + static_cast<std::size_t>(y) * rowBytes,
                         source + static_cast<std::size_t>(y) * view.rowStride,
                         static_cast<std::size_t>(view.width), channelBytes);
        });
    } else {
        Parallel::For(0, view.height, view.width * view.height, [&](int y) {
            std::memcpy(result.bytes.data() + static_cast<std::size_t>(y) * rowBytes,
                        source + static_cast<std::size_t>(y) * view.rowStride,
                        rowBytes);
        });
    }
    return result;
}

}
