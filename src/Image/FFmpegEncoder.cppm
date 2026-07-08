module;
#include <utility>
#include <algorithm>
#include <cstring>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <OpenImageIO/imageio.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libavutil/mastering_display_metadata.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

module Encoder.FFmpegEncoder;
import Image;
import :Impl;

namespace {

QString ffmpegErrorString(int err)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_make_error_string(buffer, AV_ERROR_MAX_STRING_SIZE, err);
    return QString::fromUtf8(buffer);
}

// HW エンコーダー名を codec id と encoder name に分解
struct EncoderNameInfo {
    AVCodecID codecId = AV_CODEC_ID_NONE;
    QString encoderName;  // 空なら自動
};

// NVENC, AMF, QSV, VAAPI の短縮名マッピング
static QStringList knownHardwareEncodersFor(AVCodecID codecId)
{
    switch (codecId) {
    case AV_CODEC_ID_H264:
        return {"h264_nvenc", "h264_amf", "h264_qsv", "h264_vaapi"};
    case AV_CODEC_ID_HEVC:
        return {"hevc_nvenc", "hevc_amf", "hevc_qsv", "hevc_vaapi"};
    case AV_CODEC_ID_VP9:
        return {"vp9_qsv", "vp9_vaapi"};
    default:
        return {};
    }
}

} // namespace

