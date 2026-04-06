module;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <QString>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDir>

module Encoder.FFmpegEncoder;

import std;
import Image;
import :Impl;

namespace {

QString ffmpegErrorString(int err)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(err, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

AVPixelFormat preferredPixelFormatForCodec(const AVCodec* codec, AVCodecID codecId)
{
    if (codec && codec->pix_fmts) {
        for (const AVPixelFormat* fmt = codec->pix_fmts; *fmt != AV_PIX_FMT_NONE; ++fmt) {
            if (*fmt == AV_PIX_FMT_NV12 ||
                *fmt == AV_PIX_FMT_P010LE ||
                *fmt == AV_PIX_FMT_YUV420P ||
                *fmt == AV_PIX_FMT_YUV422P ||
                *fmt == AV_PIX_FMT_YUV444P ||
                *fmt == AV_PIX_FMT_RGBA ||
                *fmt == AV_PIX_FMT_BGRA) {
                return *fmt;
            }
        }
        return codec->pix_fmts[0];
    }

    switch (codecId) {
    case AV_CODEC_ID_PRORES:
        return AV_PIX_FMT_YUV422P10;
    case AV_CODEC_ID_H264:
    case AV_CODEC_ID_HEVC:
    case AV_CODEC_ID_VP9:
        return AV_PIX_FMT_YUV420P;
    case AV_CODEC_ID_MJPEG:
        return AV_PIX_FMT_YUVJ420P;
    case AV_CODEC_ID_PNG:
    case AV_CODEC_ID_APNG:
    case AV_CODEC_ID_WEBP:
        return AV_PIX_FMT_RGBA;
    case AV_CODEC_ID_GIF:
        return AV_PIX_FMT_PAL8;
    default:
        return AV_PIX_FMT_YUV420P;
    }
}

QStringList hardwareEncoderCandidatesForCodec(const QString& codecName)
{
    const QString codec = codecName.trimmed().toLower();
    if (codec == "h264" || codec == "avc" || codec == "libx264") {
        return {"h264_nvenc", "h264_qsv", "h264_amf", "h264_vaapi"};
    }
    if (codec == "h265" || codec == "hevc" || codec == "libx265") {
        return {"hevc_nvenc", "hevc_qsv", "hevc_amf", "hevc_vaapi"};
    }
    if (codec == "vp9" || codec == "libvpx-vp9") {
        return {"vp9_qsv", "vp9_vaapi"};
    }
    return {};
}

AVHWDeviceType hardwareDeviceTypeForEncoderName(const QString& encoderName)
{
    const QString value = encoderName.trimmed().toLower();
    if (value.contains("nvenc")) {
        return av_hwdevice_find_type_by_name("cuda");
    }
    if (value.contains("qsv")) {
        return av_hwdevice_find_type_by_name("qsv");
    }
    if (value.contains("vaapi")) {
        return av_hwdevice_find_type_by_name("vaapi");
    }
    return AV_HWDEVICE_TYPE_NONE;
}

AVPixelFormat hardwarePixelFormatForEncoderName(const QString& encoderName)
{
    const QString value = encoderName.trimmed().toLower();
    if (value.contains("nvenc")) {
        return AV_PIX_FMT_CUDA;
    }
    if (value.contains("qsv")) {
        return AV_PIX_FMT_QSV;
    }
    if (value.contains("vaapi")) {
        return AV_PIX_FMT_VAAPI;
    }
    return AV_PIX_FMT_NONE;
}

} // namespace

namespace ArtifactCore {

class FFmpegEncoder::Impl {
public:
    Impl() {
        // FFmpeg ライブラリ初期化
        avformat_network_init();
    }

    ~Impl() {
        close();
    }

