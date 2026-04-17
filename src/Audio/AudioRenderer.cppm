module;
#include <QDebug>
#include <QString>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QMediaDevices>

module AudioRenderer;

import std;
import Audio.Backend;
#ifdef _WIN32
import Audio.Backend.WASAPI;
import Audio.Backend.ASIOStub;
#endif
import Audio.Backend.Qt;
import Audio.Segment;
import Audio.RingBuffer;
import ArtifactCore.Utils.PerformanceProfiler;

namespace ArtifactCore {

namespace {

AudioBackendFormat toBackendFormat(const QAudioFormat& format)
{
  AudioBackendFormat backendFormat;
  backendFormat.sampleRate = format.sampleRate();
  backendFormat.channelCount = format.channelCount();
  backendFormat.sampleFormat = (format.sampleFormat() == QAudioFormat::Int16)
      ? AudioBackendSampleFormat::Int16
      : AudioBackendSampleFormat::Float32;
  return backendFormat;
}

} // namespace

struct AudioRenderer::Impl {
  float masterVolume = 1.0f; // Linear gain multiplier
  bool isMute = false;
  bool active = false;
  bool deviceOpen = false;
  QString deviceName;

  std::unique_ptr<AudioBackend> backend;
  std::unique_ptr<AudioRingBuffer> ringBuffer;
  std::atomic<size_t> underflowCount{0};
  std::atomic<size_t> overflowCount{0};
  std::atomic<size_t> partialUnderflowCount{0};
  std::atomic<size_t> openAttemptCount{0};
  std::atomic<size_t> startAttemptCount{0};
  std::atomic<size_t> stopCount{0};
  std::atomic<size_t> closeCount{0};

  // Level metering
  std::function<void(const AudioLevelData&)> levelCallback;
  int levelCallbackCounter = 0; // Throttle callback frequency

  // We'll use 48kHz Stereo as our internal processing format for the renderer
  int sampleRate = 48000;
  int channels = 2;
  AudioBackendType backendType = AudioBackendType::Auto;

  Impl() {
    ringBuffer =
        std::make_unique<AudioRingBuffer>(48000 * 8); // 8-second stereo buffer
    backend = createBackend(AudioBackendType::Auto);
  }

  static float sampleToDb(float sample) {
    const float absVal = std::abs(sample);
    if (absVal < 0.00001f) {
      return -60.0f;
    }
    return std::clamp(20.0f * std::log10(absVal), -60.0f, 0.0f);
  }

  std::unique_ptr<AudioBackend> createBackend(AudioBackendType type) {
#ifdef _WIN32
    switch (type) {
    case AudioBackendType::WASAPI:
      return std::make_unique<WASAPIBackend>();
    case AudioBackendType::ASIO:
      return std::make_unique<ASIOBackendStub>();
    case AudioBackendType::Qt:
      return std::make_unique<QtAudioBackend>();
    case AudioBackendType::Auto:
    default:
      return std::make_unique<WASAPIBackend>();
    }
#else
    switch (type) {
    case AudioBackendType::Qt:
      return std::make_unique<QtAudioBackend>();
    case AudioBackendType::Auto:
    default:
      return std::make_unique<QtAudioBackend>();
    }
#endif
  }

  void audioCallback(float *buffer, int frames, int channelsRequested) {
    const auto cbStart = std::chrono::high_resolution_clock::now();

    if (!active || isMute) {
      std::memset(buffer, 0, frames * channelsRequested * sizeof(float));
      AudioEngineProfiler::instance().recordCallback(
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::high_resolution_clock::now() - cbStart).count(),
          frames, 0);
      return;
    }

    AudioSegment segment;
    segment.sampleRate = sampleRate;

    const bool success = ringBuffer->read(segment, frames);
    const int availableFrames = segment.frameCount();

