module;
#include <utility>

#include <QDebug>
#include <QImage>
#include <QtGlobal>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <variant>
#include "MediaImageFrameDecoderFFmpegHeaders.h"


module MediaImageFrameDecoder;

import Video.VideoFrame;
import Memory.SharedPtr;


namespace ArtifactCore {

namespace {
QString ffmpegErrorString(int err) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, err);
    return QString::fromLatin1(buffer);
}

QImage makeQImageFromCpuVideoFrame(const CpuVideoFrame& frame) {
    if (!frame.isValid() || frame.meta.pixelFormat != VideoFramePixelFormat::RGB24) {
        return {};
    }
    QImage image(frame.meta.width, frame.meta.height, QImage::Format_RGB888);
    if (image.isNull()) {
        return {};
    }
    const int rowBytes = std::min<int>(frame.strideBytes, image.bytesPerLine());
    for (int y = 0; y < frame.meta.height; ++y) {
        std::memcpy(image.scanLine(y),
                    frame.bytes.data() + static_cast<size_t>(y) * frame.strideBytes,
                    static_cast<size_t>(rowBytes));
    }
    return image;
}

CpuVideoFrame makeCpuVideoFrameFromQImage(const QImage& source) {
    if (source.isNull()) {
        return {};
    }

    const QImage rgba = source.convertToFormat(QImage::Format_RGBA8888);
    CpuVideoFrame out;
    out.meta.width = rgba.width();
    out.meta.height = rgba.height();
    out.meta.pixelFormat = VideoFramePixelFormat::RGBA8;
    out.meta.color.colorSpace = static_cast<int>(AVCOL_SPC_RGB);
    out.meta.color.colorRange = static_cast<int>(AVCOL_RANGE_JPEG);
    out.meta.color.colorPrimaries = static_cast<int>(AVCOL_PRI_BT709);
    out.meta.color.colorTransfer = static_cast<int>(AVCOL_TRC_IEC61966_2_1);
    out.strideBytes = rgba.bytesPerLine();
    out.bytes.resize(static_cast<size_t>(out.strideBytes) * static_cast<size_t>(out.meta.height));
    for (int y = 0; y < out.meta.height; ++y) {
        const std::uint8_t* src = rgba.constScanLine(y);
        std::uint8_t* dst = out.bytes.data() + static_cast<size_t>(y) * static_cast<size_t>(out.strideBytes);
        std::memcpy(dst, src, static_cast<size_t>(out.strideBytes));
    }
    return out;
}

void configureSwsColorRange(SwsContext* context, const AVFrame* frame) {
    if (!context || !frame) {
        return;
    }
    const int* coefficients = sws_getCoefficients(SWS_CS_DEFAULT);
    if (!coefficients) {
        return;
    }
    const int sourceRange = frame->color_range == AVCOL_RANGE_MPEG ? 1 : 0;
    sws_setColorspaceDetails(context, coefficients, sourceRange,
                             coefficients, 1, 0, 1 << 16, 1 << 16);
}

int normalizedVideoRotation(int rotationDegrees) {
    const int normalized = ((rotationDegrees % 360) + 360) % 360;
    if (normalized == 90 || normalized == 180 || normalized == 270) {
        return normalized;
    }
    return 0;
}

SwsContext* cpuVideoSwsContext(AVFrame* frame, SwsContext*& cachedContext) {
    if (!frame || frame->width <= 0 || frame->height <= 0) {
        return nullptr;
    }
    cachedContext = sws_getCachedContext(
        cachedContext,
        frame->width, frame->height, static_cast<AVPixelFormat>(frame->format),
        frame->width, frame->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    return cachedContext;
}

CpuVideoFrame makeCpuVideoFrameFromFrame(AVFrame* frame, SwsContext*& swsCtx, int64_t pts, int rotationDegrees = 0) {
    CpuVideoFrame out;
    if (!frame || frame->width <= 0 || frame->height <= 0) {
        return out;
    }

    swsCtx = cpuVideoSwsContext(frame, swsCtx);
    if (!swsCtx) {
        return out;
    }
    configureSwsColorRange(swsCtx, frame);

    const int rotation = normalizedVideoRotation(rotationDegrees);
    const bool swapDimensions = rotation == 90 || rotation == 270;
    out.meta.width = frame->width;
    out.meta.height = frame->height;
    out.meta.pts = pts;
    out.meta.color.colorSpace = static_cast<int>(AVCOL_SPC_RGB);
    out.meta.color.colorRange = static_cast<int>(AVCOL_RANGE_JPEG);
    out.meta.color.colorPrimaries = static_cast<int>(frame->color_primaries);
    out.meta.color.colorTransfer = static_cast<int>(frame->color_trc);
    out.meta.pixelFormat = VideoFramePixelFormat::RGB24;
    out.strideBytes = out.meta.width * 3;
    out.bytes.resize(static_cast<size_t>(out.strideBytes) *
                     static_cast<size_t>(out.meta.height));

    uint8_t* dst[4] = {};
    int dstLinesize[4] = {};
    dst[0] = out.bytes.data();
    dstLinesize[0] = out.strideBytes;
    if (sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, dst, dstLinesize) != frame->height)
    {
        return {};
    }
    if (rotation != 0)
    {
        const int source_width = out.meta.width;
        const int source_height = out.meta.height;
        const int source_stride = out.strideBytes;
        auto source_bytes = std::move(out.bytes);
        out.meta.width = swapDimensions ? source_height : source_width;
        out.meta.height = swapDimensions ? source_width : source_height;
        out.strideBytes = out.meta.width * 3;
        out.bytes.resize(source_bytes.size());
        for (int y = 0; y < source_height; ++y)
        {
            for (int x = 0; x < source_width; ++x)
            {
                const int target_x = rotation == 90 ? source_height - 1 - y
                    : rotation == 180 ? source_width - 1 - x : y;
                const int target_y = rotation == 90 ? x
                    : rotation == 180 ? source_height - 1 - y : source_width - 1 - x;
                const auto source_offset = static_cast<size_t>(y) * source_stride + static_cast<size_t>(x) * 3;
                const auto target_offset = static_cast<size_t>(target_y) * out.strideBytes
                    + static_cast<size_t>(target_x) * 3;
                std::memcpy(out.bytes.data() + target_offset, source_bytes.data() + source_offset, 3);
            }
        }
    }
    return out;
}