    bool open(const QString& outputPath, const FFmpegEncoderSettings& settings) {
        if (isOpen_) {
            lastError_ = "Encoder is already open";
            return false;
        }

        settings_ = settings;
        width_ = settings.width;
        height_ = settings.height;
        isImageSequence_ = false;

        // 出力ディレクトリ作成
        QFileInfo fileInfo(outputPath);
        QDir dir(fileInfo.absolutePath());
        if (!dir.exists() && !dir.mkpath(".")) {
            lastError_ = QStringLiteral("Failed to create output directory: %1").arg(dir.absolutePath());
            return false;
        }

        // 出力フォーマット検索
        const AVOutputFormat* fmt = av_guess_format(
            settings.container.toUtf8().constData(),
            nullptr,
            nullptr);
        if (!fmt) {
            lastError_ = QStringLiteral("Failed to find output format: %1").arg(settings.container);
            return false;
        }

        // 出力コンテキスト作成
        fmtCtx_ = avformat_alloc_context();
        if (!fmtCtx_) {
            lastError_ = "Failed to allocate output format context";
            return false;
        }
        fmtCtx_->oformat = fmt;

        // ストリーム作成
        stream_ = avformat_new_stream(fmtCtx_, nullptr);
        if (!stream_) {
            lastError_ = "Failed to create stream";
            return false;
        }
        stream_->id = 0;

        // コーデック検索
        AVCodecID codecId = AV_CODEC_ID_NONE;
        
        // コンテナとコーデックの組み合わせ検証
        const QString codecLower = settings.videoCodec.toLower().trimmed();
        if (codecLower == "h264" || codecLower == "avc" || codecLower == "libx264") {
            codecId = AV_CODEC_ID_H264;
        } else if (codecLower == "h265" || codecLower == "hevc" || codecLower == "libx265") {
            codecId = AV_CODEC_ID_HEVC;
        } else if (codecLower == "prores" || codecLower == "apple_prores") {
            codecId = AV_CODEC_ID_PRORES;
        } else if (codecLower == "vp9" || codecLower == "libvpx-vp9") {
            codecId = AV_CODEC_ID_VP9;
        } else if (codecLower == "mjpeg" || codecLower == "motion_jpeg") {
            codecId = AV_CODEC_ID_MJPEG;
        } else if (codecLower == "png") {
            codecId = AV_CODEC_ID_PNG;
        } else if (codecLower == "gif") {
            codecId = AV_CODEC_ID_GIF;
        } else if (codecLower == "apng") {
            codecId = AV_CODEC_ID_APNG;
        } else if (codecLower == "webp" || codecLower == "libwebp_anim" || codecLower == "libwebp") {
            codecId = AV_CODEC_ID_WEBP;
        } else {
            codecId = AV_CODEC_ID_H264;  // デフォルト
        }

        const QString encoderName = settings.encoderName.trimmed();
        const AVCodec* codec = nullptr;
        if (!encoderName.isEmpty()) {
            codec = avcodec_find_encoder_by_name(encoderName.toUtf8().constData());
        }
        if (!codec) {
            codec = avcodec_find_encoder(codecId);
        }
        if (!codec) {
            lastError_ = QStringLiteral("Failed to find encoder for codec: %1").arg(settings.videoCodec);
            return false;
        }

        // コーデックコンテキスト作成
        codecCtx_ = avcodec_alloc_context3(codec);
        if (!codecCtx_) {
            lastError_ = "Failed to allocate codec context";
            return false;
        }

        hwDeviceType_ = hardwareDeviceTypeForEncoderName(encoderName);
        hwPixFmt_ = hardwarePixelFormatForEncoderName(encoderName);
        useHardwareFrames_ = (hwDeviceType_ != AV_HWDEVICE_TYPE_NONE && hwPixFmt_ != AV_PIX_FMT_NONE);

        // コーデックパラメーター設定
        codecCtx_->codec_id = codecId;
        codecCtx_->codec_type = AVMEDIA_TYPE_VIDEO;
        codecCtx_->width = width_;
        codecCtx_->height = height_;
        codecCtx_->time_base = AVRational{1, static_cast<int>(settings.fps)};
        codecCtx_->framerate = AVRational{static_cast<int>(settings.fps), 1};
        codecCtx_->gop_size = settings.gopSize;
        codecCtx_->max_b_frames = settings.maxBFrames;
        
        // ピクセルフォーマット設定（コーデックによる）
        const AVPixelFormat preferredPixFmt = preferredPixelFormatForCodec(codec, codecId);
        if (codecId == AV_CODEC_ID_PRORES) {
            const QString profile = settings.profile.trimmed().toLower();
            if (profile.contains("4444")) {
                codecCtx_->pix_fmt = AV_PIX_FMT_YUVA444P10LE;  // ProRes 4444 は alpha を含む
            } else {
                codecCtx_->pix_fmt = AV_PIX_FMT_YUV422P10;     // ProRes 422 系
            }
            useHardwareFrames_ = false;
        } else if (useHardwareFrames_) {
            swInputPixFmt_ = preferredPixFmt;
            codecCtx_->pix_fmt = hwPixFmt_;
            codecCtx_->sw_pix_fmt = swInputPixFmt_;
            hwDeviceCtx_ = nullptr;
            if (const int ret = av_hwdevice_ctx_create(&hwDeviceCtx_, hwDeviceType_, nullptr, nullptr, 0); ret < 0) {
                lastError_ = QStringLiteral("Failed to create hardware device context for %1: %2")
                    .arg(encoderName, ffmpegErrorString(ret));
                close();
                return false;
            }

            hwFramesCtx_ = av_hwframe_ctx_alloc(hwDeviceCtx_);
            if (!hwFramesCtx_) {
                lastError_ = QStringLiteral("Failed to allocate hardware frames context for %1").arg(encoderName);
                close();
                return false;
            }

            auto* framesCtx = reinterpret_cast<AVHWFramesContext*>(hwFramesCtx_->data);
            framesCtx->format = hwPixFmt_;
            framesCtx->sw_format = swInputPixFmt_;
            framesCtx->width = width_;
            framesCtx->height = height_;
            framesCtx->initial_pool_size = 2;
            if (const int ret = av_hwframe_ctx_init(hwFramesCtx_); ret < 0) {
                lastError_ = QStringLiteral("Failed to initialize hardware frames context for %1: %2")
                    .arg(encoderName, ffmpegErrorString(ret));
                close();
                return false;
            }
            codecCtx_->hw_frames_ctx = av_buffer_ref(hwFramesCtx_);
            if (!codecCtx_->hw_frames_ctx) {
                lastError_ = QStringLiteral("Failed to reference hardware frames context for %1").arg(encoderName);
                close();
                return false;
            }
        } else {
            codecCtx_->pix_fmt = preferredPixFmt;
            swInputPixFmt_ = preferredPixFmt;
        }

        // ビットレート設定（可変品質コーデックは CRF 優先）
        if (codecId == AV_CODEC_ID_H264 || codecId == AV_CODEC_ID_HEVC || codecId == AV_CODEC_ID_VP9) {
            codecCtx_->global_quality = settings.crf * FF_QP2LAMBDA;
            codecCtx_->flags |= AV_CODEC_FLAG_QSCALE;
        } else {
            codecCtx_->bit_rate = settings.bitrateKbps * 1000;
        }

        // コーデック固有オプション
        if (codecId == AV_CODEC_ID_H264) {
            av_opt_set(codecCtx_->priv_data, "preset", settings.preset.toUtf8().constData(), 0);
            if (settings.zerolatency) {
                av_opt_set(codecCtx_->priv_data, "tune", "zerolatency", 0);
            }
            av_opt_set(codecCtx_->priv_data, "profile", settings.profile.toUtf8().constData(), 0);
        } else if (codecId == AV_CODEC_ID_HEVC) {
            av_opt_set(codecCtx_->priv_data, "preset", settings.preset.toUtf8().constData(), 0);
            av_opt_set(codecCtx_->priv_data, "profile", settings.profile.toUtf8().constData(), 0);
        } else if (codecId == AV_CODEC_ID_VP9) {
            av_opt_set(codecCtx_->priv_data, "preset", settings.preset.toUtf8().constData(), 0);
            av_opt_set_int(codecCtx_->priv_data, "crf", settings.crf, 0);
        } else if (codecId == AV_CODEC_ID_PRORES) {
            // ProRes profile（1=proxy, 2=lt, 3=standard, 4=hq, 5=4444）
            int proresProfile = 3;  // default: standard
            const QString prof = settings.profile.toLower();
            if (prof.contains("4444")) proresProfile = 5;
            else if (prof.contains("hq")) proresProfile = 4;
            else if (prof.contains("lt")) proresProfile = 2;
            else if (prof.contains("proxy")) proresProfile = 1;
            av_opt_set_int(codecCtx_->priv_data, "profile", proresProfile, 0);
        } else if (codecId == AV_CODEC_ID_GIF) {
            av_opt_set_int(codecCtx_->priv_data, "loop", 0, 0);
        } else if (codecId == AV_CODEC_ID_APNG) {
            av_opt_set_int(codecCtx_->priv_data, "plays", 0, 0);
        } else if (codecId == AV_CODEC_ID_WEBP) {
            av_opt_set_int(codecCtx_->priv_data, "loop", 0, 0);
        }

        if (const int ret = avcodec_open2(codecCtx_, codec, nullptr); ret < 0) {
            lastError_ = QStringLiteral("Failed to open video encoder: %1 (%2)").arg(settings.videoCodec, ffmpegErrorString(ret));
            close();
            return false;
        }

        // ストリームにコーデックパラメーターをコピー
        if (const int ret = avcodec_parameters_from_context(stream_->codecpar, codecCtx_); ret < 0) {
            lastError_ = QStringLiteral("Failed to copy codec parameters to stream: %1").arg(ffmpegErrorString(ret));
            close();
            return false;
        }

        // ファイルオープン
        if (!(fmt->flags & AVFMT_NOFILE)) {
            const int ret = avio_open(&fmtCtx_->pb, outputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                lastError_ = QStringLiteral("Failed to open output file: %1 (%2)").arg(outputPath, ffmpegErrorString(ret));
                close();
                return false;
            }
        }

        // ヘッダー書き込み
        if (const int ret = avformat_write_header(fmtCtx_, nullptr); ret < 0) {
            lastError_ = QStringLiteral("Failed to write header to: %1 (%2)").arg(outputPath, ffmpegErrorString(ret));
            close();
            return false;
        }

        // スケーラー作成（RGBA → 各コーデックのピクセルフォーマット変換用）
        const AVPixelFormat dstPixFmt = useHardwareFrames_ ? swInputPixFmt_ : codecCtx_->pix_fmt;
        swsCtx_ = sws_getContext(
            width_, height_, AV_PIX_FMT_RGBA,
            width_, height_, dstPixFmt,
            SWS_BILINEAR,
            nullptr, nullptr, nullptr);

        if (!swsCtx_) {
            lastError_ = "Failed to create sws context";
            close();
            return false;
        }

        // フレーム作成
        frame_ = av_frame_alloc();
        if (!frame_) {
            lastError_ = "Failed to allocate frame";
            close();
            return false;
        }
        frame_->format = dstPixFmt;
        frame_->width = width_;
        frame_->height = height_;

        if (useHardwareFrames_) {
            hwFrame_ = av_frame_alloc();
            if (!hwFrame_) {
                lastError_ = "Failed to allocate hardware frame";
                close();
                return false;
            }
        }

        const int ret = av_frame_get_buffer(frame_, 32);
        if (ret < 0) {
            lastError_ = "Failed to allocate frame buffer";
            close();
            return false;
        }

        // パケット作成
        packet_ = av_packet_alloc();
        if (!packet_) {
            lastError_ = "Failed to allocate packet";
            close();
            return false;
        }

        isOpen_ = true;
        frameIndex_ = 0;
        lastError_.clear();
        return true;
    }

