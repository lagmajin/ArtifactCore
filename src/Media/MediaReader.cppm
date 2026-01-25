module;
#define QT_NO_KEYWORDS

#include <QDebug>
#include <QMutex>
#include <QThread>
#include <tbb/tbb.h>
extern "C" {
#include <libavformat/avformat.h>



}

module MediaReader;

import std;
import MediaSource;

namespace ArtifactCore {

MediaReader::MediaReader(MediaSource* source)
    : mediaSource_(source) {
    if (mediaSource_ && mediaSource_->getFormatContext()) {
        AVFormatContext* ctx = mediaSource_->getFormatContext();
        for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
            if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex_ = i;
            } else if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audioStreamIndex_ = i;
            }
        }
    }
}

MediaReader::~MediaReader() {
    stop();
    // Clean up queues
    AVPacket* pkt;
    while (videoQueue_.try_pop(pkt)) {
        av_packet_free(&pkt);
    }
    while (audioQueue_.try_pop(pkt)) {
        av_packet_free(&pkt);
    }
}

void MediaReader::start() {
    if (isRunning_) return;
    isRunning_ = true;
    isPaused_ = false;
    taskGroup_.run([this]() { readLoop(); });
}

void MediaReader::pause() {
    QMutexLocker locker(&mutex_);
    isPaused_ = !isPaused_;
    if (!isPaused_) {
        condition_.wakeAll();
    }
}

void MediaReader::stop() {
    isRunning_ = false;
    isPaused_ = false;
    condition_.wakeAll();
    taskGroup_.wait();
}

AVPacket* MediaReader::getNextPacket(StreamType type) {
    AVPacket* pkt = nullptr;
    if (type == StreamType::Video) {
        videoQueue_.try_pop(pkt);
    } else if (type == StreamType::Audio) {
        audioQueue_.try_pop(pkt);
    }
    return pkt;
}

void MediaReader::readLoop() {
    if (!mediaSource_ || !mediaSource_->getFormatContext()) return;

    AVFormatContext* ctx = mediaSource_->getFormatContext();
    AVPacket* packet = av_packet_alloc();
    if (!packet) return;

    while (isRunning_) {
        {
            QMutexLocker locker(&mutex_);
            if (isPaused_) {
                condition_.wait(&mutex_);
                continue;
            }
        }

        if (av_read_frame(ctx, packet) < 0) {
            // End of file or error
            break;
        }

        if (packet->stream_index == videoStreamIndex_) {
            AVPacket* pkt = av_packet_clone(packet);
            videoQueue_.push(pkt);
        } else if (packet->stream_index == audioStreamIndex_) {
            AVPacket* pkt = av_packet_clone(packet);
            audioQueue_.push(pkt);
        }

        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    isRunning_ = false;
}

} // namespace ArtifactCore