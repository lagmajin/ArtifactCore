module;
#include <utility>

#include <QDebug>
#include <QImage>
#include <QtGlobal>
#include <cstring>
#include <cstdint>
#include <memory>
#include <variant>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vulkan.h>
#include <libavutil/pixdesc.h>
}


module MediaImageFrameDecoder;

import Video.VideoFrame;


namespace ArtifactCore {

namespace {
QString ffmpegErrorString(int err) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, err);
    return QString::fromLatin1(buffer);
}

AVDictionary* makeSingleThreadCodecOpenOptions() {
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "threads", "1", 0);
    av_dict_set(&opts, "thread_type", "0", 0);
    return opts;
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
    out.strideBytes = rgba.bytesPerLine();
    out.bytes.resize(static_cast<size_t>(out.strideBytes) * static_cast<size_t>(out.meta.height));
    for (int y = 0; y < out.meta.height; ++y) {
        const std::uint8_t* src = rgba.constScanLine(y);
        std::uint8_t* dst = out.bytes.data() + static_cast<size_t>(y) * static_cast<size_t>(out.strideBytes);
        std::memcpy(dst, src, static_cast<size_t>(out.strideBytes));
    }
    return out;
}

CpuVideoFrame makeCpuVideoFrameFromFrame(AVFrame* frame, SwsContext* swsCtx, int width, int height, int64_t pts) {
    CpuVideoFrame out;
    if (!frame || !swsCtx || width <= 0 || height <= 0) {
        return out;
    }

    out.meta.width = width;
    out.meta.height = height;
    out.meta.pts = pts;
    out.meta.color.colorSpace = static_cast<int>(frame->colorspace);
    out.meta.color.colorRange = static_cast<int>(frame->color_range);
    out.meta.color.colorPrimaries = static_cast<int>(frame->color_primaries);
    out.meta.color.colorTransfer = static_cast<int>(frame->color_trc);
    out.meta.pixelFormat = VideoFramePixelFormat::RGB24;
    out.strideBytes = width * 3;
    out.bytes.resize(static_cast<size_t>(out.strideBytes) * static_cast<size_t>(height));

    uint8_t* dst[4] = {};
    int dstLinesize[4] = {};
    dst[0] = out.bytes.data();
    dstLinesize[0] = out.strideBytes;
    sws_scale(swsCtx, frame->data, frame->linesize, 0, height, dst, dstLinesize);
    return out;
}

CpuVideoFrame makeCpuVideoFrameFromDownloadedFrame(AVFrame* frame, int64_t pts)
{
    CpuVideoFrame out;
    if (!frame || frame->width <= 0 || frame->height <= 0) {
        return out;
    }

    SwsContext* swsCtx = sws_getContext(frame->width, frame->height,
                                        static_cast<AVPixelFormat>(frame->format),
                                        frame->width, frame->height,
                                        AV_PIX_FMT_RGB24,
                                        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsCtx) {
        return out;
    }

    out.meta.width = frame->width;
    out.meta.height = frame->height;
    out.meta.pts = pts;
    out.meta.pixelFormat = VideoFramePixelFormat::RGB24;
    out.strideBytes = frame->width * 3;
    out.bytes.resize(static_cast<size_t>(out.strideBytes) * static_cast<size_t>(frame->height));

    uint8_t* dst[4] = {};
    int dstLinesize[4] = {};
    dst[0] = out.bytes.data();
    dstLinesize[0] = out.strideBytes;
    sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, dst, dstLinesize);
    sws_freeContext(swsCtx);
    return out;
}