    bool openImageSequence(const QString& outputPathPattern, const FFmpegImageSequenceSettings& settings) {
        if (isOpen_) {
            lastError_ = "Encoder is already open";
            return false;
        }

        imageSeqSettings_ = settings;
        width_ = settings.width;
        height_ = settings.height;
        isImageSequence_ = true;
        currentFrameNum_ = settings.startFrame;

        // 出力ディレクトリ作成
        QFileInfo fileInfo(outputPathPattern);
        QDir dir(fileInfo.absolutePath());
        if (!dir.exists() && !dir.mkpath(".")) {
            lastError_ = QStringLiteral("Failed to create output directory: %1").arg(dir.absolutePath());
            return false;
        }

        // フォーマット検証
        const QString fmt = settings.format.toLower();
        if (fmt != "png" && fmt != "jpeg" && fmt != "jpg" && fmt != "exr" && fmt != "tiff" && fmt != "bmp") {
            lastError_ = QStringLiteral("Unsupported image sequence format: %1").arg(settings.format);
            return false;
        }
        imageFormat_ = fmt == "jpg" ? "jpeg" : fmt;

        // 出力パターン保存（%04d を後で展開）
        outputPathPattern_ = outputPathPattern;

        // 圧縮レベル設定
        if (fmt == "png") {
            compressionLevel_ = std::clamp(settings.compressionLevel, 0, 9);
        } else if (fmt == "tiff") {
            compressionLevel_ = std::clamp(settings.compressionLevel, 1, 9);
        } else if (fmt == "jpeg") {
            jpegQuality_ = std::clamp(settings.jpegQuality, 1, 100);
        }

        // ピクセルフォーマット設定
        if (settings.is32bit && fmt == "exr") {
            dstPixFmt_ = AV_PIX_FMT_RGBF32LE;  // 32bit float
        } else if (settings.is16bit && (fmt == "png" || fmt == "tiff")) {
            dstPixFmt_ = AV_PIX_FMT_RGB48LE;  // 16bit
        } else {
            dstPixFmt_ = AV_PIX_FMT_RGB24;  // 8bit
        }

        // スケーラー作成
        const AVPixelFormat swPixFmt = useHardwareFrames_ ? swInputPixFmt_ : dstPixFmt_;
        swsCtx_ = sws_getContext(
            width_, height_, AV_PIX_FMT_RGBA,
            width_, height_, swPixFmt,
            SWS_BILINEAR,
            nullptr, nullptr, nullptr);

        if (!swsCtx_) {
            lastError_ = "Failed to create sws context";
            return false;
        }

        // フレーム作成
        frame_ = av_frame_alloc();
        if (!frame_) {
            lastError_ = "Failed to allocate frame";
            return false;
        }
        frame_->format = swPixFmt;
        frame_->width = width_;
        frame_->height = height_;

        const int ret = av_frame_get_buffer(frame_, 32);
        if (ret < 0) {
            lastError_ = "Failed to allocate frame buffer";
            return false;
        }

        isOpen_ = true;
        lastError_.clear();
        return true;
    }

