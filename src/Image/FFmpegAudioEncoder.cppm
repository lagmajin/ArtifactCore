module;
#include <utility>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QDir>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/error.h>
#include <libswresample/swresample.h>
}

module Media.Encoder.FFmpegAudioEncoder;

namespace {

QString audioFfmpegErrorString(int err)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, err);
    return QString::fromUtf8(buffer);
}

} // namespace

namespace ArtifactCore {

static QString g_lastAudioError;  // static methods 用のエラー

class FFmpegAudioEncoder::Impl {
public:
    QString lastError;
};

FFmpegAudioEncoder::FFmpegAudioEncoder()
    : impl_(new Impl()) {}

FFmpegAudioEncoder::~FFmpegAudioEncoder() {
    delete impl_;
}

FFmpegAudioEncoder::FFmpegAudioEncoder(FFmpegAudioEncoder&& other) noexcept
    : impl_(other.impl_) {
    other.impl_ = new Impl();
}

FFmpegAudioEncoder& FFmpegAudioEncoder::operator=(FFmpegAudioEncoder&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = new Impl();
    }
    return *this;
}

QString FFmpegAudioEncoder::lastError() const {
    return impl_->lastError;
}

FFmpegAudioEncoder::AudioInfo FFmpegAudioEncoder::probeAudioFile(const QString& filePath)
{
    AudioInfo info;
    AVFormatContext* fmtCtx = nullptr;
    int ret = avformat_open_input(&fmtCtx, filePath.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        info.errorMessage = QStringLiteral("Cannot open audio file: %1 (%2)").arg(filePath, audioFfmpegErrorString(ret));
        return info;
    }
    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        info.errorMessage = QStringLiteral("Cannot find stream info: %1").arg(audioFfmpegErrorString(ret));
        avformat_close_input(&fmtCtx);
        return info;
    }
    int audioStreamIndex = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStreamIndex < 0) {
        info.errorMessage = QStringLiteral("No audio stream found in: %1").arg(filePath);
        avformat_close_input(&fmtCtx);
        return info;
    }
    AVStream* stream = fmtCtx->streams[audioStreamIndex];
    const AVCodecParameters* par = stream->codecpar;
    info.valid = true;
    info.codec = avcodec_get_name(par->codec_id);
    info.sampleRate = par->sample_rate;
    info.channels = par->ch_layout.nb_channels;
    info.bitrate = static_cast<int>(par->bit_rate);
    if (fmtCtx->duration != AV_NOPTS_VALUE) {
        info.durationMs = fmtCtx->duration / 1000;
    }
    avformat_close_input(&fmtCtx);
    return info;
}

