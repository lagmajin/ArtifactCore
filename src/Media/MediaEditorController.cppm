module;

#include <QDebug>
#include <QImage>
#include <cstring>

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
module MediaEditorController;

import MediaPlaybackController;
import Video.VideoFrame;

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
    const DecodedVideoFrame decoded = playbackController_->getNextVideoFrameRaw();
    if (const auto* cpu = std::get_if<CpuVideoFrame>(&decoded)) {
        QImage image(cpu->meta.width, cpu->meta.height, QImage::Format_RGB888);
        if (image.isNull()) {
            return QImage();
        }
        const int rowBytes = std::min<int>(cpu->strideBytes, image.bytesPerLine());
        for (int y = 0; y < cpu->meta.height; ++y) {
            std::memcpy(image.scanLine(y),
                        cpu->bytes.data() + static_cast<size_t>(y) * static_cast<size_t>(cpu->strideBytes),
                        static_cast<size_t>(rowBytes));
        }
        return image;
    }
    return QImage();
}

QByteArray MediaEditorController::getCurrentAudio() {
    return playbackController_->getNextAudioFrame();
}

} // namespace ArtifactCore