CpuVideoFrame makeCpuVideoFrameFromDownloadedFrame(AVFrame* frame, int64_t pts, int rotationDegrees = 0)
{
    CpuVideoFrame out;
    if (!frame || frame->width <= 0 || frame->height <= 0) {
        return out;
    }

    SwsContext* cached = nullptr;
    out = makeCpuVideoFrameFromFrame(frame, cached, pts, rotationDegrees);
    sws_freeContext(cached);
    return out;
}

CpuVideoFrame downloadHwFrameToCpuVideoFrame(AVFrame* hwFrame, int64_t pts, int rotationDegrees = 0)
{
    CpuVideoFrame out;
    if (!hwFrame) {
        return out;
    }

    AVFrame* cpuFrame = av_frame_alloc();
    if (!cpuFrame) {
        return out;
    }

    static const bool profileTransfer = [] {
        return qEnvironmentVariableIntValue(
                   "ARTIFACT_VIDEO_PREVIEW_PROFILE_HWFRAMES") != 0;
    }();
    const auto transferStarted = profileTransfer
        ? std::chrono::steady_clock::now()
        : std::chrono::steady_clock::time_point{};
    const int transferResult = av_hwframe_transfer_data(cpuFrame, hwFrame, 0);
    const auto transferFinished = profileTransfer
        ? std::chrono::steady_clock::now()
        : std::chrono::steady_clock::time_point{};
    if (transferResult < 0) {
        qWarning() << "[MediaImageFrameDecoder] hardware frame download failed:"
                   << ffmpegErrorString(transferResult);
        av_frame_free(&cpuFrame);
        return out;
    }

    out = makeCpuVideoFrameFromDownloadedFrame(cpuFrame, pts, rotationDegrees);
    if (profileTransfer) {
        static std::atomic<uint32_t> sampleCount{0};
        const uint32_t sample = sampleCount.fetch_add(1, std::memory_order_relaxed);
        if (sample < 40) {
            const double downloadMs = std::chrono::duration<double, std::milli>(
                transferFinished - transferStarted).count();
            const double conversionMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - transferFinished).count();
            const AVD3D12VAFrame* d3d12Frame = nullptr;
            if (hwFrame->format == AV_PIX_FMT_D3D12 && hwFrame->data[0]) {
                d3d12Frame = reinterpret_cast<const AVD3D12VAFrame*>(hwFrame->data[0]);
            }
            std::fprintf(stderr,
                         "[MediaImageFrameTransferProfile] sample=%u backend=%s "
                         "size=%dx%d pts=%lld downloadMs=%.3f cpuConvertMs=%.3f "
                         "fenceValue=%llu texture=%p\n",
                         sample, hwFrame->format == AV_PIX_FMT_D3D12 ? "d3d12" : "other",
                         hwFrame->width, hwFrame->height, static_cast<long long>(pts),
                         downloadMs, conversionMs,
                         static_cast<unsigned long long>(d3d12Frame
                             ? d3d12Frame->sync_ctx.fence_value : 0),
                         d3d12Frame ? static_cast<void*>(d3d12Frame->texture) : nullptr);
        }
    }
    av_frame_free(&cpuFrame);
    return out;
}

VideoFramePixelFormat mapAvPixelFormatToVideoFramePixelFormat(AVPixelFormat format)
{
    switch (format) {
    case AV_PIX_FMT_RGBA:
        return VideoFramePixelFormat::RGBA8;
    case AV_PIX_FMT_BGRA:
        return VideoFramePixelFormat::BGRA8;
    case AV_PIX_FMT_RGB24:
        return VideoFramePixelFormat::RGB24;
    case AV_PIX_FMT_NV12:
        return VideoFramePixelFormat::NV12;
    case AV_PIX_FMT_YUV420P:
        return VideoFramePixelFormat::YUV420P;
    case AV_PIX_FMT_VULKAN:
        return VideoFramePixelFormat::VulkanImage;
    default:
        return VideoFramePixelFormat::Unknown;
    }
}

