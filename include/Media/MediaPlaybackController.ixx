module;
#define QT_NO_KEYWORDS

#include <cstdint>
#include <functional>
#include <vector>
#include <QString>
#include <QImage>
#include <QByteArray>
#include <QSize>
extern "C" {
#include <vulkan/vulkan.h>
}
export module MediaPlaybackController;

import Media.MetaData;
import Playback.State;
import Video.VideoFrame;


export namespace ArtifactCore {

 // V[Nx
 enum class SeekMode {
  Fast,       // iL[t[̂݁j
  Accurate    // miׂẴt[j
 };

 // Đx
 enum class PlaybackSpeed {
  QuarterSpeed,    // 0.25x
  HalfSpeed,       // 0.5x
  NormalSpeed,     // 1.0x
  DoubleSpeed,     // 2.0x
  QuadrupleSpeed   // 4.0x
 };

 enum class DecoderBackend {
  FFmpeg,
  MediaFoundation
 };

 // Đ
 struct PlaybackInfo {
  int64_t currentPositionMs = 0;     // ݈ʒui~bj
  int64_t durationMs = 0;            // f[Vi~bj
  double currentPositionSec = 0.0;   // ݈ʒuibj
  double durationSec = 0.0;          // f[Vibj
  double percentageComplete = 0.0;   // ii0.0-100.0j
  int64_t currentFrame = 0;          // ݂̃t[ԍ
  int64_t totalFrames = 0;           // t[
  double fps = 0.0;                  // t[[g
  PlaybackState state = PlaybackState::Stopped;
  double playbackSpeed = 1.0;        // Đx
  bool isMuted = false;              // ~[g
  float volume = 1.0f;               // ʁi0.0-1.0j
 };

 // ĐCxg̃R[obN^
 using PlaybackStateChangedCallback = std::function<void(PlaybackState)>;
 using PositionChangedCallback = std::function<void(int64_t)>;
 using ErrorCallback = std::function<void(const QString&)>;
 using EndOfMediaCallback = std::function<void()>;

 class MediaPlaybackController {
 private:
  class Impl;
  Impl* impl_;
  friend class PlaybackBackend;
  friend class FFmpegPlaybackBackend;
  friend class MFPlaybackBackend;

 public:
  MediaPlaybackController();
  ~MediaPlaybackController();

  // Rs[/[u
  MediaPlaybackController(const MediaPlaybackController&) = delete;
  MediaPlaybackController& operator=(const MediaPlaybackController&) = delete;
  MediaPlaybackController(MediaPlaybackController&&) noexcept;
  MediaPlaybackController& operator=(MediaPlaybackController&&) noexcept;

  // ---- fBA ----

  // fBAJ
  bool openMedia(const QString& url);
  
  // t@C烁fBAJ
  bool openMediaFile(const QString& filePath);
  
  // fBA
  void closeMedia();
  
  // fBAJĂ邩
  bool isMediaOpen() const;

  // ---- Đ ----

  // Đ
  void play();
  
  // ꎞ~
  void pause();
  
  // ~
  void stop();
  
  // Đ/ꎞ~gO
  void togglePlayPause();

  // ---- V[N ----

  // wʒuɃV[Ni~bj
  void seek(int64_t timestampMs, SeekMode mode = SeekMode::Fast);
  
  // wʒuɃV[Nibj
  void seekToSeconds(double seconds, SeekMode mode = SeekMode::Fast);
  
  // wt[ɃV[N
  void seekToFrame(int64_t frameNumber);
  
  // ΓIɃV[Ni~bj
  void seekRelative(int64_t deltaMs);
  
  // ŏɖ߂
  void seekToBeginning();
  
  // ŌɈړ
  void seekToEnd();
  
  // ̃t[
  void stepForward();
  
  // Õt[
  void stepBackward();

  // ---- Đx ----

  // Đxݒi0.25x`4.0xj
  void setPlaybackSpeed(double speed);
  void setPlaybackSpeed(PlaybackSpeed speed);
  double getPlaybackSpeed() const;

