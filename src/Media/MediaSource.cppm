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

    // [Fix 1] Windows バックスラッシュを / に正規化して FFmpeg に渡す
    const std::string pathUtf8 = QString(url).replace(QLatin1Char('\\'), QLatin1Char('/')).toUtf8().toStdString();

    // [Fix 2] FFmpeg に渡す前にファイル存在を確認（エラー理由を明確化）
    if (!QFileInfo::exists(url)) {
        qCritical() << "[MediaSource] File does not exist:" << url;
        return false;
    }

    formatContext_ = avformat_alloc_context();
    if (!formatContext_) {
        qCritical() << "[MediaSource] Failed to allocate AVFormatContext.";
        return false;
    }

    // [Fix 3] avformat_open_input のエラーコードを av_strerror で出力
    int ret = avformat_open_input(&formatContext_, pathUtf8.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret);
        qCritical() << "[MediaSource] avformat_open_input failed:"
                    << "path=" << url
                    << "errcode=" << ret
                    << "msg=" << errbuf;
        avformat_free_context(formatContext_);
        formatContext_ = nullptr;
        return false;
    }

    // [Fix 4] probesize / analyzeduration に上限を設けてタイムアウトを防ぐ
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "probesize",       "2000000", 0); // 2 MB
    av_dict_set(&opts, "analyzeduration", "2000000", 0); // 2 秒 (μs)
    ret = avformat_find_stream_info(formatContext_, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret);
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
    return true;
}

bool MediaSource::seek(int64_t timestampMs) {
    if (!formatContext_) return false;

    // Assume seeking to the first video stream
    int streamIndex = -1;
    for (unsigned int i = 0; i < formatContext_->nb_streams; ++i) {
        if (formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            streamIndex = i;
            break;
        }
    }
    if (streamIndex == -1) return false;

    AVStream* stream = formatContext_->streams[streamIndex];
    int64_t ts = av_rescale_q(timestampMs, AVRational{1, 1000}, stream->time_base);

    if (av_seek_frame(formatContext_, streamIndex, ts, AVSEEK_FLAG_BACKWARD) < 0) {
        qWarning() << "MediaSource::seek: av_seek_frame failed.";
        return false;
    }

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
    qDebug() << "MediaSource::close: Resources released.";
}

} // namespace ArtifactCore