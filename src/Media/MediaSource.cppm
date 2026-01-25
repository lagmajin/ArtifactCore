module;

#include <QDebug>
#include <QString>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

module MediaSource;

import std;

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

    formatContext_ = avformat_alloc_context();
    if (!formatContext_) {
        qWarning() << "MediaSource::open: Failed to allocate AVFormatContext.";
        return false;
    }

    if (avformat_open_input(&formatContext_, url.toUtf8().constData(), nullptr, nullptr) < 0) {
        qWarning() << "MediaSource::open: Failed to open input:" << url;
        avformat_free_context(formatContext_);
        formatContext_ = nullptr;
        return false;
    }

    if (avformat_find_stream_info(formatContext_, nullptr) < 0) {
        qWarning() << "MediaSource::open: Failed to find stream info.";
        avformat_close_input(&formatContext_);
        formatContext_ = nullptr;
        return false;
    }

    qDebug() << "MediaSource::open: Successfully opened:" << url;
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