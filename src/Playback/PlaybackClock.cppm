module;

#include <chrono>
#include <QMutex>
#include <QMutexLocker>
#include <QString>

module Playback.Clock;

import std;
import Frame.Rate;
import Frame.Position;

namespace ArtifactCore {

 class PlaybackClock::Impl {
 public:
  mutable QMutex mutex_;
  
  // ���
  PlaybackState state_ = PlaybackState::Stopped;
  
  // ���ԊǗ�
  std::chrono::high_resolution_clock::time_point startTime_;
  std::chrono::high_resolution_clock::time_point pauseTime_;
  std::chrono::microseconds pausedDuration_{0};
  std::chrono::microseconds lastDeltaTime_{0};
  std::chrono::high_resolution_clock::time_point lastUpdateTime_;
  
  // �t���[���Ǘ�
  FrameRate frameRate_{30.0};  // �f�t�H���g30fps
  int64_t startFrame_ = 0;
  int64_t currentFrame_ = 0;
  
  // �Đ����x
  double playbackSpeed_ = 1.0;
  
  // ���[�v
  bool looping_ = false;
  int64_t loopStart_ = 0;
  int64_t loopEnd_ = 0;
  
  // オーディオ同期
  bool audioSyncEnabled_ = false;
  std::chrono::microseconds audioOffset_{0};
  std::chrono::high_resolution_clock::time_point lastAudioSyncTime_;
  
  // ドロップフレーム検出
  bool dropFrameDetectionEnabled_ = false;
  int64_t droppedFrameCount_ = 0;
  int64_t lastFrameNumber_ = -1;
  
  Impl() = default;
  
  // QMutex はコピー不可能なので、手動でコピー
  Impl(const Impl& other) 
   : state_(other.state_)
   , startTime_(other.startTime_)
   , pauseTime_(other.pauseTime_)
   , pausedDuration_(other.pausedDuration_)
   , lastDeltaTime_(other.lastDeltaTime_)
   , lastUpdateTime_(other.lastUpdateTime_)
   , frameRate_(other.frameRate_)
   , startFrame_(other.startFrame_)
   , currentFrame_(other.currentFrame_)
   , playbackSpeed_(other.playbackSpeed_)
   , looping_(other.looping_)
   , loopStart_(other.loopStart_)
   , loopEnd_(other.loopEnd_)
   , audioSyncEnabled_(other.audioSyncEnabled_)
   , audioOffset_(other.audioOffset_)
   , lastAudioSyncTime_(other.lastAudioSyncTime_)
   , dropFrameDetectionEnabled_(other.dropFrameDetectionEnabled_)
   , droppedFrameCount_(other.droppedFrameCount_)
   , lastFrameNumber_(other.lastFrameNumber_)
  {}
  
  Impl& operator=(const Impl& other) {
   if (this != &other) {
    state_ = other.state_;
    startTime_ = other.startTime_;
    pauseTime_ = other.pauseTime_;
    pausedDuration_ = other.pausedDuration_;
    lastDeltaTime_ = other.lastDeltaTime_;
    lastUpdateTime_ = other.lastUpdateTime_;
    frameRate_ = other.frameRate_;
    startFrame_ = other.startFrame_;
    currentFrame_ = other.currentFrame_;
    playbackSpeed_ = other.playbackSpeed_;
    looping_ = other.looping_;
    loopStart_ = other.loopStart_;
    loopEnd_ = other.loopEnd_;
    audioSyncEnabled_ = other.audioSyncEnabled_;
    audioOffset_ = other.audioOffset_;
    lastAudioSyncTime_ = other.lastAudioSyncTime_;
    dropFrameDetectionEnabled_ = other.dropFrameDetectionEnabled_;
    droppedFrameCount_ = other.droppedFrameCount_;
    lastFrameNumber_ = other.lastFrameNumber_;
   }
   return *this;
  }

