module;

#define QT_NO_KEYWORDS
#include <QDebug>
#include <QString>
#include <QImage>
#include <QByteArray>
#include <QSize>
#include <Qt>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

}

module MediaPlaybackController;

import std;

namespace ArtifactCore {

 class MediaPlaybackController::Impl {
 public:
  MediaSource* mediaSource_ = nullptr;
  MediaReader* mediaReader_ = nullptr;
  MediaImageFrameDecoder* videoDecoder_ = nullptr;
  MediaAudioDecoder* audioDecoder_ = nullptr;

  PlaybackState state_ = PlaybackState::Stopped;
  double playbackSpeed_ = 1.0;
  float volume_ = 1.0f;
  bool isMuted_ = false;
  bool isLooping_ = false;
  int64_t loopStartMs_ = 0;
  int64_t loopEndMs_ = -1;

  int64_t currentPositionMs_ = 0;
  int64_t durationMs_ = 0;
  int64_t currentFrame_ = 0;
  int64_t totalFrames_ = 0;
  double fps_ = 0.0;

  PlaybackStateChangedCallback stateChangedCallback_;
  PositionChangedCallback positionChangedCallback_;
  ErrorCallback errorCallback_;
  EndOfMediaCallback endOfMediaCallback_;

  QString lastError_;
  MediaMetaData metadata_;

  Impl() {
   mediaSource_ = new MediaSource();
   mediaReader_ = new MediaReader(mediaSource_);
   videoDecoder_ = new MediaImageFrameDecoder();
   audioDecoder_ = new MediaAudioDecoder();
  }

  ~Impl() {
   delete audioDecoder_;
   delete videoDecoder_;
   delete mediaReader_;
   delete mediaSource_;
  }

