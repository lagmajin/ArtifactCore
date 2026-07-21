module;

#include <QDebug>
#include <QString>
#include <QFileInfo>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
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
module MediaSource;

namespace ArtifactCore {

static QString av_strerror_string(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, errnum);
    return QString::fromUtf8(errbuf);
}

MediaSource::MediaSource() {
    // Initialize if needed
}

MediaSource::~MediaSource() {
    close();
}

bool MediaSource::open(const QString& url) {
    close();
    url_ = url;
    lastError_.clear();

    // [Fix 1] Windows バックスラッシュを / に正規化して FFmpeg に渡す
    QString normalizedUrl = url;
    const QByteArray pathUtf8 = normalizedUrl.replace(QLatin1Char('\\'), QLatin1Char('/')).toUtf8();

    // [Fix 2] FFmpeg に渡す前にファイル存在を確認（エラー理由を明確化）
    if (!QFileInfo::exists(url)) {
        lastError_ = QStringLiteral("File does not exist: %1").arg(url);
        qCritical() << "[MediaSource] File does not exist:" << url;
        return false;
    }

    formatContext_ = avformat_alloc_context();
    if (!formatContext_) {
        lastError_ = QStringLiteral("Failed to allocate AVFormatContext");
        qCritical() << "[MediaSource] Failed to allocate AVFormatContext.";
        return false;
    }

    // [Fix 3] avformat_open_input のエラーコードを av_strerror で出力
    int ret = avformat_open_input(&formatContext_, pathUtf8.constData(), nullptr, nullptr);
    if (ret < 0) {
        lastError_ = QStringLiteral("avformat_open_input failed: %1 (%2)")
                         .arg(av_strerror_string(ret))
                         .arg(ret);
        qCritical() << "[MediaSource] avformat_open_input failed:"
                    << "path=" << url
                    << "errcode=" << ret
                    << "msg=" << av_strerror_string(ret);
        avformat_free_context(formatContext_);
        formatContext_ = nullptr;
        return false;
    }

    // Constrain stream probing: limit probe duration and disable decoder threading.
    // probesize/analyzeduration are format-level; set on the context directly.
    formatContext_->probesize = 2000000;
    formatContext_->max_analyze_duration = 2000000;
    // Build a per-stream codec options array — avformat_find_stream_info expects nb_streams entries.
    {
        const unsigned nbStreams = formatContext_->nb_streams;
        std::vector<AVDictionary*> streamOpts(nbStreams, nullptr);
        for (unsigned i = 0; i < nbStreams; ++i) {
            av_dict_set(&streamOpts[i], "threads", "1", 0);
            av_dict_set(&streamOpts[i], "thread_type", "0", 0);
        }
        ret = avformat_find_stream_info(formatContext_, nbStreams > 0 ? streamOpts.data() : nullptr);
        for (auto& d : streamOpts) av_dict_free(&d);
    }
    if (ret < 0) {
        lastError_ = QStringLiteral("avformat_find_stream_info failed: %1 (%2)")
                         .arg(av_strerror_string(ret))
                         .arg(ret);
        qCritical() << "[MediaSource] avformat_find_stream_info failed:"
                    << "path=" << url
                    << "msg=" << av_strerror_string(ret);
        avformat_close_input(&formatContext_);
        formatContext_ = nullptr;
        return false;
    }

    qDebug() << "[MediaSource] Opened:" << url
             << "streams=" << formatContext_->nb_streams
             << "duration_ms=" << (formatContext_->duration / 1000);
    lastError_.clear();
    return true;
}

bool MediaSource::seek(int64_t timestampMs) {
    if (!formatContext_) {
        lastError_ = QStringLiteral("seek failed: media is not open");
        return false;
    }

    // Assume seeking to the first video stream
    int streamIndex = -1;
    for (unsigned int i = 0; i < formatContext_->nb_streams; ++i) {
        if (formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            streamIndex = i;
            break;
        }
    }
    if (streamIndex == -1) {
        lastError_ = QStringLiteral("seek failed: no video stream available");
        qWarning() << "[MediaSource] seek failed: no video stream available";
        return false;
    }

    AVStream* stream = formatContext_->streams[streamIndex];
    int64_t ts = av_rescale_q(timestampMs, AVRational{1, 1000}, stream->time_base);
    const int64_t streamOrigin =
        stream->start_time != AV_NOPTS_VALUE ? stream->start_time : 0;
    ts += streamOrigin;

    int ret = av_seek_frame(formatContext_, streamIndex, ts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        qWarning() << "[MediaSource] av_seek_frame failed, retrying with avformat_seek_file"
                   << "timestampMs=" << timestampMs
                   << "err=" << av_strerror_string(ret);
        const int64_t offset = av_rescale_q(2, AVRational{1, 1}, stream->time_base);
        const int64_t minTs = std::max<int64_t>(streamOrigin, ts - offset);
        const int64_t maxTs = ts + offset;
        ret = avformat_seek_file(formatContext_, streamIndex, minTs, ts, maxTs, AVSEEK_FLAG_BACKWARD);
    }

    if (ret < 0) {
        lastError_ = QStringLiteral("seek failed at %1 ms: %2")
                         .arg(timestampMs)
                         .arg(av_strerror_string(ret));
        qWarning() << "[MediaSource] seek failed:"
                   << "timestampMs=" << timestampMs
                   << "err=" << av_strerror_string(ret);
        return false;
    }

    // Discard demuxer-side buffered packets before the decoder is flushed by
    // the caller.  Otherwise a seek can feed packets from the previous cursor
    // into the new decode sequence.
    avformat_flush(formatContext_);
    lastError_.clear();
    return true;
}