    bool addImage(const ImageF32x4_RGBA& image) {
        if (!isOpen_ || !swsCtx_) {
            lastError_ = "Encoder is not open";
            return false;
        }

        const int w = image.width();
        const int h = image.height();
        if (w != width_ || h != height_) {
            lastError_ = QStringLiteral("Image size mismatch: expected %1x%2, got %3x%4")
                .arg(width_).arg(height_).arg(w).arg(h);
            return false;
        }

        // 連番画像出力の場合
        if (isImageSequence_) {
            return addImageSequenceFrame(image);
        }

        // ビデオエンコードの場合
        if (!codecCtx_) {
            lastError_ = "Video encoder not initialized";
            return false;
        }

        // 入力画像データを RGBA 形式で一時バッファにコピー
        const auto srcMat = image.toCVMat();
        if (srcMat.empty()) {
            lastError_ = "Failed to convert image to cv::Mat";
            return false;
        }
        const float* srcData = reinterpret_cast<const float*>(srcMat.data);

        // 一時 RGBA フレーム作成
        AVFrame* rgbaFrame = av_frame_alloc();
        if (!rgbaFrame) {
            lastError_ = "Failed to allocate temporary RGBA frame";
            return false;
        }
        rgbaFrame->format = AV_PIX_FMT_RGBA;
        rgbaFrame->width = w;
        rgbaFrame->height = h;

        if (av_frame_get_buffer(rgbaFrame, 32) < 0) {
            av_frame_free(&rgbaFrame);
            lastError_ = "Failed to allocate RGBA frame buffer";
            return false;
        }

        // フレームをロック
        if (av_frame_make_writable(rgbaFrame) < 0) {
            av_frame_free(&rgbaFrame);
            lastError_ = "Failed to make RGBA frame writable";
            return false;
        }

        // 画像データをコピー（float [0,1] → uint8 [0,255]）
        uint8_t* dst = rgbaFrame->data[0];
        for (int i = 0; i < w * h; ++i) {
            const int r = static_cast<uint8_t>(std::clamp(srcData[i * 4 + 0] * 255.0f, 0.0f, 255.0f));
            const int g = static_cast<uint8_t>(std::clamp(srcData[i * 4 + 1] * 255.0f, 0.0f, 255.0f));
            const int b = static_cast<uint8_t>(std::clamp(srcData[i * 4 + 2] * 255.0f, 0.0f, 255.0f));
            const int a = static_cast<uint8_t>(std::clamp(srcData[i * 4 + 3] * 255.0f, 0.0f, 255.0f));
            dst[i * 4 + 0] = r;
            dst[i * 4 + 1] = g;
            dst[i * 4 + 2] = b;
            dst[i * 4 + 3] = a;
        }

        // 指定ピクセルフォーマットに変換
        if (av_frame_make_writable(frame_) < 0) {
            av_frame_free(&rgbaFrame);
            lastError_ = "Failed to make frame writable";
            return false;
        }

        uint8_t* srcDataPtr[1] = {dst};
        const int srcLinesize[1] = {w * 4};
        sws_scale(swsCtx_, srcDataPtr, srcLinesize, 0, h, frame_->data, frame_->linesize);

        av_frame_free(&rgbaFrame);

        AVFrame* frameToEncode = frame_;
        if (useHardwareFrames_) {
            if (!hwFrame_ || !hwFramesCtx_) {
                lastError_ = "Hardware frame context is not initialized";
                return false;
            }
            av_frame_unref(hwFrame_);
            hwFrame_->format = hwPixFmt_;
            hwFrame_->width = width_;
            hwFrame_->height = height_;
            if (const int ret = av_hwframe_get_buffer(hwFramesCtx_, hwFrame_, 0); ret < 0) {
                lastError_ = QStringLiteral("Failed to allocate hardware frame: %1").arg(ffmpegErrorString(ret));
                return false;
            }
            if (const int ret = av_hwframe_transfer_data(hwFrame_, frame_, 0); ret < 0) {
                lastError_ = QStringLiteral("Failed to transfer frame to hardware surface: %1").arg(ffmpegErrorString(ret));
                return false;
            }
            frameToEncode = hwFrame_;
        }

        // フレームにタイムスタンプ設定
        frameToEncode->pts = frameIndex_++;

        // エンコード
        int ret = avcodec_send_frame(codecCtx_, frameToEncode);
        if (ret < 0) {
            lastError_ = QStringLiteral("Failed to send frame to encoder: %1 (%2)").arg(ret).arg(ffmpegErrorString(ret));
            return false;
        }

        while (ret >= 0) {
            ret = avcodec_receive_packet(codecCtx_, packet_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                lastError_ = QStringLiteral("Failed to receive packet from encoder: %1 (%2)").arg(ret).arg(ffmpegErrorString(ret));
                return false;
            }

            // パケットにストリームインデックスとタイムスタンプ設定
            packet_->stream_index = stream_->index;
            packet_->pts = packet_->pts * stream_->time_base.den / codecCtx_->time_base.den;
            packet_->dts = packet_->dts * stream_->time_base.den / codecCtx_->time_base.den;
            packet_->duration = packet_->duration * stream_->time_base.den / codecCtx_->time_base.den;

            // ファイルに書き込み
            ret = av_interleaved_write_frame(fmtCtx_, packet_);
            if (ret < 0) {
                lastError_ = QStringLiteral("Failed to write packet: %1").arg(ret);
                av_packet_unref(packet_);
                return false;
            }

            av_packet_unref(packet_);
        }

        return true;
    }

