module;
#include "../Define/DllExportMacro.hpp"
#include <memory>
#include <QString>

export module AudioRenderer;

import Audio.Backend;
import Audio.Segment;

export namespace ArtifactCore {

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
  void closeDevice();

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

  /**
   * @brief Feed audio data to the renderer buffer
   */
  void enqueue(const AudioSegment& segment);
  void clearBuffer();

 private:
  struct Impl;
  Impl* impl_;
 };

} // namespace ArtifactCore