    std::memset(buffer, 0, frames * channelsRequested * sizeof(float));
    if (success && availableFrames > 0) {
      if (availableFrames < frames) {
        const size_t count = ++partialUnderflowCount;
        if (count <= 16 || (count % 50) == 0) {
          qWarning() << "[AudioRenderer] partial underflow"
                     << "requestedFrames=" << frames
                     << "availableFrames=" << availableFrames;
        }
      }

      double leftSumSq = 0.0;
      double rightSumSq = 0.0;
      float leftPeakAbs = 0.0f;
      float rightPeakAbs = 0.0f;
      int leftCount = 0;
      int rightCount = 0;

      for (int i = 0; i < availableFrames; ++i) {
        for (int ch = 0; ch < channelsRequested; ++ch) {
          float sample = 0.0f;
          if (segment.channelCount() > ch &&
              i < segment.channelData[ch].size()) {
            sample = segment.channelData[ch][i];
          } else if (segment.channelCount() == 1 &&
                     i < segment.channelData[0].size()) {
            sample = segment.channelData[0][i];
          }

          sample *= masterVolume;
          sample = std::clamp(sample, -1.0f, 1.0f);

          if (availableFrames < frames) {
            const int fadeStart = std::max(0, availableFrames - 64);
            if (i >= fadeStart) {
              const float t = static_cast<float>(i - fadeStart) / static_cast<float>(availableFrames - fadeStart);
              const float fadeGain = 0.5f * (1.0f + std::cos(3.14159265f * t));
              sample *= fadeGain;
            }
          }

          buffer[i * channelsRequested + ch] = sample;

          if (ch == 0) {
            leftSumSq += static_cast<double>(sample) * sample;
            const float absSample = std::abs(sample);
            if (absSample > leftPeakAbs) leftPeakAbs = absSample;
            ++leftCount;
          } else if (ch == 1) {
            rightSumSq += static_cast<double>(sample) * sample;
            const float absSample = std::abs(sample);
            if (absSample > rightPeakAbs) rightPeakAbs = absSample;
            ++rightCount;
          }
        }
      }

      if (levelCallback && availableFrames > 0) {
        ++levelCallbackCounter;
        if (levelCallbackCounter >= 4) {
          levelCallbackCounter = 0;
          AudioLevelData levels;
          levels.leftRms = (leftCount > 0) ? sampleToDb(static_cast<float>(std::sqrt(leftSumSq / leftCount))) : -60.0f;
          levels.rightRms = (rightCount > 0) ? sampleToDb(static_cast<float>(std::sqrt(rightSumSq / rightCount))) : -60.0f;
          levels.leftPeak = sampleToDb(leftPeakAbs);
          levels.rightPeak = sampleToDb(rightPeakAbs);
          levelCallback(levels);
        }
      }
    } else {
      const size_t count = ++underflowCount;
      if (count <= 8) {
        qWarning() << "[AudioRenderer] underflow"
                   << "requestedFrames=" << frames
                   << "availableFrames=" << availableFrames;
      }
    }

    AudioEngineProfiler::instance().recordCallback(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now() - cbStart).count(),
        frames, availableFrames);
  }
};

AudioRenderer::AudioRenderer() : impl_(new Impl()) {}

AudioRenderer::~AudioRenderer() {
  stop();
  delete impl_;
}

