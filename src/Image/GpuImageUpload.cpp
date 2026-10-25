module;
#include <cstdint>
#include <limits>

module Image.GpuImageUpload;

namespace ArtifactCore {

GpuImageUploadBuffer makeGpuImageUploadBuffer(const ImageSurfaceView& view) {
    return makeGpuImageUploadBuffer(view, GpuImageChannelOrder::Bgra);
}

GpuImageUploadBuffer makeGpuImageUploadBuffer(
    const ImageSurfaceView& view, GpuImageChannelOrder targetOrder) {
    GpuImageUploadBuffer result;
    makeGpuImageUploadBuffer(view, targetOrder, result);
    return result;
}

bool makeGpuImageUploadBuffer(const ImageSurfaceView& view,
                              GpuImageUploadBuffer& result) {
    return makeGpuImageUploadBuffer(view, GpuImageChannelOrder::Bgra, result);
}

bool makeGpuImageUploadBuffer(const ImageSurfaceView& view,
                              GpuImageChannelOrder targetOrder,
                              GpuImageUploadBuffer& result) {
    result.bytes.clear();
    result.sourceData = nullptr;
    result.width = 0;
    result.height = 0;
    result.rowStride = 0;
    result.descriptor = SurfaceColorDescriptor::unknown();
    if (!view.isValid()) {
        return false;
    }

    const auto minimumRowStride = view.precision == SurfacePrecision::Float16
        ? static_cast<std::size_t>(view.width) * 8u
        : static_cast<std::size_t>(view.width) * 16u;
    const auto expectedStorage = view.precision == SurfacePrecision::Float16
        ? SurfacePixelStorage::RGBA16Float
        : SurfacePixelStorage::RGBA32Float;
    if (view.rowStride < minimumRowStride ||
        view.descriptor.storage != expectedStorage ||
        view.descriptor.channelOrder != SurfaceChannelOrder::BGRA ||
        targetOrder != GpuImageChannelOrder::Bgra) {
        return false;
    }

    const std::size_t rowBytes = minimumRowStride;
    if (static_cast<std::size_t>(view.height) >
        (std::numeric_limits<std::size_t>::max)() / rowBytes) {
        return false;
    }
    const auto maxSize = (std::numeric_limits<std::size_t>::max)();
    if (static_cast<std::size_t>(view.height - 1) >
        (maxSize - rowBytes) / view.rowStride) {
        return false;
    }
    result.width = view.width;
    result.height = view.height;
    result.rowStride = rowBytes;
    result.format = view.precision == SurfacePrecision::Float16
                        ? GpuImageFormat::Rgba16Float
                        : GpuImageFormat::Rgba32Float;
    result.descriptor = view.descriptor;
    result.descriptor.channelOrder = SurfaceChannelOrder::BGRA;
    result.sourceData = view.data;
    result.rowStride = view.rowStride;
    return result.isValid();
}

}
