module;
#include <QString>

module AudioRenderer;

namespace ArtifactCore {

struct AudioRenderer::Impl {
    float masterVolume = 1.0f;
    bool isMute = false;
    bool active = false;
    QString deviceName;

    Impl() = default;
};

AudioRenderer::AudioRenderer() : impl_(new Impl()) {}

AudioRenderer::~AudioRenderer() {
    delete impl_;
}

bool AudioRenderer::openDevice(const QString& deviceName) {
    if (!impl_) return false;
    impl_->deviceName = deviceName;
    return true; // Stub
}

void AudioRenderer::closeDevice() {
    if (impl_) impl_->deviceName.clear();
}

void AudioRenderer::start() {
    if (impl_) impl_->active = true;
}

void AudioRenderer::stop() {
    if (impl_) impl_->active = false;
}

bool AudioRenderer::isActive() const {
    return impl_ ? impl_->active : false;
}

void AudioRenderer::setMasterVolume(float db) {
    if (impl_) impl_->masterVolume = db;
}

float AudioRenderer::masterVolume() const {
    return impl_ ? impl_->masterVolume : 1.0f;
}

void AudioRenderer::setMute(bool mute) {
    if (impl_) impl_->isMute = mute;
}

bool AudioRenderer::isMute() const {
    return impl_ ? impl_->isMute : false;
}

void AudioRenderer::enqueue(const AudioSegment& segment) {
    // Stub
}

} // namespace ArtifactCore