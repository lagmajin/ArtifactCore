module;
#include <QDebug>
#include <QString>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QMediaDevices>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>

module AudioRenderer;

import Audio.Backend;
#ifdef _WIN32
import Audio.Backend.WASAPI;
import Audio.Backend.ASIOStub;
#endif
import Audio.Backend.Qt;
import Audio.Segment;
import Audio.RingBuffer;
import Memory.TrackedPtr;
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
  std::atomic<bool> isMute{false};
  std::atomic<bool> active{false};
  std::atomic<bool> deviceOpen{false};
  QString deviceName;

  std::unique_ptr<AudioBackend> backend;
  std::unique_ptr<AudioRingBuffer> ringBuffer;
  // PERF: コールバック毎に AudioSegment を新規生成すると QVector の heap アロケーションが発生するため、
  // 再利用可能なバッファを保持する。resize() は同じサイズなら no-op、
  // 部分読出し時は事前に zero-fill して未使用領域を無音に保つ。
  AudioSegment readBuffer_;
  std::atomic<size_t> underflowCount{0};
  std::atomic<size_t> overflowCount{0};
  std::atomic<size_t> partialUnderflowCount{0};
  std::atomic<size_t> openAttemptCount{0};
  std::atomic<size_t> startAttemptCount{0};
  std::atomic<size_t> stopCount{0};
  std::atomic<size_t> closeCount{0};

  // Level metering — shared_ptr + atomic for lock-free read in audio callback.
  // PERF: std::function + mutex はコールバック毎に mutex ロック + ハップ割り当てを発生させるため、
  // atomic<shared_ptr> で参照を差し替え、読み側は load で参照する。
  std::atomic<std::shared_ptr<std::function<void(const AudioLevelData&)>>> levelCallback_;
  std::atomic<int> levelCallbackCounter{0}; // Throttle callback frequency

  // We'll use 48kHz Stereo as our internal processing format for the renderer
  int sampleRate = 48000;
  int channels = 2;
  AudioBackendType backendType = AudioBackendType::Auto;
  bool exclusiveMode = false;

  // Thread safety for concurrent access
  std::atomic<float> masterVolumeLinear{1.0f};

  Impl() {
    ringBuffer = std::make_unique<AudioRingBuffer>(48000 * 8); // 8-second stereo buffer
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
    case AudioBackendType::WASAPI: {
      auto wasapi = std::make_unique<WASAPIBackend>();
      wasapi->setExclusive(exclusiveMode);
      return wasapi;
    }
    case AudioBackendType::ASIO:
      return std::unique_ptr<AudioBackend>(new ASIOBackendStub());
    case AudioBackendType::Qt:
      return std::unique_ptr<AudioBackend>(new QtAudioBackend());
    case AudioBackendType::Auto:
    default: {
      auto wasapi = std::make_unique<WASAPIBackend>();
      wasapi->setExclusive(exclusiveMode);
      return wasapi;
    }
    }
#else
    switch (type) {
    case AudioBackendType::Qt:
      return std::unique_ptr<AudioBackend>(new QtAudioBackend());
    case AudioBackendType::Auto:
    default:
      return std::unique_ptr<AudioBackend>(new QtAudioBackend());
    }