void MediaSource::close() {
    if (formatContext_) {
        avformat_close_input(&formatContext_);
        formatContext_ = nullptr;
    }
    if (ioContext_) {
        avio_context_free(&ioContext_);
        ioContext_ = nullptr;
    }
    url_.clear();
    lastError_.clear();
    qDebug() << "MediaSource::close: Resources released.";
}

// ---------------------------------------------------------------------------
// Video codec probe — lightweight format-level check for editing suitability
// ---------------------------------------------------------------------------

static bool isEditingFriendlyCodec(AVCodecID id) {
    switch (id) {
    // Intra-frame / mezzanine codecs well suited for editing
    case AV_CODEC_ID_PRORES:
    case AV_CODEC_ID_DNXHD:
    case AV_CODEC_ID_FFV1:
    case AV_CODEC_ID_UTVIDEO:
    case AV_CODEC_ID_MAGICYUV:
    case AV_CODEC_ID_HUFFYUV:
    case AV_CODEC_ID_FFVHUFF:
    case AV_CODEC_ID_RAWVIDEO:
    case AV_CODEC_ID_MJPEG:
    case AV_CODEC_ID_MJPEGB:
    case AV_CODEC_ID_BMP:
    case AV_CODEC_ID_PNG:
    case AV_CODEC_ID_TIFF:
    case AV_CODEC_ID_DPX:
    case AV_CODEC_ID_TARGA:
        return true;
    default:
        return false;
    }
}

static bool isLongGopCodec(AVCodecID id) {
    switch (id) {
    // Long-GOP inter-frame codecs that hinder editing performance
    case AV_CODEC_ID_H264:
    case AV_CODEC_ID_HEVC:
    case AV_CODEC_ID_VP9:
    case AV_CODEC_ID_AV1:
    case AV_CODEC_ID_MPEG4:
    case AV_CODEC_ID_MPEG2VIDEO:
    case AV_CODEC_ID_VC1:
    case AV_CODEC_ID_WMV3:
    case AV_CODEC_ID_VP8:
    case AV_CODEC_ID_H263:
        return true;
    default:
        return false;
    }
}

VideoProbeResult probeVideoFile(const QString& filePath) {
    VideoProbeResult result;

    QString normalizedPath = filePath;
    QByteArray pathUtf8 = normalizedPath.replace(QLatin1Char('\\'), QLatin1Char('/')).toUtf8();

    AVFormatContext* fmtCtx = avformat_alloc_context();
    if (!fmtCtx) {
        result.probeError = QStringLiteral("Failed to allocate AVFormatContext");
        return result;
    }

    int ret = avformat_open_input(&fmtCtx, pathUtf8.constData(), nullptr, nullptr);
    if (ret < 0) {
        result.probeError = QStringLiteral("avformat_open_input failed: %1").arg(av_strerror_string(ret));
        avformat_free_context(fmtCtx);
        return result;
    }

    fmtCtx->probesize = 512000;
    fmtCtx->max_analyze_duration = 1000000;

    ret = avformat_find_stream_info(fmtCtx, nullptr);
    if (ret < 0) {
        result.probeError = QStringLiteral("avformat_find_stream_info failed");
        avformat_close_input(&fmtCtx);
        return result;
    }

    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        AVStream* st = fmtCtx->streams[i];
        if (!st || !st->codecpar)
            continue;
        if (st->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
            continue;

        result.hasVideoStream = true;
        result.codecName = QString::fromUtf8(avcodec_get_name(st->codecpar->codec_id));

        if (isEditingFriendlyCodec(st->codecpar->codec_id)) {
            result.isEditingFriendly = true;
        } else if (isLongGopCodec(st->codecpar->codec_id)) {
            result.isEditingFriendly = false;
        } else {
            // Unknown codec — assume editing-friendly to avoid false alarms
            result.isEditingFriendly = true;
        }
        break;
    }

    avformat_close_input(&fmtCtx);
    return result;
}

} // namespace ArtifactCore
