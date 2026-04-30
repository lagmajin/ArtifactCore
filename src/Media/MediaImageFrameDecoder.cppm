module;
#include <utility>

#include <QDebug>
#include <QImage>
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}


module MediaImageFrameDecoder;


import Video.VideoFrame;
import std;


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
}

MediaImageFrameDecoder::MediaImageFrameDecoder() {}

MediaImageFrameDecoder::~MediaImageFrameDecoder() {
    if (swsCtx_) sws_freeContext(swsCtx_);
    if (codecContext_) avcodec_free_context(&codecContext_);
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
    if (!swsCtx_) {
        qWarning() << "[MediaImageFrameDecoder] initialize failed: could not create sws context"
                   << "pix_fmt=" << codecContext_->pix_fmt
                   << "size=" << codecContext_->width << "x" << codecContext_->height
                   << "reason=unsupported format conversion";
        return false;
    }

    qDebug() << "[MediaImageFrameDecoder] initialized successfully"
             << "codec=" << codec->name
             << "size=" << codecContext_->width << "x" << codecContext_->height
             << "pix_fmt=" << codecContext_->pix_fmt;
    return true;
}

QImage MediaImageFrameDecoder::decodeFrame(AVPacket* packet) {
    if (!codecContext_ || !swsCtx_) return QImage();

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
    if (!codecContext_ || !swsCtx_) {
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
    if (!codecContext_ || !swsCtx_) return QImage();

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

    QImage img(codecContext_->width, codecContext_->height, QImage::Format_RGB888);
    uint8_t* dst[4];
    int dstLinesize[4];
    av_image_fill_arrays(dst, dstLinesize, img.bits(), AV_PIX_FMT_RGB24, img.width(), img.height(), 1);
    sws_scale(swsCtx_, frame->data, frame->linesize, 0, codecContext_->height, dst, dstLinesize);

    av_frame_free(&frame);
    return img;
}

DecodedVideoFrame MediaImageFrameDecoder::receiveFrameRaw() {
    if (!codecContext_ || !swsCtx_) {
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
    CpuVideoFrame out = makeCpuVideoFrameFromFrame(frame, swsCtx_, codecContext_->width, codecContext_->height, pts);
    av_frame_free(&frame);
    return out;
}

void MediaImageFrameDecoder::flush() {
    if (codecContext_) avcodec_flush_buffers(codecContext_);
}

} // namespace ArtifactCore
