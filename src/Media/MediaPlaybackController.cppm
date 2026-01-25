module;

#include <QDebug>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

module MediaPlaybackController;

import std;
import MediaSource;
import MediaReader;
import MediaImageFrameDecoder;
import MediaAudioDecoder;

namespace ArtifactCore {

MediaPlaybackController::MediaPlaybackController()
    : mediaSource_(new MediaSource()),
      mediaReader_(new MediaReader(mediaSource_)),
      videoDecoder_(new MediaImageFrameDecoder()),
      audioDecoder_(new MediaAudioDecoder()) {}

MediaPlaybackController::~MediaPlaybackController() {
    closeMedia();
    delete mediaReader_;
    delete mediaSource_;
    delete videoDecoder_;
    delete audioDecoder_;
}

bool MediaPlaybackController::openMedia(const QString& url) {
    if (!mediaSource_->open(url)) return false;

    AVFormatContext* ctx = mediaSource_->getFormatContext();
    for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
        AVCodecParameters* params = ctx->streams[i]->codecpar;
        if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (!videoDecoder_->initialize(params)) {
                qWarning() << "Failed to initialize video decoder";
            }
        } else if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (!audioDecoder_->initialize(params)) {
                qWarning() << "Failed to initialize audio decoder";
            }
        }
    }

    return true;
}

void MediaPlaybackController::closeMedia() {
    stop();
    mediaSource_->close();
}

void MediaPlaybackController::play() {
    if (!isPlaying_) {
        mediaReader_->start();
        isPlaying_ = true;
    }
}

void MediaPlaybackController::pause() {
    mediaReader_->pause();
    isPlaying_ = false;
}

void MediaPlaybackController::stop() {
    mediaReader_->stop();
    videoDecoder_->flush();
    audioDecoder_->flush();
    isPlaying_ = false;
}

void MediaPlaybackController::seek(int64_t timestampMs) {
    mediaSource_->seek(timestampMs);
    videoDecoder_->flush();
    audioDecoder_->flush();
}

QImage MediaPlaybackController::getNextVideoFrame() {
    AVPacket* pkt = mediaReader_->getNextPacket(StreamType::Video);
    if (!pkt) return QImage();
    QImage img = videoDecoder_->decodeFrame(pkt);
    av_packet_free(&pkt);
    return img;
}

QByteArray MediaPlaybackController::getNextAudioFrame() {
    AVPacket* pkt = mediaReader_->getNextPacket(StreamType::Audio);
    if (!pkt) return QByteArray();
    QByteArray audio = audioDecoder_->decodeFrame(pkt);
    av_packet_free(&pkt);
    return audio;
}

} // namespace ArtifactCore