AVPixelFormat chooseBestDecoderPixelFormat(AVCodecContext* ctx, const AVPixelFormat* formats)
{
    if (!formats) {
        return AV_PIX_FMT_NONE;
    }

    if (ctx && ctx->hw_device_ctx) {
        const auto* deviceContext = reinterpret_cast<const AVHWDeviceContext*>(
            ctx->hw_device_ctx->data);
        if (deviceContext &&
            deviceContext->type == AV_HWDEVICE_TYPE_D3D11VA) {
            for (const AVPixelFormat* fmt = formats; *fmt != AV_PIX_FMT_NONE; ++fmt) {
                if (*fmt == AV_PIX_FMT_D3D11) {
                    return *fmt;
                }
            }
        }
        if (deviceContext &&
            deviceContext->type == AV_HWDEVICE_TYPE_D3D12VA) {
            for (const AVPixelFormat* fmt = formats; *fmt != AV_PIX_FMT_NONE; ++fmt) {
                if (*fmt == AV_PIX_FMT_D3D12) {
                    return *fmt;
                }
            }
        }
        for (const AVPixelFormat* fmt = formats; *fmt != AV_PIX_FMT_NONE; ++fmt) {
            if (*fmt == AV_PIX_FMT_VULKAN) {
                return *fmt;
            }
        }
    }

    return formats[0];
}

GpuVideoFrame makeGpuVideoFrameFromFrame(AVFrame* frame)
{
    GpuVideoFrame out;
    if (!frame || !frame->data[0]) {
        return out;
    }

    if (frame->format == AV_PIX_FMT_D3D12) {
        const auto* d3d12Frame = reinterpret_cast<const AVD3D12VAFrame*>(frame->data[0]);
        if (!d3d12Frame || !d3d12Frame->texture) {
            return out;
        }

        std::shared_ptr<AVFrame> clonedFrame(
            av_frame_clone(frame),
            [](AVFrame* cloned) {
                if (cloned) {
                    av_frame_free(&cloned);
                }
            });
        SharedPtr<void> frameRef(clonedFrame);
        if (!frameRef) {
            return out;
        }

        VideoFramePixelFormat pixelFormat = VideoFramePixelFormat::NV12;
        if (frame->hw_frames_ctx && frame->hw_frames_ctx->data) {
            const auto* hwFrames = reinterpret_cast<const AVHWFramesContext*>(frame->hw_frames_ctx->data);
            if (hwFrames) {
                pixelFormat = mapAvPixelFormatToVideoFramePixelFormat(hwFrames->sw_format);
            }
        }
        out.meta.width = frame->width;
        out.meta.height = frame->height;
        out.meta.pts = frame->best_effort_timestamp != AV_NOPTS_VALUE
            ? frame->best_effort_timestamp
            : frame->pts;
        out.meta.pixelFormat = pixelFormat;
        out.meta.color.colorSpace = static_cast<int>(frame->colorspace);
        out.meta.color.colorRange = static_cast<int>(frame->color_range);
        out.meta.color.colorPrimaries = static_cast<int>(frame->color_primaries);
        out.meta.color.colorTransfer = static_cast<int>(frame->color_trc);
        out.storage = VideoFrameStorageKind::D3D12Texture;
        out.lifetime = frameRef;

        D3D12VideoFrameHandle handle;
        handle.texture = d3d12Frame->texture;
        handle.fence = d3d12Frame->sync_ctx.fence;
        handle.fenceValue = d3d12Frame->sync_ctx.fence_value;
        handle.subresourceIndex = d3d12Frame->subresource_index;
        out.handle = handle;
        return out;
    }

    if (frame->format != AV_PIX_FMT_VULKAN) {
        return out;
    }

    const auto* vkFrame = reinterpret_cast<const AVVkFrame*>(frame->data[0]);
    if (!vkFrame || vkFrame->img[0] == VK_NULL_HANDLE) {
        return out;
    }

    std::shared_ptr<AVFrame> clonedFrame(
        av_frame_clone(frame),
        [](AVFrame* cloned) {
            if (cloned) {
                av_frame_free(&cloned);
            }
        });
    SharedPtr<void> frameRef(clonedFrame);
    if (!frameRef) {
        return out;
    }

    VideoFramePixelFormat pixelFormat = VideoFramePixelFormat::VulkanImage;
    std::uint32_t nativeFormat = 0;
    std::uint32_t planeCount = 0;
    if (frame->hw_frames_ctx && frame->hw_frames_ctx->data) {
        const auto* hwFrames = reinterpret_cast<const AVHWFramesContext*>(frame->hw_frames_ctx->data);
        if (hwFrames) {
            pixelFormat = mapAvPixelFormatToVideoFramePixelFormat(hwFrames->sw_format);
            if (const auto* vkFrames = reinterpret_cast<const AVVulkanFramesContext*>(hwFrames->hwctx)) {
                nativeFormat = static_cast<std::uint32_t>(vkFrames->format[0]);
            }
        }
    }

    for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
        if (vkFrame->img[i] == VK_NULL_HANDLE) {
            break;
        }
        ++planeCount;
    }

    out.meta.width = frame->width;
    out.meta.height = frame->height;
    out.meta.pts = frame->best_effort_timestamp != AV_NOPTS_VALUE
        ? frame->best_effort_timestamp
        : frame->pts;
    out.meta.pixelFormat = pixelFormat;
    out.meta.color.colorSpace = static_cast<int>(frame->colorspace);
    out.meta.color.colorRange = static_cast<int>(frame->color_range);
    out.meta.color.colorPrimaries = static_cast<int>(frame->color_primaries);
    out.meta.color.colorTransfer = static_cast<int>(frame->color_trc);
    out.storage = VideoFrameStorageKind::VulkanImage;
    out.lifetime = frameRef;

    VulkanVideoFrameHandle handle;
    handle.image = reinterpret_cast<void*>(vkFrame->img[0]);
    handle.memory = reinterpret_cast<void*>(vkFrame->mem[0]);
    handle.semaphore = reinterpret_cast<void*>(vkFrame->sem[0]);
    handle.semaphoreValue = vkFrame->sem_value[0];
    handle.nativeFormat = nativeFormat;
    handle.imageLayout = static_cast<std::uint32_t>(vkFrame->layout[0]);
    handle.planeCount = planeCount > 0 ? planeCount : 1u;
    out.handle = handle;
    return out;
}