bool FFmpegAudioEncoder::muxAudioWithVideo(
    const QString& videoPath,
    const QString& audioPath,
    const QString& outputPath,
    const QString& audioCodec,
    int audioBitrate)
{
    // 入力ファイルを開く
    AVFormatContext* videoFmtCtx = nullptr;
    int ret = avformat_open_input(&videoFmtCtx, videoPath.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        g_lastAudioError = QStringLiteral("Failed to open video input: %1 (%2)").arg(videoPath, audioFfmpegErrorString(ret));
        return false;
    }
    avformat_find_stream_info(videoFmtCtx, nullptr);

    AVFormatContext* audioFmtCtx = nullptr;
    ret = avformat_open_input(&audioFmtCtx, audioPath.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        g_lastAudioError = QStringLiteral("Failed to open audio input: %1 (%2)").arg(audioPath, audioFfmpegErrorString(ret));
        avformat_close_input(&videoFmtCtx);
        return false;
    }
    avformat_find_stream_info(audioFmtCtx, nullptr);

    // 音声ストリーム検索
    int audioStreamIdx = av_find_best_stream(audioFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStreamIdx < 0) {
        g_lastAudioError = QStringLiteral("No audio stream found in: %1").arg(audioPath);
        avformat_close_input(&audioFmtCtx);
        avformat_close_input(&videoFmtCtx);
        return false;
    }

    AVStream* inAudioStream = audioFmtCtx->streams[audioStreamIdx];

    // 音声デコーダー作成
    const AVCodec* aDecoder = avcodec_find_decoder(inAudioStream->codecpar->codec_id);
    if (!aDecoder) {
        g_lastAudioError = QStringLiteral("No decoder found for audio codec: %1").arg(avcodec_get_name(inAudioStream->codecpar->codec_id));
        avformat_close_input(&audioFmtCtx);
        avformat_close_input(&videoFmtCtx);
        return false;
    }

    AVCodecContext* aDecoderCtx = avcodec_alloc_context3(aDecoder);
    avcodec_parameters_to_context(aDecoderCtx, inAudioStream->codecpar);
    ret = avcodec_open2(aDecoderCtx, aDecoder, nullptr);
    if (ret < 0) {
        g_lastAudioError = QStringLiteral("Failed to open audio decoder: %1").arg(audioFfmpegErrorString(ret));
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&audioFmtCtx);
        avformat_close_input(&videoFmtCtx);
        return false;
    }

    // 出力フォーマット作成
    AVFormatContext* outFmtCtx = nullptr;
    ret = avformat_alloc_output_context2(&outFmtCtx, nullptr, nullptr, outputPath.toUtf8().constData());
    if (!outFmtCtx) {
        g_lastAudioError = QStringLiteral("Failed to create output context for: %1").arg(outputPath);
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&audioFmtCtx);
        avformat_close_input(&videoFmtCtx);
        return false;
    }

    // ビデオストリーム検索
    int videoStreamIdx = av_find_best_stream(videoFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIdx < 0) {
        g_lastAudioError = QStringLiteral("No video stream found in: %1").arg(videoPath);
        avformat_free_context(outFmtCtx);
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&audioFmtCtx);
        avformat_close_input(&videoFmtCtx);
        return false;
    }

    // ビデオストリームをコピー
    AVStream* outVideoStream = avformat_new_stream(outFmtCtx, nullptr);
    if (!outVideoStream) {
        g_lastAudioError = "Failed to create output video stream";
        avformat_free_context(outFmtCtx);
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&audioFmtCtx);
        avformat_close_input(&videoFmtCtx);
        return false;
    }

    avcodec_parameters_copy(outVideoStream->codecpar, videoFmtCtx->streams[videoStreamIdx]->codecpar);
    outVideoStream->time_base = videoFmtCtx->streams[videoStreamIdx]->time_base;

    // 音声エンコーダー作成
    QString targetCodec = audioCodec.isEmpty() ? QStringLiteral("aac") : audioCodec;
    AVCodecID audioCodecId = AV_CODEC_ID_AAC;
    if (targetCodec == "mp3") audioCodecId = AV_CODEC_ID_MP3;
    else if (targetCodec == "opus") audioCodecId = AV_CODEC_ID_OPUS;
    else if (targetCodec == "flac") audioCodecId = AV_CODEC_ID_FLAC;
    else if (targetCodec == "vorbis") audioCodecId = AV_CODEC_ID_VORBIS;

    const AVCodec* aEncoder = avcodec_find_encoder(audioCodecId);
    if (!aEncoder) {
        // フォールバック: AAC
        audioCodecId = AV_CODEC_ID_AAC;
        aEncoder = avcodec_find_encoder(audioCodecId);
    }
    if (!aEncoder) {
        g_lastAudioError = QStringLiteral("No audio encoder found for: %1 (AAC fallback also unavailable)").arg(targetCodec);
        avformat_free_context(outFmtCtx);
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&audioFmtCtx);
        avformat_close_input(&videoFmtCtx);
        return false;
    }

    AVStream* outAudioStream = avformat_new_stream(outFmtCtx, aEncoder);
    if (!outAudioStream) {
        g_lastAudioError = "Failed to create output audio stream";
        avformat_free_context(outFmtCtx);
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&audioFmtCtx);
        avformat_close_input(&videoFmtCtx);
        return false;
    }

    AVCodecContext* aEncoderCtx = avcodec_alloc_context3(aEncoder);
    aEncoderCtx->sample_rate = aDecoderCtx->sample_rate;
    if (audioBitrate > 0) {
        aEncoderCtx->bit_rate = audioBitrate;
    } else {
        aEncoderCtx->bit_rate = 128000;
    }

    av_channel_layout_default(&aEncoderCtx->ch_layout, aDecoderCtx->ch_layout.nb_channels);

    if (aEncoder->sample_fmts) {
        aEncoderCtx->sample_fmt = aEncoder->sample_fmts[0];
    } else {
        aEncoderCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }

    aEncoderCtx->time_base = AVRational{1, aEncoderCtx->sample_rate};

    ret = avcodec_open2(aEncoderCtx, aEncoder, nullptr);
    if (ret < 0) {
        g_lastAudioError = QStringLiteral("Failed to open audio encoder %1: %2").arg(targetCodec, audioFfmpegErrorString(ret));
        avcodec_free_context(&aEncoderCtx);
        avformat_free_context(outFmtCtx);
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&audioFmtCtx);
        avformat_close_input(&videoFmtCtx);
        return false;
    }

    avcodec_parameters_from_context(outAudioStream->codecpar, aEncoderCtx);
    outAudioStream->time_base = aEncoderCtx->time_base;

    // SwResample コンテキスト作成
    SwrContext* swrCtx = nullptr;
    ret = swr_alloc_set_opts2(&swrCtx,
        &aEncoderCtx->ch_layout, aEncoderCtx->sample_fmt, aEncoderCtx->sample_rate,
        &aDecoderCtx->ch_layout, aDecoderCtx->sample_fmt, aDecoderCtx->sample_rate,
        0, nullptr);
    if (ret < 0 || !swrCtx) {
        g_lastAudioError = QStringLiteral("Failed to create audio resampler: %1").arg(audioFfmpegErrorString(ret));
        avcodec_free_context(&aEncoderCtx);
        avformat_free_context(outFmtCtx);
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&audioFmtCtx);
        avformat_close_input(&videoFmtCtx);
        return false;
    }
    swr_init(swrCtx);

    // 出力ファイルオープン
    if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outFmtCtx->pb, outputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            g_lastAudioError = QStringLiteral("Failed to open output file: %1 (%2)").arg(outputPath, audioFfmpegErrorString(ret));
            swr_free(&swrCtx);
            avcodec_free_context(&aEncoderCtx);
            avformat_free_context(outFmtCtx);
            avcodec_free_context(&aDecoderCtx);
            avformat_close_input(&audioFmtCtx);
            avformat_close_input(&videoFmtCtx);
            return false;
        }
    }

    ret = avformat_write_header(outFmtCtx, nullptr);
    if (ret < 0) {
        g_lastAudioError = QStringLiteral("Failed to write header: %1").arg(audioFfmpegErrorString(ret));
        if (outFmtCtx->pb) avio_closep(&outFmtCtx->pb);
        swr_free(&swrCtx);
        avcodec_free_context(&aEncoderCtx);
        avformat_free_context(outFmtCtx);
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&audioFmtCtx);
        avformat_close_input(&videoFmtCtx);
        return false;
    }

    // ビデオパケットをコピー
    AVPacket* pkt = av_packet_alloc();
    av_seek_frame(videoFmtCtx, videoStreamIdx, 0, AVSEEK_FLAG_BACKWARD);
    while (av_read_frame(videoFmtCtx, pkt) >= 0) {
        if (pkt->stream_index == videoStreamIdx) {
            pkt->stream_index = outVideoStream->index;
            av_packet_rescale_ts(pkt, videoFmtCtx->streams[videoStreamIdx]->time_base, outVideoStream->time_base);
            pkt->pos = -1;
            ret = av_interleaved_write_frame(outFmtCtx, pkt);
            if (ret < 0) {
                g_lastAudioError = QStringLiteral("Failed to write video packet during mux: %1").arg(audioFfmpegErrorString(ret));
                av_packet_unref(pkt);
                // Continue anyway, fall back to partial output
                break;
            }
        }
        av_packet_unref(pkt);
    }

    // 音声デコード → リサンプル → エンコード → 書き込み
    AVFrame* decodedFrame = av_frame_alloc();
    AVFrame* resampledFrame = av_frame_alloc();
    AVPacket* encPkt = av_packet_alloc();

    resampledFrame->format = aEncoderCtx->sample_fmt;
    av_channel_layout_copy(&resampledFrame->ch_layout, &aEncoderCtx->ch_layout);
    resampledFrame->sample_rate = aEncoderCtx->sample_rate;
    resampledFrame->nb_samples = aEncoderCtx->frame_size > 0 ? aEncoderCtx->frame_size : 1024;
    av_frame_get_buffer(resampledFrame, 0);

    int64_t audioPts = 0;
    av_seek_frame(audioFmtCtx, audioStreamIdx, 0, AVSEEK_FLAG_BACKWARD);
    while (av_read_frame(audioFmtCtx, pkt) >= 0) {
        if (pkt->stream_index != audioStreamIdx) {
            av_packet_unref(pkt);
            continue;
        }
        ret = avcodec_send_packet(aDecoderCtx, pkt);
        av_packet_unref(pkt);
        if (ret < 0) continue;

        while (avcodec_receive_frame(aDecoderCtx, decodedFrame) >= 0) {
            ret = swr_convert(swrCtx,
                resampledFrame->data, resampledFrame->nb_samples,
                (const uint8_t**)decodedFrame->data, decodedFrame->nb_samples);
            if (ret > 0) {
                resampledFrame->nb_samples = ret;
                resampledFrame->pts = audioPts;
                audioPts += ret;

                ret = avcodec_send_frame(aEncoderCtx, resampledFrame);
                if (ret >= 0) {
                    while (avcodec_receive_packet(aEncoderCtx, encPkt) >= 0) {
                        encPkt->stream_index = outAudioStream->index;
                        av_packet_rescale_ts(encPkt, aEncoderCtx->time_base, outAudioStream->time_base);
                        av_interleaved_write_frame(outFmtCtx, encPkt);
                        av_packet_unref(encPkt);
                    }
                }
            }
            av_frame_unref(decodedFrame);
        }
    }

    // フラッシュデコーダー
    avcodec_send_packet(aDecoderCtx, nullptr);
    while (avcodec_receive_frame(aDecoderCtx, decodedFrame) >= 0) {
        ret = swr_convert(swrCtx,
            resampledFrame->data, resampledFrame->nb_samples,
            (const uint8_t**)decodedFrame->data, decodedFrame->nb_samples);
        if (ret > 0) {
            resampledFrame->nb_samples = ret;
            resampledFrame->pts = audioPts;
            audioPts += ret;
            avcodec_send_frame(aEncoderCtx, resampledFrame);
            while (avcodec_receive_packet(aEncoderCtx, encPkt) >= 0) {
                encPkt->stream_index = outAudioStream->index;
                av_packet_rescale_ts(encPkt, aEncoderCtx->time_base, outAudioStream->time_base);
                av_interleaved_write_frame(outFmtCtx, encPkt);
                av_packet_unref(encPkt);
            }
        }
        av_frame_unref(decodedFrame);
    }

    // フラッシュエンコーダー
    avcodec_send_frame(aEncoderCtx, nullptr);
    while (avcodec_receive_packet(aEncoderCtx, encPkt) >= 0) {
        encPkt->stream_index = outAudioStream->index;
        av_packet_rescale_ts(encPkt, aEncoderCtx->time_base, outAudioStream->time_base);
        av_interleaved_write_frame(outFmtCtx, encPkt);
        av_packet_unref(encPkt);
    }

    av_write_trailer(outFmtCtx);

    // クリーンアップ
    av_packet_free(&encPkt);
    av_frame_free(&resampledFrame);
    av_frame_free(&decodedFrame);
    swr_free(&swrCtx);
    avcodec_free_context(&aEncoderCtx);
    if (outFmtCtx->pb) avio_closep(&outFmtCtx->pb);
    avformat_free_context(outFmtCtx);
    avcodec_free_context(&aDecoderCtx);
    avformat_close_input(&audioFmtCtx);
    avformat_close_input(&videoFmtCtx);

    g_lastAudioError.clear();
    return true;
}