  std::chrono::microseconds calculateElapsedTime() const {
   auto now = std::chrono::high_resolution_clock::now();
   
   if (state_ == PlaybackState::Playing) {
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
     now - startTime_) - pausedDuration_;
    return std::chrono::microseconds(static_cast<int64_t>(elapsed.count() * playbackSpeed_));
   } else if (state_ == PlaybackState::Paused) {
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
     pauseTime_ - startTime_) - pausedDuration_;
    return std::chrono::microseconds(static_cast<int64_t>(elapsed.count() * playbackSpeed_));
   }
   
   return std::chrono::microseconds{0};
  }

  int64_t timeToFrame(std::chrono::microseconds time) const {
   double seconds = time.count() / 1000000.0;
   return startFrame_ + static_cast<int64_t>(seconds * frameRate_.framerate());
  }

  std::chrono::microseconds frameToTime(int64_t frame) const {
   double seconds = (frame - startFrame_) / frameRate_.framerate();
   return std::chrono::microseconds(static_cast<int64_t>(seconds * 1000000.0));
  }

  void updateCurrentFrame() {
   auto elapsed = calculateElapsedTime();
   currentFrame_ = timeToFrame(elapsed);
   
   // ���[�v����
   if (looping_ && loopEnd_ > loopStart_) {
    if (playbackSpeed_ >= 0) {
     // ���Đ�
     if (currentFrame_ >= loopEnd_) {
      int64_t loopLength = loopEnd_ - loopStart_;
      int64_t overflow = currentFrame_ - loopEnd_;
      currentFrame_ = loopStart_ + (overflow % loopLength);
      
      // ���Ԃ����Z�b�g
      auto loopDuration = frameToTime(loopLength);
      auto now = std::chrono::high_resolution_clock::now();
      startTime_ = now - frameToTime(currentFrame_ - loopStart_);
      pausedDuration_ = std::chrono::microseconds{0};
     }
    } else {
     // �t�Đ�
     if (currentFrame_ <= loopStart_) {
      int64_t loopLength = loopEnd_ - loopStart_;
      int64_t underflow = loopStart_ - currentFrame_;
      currentFrame_ = loopEnd_ - (underflow % loopLength);
      
      auto loopDuration = frameToTime(loopLength);
      auto now = std::chrono::high_resolution_clock::now();
      startTime_ = now - frameToTime(loopEnd_ - currentFrame_);
      pausedDuration_ = std::chrono::microseconds{0};
     }
    }
   }
   
   // �h���b�v�t���[�����o
   if (dropFrameDetectionEnabled_ && lastFrameNumber_ >= 0) {
    int64_t expectedFrame = lastFrameNumber_ + 1;
    if (currentFrame_ > expectedFrame) {
     droppedFrameCount_ += (currentFrame_ - expectedFrame);
    }
   }
   lastFrameNumber_ = currentFrame_;
  }
 };

 PlaybackClock::PlaybackClock() : impl_(new Impl()) {}

 PlaybackClock::PlaybackClock(const FrameRate& frameRate) : impl_(new Impl()) {
  impl_->frameRate_ = frameRate;
 }

 PlaybackClock::PlaybackClock(const PlaybackClock& other)
  : impl_(new Impl()) {
  // Lock the source impl while copying to avoid data race
  if (other.impl_) {
    QMutexLocker otherLocker(&other.impl_->mutex_);
    *impl_ = *other.impl_;
  }
 }

 PlaybackClock::PlaybackClock(PlaybackClock&& other) noexcept 
  : impl_(other.impl_) {
  other.impl_ = nullptr;
 }

 PlaybackClock::~PlaybackClock() {
  delete impl_;
 }

 PlaybackClock& PlaybackClock::operator=(const PlaybackClock& other) {
  if (this == &other) return *this;

  // Ensure both impl_ pointers are valid
  if (!impl_) impl_ = new Impl();

  Impl* a = impl_;
  Impl* b = other.impl_;
  if (!b) {
    // nothing to copy from
    return *this;
  }

  // Acquire mutexes in address order to avoid deadlock
  if (a < b) {
    QMutexLocker lockerA(&a->mutex_);
    QMutexLocker lockerB(&b->mutex_);
    *a = *b;
  } else if (a > b) {
    QMutexLocker lockerB(&b->mutex_);
    QMutexLocker lockerA(&a->mutex_);
    *a = *b;
  } else {
    // same impl instance
  }

  return *this;
 }

 PlaybackClock& PlaybackClock::operator=(PlaybackClock&& other) noexcept {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

void PlaybackClock::start() {
  QMutexLocker locker(&impl_->mutex_);
  impl_->state_ = PlaybackState::Playing;
  impl_->startTime_ = std::chrono::high_resolution_clock::now();
  impl_->pausedDuration_ = std::chrono::microseconds{0};
  impl_->lastUpdateTime_ = impl_->startTime_;
  impl_->lastFrameNumber_ = -1;
 }

void PlaybackClock::pause() {
  QMutexLocker locker(&impl_->mutex_);
  if (impl_->state_ == PlaybackState::Playing) {
   impl_->state_ = PlaybackState::Paused;
   impl_->pauseTime_ = std::chrono::high_resolution_clock::now();
  }
 }

void PlaybackClock::stop() {
  QMutexLocker locker(&impl_->mutex_);
  impl_->state_ = PlaybackState::Stopped;
  impl_->currentFrame_ = impl_->startFrame_;
  impl_->pausedDuration_ = std::chrono::microseconds{0};
  impl_->lastFrameNumber_ = -1;
 }

void PlaybackClock::resume() {
  QMutexLocker locker(&impl_->mutex_);
  if (impl_->state_ == PlaybackState::Paused) {
   auto now = std::chrono::high_resolution_clock::now();
   auto pauseDuration = std::chrono::duration_cast<std::chrono::microseconds>(
    now - impl_->pauseTime_);
   impl_->pausedDuration_ += pauseDuration;
   impl_->state_ = PlaybackState::Playing;
   impl_->lastUpdateTime_ = now;
  }
 }

PlaybackState PlaybackClock::state() const {
  QMutexLocker locker(&impl_->mutex_);
  return impl_->state_;
 }

bool PlaybackClock::isPlaying() const {
  return state() == PlaybackState::Playing;
 }

bool PlaybackClock::isPaused() const {
  return state() == PlaybackState::Paused;
 }

bool PlaybackClock::isStopped() const {
  return state() == PlaybackState::Stopped;
 }

std::chrono::microseconds PlaybackClock::elapsedTime() const {
  QMutexLocker locker(&impl_->mutex_);
  return impl_->calculateElapsedTime();
 }

std::chrono::milliseconds PlaybackClock::elapsedTimeMs() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime());
 }

