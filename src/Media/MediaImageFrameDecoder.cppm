module;

#include <QDebug>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

module MediaImageFrameDecoder;

import std;

namespace ArtifactCore {

MediaImageFrameDecoder::MediaImageFrameDecoder() {}

MediaImageFrameDecoder::~MediaImageFrameDecoder() {
    if (swsCtx_) sws_freeContext(swsCtx_);
    if (codecContext_) avcodec_free_context(&codecContext_);
}

bool MediaImageFrameDecoder::initialize(AVCodecParameters* codecParams) {
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) return false;

    codecContext_ = avcodec_alloc_context3(codec);
    if (!codecContext_) return false;

    if (avcodec_parameters_to_context(codecContext_, codecParams) < 0) {
        avcodec_free_context(&codecContext_);
        return false;
    }

    if (avcodec_open2(codecContext_, codec, nullptr) < 0) {
        avcodec_free_context(&codecContext_);
        return false;
    }

    swsCtx_ = sws_getContext(codecContext_->width, codecContext_->height, codecContext_->pix_fmt,
                             codecContext_->width, codecContext_->height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
    return swsCtx_ != nullptr;
}

QImage MediaImageFrameDecoder::decodeFrame(AVPacket* packet) {
    if (!codecContext_ || !swsCtx_) return QImage();

    AVFrame* frame = av_frame_alloc();
    if (!frame) return QImage();

    int ret = avcodec_send_packet(codecContext_, packet);
    if (ret < 0) {
        av_frame_free(&frame);
        return QImage();
    }

    ret = avcodec_receive_frame(codecContext_, frame);
    if (ret < 0) {
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

void MediaImageFrameDecoder::flush() {
    if (codecContext_) avcodec_flush_buffers(codecContext_);
}

} // namespace ArtifactCore