    bool addImageSequenceFrame(const ImageF32x4_RGBA& image) {
        // 出力ファイル名生成（%04d を展開）
        const QString frameNumStr = QString::number(currentFrameNum_).rightJustified(imageSeqSettings_.padding, '0');
        QString outputPath = outputPathPattern_;
        outputPath.replace("%04d", frameNumStr);
        outputPath.replace("%d", frameNumStr);
        
        // 拡張子がなければ追加
        if (!outputPath.contains(".")) {
            outputPath += "." + imageFormat_;
        }

        // 画像データを準備
        const auto srcMat = image.toCVMat();
        if (srcMat.empty()) {
            lastError_ = "Failed to convert image to cv::Mat";
            return false;
        }
        const float* srcData = reinterpret_cast<const float*>(srcMat.data);
        const int w = image.width();
        const int h = image.height();

        // 一時 RGBA フレーム作成
        AVFrame* rgbaFrame = av_frame_alloc();
        if (!rgbaFrame) {
            lastError_ = "Failed to allocate temporary RGBA frame";
            return false;
        }
        rgbaFrame->format = AV_PIX_FMT_RGBA;
        rgbaFrame->width = w;
        rgbaFrame->height = h;

        if (av_frame_get_buffer(rgbaFrame, 32) < 0) {
            av_frame_free(&rgbaFrame);
            lastError_ = "Failed to allocate RGBA frame buffer";
            return false;
        }

        if (av_frame_make_writable(rgbaFrame) < 0) {
            av_frame_free(&rgbaFrame);
            lastError_ = "Failed to make RGBA frame writable";
            return false;
        }

        // 画像データをコピー（float → 8bit/16bit）
        if (dstPixFmt_ == AV_PIX_FMT_RGB48LE) {
            // 16bit 出力
            uint16_t* dst = reinterpret_cast<uint16_t*>(rgbaFrame->data[0]);
            for (int i = 0; i < w * h * 4; ++i) {
                dst[i] = static_cast<uint16_t>(std::clamp(srcData[i] * 65535.0f, 0.0f, 65535.0f));
            }
        } else if (dstPixFmt_ == AV_PIX_FMT_RGBF32LE) {
            // 32bit float 出力
            float* dst = reinterpret_cast<float*>(rgbaFrame->data[0]);
            for (int i = 0; i < w * h * 4; ++i) {
                dst[i] = srcData[i];
            }
        } else {
            // 8bit 出力
            uint8_t* dst = rgbaFrame->data[0];
            for (int i = 0; i < w * h * 4; ++i) {
                dst[i] = static_cast<uint8_t>(std::clamp(srcData[i] * 255.0f, 0.0f, 255.0f));
            }
        }

        // 指定フォーマットに変換
        if (av_frame_make_writable(frame_) < 0) {
            av_frame_free(&rgbaFrame);
            lastError_ = "Failed to make frame writable";
            return false;
        }

        uint8_t* srcDataPtr[1] = {rgbaFrame->data[0]};
        const int srcLinesize[1] = {w * 4};
        sws_scale(swsCtx_, srcDataPtr, srcLinesize, 0, h, frame_->data, frame_->linesize);

        av_frame_free(&rgbaFrame);

        // 画像エンコーダー取得
        const QString fmt = imageFormat_;
        AVCodecID codecId = AV_CODEC_ID_NONE;
        if (fmt == "png") codecId = AV_CODEC_ID_PNG;
        else if (fmt == "jpeg") codecId = AV_CODEC_ID_MJPEG;
        else if (fmt == "tiff") codecId = AV_CODEC_ID_TIFF;
        else if (fmt == "bmp") codecId = AV_CODEC_ID_BMP;
        else if (fmt == "exr") {
            // EXR は FFmpeg の標準エンコーダーでは非対応の場合がある
            lastError_ = "EXR format requires special handling (use OpenEXR library directly)";
            // 簡易的に TIFF で出力
            codecId = AV_CODEC_ID_TIFF;
        }

        const AVCodec* codec = avcodec_find_encoder(codecId);
        if (!codec) {
            lastError_ = QStringLiteral("Failed to find encoder for format: %1").arg(fmt);
            return false;
        }

        // 画像エンコードコンテキスト作成
        AVCodecContext* imgCodecCtx = avcodec_alloc_context3(codec);
        if (!imgCodecCtx) {
            lastError_ = "Failed to allocate image codec context";
            return false;
        }

        imgCodecCtx->codec_id = codecId;
        imgCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
        imgCodecCtx->width = w;
        imgCodecCtx->height = h;
        imgCodecCtx->pix_fmt = dstPixFmt_;
        imgCodecCtx->time_base = AVRational{1, 30};

        // 圧縮設定
        if (fmt == "png" || fmt == "tiff") {
            imgCodecCtx->compression_level = compressionLevel_;
        } else if (fmt == "jpeg") {
            imgCodecCtx->global_quality = jpegQuality_ * FF_QP2LAMBDA;
            imgCodecCtx->flags |= AV_CODEC_FLAG_QSCALE;
        }

        // エンコーダー初期化
        if (const int ret = avcodec_open2(imgCodecCtx, codec, nullptr); ret < 0) {
            avcodec_free_context(&imgCodecCtx);
            lastError_ = QStringLiteral("Failed to open image encoder for: %1 (%2)").arg(fmt, ffmpegErrorString(ret));
            return false;
        }

        // エンコード
        AVPacket* packet = av_packet_alloc();
        if (!packet) {
            avcodec_free_context(&imgCodecCtx);
            lastError_ = "Failed to allocate packet";
            return false;
        }

        int ret = avcodec_send_frame(imgCodecCtx, frame_);
        if (ret < 0) {
            av_packet_free(&packet);
            avcodec_free_context(&imgCodecCtx);
            lastError_ = QStringLiteral("Failed to send frame to image encoder: %1 (%2)").arg(ret).arg(ffmpegErrorString(ret));
            return false;
        }

        ret = avcodec_receive_packet(imgCodecCtx, packet);
        if (ret < 0) {
            av_packet_free(&packet);
            avcodec_free_context(&imgCodecCtx);
            lastError_ = QStringLiteral("Failed to receive packet from image encoder: %1 (%2)").arg(ret).arg(ffmpegErrorString(ret));
            return false;
        }

        // ファイルに書き込み
        QFile file(outputPath);
        if (!file.open(QIODevice::WriteOnly)) {
            av_packet_free(&packet);
            avcodec_free_context(&imgCodecCtx);
            lastError_ = QStringLiteral("Failed to open output file: %1").arg(outputPath);
            return false;
        }

        file.write(reinterpret_cast<char*>(packet->data), packet->size);
        file.close();

        av_packet_free(&packet);
        avcodec_free_context(&imgCodecCtx);

        currentFrameNum_++;
        return true;
    }