double PlaybackClock::elapsedSeconds() const {
  return elapsedTime().count() / 1000000.0;
 }

int64_t PlaybackClock::currentFrame() const {
  QMutexLocker locker(&impl_->mutex_);
  const_cast<Impl*>(impl_)->updateCurrentFrame();
  return impl_->currentFrame_;
 }

FramePosition PlaybackClock::currentPosition() const {
  return FramePosition(currentFrame());
 }

void PlaybackClock::setFrame(int64_t frame) {
  QMutexLocker locker(&impl_->mutex_);
  impl_->currentFrame_ = frame;
  impl_->startFrame_ = frame;
  
  if (impl_->state_ == PlaybackState::Playing || impl_->state_ == PlaybackState::Paused) {
   auto now = std::chrono::high_resolution_clock::now();
   impl_->startTime_ = now;
   impl_->pausedDuration_ = std::chrono::microseconds{0};
   if (impl_->state_ == PlaybackState::Paused) {
    impl_->pauseTime_ = now;
   }
  }
 }

void PlaybackClock::setPosition(const FramePosition& position) {
  setFrame(position.framePosition());
 }

void PlaybackClock::setFrameRate(const FrameRate& frameRate) {
  QMutexLocker locker(&impl_->mutex_);
  impl_->frameRate_ = frameRate;
 }