bool canPresentGpuFrameDirectly(const GpuVideoFrame& frame)
{
    if (!frame.isValid()) {
        return false;
    }
    const auto* handle = std::get_if<VulkanVideoFrameHandle>(&frame.handle);
    if (!handle || handle->planeCount != 1u) {
        return false;
    }
    switch (frame.meta.pixelFormat) {
    case VideoFramePixelFormat::RGBA8:
    case VideoFramePixelFormat::BGRA8:
    case VideoFramePixelFormat::RGBA32F:
        return true;
    default:
        return false;
    }
}

bool directVulkanVideoFramesEnabled()
{
    // AVVkFrame exposes a timeline semaphore that must be waited and re-signaled
    // for every queue submission.  The current Diligent bridge imports only the
    // VkImage, so enabling direct presentation would race the decoder queue.
    // Keep the explicit opt-in disabled until that ownership/synchronization
    // bridge exists; hardware frames are downloaded through the safe path below.
    return false;
}

void freeHwDeviceContext(AVBufferRef*& ref)
{
    if (ref) {
        av_buffer_unref(&ref);
    }
}
}

MediaImageFrameDecoder::MediaImageFrameDecoder() {}

MediaImageFrameDecoder::~MediaImageFrameDecoder() {
    freeHwDeviceContext(hwDeviceCtx_);
    freeHwDeviceContext(frameCtx_);
    if (swsCtx_) sws_freeContext(swsCtx_);
    if (codecContext_) avcodec_free_context(&codecContext_);
}

void MediaImageFrameDecoder::setVulkanDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t queueFamilyIndex)
{
    freeHwDeviceContext(hwDeviceCtx_);
    freeHwDeviceContext(frameCtx_);

    if (!instance || !physicalDevice || !device) {
        return;
    }

    AVBufferRef* deviceRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN);
    if (!deviceRef) {
        qWarning() << "[MediaImageFrameDecoder] failed to allocate Vulkan hwdevice context";
        return;
    }

    auto* hwDevice = reinterpret_cast<AVHWDeviceContext*>(deviceRef->data);
    auto* vkDevice = reinterpret_cast<AVVulkanDeviceContext*>(hwDevice->hwctx);
    if (!vkDevice) {
        av_buffer_unref(&deviceRef);
        qWarning() << "[MediaImageFrameDecoder] failed to access Vulkan hwdevice payload";
        return;
    }

    vkDevice->inst = instance;
    vkDevice->phys_dev = physicalDevice;
    vkDevice->act_dev = device;
    vkDevice->nb_enabled_inst_extensions = 0;
    vkDevice->enabled_inst_extensions = nullptr;
    vkDevice->nb_enabled_dev_extensions = 0;
    vkDevice->enabled_dev_extensions = nullptr;
    vkDevice->nb_qf = 1;
    vkDevice->qf[0].idx = static_cast<int>(queueFamilyIndex);
    vkDevice->qf[0].num = 1;
    vkDevice->qf[0].flags = static_cast<VkQueueFlagBits>(
        VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
    vkDevice->qf[0].video_caps = static_cast<VkVideoCodecOperationFlagBitsKHR>(0);

    const int initResult = av_hwdevice_ctx_init(deviceRef);
    if (initResult < 0) {
        qWarning() << "[MediaImageFrameDecoder] Vulkan hwdevice init failed:" << ffmpegErrorString(initResult);
        av_buffer_unref(&deviceRef);
        return;
    }

    hwDeviceCtx_ = deviceRef;
}

