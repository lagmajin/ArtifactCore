module;
#define QT_NO_KEYWORDS
#include <QDebug>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

module MediaAudioDecoder;

import std;

namespace ArtifactCore {

MediaAudioDecoder::MediaAudioDecoder() {}

MediaAudioDecoder::~MediaAudioDecoder() {
    if (swrCtx_) swr_free(&swrCtx_);
    if (codecContext_) avcodec_free_context(&codecContext_);
}

bool MediaAudioDecoder::initialize(AVCodecParameters* codecParams) {
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

    swrCtx_ = swr_alloc();
    if (!swrCtx_) return false;

    av_opt_set_chlayout(swrCtx_, "in_chlayout", &codecContext_->ch_layout, 0);
    av_opt_set_int(swrCtx_, "in_sample_rate", codecContext_->sample_rate, 0);
    av_opt_set_sample_fmt(swrCtx_, "in_sample_fmt", codecContext_->sample_fmt, 0);
    AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
    av_opt_set_chlayout(swrCtx_, "out_chlayout", &out_layout, 0);
    av_opt_set_int(swrCtx_, "out_sample_rate", 44100, 0);
    av_opt_set_sample_fmt(swrCtx_, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if (swr_init(swrCtx_) < 0) {
        swr_free(&swrCtx_);
        return false;
    }

    return true;
}

QByteArray MediaAudioDecoder::decodeFrame(AVPacket* packet) {
    if (!codecContext_ || !swrCtx_) return QByteArray();

    AVFrame* frame = av_frame_alloc();
    if (!frame) return QByteArray();

    int ret = avcodec_send_packet(codecContext_, packet);
    if (ret < 0) {
        av_frame_free(&frame);
        return QByteArray();
    }

    ret = avcodec_receive_frame(codecContext_, frame);
    if (ret < 0) {
        av_frame_free(&frame);
        return QByteArray();
    }

    int outSamples = swr_get_delay(swrCtx_, 44100) + frame->nb_samples;
    uint8_t* outBuffer = new uint8_t[outSamples * 2 * 2]; // S16 stereo
    uint8_t* outPtr[1] = { outBuffer };
    int converted = swr_convert(swrCtx_, outPtr, outSamples, (const uint8_t**)frame->data, frame->nb_samples);
    QByteArray audioData((char*)outBuffer, converted * 2 * 2);
    delete[] outBuffer;

    av_frame_free(&frame);
    return audioData;
}

void MediaAudioDecoder::flush() {
    if (codecContext_) avcodec_flush_buffers(codecContext_);
}

} // namespace ArtifactCore