module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
#include <memory>
#include <functional>
#include <chrono>
#include <QString>

export module AudioRenderer;

import Audio.Backend;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief オーディオバックエンドの種類
 */
enum class AudioBackendType {
    Auto,    // 自動検出
    WASAPI,  // WASAPI 共有モード
    ASIO,    // ASIO（スタブ）
    Qt       // Qt 標準
};

/**
 * @brief Audio level metering data (in dBFS)
 */
struct AudioLevelData {
    float leftRms = -60.0f;
    float rightRms = -60.0f;
    float leftPeak = -60.0f;
    float rightPeak = -60.0f;
};

 /**
  * @brief High-level component for outputting AudioSegments to hardware.
  *
  * It maintains an AudioBackend and provides control over final output stage
  * (Master volume, device selection, clipping protection).
  */
 class LIBRARY_DLL_API AudioRenderer {
 public:
  AudioRenderer();
  ~AudioRenderer();

  AudioRenderer(const AudioRenderer&) = delete;
  AudioRenderer& operator=(const AudioRenderer&) = delete;

  /**
   * @brief Initialize and open an audio device
   */
  bool openDevice(const QString& deviceName = "");
  bool openDevice(AudioBackendType type, const QString& deviceName = "");
  void closeDevice();
  bool isDeviceOpen() const;

  /**
   * @brief Start/Stop audio processing playback
   */
  void start();
  void stop();
  bool isActive() const;

  /**
   * @brief Master controls
   */
  void setMasterVolume(float db);
  float masterVolume() const;

  void setMute(bool mute);
  bool isMute() const;

  int sampleRate() const;
  int channelCount() const;
  QString backendName() const;
  AudioBackendType currentBackendType() const;

  /**
   * @brief Feed audio data to the renderer buffer
   */
  void enqueue(const AudioSegment& segment);
  void clearBuffer();
  size_t bufferedFrames() const;
  size_t underflowCount() const;
  size_t overflowCount() const;

  /**
   * @brief Set callback for receiving real-time level metering data
   * @param callback Called with AudioLevelData (dBFS values) from the output buffer
   */
  void setLevelCallback(std::function<void(const AudioLevelData&)> callback);

  // A/V Sync support
  std::chrono::microseconds getAudioPosition() const;
  void reportUnderflow();
  void reportOverflow();

 private:
  struct Impl;
  Impl* impl_;
 };

} // namespace ArtifactCore
