module;

#include <QDebug>

module MediaEditorController;

import std;
import MediaPlaybackController;

namespace ArtifactCore {

MediaEditorController::MediaEditorController()
    : playbackController_(new MediaPlaybackController()) {}

MediaEditorController::~MediaEditorController() {
    delete playbackController_;
}

bool MediaEditorController::openMedia(const QString& filePath) {
    return playbackController_->openMedia(filePath);
}

void MediaEditorController::play() {
    playbackController_->play();
}

void MediaEditorController::pause() {
    playbackController_->pause();
}

void MediaEditorController::stop() {
    playbackController_->stop();
}

void MediaEditorController::seek(int64_t timeMs) {
    playbackController_->seek(timeMs);
}

QImage MediaEditorController::getCurrentFrame() {
    return playbackController_->getNextVideoFrame();
}

QByteArray MediaEditorController::getCurrentAudio() {
    return playbackController_->getNextAudioFrame();
}

} // namespace ArtifactCore