FrameRate PlaybackClock::frameRate() const {
  QMutexLocker locker(&impl_->mutex_);
  return impl_->frameRate_;
 }

double PlaybackClock::framesPerSecond() const {
  return frameRate().framerate();
 }

void PlaybackClock::setPlaybackSpeed(double speed) {
  QMutexLocker locker(&impl_->mutex_);
  
  // ���݂̃t���[���ʒu��ێ�
  auto currentFrame = impl_->currentFrame_;
  
  impl_->playbackSpeed_ = speed;
  
  // �^�C�~���O���Čv�Z
  if (impl_->state_ == PlaybackState::Playing || impl_->state_ == PlaybackState::Paused) {
   auto now = std::chrono::high_resolution_clock::now();
   auto elapsed = impl_->frameToTime(currentFrame - impl_->startFrame_);
   impl_->startTime_ = now - elapsed;
   impl_->pausedDuration_ = std::chrono::microseconds{0};
   
   if (impl_->state_ == PlaybackState::Paused) {
    impl_->pauseTime_ = now;
   }
  }
 }

double PlaybackClock::playbackSpeed() const {
  QMutexLocker locker(&impl_->mutex_);
  return impl_->playbackSpeed_;
 }

bool PlaybackClock::isReversePlaying() const {
  return playbackSpeed() < 0.0;
 }

void PlaybackClock::setLoopRange(int64_t startFrame, int64_t endFrame) {
  QMutexLocker locker(&impl_->mutex_);
  impl_->looping_ = true;
  impl_->loopStart_ = startFrame;
  impl_->loopEnd_ = endFrame;
 }

void PlaybackClock::clearLoopRange() {
  QMutexLocker locker(&impl_->mutex_);
  impl_->looping_ = false;
 }

bool PlaybackClock::isLooping() const {
  QMutexLocker locker(&impl_->mutex_);
  return impl_->looping_;
 }

int64_t PlaybackClock::loopStartFrame() const {
  QMutexLocker locker(&impl_->mutex_);
  return impl_->loopStart_;
 }

int64_t PlaybackClock::loopEndFrame() const {
  QMutexLocker locker(&impl_->mutex_);
  return impl_->loopEnd_;
 }

void PlaybackClock::syncToAudioClock(std::chrono::microseconds audioTime) {
  QMutexLocker locker(&impl_->mutex_);
  
  if (!impl_->audioSyncEnabled_) return;
  
  auto videoTime = impl_->calculateElapsedTime();
  impl_->audioOffset_ = audioTime - videoTime;
  impl_->lastAudioSyncTime_ = std::chrono::high_resolution_clock::now();
  
  // �I�[�f�B�I�Ƃ̍����傫���ꍇ�͒���
  if (std::abs(impl_->audioOffset_.count()) > 16666) {  // 1�t���[�����ȏ�̃Y��
   auto now = std::chrono::high_resolution_clock::now();
   impl_->startTime_ = now - audioTime;
   impl_->pausedDuration_ = std::chrono::microseconds{0};
  }
 }

void PlaybackClock::setAudioSyncEnabled(bool enabled) {
  QMutexLocker locker(&impl_->mutex_);
  impl_->audioSyncEnabled_ = enabled;
 }

bool PlaybackClock::isAudioSyncEnabled() const {
  QMutexLocker locker(&impl_->mutex_);
  return impl_->audioSyncEnabled_;
 }

std::chrono::microseconds PlaybackClock::audioOffset() const {
  QMutexLocker locker(&impl_->mutex_);
  return impl_->audioOffset_;
 }

void PlaybackClock::setDropFrameDetectionEnabled(bool enabled) {
  QMutexLocker locker(&impl_->mutex_);
  impl_->dropFrameDetectionEnabled_ = enabled;
 }

bool PlaybackClock::isDropFrameDetectionEnabled() const {
  QMutexLocker locker(&impl_->mutex_);
  return impl_->dropFrameDetectionEnabled_;
 }