bool MediaImageFrameDecoder::initialize(AVCodecParameters* codecParams,
                                        int displayRotationDegrees,
                                        bool enableD3D11va,
                                        bool enableVulkan,
                                        bool enableD3D12va,
                                        void* sharedD3D12Device) {
    if (!codecParams) {
        qWarning() << "[MediaImageFrameDecoder] initialize failed: null codecParams";
        return false;
    }
    displayRotationDegrees_ = normalizedVideoRotation(-displayRotationDegrees);

    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }
    if (codecContext_) {
        avcodec_free_context(&codecContext_);
    }
    d3d11vaRequested_ = enableD3D11va;
    d3d12vaRequested_ = enableD3D12va;
    bool existingHardwareDevice = false;
    if (hwDeviceCtx_ && hwDeviceCtx_->data) {
        const auto* existingDevice = reinterpret_cast<const AVHWDeviceContext*>(
            hwDeviceCtx_->data);
        existingHardwareDevice =
            existingDevice->type == AV_HWDEVICE_TYPE_D3D11VA ||
            existingDevice->type == AV_HWDEVICE_TYPE_D3D12VA ||
            existingDevice->type == AV_HWDEVICE_TYPE_VULKAN;
    }
    if (d3d11vaRequested_ || d3d12vaRequested_ || enableVulkan ||
        existingHardwareDevice) {
        freeHwDeviceContext(hwDeviceCtx_);
        freeHwDeviceContext(frameCtx_);
    }
    if (enableVulkan) {
        bool vulkanHardwareBackendAvailable = false;
        for (AVHWDeviceType type = av_hwdevice_iterate_types(
                 AV_HWDEVICE_TYPE_NONE);
             type != AV_HWDEVICE_TYPE_NONE;
             type = av_hwdevice_iterate_types(type)) {
            if (type == AV_HWDEVICE_TYPE_VULKAN) {
                vulkanHardwareBackendAvailable = true;
                break;
            }
        }
        if (!vulkanHardwareBackendAvailable) {
            std::fprintf(stderr,
                         "[MediaImageFrameDecoderProbe] Vulkan requested but "
                         "this FFmpeg build has no Vulkan hwdevice backend\n");
            std::fflush(stderr);
            enableVulkan = false;
        }
    }
    if (enableVulkan) {
        std::fprintf(stderr,
                     "[MediaImageFrameDecoderProbe] Vulkan requested codecId=%d\n",
                     codecParams->codec_id);
        std::fflush(stderr);
        const int deviceResult = av_hwdevice_ctx_create(
            &hwDeviceCtx_, AV_HWDEVICE_TYPE_VULKAN, nullptr, nullptr, 0);
        std::fprintf(stderr,
                     "[MediaImageFrameDecoderProbe] Vulkan device init result=%d "
                     "error=%s context=%p\n",
                     deviceResult,
                     deviceResult < 0 ? ffmpegErrorString(deviceResult).toUtf8().constData()
                                      : "none",
                     static_cast<void*>(hwDeviceCtx_));
        std::fflush(stderr);
        if (deviceResult < 0 || !hwDeviceCtx_) {
            qWarning() << "[MediaImageFrameDecoder] Vulkan device init failed; "
                          "using software decode:"
                       << ffmpegErrorString(deviceResult);
            enableVulkan = false;
        }
    }
    if (d3d11vaRequested_) {
        std::fprintf(stderr,
                     "[MediaImageFrameDecoderProbe] D3D11VA requested codecId=%d\n",
                     codecParams->codec_id);
        std::fflush(stderr);
    }
    if (d3d11vaRequested_) {
        const int deviceResult = av_hwdevice_ctx_create(
            &hwDeviceCtx_, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
        std::fprintf(stderr,
                     "[MediaImageFrameDecoderProbe] D3D11VA device init result=%d "
                     "context=%p\n",
                     deviceResult, static_cast<void*>(hwDeviceCtx_));
        std::fflush(stderr);
        if (deviceResult < 0 || !hwDeviceCtx_) {
            qWarning() << "[MediaImageFrameDecoder] D3D11VA device init failed; "
                          "using software decode:"
                       << ffmpegErrorString(deviceResult);
            d3d11vaRequested_ = false;
        }
    }
    if (d3d12vaRequested_) {
        std::fprintf(stderr,
                     "[MediaImageFrameDecoderProbe] D3D12VA requested codecId=%d\n",
                     codecParams->codec_id);
        std::fflush(stderr);
        int deviceResult = AVERROR(ENOMEM);
        if (sharedD3D12Device) {
            hwDeviceCtx_ = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D12VA);
            auto* deviceContext = hwDeviceCtx_
                ? reinterpret_cast<AVHWDeviceContext*>(hwDeviceCtx_->data)
                : nullptr;
            auto* d3d12Context = deviceContext
                ? reinterpret_cast<AVD3D12VADeviceContext*>(deviceContext->hwctx)
                : nullptr;
            if (d3d12Context) {
                d3d12Context->device = static_cast<ID3D12Device*>(sharedD3D12Device);
                d3d12Context->device->AddRef();
                deviceResult = av_hwdevice_ctx_init(hwDeviceCtx_);
            }
            if (deviceResult < 0) {
                freeHwDeviceContext(hwDeviceCtx_);
            }
        } else {
            deviceResult = av_hwdevice_ctx_create(
                &hwDeviceCtx_, AV_HWDEVICE_TYPE_D3D12VA, nullptr, nullptr, 0);
        }
        if (deviceResult >= 0 && hwDeviceCtx_) {
            auto* deviceContext = reinterpret_cast<AVHWDeviceContext*>(
                hwDeviceCtx_->data);
            auto* d3d12Context = reinterpret_cast<AVD3D12VADeviceContext*>(
                deviceContext->hwctx);
            // The decoded texture is read by FFmpeg's COPY queue and, on the
            // shared-device path, Diligent's DIRECT queue. Simultaneous-access
            // textures keep a COMMON layout on COPY queues; the existing
            // decoder/conversion fences still order writes before either read.
            d3d12Context->resource_flags = static_cast<D3D12_RESOURCE_FLAGS>(
                d3d12Context->resource_flags |
                D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS);
        }
        std::fprintf(stderr,
                     "[MediaImageFrameDecoderProbe] D3D12VA device init result=%d "
                     "error=%s context=%p sharedDevice=%d\n",
                     deviceResult,
                     deviceResult < 0 ? ffmpegErrorString(deviceResult).toUtf8().constData()
                                      : "none",
                     static_cast<void*>(hwDeviceCtx_),
                     sharedD3D12Device ? 1 : 0);
        std::fflush(stderr);
        if (deviceResult < 0 || !hwDeviceCtx_) {
            qWarning() << "[MediaImageFrameDecoder] D3D12VA device init failed; "
                          "using software decode:"
                       << ffmpegErrorString(deviceResult);
            d3d12vaRequested_ = false;
        }
    }
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) {
        qWarning() << "[MediaImageFrameDecoder] initialize failed: codec not found"
                   << "codec_id=" << codecParams->codec_id;
        return false;
    }

    codecContext_ = avcodec_alloc_context3(codec);
    if (!codecContext_) {
        qWarning() << "[MediaImageFrameDecoder] initialize failed: could not allocate codec context";
        return false;
    }

    if (avcodec_parameters_to_context(codecContext_, codecParams) < 0) {
        qWarning() << "[MediaImageFrameDecoder] initialize failed: could not copy codec parameters";
        avcodec_free_context(&codecContext_);
        return false;
    }

    codecContext_->get_format = chooseBestDecoderPixelFormat;

    if (hwDeviceCtx_) {
        codecContext_->hw_device_ctx = av_buffer_ref(hwDeviceCtx_);
        codecContext_->get_format = chooseBestDecoderPixelFormat;
    }

    int ret = avcodec_open2(codecContext_, codec, nullptr);
    if (ret < 0 &&
        (d3d11vaRequested_ || d3d12vaRequested_ || enableVulkan)) {
        qWarning() << "[MediaImageFrameDecoder] hardware codec open failed; "
                      "retrying software decode:"
                   << ffmpegErrorString(ret);
        avcodec_free_context(&codecContext_);
        codecContext_ = avcodec_alloc_context3(codec);
        if (!codecContext_ ||
            avcodec_parameters_to_context(codecContext_, codecParams) < 0) {
            avcodec_free_context(&codecContext_);
            codecContext_ = nullptr;
            freeHwDeviceContext(hwDeviceCtx_);
            d3d11vaRequested_ = false;
            d3d12vaRequested_ = false;
            enableVulkan = false;
            return false;
        }
        freeHwDeviceContext(hwDeviceCtx_);
        d3d11vaRequested_ = false;
        d3d12vaRequested_ = false;
        enableVulkan = false;
        ret = avcodec_open2(codecContext_, codec, nullptr);
    }
    if (ret < 0) {
        qWarning() << "[MediaImageFrameDecoder] initialize failed: could not open codec"
                   << "codec=" << codec->name
                   << "reason=" << ffmpegErrorString(ret);
        avcodec_free_context(&codecContext_);
        return false;
    }

    if ((d3d11vaRequested_ || d3d12vaRequested_ || enableVulkan) &&
        codecContext_->hw_device_ctx) {
        std::fprintf(stderr,
                     "[MediaImageFrameDecoderProbe] hardware device attached "
                     "type=%s codec=%s\n",
                     enableVulkan ? "vulkan"
                                   : d3d12vaRequested_ ? "d3d12va" : "d3d11va",
                     codec->name);
        std::fflush(stderr);
    }

    swsCtx_ = sws_getContext(
        codecContext_->width, codecContext_->height, codecContext_->pix_fmt,
        codecContext_->width, codecContext_->height,
        AV_PIX_FMT_RGB24, SWS_BILINEAR,
        nullptr, nullptr, nullptr);
    if (swsCtx_) {
    } else {
        qWarning() << "[MediaImageFrameDecoder] initialize: sws context unavailable,"
                   << "raw decode will continue"
                   << "pix_fmt=" << codecContext_->pix_fmt
                   << "size=" << codecContext_->width << "x" << codecContext_->height;
    }

    qDebug() << "[MediaImageFrameDecoder] initialized successfully"
             << "codec=" << codec->name
             << "size=" << codecContext_->width << "x" << codecContext_->height
             << "pix_fmt=" << codecContext_->pix_fmt;
    return true;
}

