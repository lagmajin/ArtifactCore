module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <QString>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QMediaDevices>
#include <QIODevice>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>
#include <QVector>

export module Audio.Backend.Qt;

import Audio.Backend;

namespace ArtifactCore {

namespace {
const char* sampleFormatToString(AudioBackendSampleFormat format)
{
    switch (format) {
    case AudioBackendSampleFormat::Float32:
        return "Float32";
    case AudioBackendSampleFormat::Int16:
        return "Int16";
    }
    return "Unknown";
}

QAudioDevice resolveDevice(const AudioDeviceInfo& deviceInfo)
{
    QAudioDevice resolved = QMediaDevices::defaultAudioOutput();
    if (deviceInfo.description.isEmpty()) {
        return resolved;
    }

    const auto outputs = QMediaDevices::audioOutputs();
    for (const auto& device : outputs) {
        if (device.description() == deviceInfo.description) {
            return device;
        }
    }

    return resolved;
}
} // namespace

export class QtAudioBackend : public QIODevice, public AudioBackend {
public:
    QtAudioBackend() : QIODevice() {}
    ~QtAudioBackend() { close(); }

    // AudioBackend interface
    bool open(const AudioDeviceInfo& device, const AudioBackendFormat& format) override {
        close();
        
        format_ = format;
        QAudioFormat qtFormat;
        qtFormat.setSampleRate(format_.sampleRate);
        qtFormat.setChannelCount(format_.channelCount);
        qtFormat.setSampleFormat(format_.sampleFormat == AudioBackendSampleFormat::Float32
                                     ? QAudioFormat::Float
                                     : QAudioFormat::Int16);
        audioSink_ = std::make_unique<QAudioSink>(resolveDevice(device), qtFormat);
        
        if (!QIODevice::open(QIODevice::ReadOnly)) {
            return false;
        }

        return true;
    }

    void close() override {
        stop();
        if (audioSink_) {
            audioSink_->stop();
            audioSink_.reset();
        }
        QIODevice::close();
    }

    void start(AudioCallback callback) override {
        if (!audioSink_) return;

        {
            QMutexLocker locker(&mutex_);
            if (active_) return;
            callback_ = std::move(callback);
            active_ = true;
        }
        audioSink_->start(this);
    }

    void stop() override {
        {
            QMutexLocker locker(&mutex_);
            active_ = false;
        }
        if (audioSink_) {
            audioSink_->stop();
        }
    }

    bool isActive() const override { return active_; }
    AudioBackendFormat currentFormat() const override { return format_; }
    QString backendName() const override { return "QtMultimedia"; }

    // QIODevice interface (called by QAudioSink to pull data)
    qint64 readData(char* data, qint64 maxlen) override {
        AudioCallback callback;
        {
            QMutexLocker locker(&mutex_);
            if (!active_ || !callback_) {
                std::memset(data, 0, maxlen);
                return maxlen;
            }
            callback = callback_;
        }

        if (!callback) {
            std::memset(data, 0, maxlen);
            return maxlen;
        }

        const int channels = std::max(1, format_.channelCount);
        const auto sampleFormat = format_.sampleFormat;

        if (sampleFormat == AudioBackendSampleFormat::Float32) {
            const int sampleSize = static_cast<int>(sizeof(float));
            const int frames = static_cast<int>(maxlen / (sampleSize * channels));
            if (frames > 0) {
                callback(reinterpret_cast<float*>(data), frames, channels);
            }
            return maxlen;
        }

        if (sampleFormat == AudioBackendSampleFormat::Int16) {
            const int frames = static_cast<int>(maxlen / (static_cast<int>(sizeof(qint16)) * channels));
            if (frames <= 0) {
                return 0;
            }

            QVector<float> tempBuffer(frames * channels, 0.0f);
            callback(tempBuffer.data(), frames, channels);

            auto* out = reinterpret_cast<qint16*>(data);
            for (int i = 0; i < frames * channels; ++i) {
                const float sample = std::clamp(tempBuffer[i], -1.0f, 1.0f);
                out[i] = static_cast<qint16>(std::lround(sample * 32767.0f));
            }
            return static_cast<qint64>(frames) * channels * static_cast<qint64>(sizeof(qint16));
        }

        qWarning() << "[QtAudioBackend] Unsupported output sample format"
                   << sampleFormatToString(sampleFormat)
                   << "for device pull. Writing silence.";
        std::memset(data, 0, maxlen);
        return maxlen;
    }

    qint64 writeData(const char* data, qint64 len) override { return 0; }
    bool isSequential() const override { return true; }

private:
    std::unique_ptr<QAudioSink> audioSink_;
    AudioBackendFormat format_;
    AudioCallback callback_;
    bool active_ = false;
    mutable QMutex mutex_;
};

} // namespace ArtifactCore