  void setDecoderBackend(DecoderBackend backend);
  DecoderBackend getDecoderBackend() const;
  void setVulkanDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t queueFamilyIndex);
  // Optional shared native device used only by the direct D3D12VA decoder.
  // The controller retains the COM device until it is replaced or destroyed.
  void setD3D12Device(void* device);

  // ʂݒi0.0-1.0j
  void setVolume(float volume);
  float getVolume() const;
  
  // ~[g
  void setMuted(bool muted);
  bool isMuted() const;
  void toggleMute();

  // ---- t[擾 ----

  // ̃rfIt[擾
  QImage getNextVideoFrame();
  DecodedVideoFrame getNextVideoFrameRaw();
  
  // ݂̃rfIt[擾iĐʒui߂Ȃj
  QImage getCurrentVideoFrame();
  DecodedVideoFrame getCurrentVideoFrameRaw();
  
  // wʒũrfIt[擾
  QImage getVideoFrameAt(int64_t timestampMs);
  DecodedVideoFrame getVideoFrameAtRaw(int64_t timestampMs);
  QImage getVideoFrameAtFrame(int64_t frameNumber);
  DecodedVideoFrame getVideoFrameAtFrameRaw(int64_t frameNumber);
  QImage getVideoFrameAtFrameDirect(int64_t frameNumber);
  DecodedVideoFrame getVideoFrameAtFrameDirectRaw(int64_t frameNumber);
  DecodedVideoFrame getVideoFrameAtFrameDirectRaw(
      int64_t frameNumber, bool retainD3D12Frame);
  
  // ̃I[fBIt[擾
  QByteArray getNextAudioFrame();
  // True only when the reader has ended and its audio packet queue is empty.
  bool isAudioStreamFinished() const;

  // Set the PCM format returned by getNextAudioFrame().
  bool setAudioOutputSampleRate(int sampleRate);

  // ---- Ԏ擾 ----

  // Đ
  PlaybackState getState() const;
  bool isPlaying() const;
  bool isPaused() const;
  bool isStopped() const;
  
  // Đ
  PlaybackInfo getPlaybackInfo() const;
  
  // ^f[^擾
  MediaMetaData getMetadata() const;
  
  // ݈ʒui~bj
  int64_t getCurrentPosition() const;
  
  // ݈ʒuibj
  double getCurrentPositionSeconds() const;
  
  // f[Vi~bj
  int64_t getDuration() const;
  
  // f[Vibj
  double getDurationSeconds() const;
  
  // ݂̃t[ԍ
  int64_t getCurrentFrame() const;
  
  // t[
  int64_t getTotalFrames() const;
  
  // t[[g
  double getFrameRate() const;
  
  // ii0.0-100.0j
  double getProgressPercentage() const;

  // ---- R[obNݒ ----

  // ĐԕύXR[obN
  void setStateChangedCallback(PlaybackStateChangedCallback callback);
  
  // ĐʒuύXR[obN
  void setPositionChangedCallback(PositionChangedCallback callback);
  
  // G[R[obN
  void setErrorCallback(ErrorCallback callback);
  
  // fBAIR[obN
  void setEndOfMediaCallback(EndOfMediaCallback callback);

  // ---- [vݒ ----

  // [vĐL/
  void setLooping(bool enabled);
  bool isLooping() const;
  
  // [v͈͂ݒi~bj
  void setLoopRange(int64_t startMs, int64_t endMs);
  void clearLoopRange();

  // ---- ̑ ----

  // obt@Oii0.0-100.0j
  double getBufferingProgress() const;
  
  // Ō̃G[bZ[W
  QString getLastError() const;
  QString getDebugState() const;
  
  // TlC𐶐
  QImage generateThumbnail(int64_t timestampMs, const QSize& size);
  
  // ̃TlC𐶐
  std::vector<QImage> generateThumbnails(int count, const QSize& size);
 };

} // namespace ArtifactCore