QImage MediaImageFrameDecoder::decodeFrame(AVPacket* packet) {
    if (!codecContext_) return QImage();

    AVFrame* frame = av_frame_alloc();
    if (!frame) return QImage();

    int ret = avcodec_send_packet(codecContext_, packet);
    if (ret < 0) {
        qWarning() << "[MediaImageFrameDecoder] send_packet failed:" << ffmpegErrorString(ret);
        av_frame_free(&frame);
        return QImage();
    }

    QImage result;
    while (true) {
        ret = avcodec_receive_frame(codecContext_, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            qWarning() << "[MediaImageFrameDecoder] receive_frame failed:" << ffmpegErrorString(ret);
            av_frame_free(&frame);
            return QImage();
        }

        lastPts_ = frame->best_effort_timestamp != AV_NOPTS_VALUE
            ? frame->best_effort_timestamp
            : frame->pts;

        if (frame->format == AV_PIX_FMT_VULKAN) {
            av_frame_unref(frame);
            continue;
        }

        if (frame->format == AV_PIX_FMT_D3D11 ||
            frame->format == AV_PIX_FMT_D3D12) {
            static std::atomic<int> d3dFrameLogCount{0};
            if (d3dFrameLogCount.fetch_add(1, std::memory_order_relaxed) < 8) {
                std::fprintf(stderr,
                             "[MediaImageFrameDecoderProbe] D3D hardware frame "
                             "received format=%d %dx%d\n",
                             frame->format, frame->width, frame->height);
                std::fflush(stderr);
            }
            CpuVideoFrame decoded = downloadHwFrameToCpuVideoFrame(
                frame, lastPts_, displayRotationDegrees_);
            result = makeQImageFromCpuVideoFrame(decoded);
            av_frame_unref(frame);
            continue;
        }

        CpuVideoFrame decoded = makeCpuVideoFrameFromFrame(
            frame, swsCtx_, lastPts_, displayRotationDegrees_);
        result = makeQImageFromCpuVideoFrame(decoded);
        av_frame_unref(frame);
    }

    av_frame_free(&frame);
    return result;
}

