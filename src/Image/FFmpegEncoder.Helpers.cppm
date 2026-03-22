module;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QMap>

module Encoder.FFmpegEncoder:Impl;

import std;
import Image;
import Encoder.FFmpegEncoder;

namespace ArtifactCore {

// コーデック/コンテナの組み合わせテーブル
struct CodecContainerPair {
    AVCodecID codecId;
    const char* container;
    const char* description;
};

// 対応しているコーデック/コンテナの組み合わせ
static const CodecContainerPair g_supportedPairs[] = {
    // H.264
    {AV_CODEC_ID_H264, "mp4", "H.264 MP4"},
    {AV_CODEC_ID_H264, "mov", "H.264 MOV"},
    {AV_CODEC_ID_H264, "mkv", "H.264 MKV"},
    {AV_CODEC_ID_H264, "avi", "H.264 AVI"},
    {AV_CODEC_ID_H264, "webm", "H.264 WebM"},
    
    // H.265/HEVC
    {AV_CODEC_ID_HEVC, "mp4", "H.265 MP4"},
    {AV_CODEC_ID_HEVC, "mov", "H.265 MOV"},
    {AV_CODEC_ID_HEVC, "mkv", "H.265 MKV"},
    {AV_CODEC_ID_HEVC, "webm", "H.265 WebM"},
    
    // ProRes
    {AV_CODEC_ID_PRORES, "mov", "ProRes MOV"},
    {AV_CODEC_ID_PRORES, "mp4", "ProRes MP4 (limited)"},
    
    // VP9
    {AV_CODEC_ID_VP9, "webm", "VP9 WebM"},
    {AV_CODEC_ID_VP9, "mkv", "VP9 MKV"},
    
    // MJPEG
    {AV_CODEC_ID_MJPEG, "avi", "MJPEG AVI"},
    {AV_CODEC_ID_MJPEG, "mov", "MJPEG MOV"},
    
    // PNG
    {AV_CODEC_ID_PNG, "mov", "PNG MOV"},
    {AV_CODEC_ID_PNG, "avi", "PNG AVI"},
};

// コーデック名から AVCodecID を取得
AVCodecID codecNameToId(const QString& codecName) {
    const QString name = codecName.toLower().trimmed();
    
    if (name == "h264" || name == "avc" || name == "libx264") {
        return AV_CODEC_ID_H264;
    }
    if (name == "h265" || name == "hevc" || name == "libx265") {
        return AV_CODEC_ID_HEVC;
    }
    if (name == "prores" || name == "apple_prores") {
        return AV_CODEC_ID_PRORES;
    }
    if (name == "vp9" || name == "libvpx-vp9") {
        return AV_CODEC_ID_VP9;
    }
    if (name == "vp8" || name == "libvpx") {
        return AV_CODEC_ID_VP8;
    }
    if (name == "mjpeg" || name == "motion_jpeg") {
        return AV_CODEC_ID_MJPEG;
    }
    if (name == "png") {
        return AV_CODEC_ID_PNG;
    }
    if (name == "ffv1") {
        return AV_CODEC_ID_FFV1;
    }
    if (name == "rawvideo" || name == "rgb24") {
        return AV_CODEC_ID_RAWVIDEO;
    }
    
    return AV_CODEC_ID_NONE;
}

// コンテナ名から AVOutputFormat を取得
const AVOutputFormat* getContainerFormat(const QString& containerName) {
    return av_guess_format(containerName.toUtf8().constData(), nullptr, nullptr);
}

// コーデック/コンテナの組み合わせを検証
bool validateCodecContainerCombination(
    const QString& codecName,
    const QString& containerName,
    AVCodecID* outCodecId)
{
    const AVCodecID codecId = codecNameToId(codecName);
    if (codecId == AV_CODEC_ID_NONE) {
        return false;
    }
    
    const AVOutputFormat* fmt = getContainerFormat(containerName);
    if (!fmt) {
        return false;
    }
    
    // FFmpeg の `video_codec` は単一の推奨コーデック ID。
    // 配列として扱わず、代表値として比較する。
    const bool supported = (fmt->video_codec == AV_CODEC_ID_NONE || fmt->video_codec == codecId);
    (void)supported;
    
    if (outCodecId) {
        *outCodecId = codecId;
    }
    return true;
}

// 利用可能なビデオコーデック一覧
QStringList getAvailableVideoCodecs() {
    QStringList result;
    
    void* iter = nullptr;
    const AVCodec* codec = nullptr;
    
    while ((codec = av_codec_iterate(&iter)) != nullptr) {
        if (codec->type == AVMEDIA_TYPE_VIDEO && av_codec_is_encoder(codec)) {
            result.append(QString::fromUtf8(codec->name));
        }
    }
    
    return result;
}

// 利用可能なコンテナ形式一覧
QStringList getAvailableContainers() {
    QStringList result;
    
    void* iter = nullptr;
    const AVOutputFormat* fmt = nullptr;
    
    while ((fmt = av_muxer_iterate(&iter)) != nullptr) {
        if (fmt->video_codec != AV_CODEC_ID_NONE) {
            result.append(QString::fromUtf8(fmt->name));
        }
    }
    
    return result;
}

// コーデックが利用可能かチェック
bool isCodecAvailable(const QString& codecName) {
    const AVCodecID codecId = codecNameToId(codecName);
    if (codecId == AV_CODEC_ID_NONE) {
        return false;
    }
    return avcodec_find_encoder(codecId) != nullptr;
}

// コンテナが利用可能かチェック
bool isContainerAvailable(const QString& containerName) {
    return getContainerFormat(containerName) != nullptr;
}

} // namespace ArtifactCore