CpuVideoFrame downloadHwFrameToCpuVideoFrame(AVFrame* hwFrame, int64_t pts)
{
    CpuVideoFrame out;
    if (!hwFrame) {
        return out;
    }

    AVFrame* cpuFrame = av_frame_alloc();
    if (!cpuFrame) {
        return out;
    }

    const int transferResult = av_hwframe_transfer_data(cpuFrame, hwFrame, 0);
    if (transferResult < 0) {
        qWarning() << "[MediaImageFrameDecoder] Vulkan frame download failed:"
                   << ffmpegErrorString(transferResult);
        av_frame_free(&cpuFrame);
        return out;
    }

    out = makeCpuVideoFrameFromDownloadedFrame(cpuFrame, pts);
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
    if (!frame || frame->format != AV_PIX_FMT_VULKAN || !frame->data[0]) {
        return out;
    }

    const auto* vkFrame = reinterpret_cast<const AVVkFrame*>(frame->data[0]);
    if (!vkFrame || vkFrame->img[0] == VK_NULL_HANDLE) {
        return out;
    }

    const auto frameRef = std::shared_ptr<void>(
        av_frame_clone(frame),
        [](void* ptr) {
            AVFrame* cloned = static_cast<AVFrame*>(ptr);
            if (cloned) {
                av_frame_free(&cloned);
            }
        });
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
    const QString value = qEnvironmentVariable("ARTIFACT_ENABLE_VULKAN_VIDEO_DIRECT")
                              .trimmed()
                              .toLower();
    return value == QStringLiteral("1") ||
           value == QStringLiteral("true") ||
           value == QStringLiteral("on") ||
           value == QStringLiteral("yes");
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

bool MediaImageFrameDecoder::initialize(AVCodecParameters* codecParams) {
    if (!codecParams) {
        qWarning() << "[MediaImageFrameDecoder] initialize failed: null codecParams";
        return false;
    }

    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }
    if (codecContext_) {
        avcodec_free_context(&codecContext_);
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

    // Avoid auto-spawning multiple FFmpeg decode workers for single-frame probes.
    codecContext_->thread_count = 1;
    codecContext_->thread_type = 0;
    codecContext_->get_format = chooseBestDecoderPixelFormat;

    if (hwDeviceCtx_) {
        codecContext_->hw_device_ctx = av_buffer_ref(hwDeviceCtx_);
    }

    AVDictionary* codecOpts = makeSingleThreadCodecOpenOptions();
    int ret = avcodec_open2(codecContext_, codec, &codecOpts);
    av_dict_free(&codecOpts);
    if (ret < 0) {
        qWarning() << "[MediaImageFrameDecoder] initialize failed: could not open codec"
                   << "codec=" << codec->name
                   << "reason=" << ffmpegErrorString(ret);
        avcodec_free_context(&codecContext_);
        return false;
    }

    swsCtx_ = sws_getContext(codecContext_->width, codecContext_->height, codecContext_->pix_fmt,
                             codecContext_->width, codecContext_->height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
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

        if (frame->format == AV_PIX_FMT_VULKAN || !swsCtx_) {
            av_frame_unref(frame);
            continue;
        }

        QImage img(codecContext_->width, codecContext_->height, QImage::Format_RGB888);
        uint8_t* dst[4];
        int dstLinesize[4];
        av_image_fill_arrays(dst, dstLinesize, img.bits(), AV_PIX_FMT_RGB24, img.width(), img.height(), 1);
        sws_scale(swsCtx_, frame->data, frame->linesize, 0, codecContext_->height, dst, dstLinesize);
        result = img;
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
            if (!directVulkanVideoFramesEnabled() || !canPresentGpuFrameDirectly(out)) {
                CpuVideoFrame cpu = downloadHwFrameToCpuVideoFrame(frame, pts);
                av_frame_unref(frame);
                av_frame_free(&frame);
                return cpu.isValid() ? DecodedVideoFrame{std::move(cpu)}
                                     : DecodedVideoFrame{std::monostate{}};
            }
            av_frame_unref(frame);
            av_frame_free(&frame);
            return out;
        }
        if (!swsCtx_) {
            av_frame_unref(frame);
            av_frame_free(&frame);
            return std::monostate{};
        }
        CpuVideoFrame out = makeCpuVideoFrameFromFrame(frame, swsCtx_, codecContext_->width, codecContext_->height, pts);
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

    if (frame->format == AV_PIX_FMT_VULKAN || !swsCtx_) {
        av_frame_free(&frame);
        return QImage();
    }

    QImage img(codecContext_->width, codecContext_->height, QImage::Format_RGB888);
    uint8_t* dst[4];
    int dstLinesize[4];
    av_image_fill_arrays(dst, dstLinesize, img.bits(), AV_PIX_FMT_RGB24, img.width(), img.height(), 1);
    sws_scale(swsCtx_, frame->data, frame->linesize, 0, codecContext_->height, dst, dstLinesize);

    av_frame_free(&frame);
    return img;
}

DecodedVideoFrame MediaImageFrameDecoder::receiveFrameRaw() {
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
            CpuVideoFrame cpu = downloadHwFrameToCpuVideoFrame(frame, pts);
            av_frame_unref(frame);
            av_frame_free(&frame);
            return cpu.isValid() ? DecodedVideoFrame{std::move(cpu)}
                                 : DecodedVideoFrame{std::monostate{}};
        }
        av_frame_unref(frame);
        av_frame_free(&frame);
        return out;
    }
    if (!swsCtx_) {
        av_frame_unref(frame);
        av_frame_free(&frame);
        return std::monostate{};
    }
    CpuVideoFrame out = makeCpuVideoFrameFromFrame(frame, swsCtx_, codecContext_->width, codecContext_->height, pts);
    av_frame_unref(frame);
    av_frame_free(&frame);
    return out;
}

void MediaImageFrameDecoder::flush() {
    if (codecContext_) avcodec_flush_buffers(codecContext_);
}

} // namespace ArtifactCore
