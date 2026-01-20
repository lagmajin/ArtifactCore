module;

#include <chrono>
#include <QMutex>
#include "../Define/DllExportMacro.hpp"

export module Timeline.Clock;

import std;
import Frame.Rate;
import Frame.Position;

export namespace ArtifactCore {

 enum class PlaybackState {
  Stopped,
  Playing,
  Paused
 };

 // 高精度タイムラインクロック
 // 注意: UI更新にはSignal/Slotを使わず、定期ポーリングを推奨
 // Signal/Slotはキューイング遅延により高精度が失われるため
 class LIBRARY_DLL_API TimelineClock {
 private:
  class Impl;
  Impl* impl_;

 public:
  TimelineClock();
  explicit TimelineClock(const FrameRate& frameRate);
  TimelineClock(const TimelineClock& other);
  TimelineClock(TimelineClock&& other) noexcept;
  ~TimelineClock();

  TimelineClock& operator=(const TimelineClock& other);
  TimelineClock& operator=(TimelineClock&& other) noexcept;

  // 基本制御
  void start();
  void pause();
  void stop();
  void resume();

  // 状態取得
  PlaybackState state() const;
  bool isPlaying() const;
  bool isPaused() const;
  bool isStopped() const;

  // 時間取得
  std::chrono::microseconds elapsedTime() const;  // マイクロ秒精度
  std::chrono::milliseconds elapsedTimeMs() const;
  double elapsedSeconds() const;

  // フレーム位置
  int64_t currentFrame() const;
  FramePosition currentPosition() const;
  void setFrame(int64_t frame);
  void setPosition(const FramePosition& position);

  // フレームレート
  void setFrameRate(const FrameRate& frameRate);
  FrameRate frameRate() const;
  double framesPerSecond() const;

  // 再生速度制御
  void setPlaybackSpeed(double speed);  // 1.0 = 通常, 0.5 = 半速, 2.0 = 倍速, -1.0 = 逆再生
  double playbackSpeed() const;
  bool isReversePlaying() const;

  // ループ制御
  void setLoopRange(int64_t startFrame, int64_t endFrame);
  void clearLoopRange();
  bool isLooping() const;
  int64_t loopStartFrame() const;
  int64_t loopEndFrame() const;

  // オーディオ同期
  void syncToAudioClock(std::chrono::microseconds audioTime);
  void setAudioSyncEnabled(bool enabled);
  bool isAudioSyncEnabled() const;
  std::chrono::microseconds audioOffset() const;

  // ドロップフレーム検出
  void setDropFrameDetectionEnabled(bool enabled);
  bool isDropFrameDetectionEnabled() const;
  int64_t droppedFrameCount() const;
  void resetDroppedFrameCount();

  // タイムコード
  QString timecode() const;  // HH:MM:SS:FF
  QString timecodeWithSubframe() const;  // HH:MM:SS:FF.sf

  // デルタタイム（前回の更新からの経過時間）
  std::chrono::microseconds deltaTime();

  // リセット
  void reset();

  // デバッグ
  QString statistics() const;
 };

}

