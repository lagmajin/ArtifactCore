module;

#include <QDebug>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module MediaImageFrameDecoder;





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
                             codecContext_->width, codecContext_->height, AV_PIX_FMT_RGBA,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
    return swsCtx_ != nullptr;
}

QImage MediaImageFrameDecoder::decodeFrame(AVPacket* packet) {
    if (!codecContext_ || !swsCtx_) return QImage();

    AVFrame* frame = av_frame_alloc();
    if (!frame) return QImage();

    int ret = avcodec_send_packet(codecContext_, packet);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        qWarning() << "[MediaImageFrameDecoder] avcodec_send_packet failed:" << ret;
        av_frame_free(&frame);
        return QImage();
    }

    ret = avcodec_receive_frame(codecContext_, frame);
    if (ret < 0) {
        // EAGAIN (needs more packets) or EOF are normal, don't warn for them
        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            qWarning() << "[MediaImageFrameDecoder] avcodec_receive_frame failed:" << ret;
        }
        av_frame_free(&frame);
        return QImage(); // Needs more packets
    }

    // Use RGBX8888 to ensure the 4th byte is ignored as alpha, guaranteeing the video is fully opaque.
    QImage img(codecContext_->width, codecContext_->height, QImage::Format_RGBX8888);
    uint8_t* dst[4] = { img.bits(), nullptr, nullptr, nullptr };
    int dstLinesize[4] = { static_cast<int>(img.bytesPerLine()), 0, 0, 0 };
    
    int h = sws_scale(swsCtx_, frame->data, frame->linesize, 0, codecContext_->height, dst, dstLinesize);
    if (h <= 0) {
        qWarning() << "[MediaImageFrameDecoder] sws_scale failed or returned 0 height.";
        av_frame_free(&frame);
        return QImage();
    }

    av_frame_free(&frame);

    // Debug: Save the very first decoded frame to disk to prove it works
    static bool firstFrameSaved = false;
    if (!firstFrameSaved && !img.isNull()) {
        QString debugPath = "debug_first_frame.png";
        if (img.save(debugPath)) {
            qDebug() << "[MediaImageFrameDecoder] SUCCESS: First frame saved to" << debugPath;
        } else {
            qDebug() << "[MediaImageFrameDecoder] FAILED to save debug frame to" << debugPath;
        }
        firstFrameSaved = true;
    }

    return img;
}

void MediaImageFrameDecoder::flush() {
    if (codecContext_) avcodec_flush_buffers(codecContext_);
}

} // namespace ArtifactCore