int64_t PlaybackClock::droppedFrameCount() const {
  QMutexLocker locker(&impl_->mutex_);
  return impl_->droppedFrameCount_;
 }

void PlaybackClock::resetDroppedFrameCount() {
  QMutexLocker locker(&impl_->mutex_);
  impl_->droppedFrameCount_ = 0;
  impl_->lastFrameNumber_ = -1;
 }

QString PlaybackClock::timecode() const {
  int64_t frame = currentFrame();
  double fps = framesPerSecond();
  
  int64_t totalFrames = frame;
  int64_t frames = totalFrames % static_cast<int64_t>(fps);
  int64_t totalSeconds = totalFrames / static_cast<int64_t>(fps);
  int64_t seconds = totalSeconds % 60;
  int64_t minutes = (totalSeconds / 60) % 60;
  int64_t hours = totalSeconds / 3600;
  
  return QString("%1:%2:%3:%4")
   .arg(hours, 2, 10, QChar('0'))
   .arg(minutes, 2, 10, QChar('0'))
   .arg(seconds, 2, 10, QChar('0'))
   .arg(frames, 2, 10, QChar('0'));
 }

QString PlaybackClock::timecodeWithSubframe() const {
  int64_t frame = currentFrame();
  double fps = framesPerSecond();
  
  // �}�C�N���b�P�ʂł̏����t���[��
  auto elapsed = elapsedTime();
  double exactFrames = elapsed.count() / 1000000.0 * fps;
  double subframe = exactFrames - std::floor(exactFrames);
  int subframeInt = static_cast<int>(subframe * 100);
  
  return timecode() + QString(".%1").arg(subframeInt, 2, 10, QChar('0'));
 }

std::chrono::microseconds PlaybackClock::deltaTime() {
  QMutexLocker locker(&impl_->mutex_);
  
  auto now = std::chrono::high_resolution_clock::now();
  auto delta = std::chrono::duration_cast<std::chrono::microseconds>(
   now - impl_->lastUpdateTime_);
  
  impl_->lastUpdateTime_ = now;
  impl_->lastDeltaTime_ = delta;
  
  return std::chrono::microseconds(static_cast<int64_t>(delta.count() * impl_->playbackSpeed_));
 }

void PlaybackClock::reset() {
  QMutexLocker locker(&impl_->mutex_);
  impl_->state_ = PlaybackState::Stopped;
  impl_->currentFrame_ = impl_->startFrame_;
  impl_->pausedDuration_ = std::chrono::microseconds{0};
  impl_->audioOffset_ = std::chrono::microseconds{0};
  impl_->droppedFrameCount_ = 0;
  impl_->lastFrameNumber_ = -1;
 }

QString PlaybackClock::statistics() const {
  QMutexLocker locker(&impl_->mutex_);
  
  QString stats;
  stats += QString("State: %1\n").arg(
   impl_->state_ == PlaybackState::Playing ? "Playing" :
   impl_->state_ == PlaybackState::Paused ? "Paused" : "Stopped");
  stats += QString("Current Frame: %1\n").arg(impl_->currentFrame_);
  stats += QString("Frame Rate: %1 fps\n").arg(impl_->frameRate_.framerate());
  stats += QString("Playback Speed: %1x\n").arg(impl_->playbackSpeed_);
  stats += QString("Elapsed Time: %1 ms\n").arg(impl_->calculateElapsedTime().count() / 1000.0);
  stats += QString("Audio Sync: %1\n").arg(impl_->audioSyncEnabled_ ? "Enabled" : "Disabled");
  stats += QString("Audio Offset: %1 ��s\n").arg(impl_->audioOffset_.count());
  stats += QString("Dropped Frames: %1\n").arg(impl_->droppedFrameCount_);
  stats += QString("Looping: %1\n").arg(impl_->looping_ ? "Yes" : "No");
  
  if (impl_->looping_) {
   stats += QString("Loop Range: %1 - %2\n")
    .arg(impl_->loopStart_).arg(impl_->loopEnd_);
  }
  
  return stats;
 }

}