    void close() {
        const bool finalize = isOpen_;

        // 遅延フレームをエンコード
        if (finalize && codecCtx_) {
            avcodec_send_frame(codecCtx_, nullptr);
            while (avcodec_receive_packet(codecCtx_, packet_) >= 0) {
                packet_->stream_index = stream_->index;
                av_interleaved_write_frame(fmtCtx_, packet_);
                av_packet_unref(packet_);
            }
        }

        // トライラー書き込み
        if (finalize && fmtCtx_) {
            av_write_trailer(fmtCtx_);
        }

        // リソース解放
        if (packet_) {
            av_packet_free(&packet_);
        }
        if (frame_) {
            av_frame_free(&frame_);
        }
        if (hwFrame_) {
            av_frame_free(&hwFrame_);
        }
        if (swsCtx_) {
            sws_freeContext(swsCtx_);
        }
        if (hwFramesCtx_) {
            av_buffer_unref(&hwFramesCtx_);
        }
        if (hwDeviceCtx_) {
            av_buffer_unref(&hwDeviceCtx_);
        }
        if (codecCtx_) {
            avcodec_free_context(&codecCtx_);
        }
        if (fmtCtx_ && !(fmtCtx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&fmtCtx_->pb);
        }
        if (fmtCtx_) {
            avformat_free_context(fmtCtx_);
        }

        fmtCtx_ = nullptr;
        codecCtx_ = nullptr;
        stream_ = nullptr;
        swsCtx_ = nullptr;
        frame_ = nullptr;
        hwFrame_ = nullptr;
        hwFramesCtx_ = nullptr;
        hwDeviceCtx_ = nullptr;
        packet_ = nullptr;
        isOpen_ = false;
        useHardwareFrames_ = false;
        swInputPixFmt_ = AV_PIX_FMT_NONE;
        hwPixFmt_ = AV_PIX_FMT_NONE;
        hwDeviceType_ = AV_HWDEVICE_TYPE_NONE;
    }