DecodedVideoFrame MediaImageFrameDecoder::decodeFrameRaw(AVPacket* packet) {
    if (!codecContext_) {
        return std::monostate{};
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        return std::monostate{};
    }

    int ret = avcodec_send_packet(codecContext_, packet);
    if (ret < 0) {
        av_frame_free(&frame);
        return std::monostate{};
    }

    while (true) {
        ret = avcodec_receive_frame(codecContext_, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            av_frame_free(&frame);
            return std::monostate{};
        }

        const int64_t pts = frame->best_effort_timestamp != AV_NOPTS_VALUE
            ? frame->best_effort_timestamp
            : frame->pts;
        lastPts_ = pts;
        if (frame->format == AV_PIX_FMT_VULKAN) {
            GpuVideoFrame out = makeGpuVideoFrameFromFrame(frame);
            static std::atomic<int> vulkanFrameLogCount{0};
            if (vulkanFrameLogCount.fetch_add(1, std::memory_order_relaxed) < 8) {
                std::fprintf(stderr,
                             "[MediaImageFrameDecoderProbe] Vulkan frame received "
                             "%dx%d planes=%u format=%d\n",
                             frame->width, frame->height,
                             std::get_if<VulkanVideoFrameHandle>(&out.handle)
                                 ? std::get<VulkanVideoFrameHandle>(out.handle).planeCount
                                 : 0u,
                             frame->format);
                std::fflush(stderr);
            }
            if (!directVulkanVideoFramesEnabled() || !canPresentGpuFrameDirectly(out)) {
                CpuVideoFrame cpu = downloadHwFrameToCpuVideoFrame(
                    frame, pts, displayRotationDegrees_);
                av_frame_unref(frame);
                av_frame_free(&frame);
                return cpu.isValid() ? DecodedVideoFrame{std::move(cpu)}
                                     : DecodedVideoFrame{std::monostate{}};
            }
            av_frame_unref(frame);
            av_frame_free(&frame);
            return out;
        }
        if (frame->format == AV_PIX_FMT_D3D11 ||
            frame->format == AV_PIX_FMT_D3D12) {
            static std::atomic<int> d3dFrameLogCount{0};
            if (d3dFrameLogCount.fetch_add(1, std::memory_order_relaxed) < 8) {
                std::fprintf(stderr,
                             "[MediaImageFrameDecoderProbe] D3D hardware frame "
                             "received format=%d %dx%d\n",
                             frame->format, frame->width, frame->height);
                std::fflush(stderr);
            }
            CpuVideoFrame cpu = downloadHwFrameToCpuVideoFrame(
                frame, pts, displayRotationDegrees_);
            av_frame_unref(frame);
            av_frame_free(&frame);
            return cpu.isValid() ? DecodedVideoFrame{std::move(cpu)}
                                 : DecodedVideoFrame{std::monostate{}};
        }
        CpuVideoFrame out = makeCpuVideoFrameFromFrame(
        frame, swsCtx_, pts, displayRotationDegrees_);
        av_frame_unref(frame);
        av_frame_free(&frame);
        return out;
    }

    av_frame_free(&frame);
    return std::monostate{};
}