bool AudioRenderer::openDevice(const QString &deviceName) {
  if (!impl_)
    return false;

  if (impl_->deviceOpen &&
      (deviceName.isEmpty() || deviceName == impl_->deviceName)) {
    return true;
  }
  if (impl_->deviceOpen) {
    closeDevice();
  }

  const size_t attempt = ++impl_->openAttemptCount;
  if (attempt <= 12 || (attempt % 50) == 0) {
    qDebug() << "[AudioRenderer] openDevice"
             << "attempt=" << attempt << "active=" << impl_->active
             << "deviceOpen=" << impl_->deviceOpen
             << "requestedDevice=" << deviceName << "backend="
             << (impl_->backend ? impl_->backend->backendName()
                                : QStringLiteral("<null>"));
  }

  QAudioDevice device = QMediaDevices::defaultAudioOutput();
  if (!deviceName.isEmpty()) {
    for (const auto &dev : QMediaDevices::audioOutputs()) {
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

  const AudioBackendFormat backendFormat = toBackendFormat(format);
  const AudioDeviceInfo deviceInfo{device.description()};
  bool opened = impl_->backend->open(deviceInfo, backendFormat);
#ifdef _WIN32
  if (!opened) {
    impl_->backend = std::make_unique<QtAudioBackend>();
    opened = impl_->backend->open(deviceInfo, backendFormat);
  }
#endif
  if (opened) {
    const auto current = impl_->backend->currentFormat();
    if (current.isValid()) {
      impl_->sampleRate = current.sampleRate;
      impl_->channels = current.channelCount;
    }
    impl_->deviceOpen = true;
    impl_->deviceName = device.description();
    qDebug() << "[AudioRenderer] openDevice success"
             << "attempt=" << attempt
             << "backend=" << impl_->backend->backendName()
             << "device=" << impl_->deviceName
             << "sampleRate=" << impl_->sampleRate
             << "channels=" << impl_->channels;
  } else {
    impl_->deviceOpen = false;
    qWarning() << "[AudioRenderer] openDevice failed"
               << "attempt=" << attempt << "requestedDevice=" << deviceName;
  }
  return opened;
}

void AudioRenderer::closeDevice() {
  if (impl_) {
    const size_t count = ++impl_->closeCount;
    qDebug() << "[AudioRenderer] closeDevice"
             << "count=" << count << "active=" << impl_->active
             << "deviceOpen=" << impl_->deviceOpen << "backend="
             << (impl_->backend ? impl_->backend->backendName()
                                : QStringLiteral("<null>"))
             << "bufferedFrames=" << bufferedFrames();
    impl_->active = false;
    impl_->backend->close();
    impl_->deviceOpen = false;
    impl_->deviceName.clear();
    if (impl_->ringBuffer) {
      impl_->ringBuffer->clear();
    }
  }
}

bool AudioRenderer::isDeviceOpen() const {
  return impl_ ? impl_->deviceOpen : false;
}

void AudioRenderer::start() {
  if (impl_ && !impl_->active) {
    if (!impl_->deviceOpen) {
      qWarning() << "[AudioRenderer] start requested without open device";
      return;
    }
    const size_t attempt = ++impl_->startAttemptCount;
    qDebug() << "[AudioRenderer] start"
             << "attempt=" << attempt << "deviceOpen=" << impl_->deviceOpen
             << "bufferedFrames=" << bufferedFrames() << "backend="
             << (impl_->backend ? impl_->backend->backendName()
                                : QStringLiteral("<null>"));
    impl_->active = true;
    impl_->backend->start(
        [this](float *b, int f, int c) { impl_->audioCallback(b, f, c); });
    if (!impl_->backend->isActive()) {
      impl_->active = false;
      impl_->backend->close();
      impl_->deviceOpen = false;
      qWarning() << "[AudioRenderer] start failed"
                 << "attempt=" << attempt
                 << "bufferedFrames=" << bufferedFrames();
    } else {
      qDebug() << "[AudioRenderer] start success"
               << "attempt=" << attempt
               << "backend=" << impl_->backend->backendName()
               << "bufferedFrames=" << bufferedFrames();
    }
  }
}

void AudioRenderer::stop() {
  if (impl_ && impl_->active) {
    const size_t count = ++impl_->stopCount;
    qDebug() << "[AudioRenderer] stop"
             << "count=" << count << "bufferedFrames=" << bufferedFrames()
             << "underflows=" << impl_->underflowCount.load()
             << "partialUnderflows=" << impl_->partialUnderflowCount.load()
             << "overflows=" << impl_->overflowCount.load();
    impl_->active = false;
    impl_->backend->stop();
    if (impl_->ringBuffer) {
      impl_->ringBuffer->clear();
    }
  }
}

bool AudioRenderer::isActive() const { return impl_ ? impl_->active : false; }

void AudioRenderer::setMasterVolume(float db) {
  if (impl_) {
    // Convert dB to linear gain
    if (db <= -144.0f)
      impl_->masterVolume = 0.0f;
    else
      impl_->masterVolume = std::pow(10.0f, db / 20.0f);
  }
}

float AudioRenderer::masterVolume() const {
  if (!impl_)
    return 1.0f;
  if (impl_->masterVolume <= 0.0f)
    return -144.0f;
  return 20.0f * std::log10(impl_->masterVolume);
}

void AudioRenderer::setMute(bool mute) {
  if (impl_)
    impl_->isMute = mute;
}

bool AudioRenderer::isMute() const { return impl_ ? impl_->isMute : false; }

void AudioRenderer::enqueue(const AudioSegment &segment) {
  if (impl_ && impl_->ringBuffer) {
    if (!impl_->ringBuffer->write(segment)) {
      const size_t count = ++impl_->overflowCount;
      if (count <= 8) {
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

size_t AudioRenderer::bufferedFrames() const {
  return (impl_ && impl_->ringBuffer) ? impl_->ringBuffer->available() : 0;
}

size_t AudioRenderer::underflowCount() const {
  return impl_ ? impl_->underflowCount.load() : 0;
}

size_t AudioRenderer::overflowCount() const {
  return impl_ ? impl_->overflowCount.load() : 0;
}

int AudioRenderer::sampleRate() const {
  if (!impl_ || !impl_->backend) {
    return 0;
  }
  const auto format = impl_->backend->currentFormat();
  return format.isValid() ? format.sampleRate : impl_->sampleRate;
}

int AudioRenderer::channelCount() const {
  if (!impl_ || !impl_->backend) {
    return 0;
  }
  const auto format = impl_->backend->currentFormat();
  return format.isValid() ? format.channelCount : impl_->channels;
}

QString AudioRenderer::backendName() const {
  if (!impl_ || !impl_->backend) {
    return QString();
  }
  return impl_->backend->backendName();
}

AudioBackendType AudioRenderer::currentBackendType() const {
  if (!impl_) {
    return AudioBackendType::Auto;
  }
  return impl_->backendType;
}

bool AudioRenderer::openDevice(AudioBackendType type,
                               const QString &deviceName) {
  if (!impl_)
    return false;

  // Store the requested backend type
  impl_->backendType = type;

  // Recreate backend with the requested type
  if (impl_->deviceOpen) {
    closeDevice();
  }

  impl_->backend = impl_->createBackend(type);

  qDebug() << "[AudioRenderer] openDevice with backend type"
           << "type=" << static_cast<int>(type) << "backend="
           << (impl_->backend ? impl_->backend->backendName()
                              : QStringLiteral("<null>"));

  // Now open with the new backend
  return openDevice(deviceName);
}

void AudioRenderer::setLevelCallback(std::function<void(const AudioLevelData&)> callback) {
  if (impl_) {
    impl_->levelCallback = std::move(callback);
    impl_->levelCallbackCounter = 0;
  }
}

} // namespace ArtifactCore