namespace ArtifactCore {

class FFmpegEncoder::Impl {
public:
    Impl() {
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

        // コーデック検索 - HW エンコーダー優先
        AVCodecID codecId = AV_CODEC_ID_NONE;
        
        const QString codecLower = settings.videoCodec.toLower().trimmed();
        if (codecLower == "h264" || codecLower == "avc" || codecLower == "libx264" ||
            codecLower == "h264_nvenc" || codecLower == "h264_amf" || codecLower == "h264_qsv") {
            codecId = AV_CODEC_ID_H264;
        } else if (codecLower == "h265" || codecLower == "hevc" || codecLower == "libx265" ||
                   codecLower == "hevc_nvenc" || codecLower == "hevc_amf" || codecLower == "hevc_qsv") {
            codecId = AV_CODEC_ID_HEVC;
        } else if (codecLower == "prores" || codecLower == "apple_prores") {
            codecId = AV_CODEC_ID_PRORES;
        } else if (codecLower == "vp9" || codecLower == "libvpx-vp9" || codecLower == "vp9_qsv") {
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
            codecId = AV_CODEC_ID_H264;
        }

        // HW エンコーダーを探す
        const AVCodec* codec = nullptr;
        bool isHardwareEncoder = false;

        // 明示的なエンコーダー名が指定されていればそれを使う
        if (!settings.encoderName.isEmpty()) {
            codec = avcodec_find_encoder_by_name(settings.encoderName.toUtf8().constData());
            if (codec) {
                isHardwareEncoder = true;
            }
        }

        // preferHardware なら HW エンコーダー一覧から探す
        if (!codec && settings.preferHardware) {
            const auto hwEncoders = knownHardwareEncodersFor(codecId);
            for (const auto& name : hwEncoders) {
                codec = avcodec_find_encoder_by_name(name.toUtf8().constData());
                if (codec) {
                    isHardwareEncoder = true;
                    break;
                }
            }
        }

        // フォールバック: ソフトウェアエンコーダー
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

        // HDR 対応ピクセルフォーマットと色空間の決定
        const bool isHdr = (settings.hdrColorSpace != HDRColorSpace::SDR_BT709);

        codecCtx_->codec_id = codecId;
        codecCtx_->codec_type = AVMEDIA_TYPE_VIDEO;
        codecCtx_->width = width_;
        codecCtx_->height = height_;
        codecCtx_->time_base = AVRational{1, static_cast<int>(settings.fps)};
        codecCtx_->framerate = AVRational{static_cast<int>(settings.fps), 1};
        codecCtx_->gop_size = settings.gopSize;
        codecCtx_->max_b_frames = settings.maxBFrames;

        // ピクセルフォーマット設定（HDR 時は 10bit 以上を優先）
        if (codecId == AV_CODEC_ID_PRORES) {
            const QString profile = settings.profile.trimmed().toLower();
            if (profile.contains("4444")) {
                codecCtx_->pix_fmt = AV_PIX_FMT_YUVA444P10LE;
            } else {
                codecCtx_->pix_fmt = AV_PIX_FMT_YUV422P10;
            }
        } else if (isHdr && (codecId == AV_CODEC_ID_H264 || codecId == AV_CODEC_ID_HEVC || codecId == AV_CODEC_ID_VP9)) {
            // HDR 時は 10bit を試す。HEVC/V9 では P010 が一般的
            if (codecId == AV_CODEC_ID_HEVC) {
                codecCtx_->pix_fmt = AV_PIX_FMT_YUV420P10LE;       // HEVC 10bit
            } else if (codecId == AV_CODEC_ID_VP9) {
                codecCtx_->pix_fmt = AV_PIX_FMT_YUV420P10LE;       // VP9 10bit
            } else {
                codecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;            // H.264 10bit は限定的
            }
            // エンコーダーが 10bit 対応していない場合は 8bit にフォールバック
            if (codecCtx_->pix_fmt != AV_PIX_FMT_NONE && codec->pix_fmts) {
                bool found10 = false;
                for (int i = 0; codec->pix_fmts[i] != AV_PIX_FMT_NONE; ++i) {
                    if (codec->pix_fmts[i] == codecCtx_->pix_fmt) {
                        found10 = true;
                        break;
                    }
                }
                if (!found10) {
                    codecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;
                }
            }
        } else if (codecId == AV_CODEC_ID_MJPEG) {
            codecCtx_->pix_fmt = AV_PIX_FMT_YUVJ420P;
        } else if (codecId == AV_CODEC_ID_PNG) {
            codecCtx_->pix_fmt = AV_PIX_FMT_RGBA;
        } else if (codecId == AV_CODEC_ID_GIF) {
            codecCtx_->pix_fmt = AV_PIX_FMT_PAL8;
        } else if (codecId == AV_CODEC_ID_APNG || codecId == AV_CODEC_ID_WEBP) {
            codecCtx_->pix_fmt = AV_PIX_FMT_RGBA;
        } else {
            codecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;
        }

        // HW エンコーダーに最適なピクセルフォーマットを確認
        if (isHardwareEncoder && codecCtx_->pix_fmt == AV_PIX_FMT_YUV420P) {
            // HW エンコーダーは特定の pix_fmt を要求する
            if (codec->pix_fmts) {
                for (int i = 0; codec->pix_fmts[i] != AV_PIX_FMT_NONE; ++i) {
                    codecCtx_->pix_fmt = codec->pix_fmts[i];
                    break;
                }
            }
        }

        // ビットレート設定
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
            int proresProfile = 3;
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

        // HW エンコーダー固有オプション
        if (isHardwareEncoder) {
            const AVCodecHWConfig* hwConfig = nullptr;
            for (int i = 0; (hwConfig = avcodec_get_hw_config(codec, i)); ++i) {
                if (hwConfig->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX) {
                    break;
                }
            }
            // HW フレームコンテキストは未設定（sws_scale 経由でシステムメモリから入力）
            // NVENC はデフォルトでシステムメモリ入力を受け付ける
            av_opt_set(codecCtx_->priv_data, "preset", "p4", 0);
        }

        // HDR 色空間設定
        switch (settings.hdrColorSpace) {
        case HDRColorSpace::HDR10_PQ:
            codecCtx_->color_primaries = AVCOL_PRI_BT2020;
            codecCtx_->color_trc = AVCOL_TRC_SMPTEST2084;
            codecCtx_->colorspace = AVCOL_SPC_BT2020_NCL;
            codecCtx_->color_range = AVCOL_RANGE_MPEG;
            break;
        case HDRColorSpace::HLG_BT2020:
            codecCtx_->color_primaries = AVCOL_PRI_BT2020;
            codecCtx_->color_trc = AVCOL_TRC_ARIB_STD_B67;
            codecCtx_->colorspace = AVCOL_SPC_BT2020_NCL;
            codecCtx_->color_range = AVCOL_RANGE_MPEG;
            break;
        case HDRColorSpace::SDR_BT709:
        default:
            codecCtx_->color_range = AVCOL_RANGE_MPEG;
            codecCtx_->color_primaries = AVCOL_PRI_BT709;
            codecCtx_->color_trc = AVCOL_TRC_BT709;
            codecCtx_->colorspace = AVCOL_SPC_BT709;
            break;
        }

        if (const int ret = avcodec_open2(codecCtx_, codec, nullptr); ret < 0) {
            lastError_ = QStringLiteral("Failed to open video encoder: %1 (%2)").arg(settings.videoCodec, ffmpegErrorString(ret));
            return false;
        }

        // ストリームにコーデックパラメーターをコピー
        if (const int ret = avcodec_parameters_from_context(stream_->codecpar, codecCtx_); ret < 0) {
            lastError_ = QStringLiteral("Failed to copy codec parameters to stream: %1").arg(ffmpegErrorString(ret));
            return false;
        }

        // ファイルオープン
        if (!(fmt->flags & AVFMT_NOFILE)) {
            const int ret = avio_open(&fmtCtx_->pb, outputPath.toUtf8().constData(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                lastError_ = QStringLiteral("Failed to open output file: %1 (%2)").arg(outputPath, ffmpegErrorString(ret));
                return false;
            }
        }

        // ヘッダー書き込み
        if (const int ret = avformat_write_header(fmtCtx_, nullptr); ret < 0) {
            lastError_ = QStringLiteral("Failed to write header to: %1 (%2)").arg(outputPath, ffmpegErrorString(ret));
            return false;
        }

        // スケーラー作成（RGBA → 各コーデックのピクセルフォーマット変換用）
        const AVPixelFormat dstPixFmt = codecCtx_->pix_fmt;
        swsCtx_ = sws_getContext(
            width_, height_, AV_PIX_FMT_RGBA,
            width_, height_, dstPixFmt,
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
        frame_->format = dstPixFmt;
        frame_->width = width_;
        frame_->height = height_;

        const int ret = av_frame_get_buffer(frame_, 32);
        if (ret < 0) {
            lastError_ = "Failed to allocate frame buffer";
            return false;
        }

        // パケット作成
        packet_ = av_packet_alloc();
        if (!packet_) {
            lastError_ = "Failed to allocate packet";
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
        useOiioSequence_ = false;
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

        if (imageFormat_ == QStringLiteral("exr")) {
            useOiioSequence_ = true;
            outputPathPattern_ = outputPathPattern;
            isOpen_ = true;
            lastError_.clear();
            return true;
        }

        // 出力パターン保存
        outputPathPattern_ = outputPathPattern;

        if (fmt == "png") {
            compressionLevel_ = std::clamp(settings.compressionLevel, 0, 9);
        } else if (fmt == "tiff") {
            compressionLevel_ = std::clamp(settings.compressionLevel, 1, 9);
        } else if (fmt == "jpeg") {
            jpegQuality_ = std::clamp(settings.jpegQuality, 1, 100);
        }

        if (settings.is32bit && fmt == "exr") {
            dstPixFmt_ = AV_PIX_FMT_RGBF32LE;
        } else if (settings.is16bit && (fmt == "png" || fmt == "tiff")) {
            dstPixFmt_ = AV_PIX_FMT_RGB48LE;
        } else {
            dstPixFmt_ = AV_PIX_FMT_RGB24;
        }

        swsCtx_ = sws_getContext(
            width_, height_, AV_PIX_FMT_RGBA,
            width_, height_, dstPixFmt_,
            SWS_BILINEAR,
            nullptr, nullptr, nullptr);

        if (!swsCtx_) {
            lastError_ = "Failed to create sws context";
            return false;
        }

        frame_ = av_frame_alloc();
        if (!frame_) {
            lastError_ = "Failed to allocate frame";
            return false;
        }
        frame_->format = dstPixFmt_;
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

        if (isImageSequence_) {
            return addImageSequenceFrame(image);
        }

        if (!codecCtx_) {
            lastError_ = "Video encoder not initialized";
            return false;
        }

        const float* srcData = image.rgba32fData();
        if (!srcData) {
            lastError_ = "Failed to access RGBA32F image data";
            return false;
        }

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

        // HDR 対応: ノミナルピーク値を使って float → uint8/uint16 変換
        uint8_t* dst = rgbaFrame->data[0];
        const bool isHdr = (settings_.hdrColorSpace != HDRColorSpace::SDR_BT709);
        const float peak = isHdr ? static_cast<float>(settings_.hdrNominalPeak / 100.0) : 1.0f;

        if (codecCtx_->pix_fmt == AV_PIX_FMT_YUV420P10LE) {
            // 10bit YUV: float → 10bit スケーリング（HDR: [0, peak] → [0, 1023]）
            uint16_t* dst16 = reinterpret_cast<uint16_t*>(dst);
            for (int i = 0; i < w * h; ++i) {
                dst16[i * 4 + 0] = static_cast<uint16_t>(std::clamp((srcData[i * 4 + 0] / peak) * 1023.0f, 0.0f, 1023.0f));
                dst16[i * 4 + 1] = static_cast<uint16_t>(std::clamp((srcData[i * 4 + 1] / peak) * 1023.0f, 0.0f, 1023.0f));
                dst16[i * 4 + 2] = static_cast<uint16_t>(std::clamp((srcData[i * 4 + 2] / peak) * 1023.0f, 0.0f, 1023.0f));
                dst16[i * 4 + 3] = static_cast<uint16_t>(std::clamp((srcData[i * 4 + 3]) * 1023.0f, 0.0f, 1023.0f));
            }
        } else {
            // 8bit: float → uint8（HDR は peak で正規化）
            for (int i = 0; i < w * h; ++i) {
                dst[i * 4 + 0] = static_cast<uint8_t>(std::clamp((srcData[i * 4 + 0] / peak) * 255.0f, 0.0f, 255.0f));
                dst[i * 4 + 1] = static_cast<uint8_t>(std::clamp((srcData[i * 4 + 1] / peak) * 255.0f, 0.0f, 255.0f));
                dst[i * 4 + 2] = static_cast<uint8_t>(std::clamp((srcData[i * 4 + 2] / peak) * 255.0f, 0.0f, 255.0f));
                dst[i * 4 + 3] = static_cast<uint8_t>(std::clamp(srcData[i * 4 + 3] * 255.0f, 0.0f, 255.0f));
            }
        }

        if (av_frame_make_writable(frame_) < 0) {
            av_frame_free(&rgbaFrame);
            lastError_ = "Failed to make frame writable";
            return false;
        }

        uint8_t* srcDataPtr[1] = {dst};
        const int srcLinesize[1] = {w * 4};
        sws_scale(swsCtx_, srcDataPtr, srcLinesize, 0, h, frame_->data, frame_->linesize);

        av_frame_free(&rgbaFrame);

        frame_->pts = frameIndex_++;

        int ret = avcodec_send_frame(codecCtx_, frame_);
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

            packet_->stream_index = stream_->index;
            packet_->pts = packet_->pts * stream_->time_base.den / codecCtx_->time_base.den;
            packet_->dts = packet_->dts * stream_->time_base.den / codecCtx_->time_base.den;
            packet_->duration = packet_->duration * stream_->time_base.den / codecCtx_->time_base.den;

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

    bool addImage(const QImage& image) {
        if (!isOpen_ || !swsCtx_) {
            lastError_ = "Encoder is not open";
            return false;
        }

        QImage rgba = image;
        if (rgba.format() != QImage::Format_RGBA8888) {
            rgba = rgba.convertToFormat(QImage::Format_RGBA8888);
        }

        const int w = rgba.width();
        const int h = rgba.height();
        if (w != width_ || h != height_) {
            lastError_ = QStringLiteral("Image size mismatch: expected %1x%2, got %3x%4")
                .arg(width_).arg(height_).arg(w).arg(h);
            return false;
        }

        if (isImageSequence_) {
            ImageF32x4_RGBA floatImage;
            floatImage.setFromRGBA8(rgba.constBits(), rgba.width(), rgba.height());
            return addImage(floatImage);
        }

        if (!codecCtx_) {
            lastError_ = "Video encoder not initialized";
            return false;
        }

        if (av_frame_make_writable(frame_) < 0) {
            lastError_ = "Failed to make frame writable";
            return false;
        }

        const uint8_t* srcDataPtr[1] = { rgba.constBits() };
        const int srcLinesize[1] = { static_cast<int>(rgba.bytesPerLine()) };
        sws_scale(swsCtx_, srcDataPtr, srcLinesize, 0, h, frame_->data, frame_->linesize);

        frame_->pts = frameIndex_++;

        int ret = avcodec_send_frame(codecCtx_, frame_);
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

            packet_->stream_index = stream_->index;
            packet_->pts = packet_->pts * stream_->time_base.den / codecCtx_->time_base.den;
            packet_->dts = packet_->dts * stream_->time_base.den / codecCtx_->time_base.den;
            packet_->duration = packet_->duration * stream_->time_base.den / codecCtx_->time_base.den;

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
        const QString frameNumStr = QString::number(currentFrameNum_).rightJustified(imageSeqSettings_.padding, '0');
        QString outputPath = outputPathPattern_;
        outputPath.replace("%04d", frameNumStr);
        outputPath.replace("%d", frameNumStr);

        if (!outputPath.contains(".")) {
            outputPath += "." + imageFormat_;
        }

        if (useOiioSequence_) {
            const float* srcData = image.rgba32fData();
            if (!srcData) {
                lastError_ = "Failed to access RGBA32F image data";
                return false;
            }

            using namespace OIIO;
            const QByteArray outputPathUtf8 = outputPath.toUtf8();
            std::unique_ptr<ImageOutput> out = ImageOutput::create(outputPathUtf8.constData());
            if (!out) {
                lastError_ = QStringLiteral("Failed to create EXR output: %1").arg(outputPath);
                return false;
            }

            ImageSpec spec(image.width(), image.height(), 4, TypeDesc::FLOAT);
            spec.channelnames = {"R", "G", "B", "A"};
            if (!out->open(outputPathUtf8.constData(), spec)) {
                lastError_ = QStringLiteral("Failed to open EXR output: %1 (%2)")
                                 .arg(outputPath, QString::fromUtf8(out->geterror()));
                return false;
            }

            if (!out->write_image(TypeDesc::FLOAT, srcData)) {
                lastError_ = QStringLiteral("Failed to write EXR frame: %1")
                                 .arg(QString::fromUtf8(out->geterror()));
                out->close();
                return false;
            }

            if (!out->close()) {
                lastError_ = QStringLiteral("Failed to close EXR output: %1")
                                 .arg(QString::fromUtf8(out->geterror()));
                return false;
            }

            currentFrameNum_++;
            return true;
        }

        const float* srcData = image.rgba32fData();
        if (!srcData) {
            lastError_ = "Failed to access RGBA32F image data";
            return false;
        }
        const int w = image.width();
        const int h = image.height();

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

        if (dstPixFmt_ == AV_PIX_FMT_RGB48LE) {
            uint16_t* dst = reinterpret_cast<uint16_t*>(rgbaFrame->data[0]);
            for (int i = 0; i < w * h * 4; ++i) {
                dst[i] = static_cast<uint16_t>(std::clamp(srcData[i] * 65535.0f, 0.0f, 65535.0f));
            }
        } else if (dstPixFmt_ == AV_PIX_FMT_RGBF32LE) {
            float* dst = reinterpret_cast<float*>(rgbaFrame->data[0]);
            for (int i = 0; i < w * h * 4; ++i) {
                dst[i] = srcData[i];
            }
        } else {
            uint8_t* dst = rgbaFrame->data[0];
            for (int i = 0; i < w * h * 4; ++i) {
                dst[i] = static_cast<uint8_t>(std::clamp(srcData[i] * 255.0f, 0.0f, 255.0f));
            }
        }

        if (av_frame_make_writable(frame_) < 0) {
            av_frame_free(&rgbaFrame);
            lastError_ = "Failed to make frame writable";
            return false;
        }

        uint8_t* srcDataPtr[1] = {rgbaFrame->data[0]};
        const int srcLinesize[1] = {w * 4};
        sws_scale(swsCtx_, srcDataPtr, srcLinesize, 0, h, frame_->data, frame_->linesize);

        av_frame_free(&rgbaFrame);

        const QString fmt = imageFormat_;
        AVCodecID codecId = AV_CODEC_ID_NONE;
        if (fmt == "png") codecId = AV_CODEC_ID_PNG;
        else if (fmt == "jpeg") codecId = AV_CODEC_ID_MJPEG;
        else if (fmt == "tiff") codecId = AV_CODEC_ID_TIFF;
        else if (fmt == "bmp") codecId = AV_CODEC_ID_BMP;
        else if (fmt == "exr") {
            codecId = AV_CODEC_ID_TIFF;
        }

        const AVCodec* codec = avcodec_find_encoder(codecId);
        if (!codec) {
            lastError_ = QStringLiteral("Failed to find encoder for format: %1").arg(fmt);
            return false;
        }

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

        if (fmt == "png" || fmt == "tiff") {
            imgCodecCtx->compression_level = compressionLevel_;
        } else if (fmt == "jpeg") {
            imgCodecCtx->global_quality = jpegQuality_ * FF_QP2LAMBDA;
            imgCodecCtx->flags |= AV_CODEC_FLAG_QSCALE;
        }

        if (const int ret = avcodec_open2(imgCodecCtx, codec, nullptr); ret < 0) {
            avcodec_free_context(&imgCodecCtx);
            lastError_ = QStringLiteral("Failed to open image encoder for: %1 (%2)").arg(fmt, ffmpegErrorString(ret));
            return false;
        }

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
        if (!isOpen_) {
            return;
        }

        if (codecCtx_) {
            avcodec_send_frame(codecCtx_, nullptr);
            while (avcodec_receive_packet(codecCtx_, packet_) >= 0) {
                packet_->stream_index = stream_->index;
                av_interleaved_write_frame(fmtCtx_, packet_);
                av_packet_unref(packet_);
            }
        }

        if (fmtCtx_) {
            av_write_trailer(fmtCtx_);
        }

        if (packet_) {
            av_packet_free(&packet_);
        }
        if (frame_) {
            av_frame_free(&frame_);
        }
        if (swsCtx_) {
            sws_freeContext(swsCtx_);
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
        packet_ = nullptr;
        isOpen_ = false;
        useOiioSequence_ = false;
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
    AVPacket* packet_ = nullptr;
    SwsContext* swsCtx_ = nullptr;

    int width_ = 0;
    int height_ = 0;
    int frameIndex_ = 0;
    bool isOpen_ = false;
    QString lastError_;

    FFmpegEncoderSettings settings_;

    FFmpegImageSequenceSettings imageSeqSettings_;
    bool isImageSequence_ = false;
    bool useOiioSequence_ = false;
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

bool FFmpegEncoder::addImage(const QImage& image) {
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

bool FFmpegEncoder::isEncoderAvailableByName(const QString& encoderName) {
    const QByteArray name = encoderName.trimmed().toUtf8();
    if (name.isEmpty()) {
        return false;
    }
    return avcodec_find_encoder_by_name(name.constData()) != nullptr;
}

QStringList FFmpegEncoder::availableVideoCodecs() {
    QStringList result;

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

bool FFmpegEncoder::isContainerAvailable(const QString& containerName) {
    const AVOutputFormat* fmt = av_guess_format(
        containerName.toUtf8().constData(),
        nullptr,
        nullptr);
    return fmt != nullptr;
}

QStringList FFmpegEncoder::availableContainers() {
    QStringList result;

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
    if (fmt == "png" || fmt == "jpeg" || fmt == "jpg" || fmt == "tiff" || fmt == "bmp" || fmt == "exr") {
        return true;
    }
    return false;
}

QStringList FFmpegEncoder::availableImageSequenceFormats() {
    return QStringList{"png", "jpeg", "tiff", "bmp", "exr"};
}

} // namespace ArtifactCore