int MediaImageFrameDecoder::sendPacket(AVPacket* packet) {
    if (!codecContext_) return AVERROR(EINVAL);
    return avcodec_send_packet(codecContext_, packet);
}

QImage MediaImageFrameDecoder::receiveFrame() {
    if (!codecContext_) return QImage();

    AVFrame* frame = av_frame_alloc();
    if (!frame) return QImage();

    int ret = avcodec_receive_frame(codecContext_, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_frame_free(&frame);
        return QImage();
    }
    if (ret < 0) {
        qWarning() << "[MediaImageFrameDecoder] receive_frame failed:" << ffmpegErrorString(ret);
        av_frame_free(&frame);
        return QImage();
    }

    lastPts_ = frame->best_effort_timestamp != AV_NOPTS_VALUE
        ? frame->best_effort_timestamp
        : frame->pts;

    if (frame->format == AV_PIX_FMT_VULKAN) {
        av_frame_free(&frame);
        return QImage();
    }

    if (frame->format == AV_PIX_FMT_D3D11 ||
        frame->format == AV_PIX_FMT_D3D12) {
        static std::atomic<int> d3dFrameLogCount{0};
        if (d3dFrameLogCount.fetch_add(1, std::memory_order_relaxed) < 8) {
            std::fprintf(stderr,
                         "[MediaImageFrameDecoderProbe] D3D hardware frame "
                         "received format=%d %dx%d\n",
                         frame->format, frame->width, frame->height);
            std::fflush(stderr);
        }
        CpuVideoFrame cpu = downloadHwFrameToCpuVideoFrame(
            frame, lastPts_, displayRotationDegrees_);
        av_frame_free(&frame);
        return cpu.isValid() ? makeQImageFromCpuVideoFrame(cpu) : QImage();
    }

    CpuVideoFrame decoded = makeCpuVideoFrameFromFrame(
        frame, swsCtx_, lastPts_, displayRotationDegrees_);
    av_frame_free(&frame);
    return makeQImageFromCpuVideoFrame(decoded);
}

DecodedVideoFrame MediaImageFrameDecoder::receiveFrameRaw() {
    return receiveFrameRaw(false);
}

DecodedVideoFrame MediaImageFrameDecoder::receiveFrameRaw(bool retainD3D12Frame) {
    if (!codecContext_) {
        return std::monostate{};
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        return std::monostate{};
    }

    int ret = avcodec_receive_frame(codecContext_, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_frame_free(&frame);
        return std::monostate{};
    }
    if (ret < 0) {
        av_frame_free(&frame);
        return std::monostate{};
    }

    const int64_t pts = frame->best_effort_timestamp != AV_NOPTS_VALUE
        ? frame->best_effort_timestamp
        : frame->pts;
    lastPts_ = pts;
    if (frame->format == AV_PIX_FMT_VULKAN) {
        GpuVideoFrame out = makeGpuVideoFrameFromFrame(frame);
        if (!directVulkanVideoFramesEnabled() || !canPresentGpuFrameDirectly(out)) {
            CpuVideoFrame cpu = downloadHwFrameToCpuVideoFrame(
                frame, pts, displayRotationDegrees_);
            av_frame_unref(frame);
            av_frame_free(&frame);
            return cpu.isValid() ? DecodedVideoFrame{std::move(cpu)}
                                 : DecodedVideoFrame{std::monostate{}};
        }
        av_frame_unref(frame);
        av_frame_free(&frame);
        return out;
    }
    if (frame->format == AV_PIX_FMT_D3D11 ||
        frame->format == AV_PIX_FMT_D3D12) {
        static std::atomic<int> d3dFrameLogCount{0};
        if (d3dFrameLogCount.fetch_add(1, std::memory_order_relaxed) < 8) {
            std::fprintf(stderr,
                         "[MediaImageFrameDecoderProbe] D3D hardware frame "
                         "received format=%d %dx%d\n",
                         frame->format, frame->width, frame->height);
            std::fflush(stderr);
        }
        if (frame->format == AV_PIX_FMT_D3D12 && retainD3D12Frame) {
            GpuVideoFrame gpu = makeGpuVideoFrameFromFrame(frame);
            av_frame_unref(frame);
            av_frame_free(&frame);
            return gpu.isValid() ? DecodedVideoFrame{std::move(gpu)}
                                 : DecodedVideoFrame{std::monostate{}};
        }
        CpuVideoFrame cpu = downloadHwFrameToCpuVideoFrame(
            frame, pts, displayRotationDegrees_);
        av_frame_unref(frame);
        av_frame_free(&frame);
        return cpu.isValid() ? DecodedVideoFrame{std::move(cpu)}
                             : DecodedVideoFrame{std::monostate{}};
    }
    CpuVideoFrame out = makeCpuVideoFrameFromFrame(
        frame, swsCtx_, pts, displayRotationDegrees_);
    av_frame_unref(frame);
    av_frame_free(&frame);
    return out;
}

void MediaImageFrameDecoder::flush() {
    if (codecContext_) avcodec_flush_buffers(codecContext_);
}

} // namespace ArtifactCore