bool FFmpegAudioEncoder::encodeAudio(
    const QString& inputPath,
    const QString& outputPath,
    const QString& codec,
    int bitrate,
    int sampleRate)
{
    AVFormatContext* inFmtCtx = nullptr;
    int ret = avformat_open_input(&inFmtCtx, inputPath.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        g_lastAudioError = QStringLiteral("Failed to open input: %1 (%2)").arg(inputPath, audioFfmpegErrorString(ret));
        return false;
    }
    avformat_find_stream_info(inFmtCtx, nullptr);

    int inStreamIdx = av_find_best_stream(inFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (inStreamIdx < 0) {
        g_lastAudioError = QStringLiteral("No audio stream in: %1").arg(inputPath);
        avformat_close_input(&inFmtCtx);
        return false;
    }

    AVStream* inStream = inFmtCtx->streams[inStreamIdx];

    const AVCodec* aDecoder = avcodec_find_decoder(inStream->codecpar->codec_id);
    AVCodecContext* aDecoderCtx = avcodec_alloc_context3(aDecoder);
    avcodec_parameters_to_context(aDecoderCtx, inStream->codecpar);
    ret = avcodec_open2(aDecoderCtx, aDecoder, nullptr);
    if (ret < 0) {
        g_lastAudioError = QStringLiteral("Failed to open audio decoder: %1").arg(audioFfmpegErrorString(ret));
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&inFmtCtx);
        return false;
    }

    AVCodecID codecId = AV_CODEC_ID_AAC;
    if (codec == "mp3") codecId = AV_CODEC_ID_MP3;
    else if (codec == "opus") codecId = AV_CODEC_ID_OPUS;
    else if (codec == "flac") codecId = AV_CODEC_ID_FLAC;
    else if (codec == "vorbis") codecId = AV_CODEC_ID_VORBIS;

    const AVCodec* aEncoder = avcodec_find_encoder(codecId);
    if (!aEncoder) {
        g_lastAudioError = QStringLiteral("No audio encoder found for: %1").arg(codec);
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&inFmtCtx);
        return false;
    }

    AVFormatContext* outFmtCtx = nullptr;
    ret = avformat_alloc_output_context2(&outFmtCtx, nullptr, nullptr, outputPath.toUtf8().constData());
    if (!outFmtCtx) {
        g_lastAudioError = QStringLiteral("Failed to create output context: %1").arg(outputPath);
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&inFmtCtx);
        return false;
    }

    AVStream* outStream = avformat_new_stream(outFmtCtx, aEncoder);
    AVCodecContext* aEncoderCtx = avcodec_alloc_context3(aEncoder);
    aEncoderCtx->sample_rate = sampleRate > 0 ? sampleRate : aDecoderCtx->sample_rate;
    aEncoderCtx->bit_rate = bitrate;
    av_channel_layout_default(&aEncoderCtx->ch_layout, aDecoderCtx->ch_layout.nb_channels);
    if (aEncoder->sample_fmts) {
        aEncoderCtx->sample_fmt = aEncoder->sample_fmts[0];
    } else {
        aEncoderCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }
    aEncoderCtx->time_base = AVRational{1, aEncoderCtx->sample_rate};

    ret = avcodec_open2(aEncoderCtx, aEncoder, nullptr);
    if (ret < 0) {
        g_lastAudioError = QStringLiteral("Failed to open audio encoder: %1").arg(audioFfmpegErrorString(ret));
        avcodec_free_context(&aEncoderCtx);
        avformat_free_context(outFmtCtx);
        avcodec_free_context(&aDecoderCtx);
        avformat_close_input(&inFmtCtx);
        return false;
    }

    avcodec_parameters_from_context(outStream->codecpar, aEncoderCtx);
    outStream->time_base = aEncoderCtx->time_base;

    SwrContext* swrCtx = nullptr;
    swr_alloc_set_opts2(&swrCtx,
        &aEncoderCtx->ch_layout, aEncoderCtx->sample_fmt, aEncoderCtx->sample_rate,
        &aDecoderCtx->ch_layout, aDecoderCtx->sample_fmt, aDecoderCtx->sample_rate,
        0, nullptr);
    swr_init(swrCtx);

    if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
        avio_open(&outFmtCtx->pb, outputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
    }
    avformat_write_header(outFmtCtx, nullptr);

    AVFrame* decodedFrame = av_frame_alloc();
    AVFrame* resampledFrame = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    AVPacket* encPkt = av_packet_alloc();

    resampledFrame->format = aEncoderCtx->sample_fmt;
    av_channel_layout_copy(&resampledFrame->ch_layout, &aEncoderCtx->ch_layout);
    resampledFrame->sample_rate = aEncoderCtx->sample_rate;
    resampledFrame->nb_samples = aEncoderCtx->frame_size > 0 ? aEncoderCtx->frame_size : 1024;
    av_frame_get_buffer(resampledFrame, 0);

    int64_t audioPts = 0;
    while (av_read_frame(inFmtCtx, pkt) >= 0) {
        if (pkt->stream_index != inStreamIdx) {
            av_packet_unref(pkt);
            continue;
        }
        avcodec_send_packet(aDecoderCtx, pkt);
        av_packet_unref(pkt);
        while (avcodec_receive_frame(aDecoderCtx, decodedFrame) >= 0) {
            ret = swr_convert(swrCtx,
                resampledFrame->data, resampledFrame->nb_samples,
                (const uint8_t**)decodedFrame->data, decodedFrame->nb_samples);
            if (ret > 0) {
                resampledFrame->nb_samples = ret;
                resampledFrame->pts = audioPts;
                audioPts += ret;
                avcodec_send_frame(aEncoderCtx, resampledFrame);
                while (avcodec_receive_packet(aEncoderCtx, encPkt) >= 0) {
                    encPkt->stream_index = outStream->index;
                    av_packet_rescale_ts(encPkt, aEncoderCtx->time_base, outStream->time_base);
                    av_interleaved_write_frame(outFmtCtx, encPkt);
                    av_packet_unref(encPkt);
                }
            }
            av_frame_unref(decodedFrame);
        }
    }

    avcodec_send_packet(aDecoderCtx, nullptr);
    while (avcodec_receive_frame(aDecoderCtx, decodedFrame) >= 0) {
        swr_convert(swrCtx,
            resampledFrame->data, resampledFrame->nb_samples,
            (const uint8_t**)decodedFrame->data, decodedFrame->nb_samples);
        avcodec_send_frame(aEncoderCtx, resampledFrame);
        while (avcodec_receive_packet(aEncoderCtx, encPkt) >= 0) {
            encPkt->stream_index = outStream->index;
            av_packet_rescale_ts(encPkt, aEncoderCtx->time_base, outStream->time_base);
            av_interleaved_write_frame(outFmtCtx, encPkt);
            av_packet_unref(encPkt);
        }
        av_frame_unref(decodedFrame);
    }
    avcodec_send_frame(aEncoderCtx, nullptr);
    while (avcodec_receive_packet(aEncoderCtx, encPkt) >= 0) {
        encPkt->stream_index = outStream->index;
        av_packet_rescale_ts(encPkt, aEncoderCtx->time_base, outStream->time_base);
        av_interleaved_write_frame(outFmtCtx, encPkt);
        av_packet_unref(encPkt);
    }

    av_write_trailer(outFmtCtx);

    av_packet_free(&encPkt);
    av_packet_free(&pkt);
    av_frame_free(&resampledFrame);
    av_frame_free(&decodedFrame);
    swr_free(&swrCtx);
    avcodec_free_context(&aEncoderCtx);
    if (outFmtCtx->pb) avio_closep(&outFmtCtx->pb);
    avformat_free_context(outFmtCtx);
    avcodec_free_context(&aDecoderCtx);
    avformat_close_input(&inFmtCtx);

    g_lastAudioError.clear();
    return true;
}

} // namespace ArtifactCore