    QString lastError() const {
        return lastError_;
    }

    bool isOpen() const {
        return isOpen_;
    }

    bool isImageSequence() const {
        return isImageSequence_;
    }

private:
    AVFormatContext* fmtCtx_ = nullptr;
    AVStream* stream_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVFrame* hwFrame_ = nullptr;
    AVPacket* packet_ = nullptr;
    SwsContext* swsCtx_ = nullptr;
    AVBufferRef* hwDeviceCtx_ = nullptr;
    AVBufferRef* hwFramesCtx_ = nullptr;

    int width_ = 0;
    int height_ = 0;
    int frameIndex_ = 0;
    bool isOpen_ = false;
    bool useHardwareFrames_ = false;
    QString lastError_;

    FFmpegEncoderSettings settings_;
    AVPixelFormat swInputPixFmt_ = AV_PIX_FMT_NONE;
    AVPixelFormat hwPixFmt_ = AV_PIX_FMT_NONE;
    AVHWDeviceType hwDeviceType_ = AV_HWDEVICE_TYPE_NONE;
    
    // 連番画像出力用
    FFmpegImageSequenceSettings imageSeqSettings_;
    bool isImageSequence_ = false;
    int currentFrameNum_ = 0;
    QString outputPathPattern_;
    QString imageFormat_;
    int compressionLevel_ = 6;
    int jpegQuality_ = 90;
    AVPixelFormat dstPixFmt_ = AV_PIX_FMT_RGB24;
};

FFmpegEncoder::FFmpegEncoder() : impl_(new Impl()) {
}

FFmpegEncoder::~FFmpegEncoder() {
    delete impl_;
}

bool FFmpegEncoder::open(const QString& outputPath, const FFmpegEncoderSettings& settings) {
    return impl_->open(outputPath, settings);
}

bool FFmpegEncoder::addImage(const ImageF32x4_RGBA& image) {
    return impl_->addImage(image);
}

void FFmpegEncoder::close() {
    impl_->close();
}

QString FFmpegEncoder::lastError() const {
    return impl_->lastError();
}

bool FFmpegEncoder::isOpen() const {
    return impl_->isOpen();
}

bool FFmpegEncoder::isImageSequence() const {
    return impl_->isImageSequence();
}

bool FFmpegEncoder::openImageSequence(const QString& outputPathPattern, const FFmpegImageSequenceSettings& settings) {
    return impl_->openImageSequence(outputPathPattern, settings);
}

// 静的ヘルパー関数の実装
bool FFmpegEncoder::isCodecAvailable(const QString& codecName) {
    const QString name = codecName.toLower().trimmed();
    AVCodecID codecId = AV_CODEC_ID_NONE;

    if (name == "h264" || name == "avc" || name == "libx264") {
        codecId = AV_CODEC_ID_H264;
    } else if (name == "h265" || name == "hevc" || name == "libx265") {
        codecId = AV_CODEC_ID_HEVC;
    } else if (name == "prores" || name == "apple_prores") {
        codecId = AV_CODEC_ID_PRORES;
    } else if (name == "vp9" || name == "libvpx-vp9") {
        codecId = AV_CODEC_ID_VP9;
    } else if (name == "mjpeg") {
        codecId = AV_CODEC_ID_MJPEG;
    } else if (name == "png") {
        codecId = AV_CODEC_ID_PNG;
    } else if (name == "gif") {
        codecId = AV_CODEC_ID_GIF;
    } else if (name == "apng") {
        codecId = AV_CODEC_ID_APNG;
    } else if (name == "webp" || name == "libwebp_anim" || name == "libwebp") {
        codecId = AV_CODEC_ID_WEBP;
    }

    if (codecId == AV_CODEC_ID_NONE) {
        return false;
    }

    return avcodec_find_encoder(codecId) != nullptr;
}

bool FFmpegEncoder::isEncoderAvailable(const QString& encoderName) {
    return avcodec_find_encoder_by_name(encoderName.toUtf8().constData()) != nullptr;
}

QStringList FFmpegEncoder::availableVideoCodecs() {
    QStringList result;

    // 主要コーデックを手動で登録
    if (avcodec_find_encoder(AV_CODEC_ID_H264)) {
        result << "h264";
    }
    if (avcodec_find_encoder(AV_CODEC_ID_HEVC)) {
        result << "h265";
    }
    if (avcodec_find_encoder(AV_CODEC_ID_PRORES)) {
        result << "prores";
    }
    if (avcodec_find_encoder(AV_CODEC_ID_VP9)) {
        result << "vp9";
    }
    if (avcodec_find_encoder(AV_CODEC_ID_MJPEG)) {
        result << "mjpeg";
    }
    if (avcodec_find_encoder(AV_CODEC_ID_PNG)) {
        result << "png";
    }
    if (avcodec_find_encoder(AV_CODEC_ID_GIF)) {
        result << "gif";
    }
    if (avcodec_find_encoder(AV_CODEC_ID_APNG)) {
        result << "apng";
    }
    if (avcodec_find_encoder(AV_CODEC_ID_WEBP)) {
        result << "webp";
    }

    return result;
}

QStringList FFmpegEncoder::availableHardwareVideoEncoders() {
    QStringList result;
    const QStringList candidates = {
        "h264_nvenc", "hevc_nvenc",
        "h264_qsv", "hevc_qsv", "vp9_qsv",
        "h264_amf", "hevc_amf",
        "h264_vaapi", "hevc_vaapi", "vp9_vaapi"
    };
    for (const auto& name : candidates) {
        if (isEncoderAvailable(name)) {
            result << name;
        }
    }
    return result;
}

bool FFmpegEncoder::isContainerAvailable(const QString& containerName) {
    const AVOutputFormat* fmt = av_guess_format(
        containerName.toUtf8().constData(),
        nullptr,
        nullptr);
    return fmt != nullptr;
}

QStringList FFmpegEncoder::availableContainers() {
    QStringList result;

    // 主要コンテナを手動で登録
    if (av_guess_format("mp4", nullptr, nullptr)) {
        result << "mp4";
    }
    if (av_guess_format("mov", nullptr, nullptr)) {
        result << "mov";
    }
    if (av_guess_format("mkv", nullptr, nullptr)) {
        result << "mkv";
    }
    if (av_guess_format("avi", nullptr, nullptr)) {
        result << "avi";
    }
    if (av_guess_format("webm", nullptr, nullptr)) {
        result << "webm";
    }
    if (av_guess_format("gif", nullptr, nullptr)) {
        result << "gif";
    }
    if (av_guess_format("apng", nullptr, nullptr)) {
        result << "apng";
    }
    if (av_guess_format("webp", nullptr, nullptr)) {
        result << "webp";
    }

    return result;
}

bool FFmpegEncoder::isImageSequenceFormatAvailable(const QString& format) {
    const QString fmt = format.toLower();
    if (fmt == "png" || fmt == "jpeg" || fmt == "jpg" || fmt == "tiff" || fmt == "bmp") {
        return true;
    }
    if (fmt == "exr") {
        // EXR は FFmpeg 標準では非対応の場合がある
        return false;
    }
    return false;
}

QStringList FFmpegEncoder::availableImageSequenceFormats() {
    return QStringList{"png", "jpeg", "tiff", "bmp"};
}

} // namespace ArtifactCore
