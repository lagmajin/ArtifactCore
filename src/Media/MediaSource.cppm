module;

#include <QDebug>
#include <QString>
#include <QFileInfo>

extern "C" {
#include <libavformat/avformat.h>
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
module MediaSource;

namespace ArtifactCore {

static std::string av_strerror_string(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, errnum);
    return std::string(errbuf);
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
    const std::string pathUtf8 = QString(url).replace(QLatin1Char('\\'), QLatin1Char('/')).toUtf8().toStdString();

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
    int ret = avformat_open_input(&formatContext_, pathUtf8.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret);
        lastError_ = QStringLiteral("avformat_open_input failed: %1 (%2)")
                         .arg(QString::fromUtf8(errbuf))
                         .arg(ret);
        qCritical() << "[MediaSource] avformat_open_input failed:"
                    << "path=" << url
                    << "errcode=" << ret
                    << "msg=" << errbuf;
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
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret);
        lastError_ = QStringLiteral("avformat_find_stream_info failed: %1 (%2)")
                         .arg(QString::fromUtf8(errbuf))
                         .arg(ret);
        qCritical() << "[MediaSource] avformat_find_stream_info failed:"
                    << "path=" << url
                    << "msg=" << errbuf;
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

    int ret = av_seek_frame(formatContext_, streamIndex, ts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        qWarning() << "[MediaSource] av_seek_frame failed, retrying with avformat_seek_file"
                   << "timestampMs=" << timestampMs
                   << "err=" << av_strerror_string(ret).c_str();
        const int64_t offset = av_rescale_q(2, AVRational{1, 1}, stream->time_base);
        const int64_t minTs = std::max<int64_t>(0, ts - offset);
        const int64_t maxTs = ts + offset;
        ret = avformat_seek_file(formatContext_, streamIndex, minTs, ts, maxTs, AVSEEK_FLAG_BACKWARD);
    }

    if (ret < 0) {
        lastError_ = QStringLiteral("seek failed at %1 ms: %2")
                         .arg(timestampMs)
                         .arg(QString::fromStdString(av_strerror_string(ret)));
        qWarning() << "[MediaSource] seek failed:"
                   << "timestampMs=" << timestampMs
                   << "err=" << av_strerror_string(ret).c_str();
        return false;
    }

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

} // namespace ArtifactCore