#endif
  }

  // PERF: この関数は WASAPI レンダースレッド（TimeCritical）上で実行される。
  // - qWarning()/qDebug() は文字列フォーマット + ロック取得が伴うため RT コールバック内では禁止
  // - levelCallback は shared_ptr + atomic_load で lock-free に呼び出し
  // - readBuffer_ を再利用し、heap アロケーションを排除
  // - 音量/ミュートは atomic で読み取り、変化時のみ setMasterVolume/setMute を呼ぶ
  void audioCallback(float *buffer, int frames, int channelsRequested) {
    const auto cbStart = std::chrono::high_resolution_clock::now();

    const bool isActive = active.load(std::memory_order_acquire);
    const bool isMuted = isMute.load(std::memory_order_acquire);
    const float volume = masterVolumeLinear.load(std::memory_order_acquire);

    if (!isActive || isMuted) {
      std::memset(buffer, 0, frames * channelsRequested * sizeof(float));
      AudioEngineProfiler::instance().recordCallback(
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::high_resolution_clock::now() - cbStart).count(),
          frames, 0);
      return;
    }

    readBuffer_.sampleRate = sampleRate;
    readBuffer_.channelData.resize(channels);
    for (auto& ch : readBuffer_.channelData) {
      ch.resize(frames);
      ch.fill(0.0f);
    }

    const bool success = ringBuffer->read(readBuffer_, frames);
    const int availableFrames = readBuffer_.frameCount();

    std::memset(buffer, 0, frames * channelsRequested * sizeof(float));
    if (success && availableFrames > 0) {
      if (availableFrames < frames) {
        ++partialUnderflowCount;
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
           if (readBuffer_.channelCount() > ch &&
               i < readBuffer_.channelData[ch].size()) {
             sample = readBuffer_.channelData[ch][i];
           } else if (readBuffer_.channelCount() == 1 &&
                      i < readBuffer_.channelData[0].size()) {
             sample = readBuffer_.channelData[0][i];
           }

           sample *= volume;
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

      auto cb = levelCallback_.load(std::memory_order_acquire);
      if (cb && availableFrames > 0) {
        const int counter = levelCallbackCounter.fetch_add(1, std::memory_order_relaxed) + 1;
        if (counter >= 4) {
          levelCallbackCounter.store(0, std::memory_order_relaxed);
          AudioLevelData levels;
          levels.leftRms = (leftCount > 0) ? sampleToDb(static_cast<float>(std::sqrt(leftSumSq / leftCount))) : -60.0f;
          levels.rightRms = (rightCount > 0) ? sampleToDb(static_cast<float>(std::sqrt(rightSumSq / rightCount))) : -60.0f;
          levels.leftPeak = sampleToDb(leftPeakAbs);
          levels.rightPeak = sampleToDb(rightPeakAbs);
          (*cb)(levels);
        }
      }
    } else {
      ++underflowCount;
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

  if (impl_->deviceOpen.load(std::memory_order_acquire) &&
      (deviceName.isEmpty() || deviceName == impl_->deviceName)) {
    return true;
  }
  if (impl_->deviceOpen.load(std::memory_order_acquire)) {
    closeDevice();
  }

  // 排他モード変更を反映するためバックエンドを再作成
  impl_->backend = impl_->createBackend(impl_->backendType);

  const size_t attempt = ++impl_->openAttemptCount;
  if (attempt <= 12 || (attempt % 50) == 0) {
    qDebug() << "[AudioRenderer] openDevice"
             << "attempt=" << attempt << "active=" << impl_->active.load()
             << "deviceOpen=" << impl_->deviceOpen.load()
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
    impl_->backend = std::unique_ptr<AudioBackend>(new QtAudioBackend());
    opened = impl_->backend->open(deviceInfo, backendFormat);
  }
#endif
  if (opened) {
    const auto current = impl_->backend->currentFormat();
    if (current.isValid()) {
      impl_->sampleRate = current.sampleRate;
      impl_->channels = current.channelCount;
    }
    impl_->deviceOpen.store(true, std::memory_order_release);
    impl_->deviceName = device.description();
    qDebug() << "[AudioRenderer] openDevice success"
             << "attempt=" << attempt
             << "backend=" << impl_->backend->backendName()
             << "device=" << impl_->deviceName
             << "sampleRate=" << impl_->sampleRate
             << "channels=" << impl_->channels;
  } else {
    impl_->deviceOpen.store(false, std::memory_order_release);
    qWarning() << "[AudioRenderer] openDevice failed"
               << "attempt=" << attempt << "requestedDevice=" << deviceName;
  }
  return opened;
}

void AudioRenderer::closeDevice() {
  if (impl_) {
    const size_t count = ++impl_->closeCount;
    qDebug() << "[AudioRenderer] closeDevice"
             << "count=" << count << "active=" << impl_->active.load()
             << "deviceOpen=" << impl_->deviceOpen.load() << "backend="
             << (impl_->backend ? impl_->backend->backendName()
                                  : QStringLiteral("<null>"))
             << "bufferedFrames=" << bufferedFrames();
    impl_->active.store(false, std::memory_order_release);
    impl_->backend->close();
    impl_->deviceOpen.store(false, std::memory_order_release);
    impl_->deviceName.clear();
    if (impl_->ringBuffer) {
      impl_->ringBuffer->clear();
    }
  }
}

bool AudioRenderer::isDeviceOpen() const {
  return impl_ ? impl_->deviceOpen.load(std::memory_order_acquire) : false;
}

void AudioRenderer::start() {
  if (impl_ && !impl_->active.load(std::memory_order_acquire)) {
    if (!impl_->deviceOpen.load(std::memory_order_acquire)) {
      qWarning() << "[AudioRenderer] start requested without open device";
      return;
    }
    const size_t attempt = ++impl_->startAttemptCount;
    qDebug() << "[AudioRenderer] start"
             << "attempt=" << attempt << "deviceOpen=" << impl_->deviceOpen.load()
             << "bufferedFrames=" << bufferedFrames() << "backend="
             << (impl_->backend ? impl_->backend->backendName()
                                : QStringLiteral("<null>"));
    impl_->active.store(true, std::memory_order_release);
    impl_->backend->start(
        [this](float *b, int f, int c) { impl_->audioCallback(b, f, c); });
    if (!impl_->backend->isActive()) {
      impl_->active.store(false, std::memory_order_release);
      impl_->backend->close();
      impl_->deviceOpen.store(false, std::memory_order_release);
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

void AudioRenderer::requestStop() {
  if (impl_ && impl_->active.exchange(false, std::memory_order_acq_rel)) {
    const size_t count = ++impl_->stopCount;
    qDebug() << "[AudioRenderer] requestStop"
             << "count=" << count;
    impl_->backend->requestStop();
    if (impl_->ringBuffer) {
      impl_->ringBuffer->clear();
    }
  }
}

void AudioRenderer::stop() {
  if (impl_ && impl_->active.exchange(false, std::memory_order_acq_rel)) {
    const size_t count = ++impl_->stopCount;
    qDebug() << "[AudioRenderer] stop"
             << "count=" << count << "bufferedFrames=" << bufferedFrames()
             << "underflows=" << impl_->underflowCount.load()
             << "partialUnderflows=" << impl_->partialUnderflowCount.load()
             << "overflows=" << impl_->overflowCount.load();
    impl_->backend->stop();
    if (impl_->ringBuffer) {
      impl_->ringBuffer->clear();
    }
  }
}

bool AudioRenderer::isActive() const { return impl_ ? impl_->active.load(std::memory_order_acquire) : false; }

void AudioRenderer::setMasterVolume(float db) {
  if (impl_) {
    // Convert dB to linear gain
    const float linear = (db <= -144.0f) ? 0.0f : std::pow(10.0f, db / 20.0f);
    impl_->masterVolumeLinear.store(linear, std::memory_order_release);
  }
}

float AudioRenderer::masterVolume() const {
  const float linear = impl_ ? impl_->masterVolumeLinear.load(std::memory_order_acquire) : 1.0f;
  if (linear <= 0.0f) return -144.0f;
  return 20.0f * std::log10(linear);
}

void AudioRenderer::setMute(bool mute) {
  if (impl_)
    impl_->isMute.store(mute, std::memory_order_release);
}

bool AudioRenderer::isMute() const { return impl_ ? impl_->isMute.load(std::memory_order_acquire) : false; }

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

void AudioRenderer::setExclusiveMode(bool exclusive) {
  if (impl_) {
    impl_->exclusiveMode = exclusive;
  }
}

bool AudioRenderer::isExclusiveMode() const {
  return impl_ ? impl_->exclusiveMode : false;
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

// PERF: atomic<shared_ptr> で参照を差し替える。呼び出し側は非同期で安全に新しいコールバックを設定できる。
// audioCallback 側は load でコピーせずに参照する。
void AudioRenderer::setLevelCallback(std::function<void(const AudioLevelData&)> callback) {
  if (impl_) {
    if (callback) {
      impl_->levelCallback_.store(
          std::make_shared<std::function<void(const AudioLevelData&)>>(std::move(callback)),
          std::memory_order_release);
    } else {
      impl_->levelCallback_.store(
          std::shared_ptr<std::function<void(const AudioLevelData&)>>(),
          std::memory_order_release);
    }
    impl_->levelCallbackCounter.store(0, std::memory_order_relaxed);
  }
}

} // namespace ArtifactCore
