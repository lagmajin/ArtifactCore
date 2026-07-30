module;
#define QT_NO_KEYWORDS

#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
extern "C" {
#include <libavformat/avformat.h>



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
module MediaReader;

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
    QMutexLocker locker(&mutex_);
    while (!videoQueue_.empty()) {
        AVPacket* pkt = videoQueue_.front();
        videoQueue_.pop_front();
        av_packet_free(&pkt);
    }
    while (!audioQueue_.empty()) {
        AVPacket* pkt = audioQueue_.front();
        audioQueue_.pop_front();
        av_packet_free(&pkt);
    }
}

void MediaReader::start() {
    if (isRunning_) return;
    isRunning_ = true;
    isPaused_ = false;
    workerThread_ = std::thread([this]() { readLoop(); });
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
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

AVPacket* MediaReader::getNextPacket(StreamType type) {
    QMutexLocker locker(&mutex_);
    std::deque<AVPacket*>* queue = nullptr;
    if (type == StreamType::Video) {
        queue = &videoQueue_;
    } else if (type == StreamType::Audio) {
        queue = &audioQueue_;
    }
    if (!queue || queue->empty()) {
        return nullptr;
    }
    AVPacket* pkt = queue->front();
    queue->pop_front();
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
            QMutexLocker locker(&mutex_);
            videoQueue_.push_back(pkt);
        } else if (packet->stream_index == audioStreamIndex_) {
            AVPacket* pkt = av_packet_clone(packet);
            QMutexLocker locker(&mutex_);
            audioQueue_.push_back(pkt);
        }

        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    isRunning_ = false;
}

} // namespace ArtifactCore
