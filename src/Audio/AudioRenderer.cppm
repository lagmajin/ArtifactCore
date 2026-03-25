module;
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QMediaDevices>
#include <QDebug>
#include <QString>
#include <memory>
#include <mutex>
#include <algorithm>
#include <cmath>

module AudioRenderer;

import Audio.Backend;
#ifdef _WIN32
import Audio.Backend.WASAPI;
#endif
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
        ringBuffer = std::make_unique<AudioRingBuffer>(48000 * 4); // 4-second stereo buffer
#ifdef _WIN32
        backend = std::make_unique<WASAPIBackend>();
#else
        backend = std::make_unique<QtAudioBackend>();
#endif
    }

    void audioCallback(float* buffer, int frames, int channelsRequested) {
        if (!active || isMute) {
            std::memset(buffer, 0, frames * channelsRequested * sizeof(float));
            return;
        }

        AudioSegment segment;
        segment.sampleRate = sampleRate;
        
        const bool success = ringBuffer->read(segment, frames);
        const int availableFrames = segment.frameCount();

        std::memset(buffer, 0, frames * channelsRequested * sizeof(float));
        if (success && availableFrames > 0) {
            for (int i = 0; i < availableFrames; ++i) {
                for (int ch = 0; ch < channelsRequested; ++ch) {
                    float sample = 0.0f;
                    if (segment.channelCount() > ch && i < segment.channelData[ch].size()) {
                        sample = segment.channelData[ch][i];
                    } else if (segment.channelCount() == 1 && i < segment.channelData[0].size()) {
                        sample = segment.channelData[0][i];
                    }

                    sample *= masterVolume;
                    sample = std::clamp(sample, -1.0f, 1.0f);
                    buffer[i * channelsRequested + ch] = sample;
                }
            }
        } else {
            static int underflowLogCount = 0;
            if (underflowLogCount < 8) {
                ++underflowLogCount;
                qWarning() << "[AudioRenderer] underflow"
                           << "requestedFrames=" << frames
                           << "availableFrames=" << availableFrames;
            }
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

    bool opened = impl_->backend->open(device, format);
#ifdef _WIN32
    if (!opened) {
        impl_->backend = std::make_unique<QtAudioBackend>();
        opened = impl_->backend->open(device, format);
    }
#endif
    if (opened) {
        const auto current = impl_->backend->currentFormat();
        if (current.isValid()) {
            impl_->sampleRate = current.sampleRate();
            impl_->channels = current.channelCount();
        }
    }
    return opened;
}

void AudioRenderer::closeDevice() {
    if (impl_) {
        impl_->active = false;
        impl_->backend->stop();
        impl_->backend->close();
        impl_->deviceName.clear();
    }
}

void AudioRenderer::start() {
    if (impl_ && !impl_->active) {
        if (impl_->ringBuffer) {
            impl_->ringBuffer->clear();
        }
        impl_->active = true;
        impl_->backend->start([this](float* b, int f, int c) {
            impl_->audioCallback(b, f, c);
        });
        if (!impl_->backend->isActive()) {
            impl_->active = false;
            impl_->backend->stop();
            impl_->backend->close();
        }
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
        if (!impl_->ringBuffer->write(segment)) {
            static int overflowLogCount = 0;
            if (overflowLogCount < 8) {
                ++overflowLogCount;
                qWarning() << "[AudioRenderer] ring buffer overflow"
                           << "frames=" << segment.frameCount()
                           << "sampleRate=" << segment.sampleRate
                           << "channels=" << segment.channelCount();
            }
        }
    }
}

void AudioRenderer::clearBuffer() {
    if (impl_ && impl_->ringBuffer) {
        impl_->ringBuffer->clear();
    }
}

int AudioRenderer::sampleRate() const {
    if (!impl_ || !impl_->backend) {
        return 0;
    }
    const auto format = impl_->backend->currentFormat();
    return format.isValid() ? format.sampleRate() : impl_->sampleRate;
}

int AudioRenderer::channelCount() const {
    if (!impl_ || !impl_->backend) {
        return 0;
    }
    const auto format = impl_->backend->currentFormat();
    return format.isValid() ? format.channelCount() : impl_->channels;
}

QString AudioRenderer::backendName() const {
    if (!impl_ || !impl_->backend) {
        return QString();
    }
    return impl_->backend->backendName();
}

} // namespace ArtifactCore
