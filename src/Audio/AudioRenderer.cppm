module;
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QMediaDevices>
#include <QString>
#include <memory>
#include <mutex>
#include <algorithm>
#include <cmath>

module AudioRenderer;

import Audio.Backend;
import Audio.Backend.Qt;
import Audio.Segment;
import Audio.RingBuffer;

namespace ArtifactCore {

struct AudioRenderer::Impl {
    float masterVolume = 1.0f; // Linear gain multiplier
    bool isMute = false;
    bool active = false;
    QString deviceName;

    std::unique_ptr<AudioBackend> backend;
    std::unique_ptr<AudioRingBuffer> ringBuffer;
    
    // We'll use 48kHz Stereo as our internal processing format for the renderer
    int sampleRate = 48000;
    int channels = 2;

    Impl() {
        ringBuffer = std::make_unique<AudioRingBuffer>(48000 * 2); // 1-second stereo buffer
        backend = std::make_unique<QtAudioBackend>();
    }

    void audioCallback(float* buffer, int frames, int channelsRequested) {
        if (!active || isMute) {
            std::memset(buffer, 0, frames * channelsRequested * sizeof(float));
            return;
        }

        AudioSegment segment;
        segment.sampleRate = sampleRate;
        
        bool success = ringBuffer->read(segment, frames);
        
        if (success) {
            // Segment now contains stereo data
            for (int i = 0; i < frames; ++i) {
                for (int ch = 0; ch < channelsRequested; ++ch) {
                    float sample = 0.0f;
                    if (segment.channelCount() > ch) {
                        sample = segment.channelData[ch][i];
                    } else if (segment.channelCount() == 1) {
                        sample = segment.channelData[0][i];
                    }
                    
                    sample *= masterVolume;
                    // Simple clipping protection
                    sample = std::clamp(sample, -1.0f, 1.0f);
                    buffer[i * channelsRequested + ch] = sample;
                }
            }
        } else {
            // Underflow - silence
            std::memset(buffer, 0, frames * channelsRequested * sizeof(float));
        }
    }
};

AudioRenderer::AudioRenderer() : impl_(new Impl()) {}

AudioRenderer::~AudioRenderer() {
    stop();
    delete impl_;
}

bool AudioRenderer::openDevice(const QString& deviceName) {
    if (!impl_) return false;

    impl_->deviceName = deviceName;
    
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!deviceName.isEmpty()) {
        for (const auto& dev : QMediaDevices::audioOutputs()) {
            if (dev.description() == deviceName) {
                device = dev;
                break;
            }
        }
    }

    QAudioFormat format;
    format.setSampleRate(impl_->sampleRate);
    format.setChannelCount(impl_->channels);
    format.setSampleFormat(QAudioFormat::Float);

    if (!device.isFormatSupported(format)) {
        format = device.preferredFormat();
        impl_->sampleRate = format.sampleRate();
        impl_->channels = format.channelCount();
    }

    return impl_->backend->open(device, format);
}

void AudioRenderer::closeDevice() {
    if (impl_) {
        impl_->backend->stop();
        impl_->backend->close();
        impl_->deviceName.clear();
    }
}

void AudioRenderer::start() {
    if (impl_ && !impl_->active) {
        impl_->active = true;
        impl_->backend->start([this](float* b, int f, int c) {
            impl_->audioCallback(b, f, c);
        });
    }
}

void AudioRenderer::stop() {
    if (impl_ && impl_->active) {
        impl_->active = false;
        impl_->backend->stop();
    }
}

bool AudioRenderer::isActive() const {
    return impl_ ? impl_->active : false;
}

void AudioRenderer::setMasterVolume(float db) {
    if (impl_) {
        // Convert dB to linear gain
        if (db <= -144.0f) impl_->masterVolume = 0.0f;
        else impl_->masterVolume = std::pow(10.0f, db / 20.0f);
    }
}

float AudioRenderer::masterVolume() const {
    if (!impl_) return 1.0f;
    if (impl_->masterVolume <= 0.0f) return -144.0f;
    return 20.0f * std::log10(impl_->masterVolume);
}

void AudioRenderer::setMute(bool mute) {
    if (impl_) impl_->isMute = mute;
}

bool AudioRenderer::isMute() const {
    return impl_ ? impl_->isMute : false;
}

void AudioRenderer::enqueue(const AudioSegment& segment) {
    if (impl_ && impl_->ringBuffer) {
        impl_->ringBuffer->write(segment);
    }
}

void AudioRenderer::clearBuffer() {
    if (impl_ && impl_->ringBuffer) {
        impl_->ringBuffer->clear();
    }
}

} // namespace ArtifactCore