  void updatePlaybackInfo() {
   if (mediaSource_ && mediaSource_->isOpen()) {
    AVFormatContext* ctx = mediaSource_->getFormatContext();
    if (ctx) {
     durationMs_ = ctx->duration / 1000;
     
     for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
      if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
       AVRational fr = ctx->streams[i]->avg_frame_rate;
       if (fr.den > 0) {
        fps_ = static_cast<double>(fr.num) / fr.den;
        if (durationMs_ > 0 && fps_ > 0) {
         totalFrames_ = static_cast<int64_t>((durationMs_ / 1000.0) * fps_);
        }
       }
       break;
      }
     }
    }
   }
  }

  void notifyStateChanged(PlaybackState newState) {
   state_ = newState;
   if (stateChangedCallback_) {
    stateChangedCallback_(newState);
   }
  }

  void notifyPositionChanged(int64_t positionMs) {
   currentPositionMs_ = positionMs;
   if (fps_ > 0) {
    currentFrame_ = static_cast<int64_t>((positionMs / 1000.0) * fps_);
   }
   if (positionChangedCallback_) {
    positionChangedCallback_(positionMs);
   }
  }

  void notifyError(const QString& message) {
   lastError_ = message;
   if (errorCallback_) {
    errorCallback_(message);
   }
  }

  void notifyEndOfMedia() {
   if (isLooping_) {
    // ループ再生の場合は最初に戻る
    if (mediaSource_) {
     mediaSource_->seek(loopStartMs_);
    }
   } else {
    notifyStateChanged(PlaybackState::Stopped);
    if (endOfMediaCallback_) {
     endOfMediaCallback_();
    }
   }
  }
 };

 MediaPlaybackController::MediaPlaybackController()
  : impl_(new Impl()) {}

 MediaPlaybackController::~MediaPlaybackController() {
  delete impl_;
 }

 MediaPlaybackController::MediaPlaybackController(MediaPlaybackController&& other) noexcept
  : impl_(other.impl_) {
  other.impl_ = nullptr;
 }

 MediaPlaybackController& MediaPlaybackController::operator=(MediaPlaybackController&& other) noexcept {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 bool MediaPlaybackController::openMedia(const QString& url) {
  if (!impl_) return false;

  if (!impl_->mediaSource_->open(url)) {
   impl_->notifyError("Failed to open media: " + url);
   return false;
  }

  AVFormatContext* ctx = impl_->mediaSource_->getFormatContext();
  if (!ctx) {
   impl_->notifyError("Invalid format context");
   return false;
  }

  for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
   AVCodecParameters* params = ctx->streams[i]->codecpar;
   if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
    if (!impl_->videoDecoder_->initialize(params)) {
     qWarning() << "Failed to initialize video decoder";
    }
   } else if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
    if (!impl_->audioDecoder_->initialize(params)) {
     qWarning() << "Failed to initialize audio decoder";
    }
   }
  }

  impl_->updatePlaybackInfo();
  impl_->notifyStateChanged(PlaybackState::Stopped);
  return true;
 }

 bool MediaPlaybackController::openMediaFile(const QString& filePath) {
  return openMedia(filePath);
 }

 void MediaPlaybackController::closeMedia() {
  if (!impl_) return;
  stop();
  impl_->mediaSource_->close();
  impl_->currentPositionMs_ = 0;
  impl_->durationMs_ = 0;
  impl_->currentFrame_ = 0;
  impl_->totalFrames_ = 0;
  impl_->fps_ = 0.0;
 }

 bool MediaPlaybackController::isMediaOpen() const {
  return impl_ && impl_->mediaSource_ && impl_->mediaSource_->isOpen();
 }

 void MediaPlaybackController::play() {
  if (!impl_) return;
  if (impl_->state_ != PlaybackState::Playing) {
   impl_->mediaReader_->start();
   impl_->notifyStateChanged(PlaybackState::Playing);
  }
 }

 void MediaPlaybackController::pause() {
  if (!impl_) return;
  impl_->mediaReader_->pause();
  impl_->notifyStateChanged(PlaybackState::Paused);
 }

 void MediaPlaybackController::stop() {
  if (!impl_) return;
  impl_->mediaReader_->stop();
  impl_->videoDecoder_->flush();
  impl_->audioDecoder_->flush();
  impl_->notifyStateChanged(PlaybackState::Stopped);
  impl_->currentPositionMs_ = 0;
  impl_->currentFrame_ = 0;
 }

 void MediaPlaybackController::togglePlayPause() {
  if (!impl_) return;
  if (impl_->state_ == PlaybackState::Playing) {
   pause();
  } else {
   play();
  }
 }

 void MediaPlaybackController::seek(int64_t timestampMs, SeekMode mode) {
  if (!impl_ || !impl_->mediaSource_) return;
  
  impl_->mediaSource_->seek(timestampMs);
  impl_->videoDecoder_->flush();
  impl_->audioDecoder_->flush();
  impl_->notifyPositionChanged(timestampMs);
 }

 void MediaPlaybackController::seekToSeconds(double seconds, SeekMode mode) {
  seek(static_cast<int64_t>(seconds * 1000.0), mode);
 }

 void MediaPlaybackController::seekToFrame(int64_t frameNumber) {
  if (!impl_ || impl_->fps_ <= 0) return;
  double seconds = frameNumber / impl_->fps_;
  seekToSeconds(seconds);
 }

 void MediaPlaybackController::seekRelative(int64_t deltaMs) {
  if (!impl_) return;
  int64_t newPos = impl_->currentPositionMs_ + deltaMs;
  newPos = std::max<int64_t>(0, std::min(newPos, impl_->durationMs_));
  seek(newPos);
 }

 void MediaPlaybackController::seekToBeginning() {
  seek(0);
 }

 void MediaPlaybackController::seekToEnd() {
  if (!impl_) return;
  seek(impl_->durationMs_);
 }

 void MediaPlaybackController::stepForward() {
  if (!impl_ || impl_->fps_ <= 0) return;
  int64_t frameTimeMs = static_cast<int64_t>(1000.0 / impl_->fps_);
  seekRelative(frameTimeMs);
 }

 void MediaPlaybackController::stepBackward() {
  if (!impl_ || impl_->fps_ <= 0) return;
  int64_t frameTimeMs = static_cast<int64_t>(1000.0 / impl_->fps_);
  seekRelative(-frameTimeMs);
 }

 void MediaPlaybackController::setPlaybackSpeed(double speed) {
  if (!impl_) return;
  impl_->playbackSpeed_ = std::max(0.25, std::min(speed, 4.0));
 }

 void MediaPlaybackController::setPlaybackSpeed(PlaybackSpeed speed) {
  double speedValue = 1.0;
  switch (speed) {
   case PlaybackSpeed::QuarterSpeed: speedValue = 0.25; break;
   case PlaybackSpeed::HalfSpeed: speedValue = 0.5; break;
   case PlaybackSpeed::NormalSpeed: speedValue = 1.0; break;
   case PlaybackSpeed::DoubleSpeed: speedValue = 2.0; break;
   case PlaybackSpeed::QuadrupleSpeed: speedValue = 4.0; break;
  }
  setPlaybackSpeed(speedValue);
 }

 double MediaPlaybackController::getPlaybackSpeed() const {
  return impl_ ? impl_->playbackSpeed_ : 1.0;
 }

 void MediaPlaybackController::setVolume(float volume) {
  if (!impl_) return;
  impl_->volume_ = std::max(0.0f, std::min(volume, 1.0f));
 }

 float MediaPlaybackController::getVolume() const {
  return impl_ ? impl_->volume_ : 1.0f;
 }

 void MediaPlaybackController::setMuted(bool muted) {
  if (!impl_) return;
  impl_->isMuted_ = muted;
 }

 bool MediaPlaybackController::isMuted() const {
  return impl_ && impl_->isMuted_;
 }

 void MediaPlaybackController::toggleMute() {
  if (!impl_) return;
  impl_->isMuted_ = !impl_->isMuted_;
 }

 QImage MediaPlaybackController::getNextVideoFrame() {
  if (!impl_ || !impl_->mediaReader_ || !impl_->videoDecoder_) {
   return QImage();
  }

  AVPacket* pkt = impl_->mediaReader_->getNextPacket(StreamType::Video);
  if (!pkt) {
   impl_->notifyEndOfMedia();
   return QImage();
  }

  QImage img = impl_->videoDecoder_->decodeFrame(pkt);
  av_packet_free(&pkt);

  if (!img.isNull()) {
   impl_->notifyPositionChanged(impl_->currentPositionMs_ + static_cast<int64_t>(1000.0 / impl_->fps_));
  }

  return img;
 }

 QImage MediaPlaybackController::getCurrentVideoFrame() {
  if (!impl_ || !impl_->videoDecoder_) {
   return QImage();
  }
  // 現在位置のフレームを取得（再生位置を進めない）
  // 実際の実装では現在のフレームをキャッシュする必要がある
  return QImage();
 }

 QImage MediaPlaybackController::getVideoFrameAt(int64_t timestampMs) {
  if (!impl_) return QImage();
  seek(timestampMs, SeekMode::Accurate);
  return getNextVideoFrame();
 }

 QImage MediaPlaybackController::getVideoFrameAtFrame(int64_t frameNumber) {
  seekToFrame(frameNumber);
  return getNextVideoFrame();
 }

 QByteArray MediaPlaybackController::getNextAudioFrame() {
  if (!impl_ || !impl_->mediaReader_ || !impl_->audioDecoder_) {
   return QByteArray();
  }

  AVPacket* pkt = impl_->mediaReader_->getNextPacket(StreamType::Audio);
  if (!pkt) return QByteArray();

  QByteArray audio = impl_->audioDecoder_->decodeFrame(pkt);
  av_packet_free(&pkt);
  return audio;
 }

 PlaybackState MediaPlaybackController::getState() const {
  return impl_ ? impl_->state_ : PlaybackState::Stopped;
 }

 bool MediaPlaybackController::isPlaying() const {
  return getState() == PlaybackState::Playing;
 }

 bool MediaPlaybackController::isPaused() const {
  return getState() == PlaybackState::Paused;
 }

 bool MediaPlaybackController::isStopped() const {
  return getState() == PlaybackState::Stopped;
 }

 PlaybackInfo MediaPlaybackController::getPlaybackInfo() const {
  PlaybackInfo info;
  if (!impl_) return info;

  info.currentPositionMs = impl_->currentPositionMs_;
  info.durationMs = impl_->durationMs_;
  info.currentPositionSec = impl_->currentPositionMs_ / 1000.0;
  info.durationSec = impl_->durationMs_ / 1000.0;
  info.percentageComplete = impl_->durationMs_ > 0 ?
   (static_cast<double>(impl_->currentPositionMs_) / impl_->durationMs_) * 100.0 : 0.0;
  info.currentFrame = impl_->currentFrame_;
  info.totalFrames = impl_->totalFrames_;
  info.fps = impl_->fps_;
  info.state = impl_->state_;
  info.playbackSpeed = impl_->playbackSpeed_;
  info.isMuted = impl_->isMuted_;
  info.volume = impl_->volume_;

  return info;
 }

 MediaMetaData MediaPlaybackController::getMetadata() const {
  return impl_ ? impl_->metadata_ : MediaMetaData();
 }

 int64_t MediaPlaybackController::getCurrentPosition() const {
  return impl_ ? impl_->currentPositionMs_ : 0;
 }

 double MediaPlaybackController::getCurrentPositionSeconds() const {
  return impl_ ? impl_->currentPositionMs_ / 1000.0 : 0.0;
 }

 int64_t MediaPlaybackController::getDuration() const {
  return impl_ ? impl_->durationMs_ : 0;
 }

 double MediaPlaybackController::getDurationSeconds() const {
  return impl_ ? impl_->durationMs_ / 1000.0 : 0.0;
 }

 int64_t MediaPlaybackController::getCurrentFrame() const {
  return impl_ ? impl_->currentFrame_ : 0;
 }

 int64_t MediaPlaybackController::getTotalFrames() const {
  return impl_ ? impl_->totalFrames_ : 0;
 }

 double MediaPlaybackController::getFrameRate() const {
  return impl_ ? impl_->fps_ : 0.0;
 }

 double MediaPlaybackController::getProgressPercentage() const {
  if (!impl_ || impl_->durationMs_ <= 0) return 0.0;
  return (static_cast<double>(impl_->currentPositionMs_) / impl_->durationMs_) * 100.0;
 }

 void MediaPlaybackController::setStateChangedCallback(PlaybackStateChangedCallback callback) {
  if (impl_) impl_->stateChangedCallback_ = callback;
 }

 void MediaPlaybackController::setPositionChangedCallback(PositionChangedCallback callback) {
  if (impl_) impl_->positionChangedCallback_ = callback;
 }

 void MediaPlaybackController::setErrorCallback(ErrorCallback callback) {
  if (impl_) impl_->errorCallback_ = callback;
 }

 void MediaPlaybackController::setEndOfMediaCallback(EndOfMediaCallback callback) {
  if (impl_) impl_->endOfMediaCallback_ = callback;
 }

 void MediaPlaybackController::setLooping(bool enabled) {
  if (impl_) impl_->isLooping_ = enabled;
 }

 bool MediaPlaybackController::isLooping() const {
  return impl_ && impl_->isLooping_;
 }

 void MediaPlaybackController::setLoopRange(int64_t startMs, int64_t endMs) {
  if (!impl_) return;
  impl_->loopStartMs_ = startMs;
  impl_->loopEndMs_ = endMs;
 }

 void MediaPlaybackController::clearLoopRange() {
  if (!impl_) return;
  impl_->loopStartMs_ = 0;
  impl_->loopEndMs_ = -1;
 }

 double MediaPlaybackController::getBufferingProgress() const {
  // バッファリング進捗の実装はMediaReaderに依存
  return 100.0;
 }

 QString MediaPlaybackController::getLastError() const {
  return impl_ ? impl_->lastError_ : QString();
 }

 QImage MediaPlaybackController::generateThumbnail(int64_t timestampMs, const QSize& size) {
  if (!impl_) return QImage();
  
  seek(timestampMs, SeekMode::Fast);
  QImage frame = getNextVideoFrame();
  
  if (!frame.isNull() && size.isValid()) {
   return frame.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }
  
  return frame;
 }

 std::vector<QImage> MediaPlaybackController::generateThumbnails(int count, const QSize& size) {
  std::vector<QImage> thumbnails;
  if (!impl_ || impl_->durationMs_ <= 0 || count <= 0) {
   return thumbnails;
  }

  thumbnails.reserve(count);
  int64_t interval = impl_->durationMs_ / (count + 1);
  
  for (int i = 1; i <= count; ++i) {
   int64_t timestamp = interval * i;
   thumbnails.push_back(generateThumbnail(timestamp, size));
  }

  return thumbnails;
 }

} // namespace ArtifactCore
