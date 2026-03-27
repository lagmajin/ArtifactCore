module;

#define QT_NO_KEYWORDS
#include <QDebug>
#include <QString>
#include <QImage>
#include <QByteArray>
#include <QFileInfo>
#include <QSize>
#include <Qt>

extern "C" {
#include <libavutil/error.h>
}

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module MediaPlaybackController;

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace ArtifactCore {

class MediaPlaybackController::Impl {
 public:
  MediaSource* mediaSource_ = nullptr;
  MediaReader* mediaReader_ = nullptr;
  MediaImageFrameDecoder* videoDecoder_ = nullptr;
  MediaAudioDecoder* audioDecoder_ = nullptr;
  MFFrameExtractor* mfExtractor_ = nullptr;

  DecoderBackend backend_ = DecoderBackend::MediaFoundation;

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
  AVRational videoTimeBase_ = {1, 1000};
  int videoStreamIndex_ = -1;
  int videoPacketWaitAttempts_ = 100;

  PlaybackStateChangedCallback stateChangedCallback_;
  PositionChangedCallback positionChangedCallback_;
  ErrorCallback errorCallback_;
  EndOfMediaCallback endOfMediaCallback_;

  QString lastError_;
  MediaMetaData metadata_;
  std::mutex directDecodeMutex_;

  Impl()
      : mediaSource_(new MediaSource()),
        mediaReader_(new MediaReader(mediaSource_)),
        videoDecoder_(new MediaImageFrameDecoder()),
        audioDecoder_(new MediaAudioDecoder()),
        mfExtractor_(new MFFrameExtractor()) {}

  ~Impl() {
    delete audioDecoder_;
    delete videoDecoder_;
    delete mediaReader_;
    delete mediaSource_;
    delete mfExtractor_;
  }

  QImage decodeVideoFrameDirectAtFrame(int64_t frameNumber) {
    if (backend_ == DecoderBackend::MediaFoundation && mfExtractor_ && mfExtractor_->isOpen()) {
      auto frame = mfExtractor_->extractFrameAtIndex(frameNumber);
      if (frame && frame->isValid()) {
        QImage img(frame->data.data(), frame->width, frame->height, QImage::Format_RGBA8888);
        return img.copy();
      }
      return QImage();
    }

    if (!mediaSource_ || !mediaSource_->isOpen() || !videoDecoder_ || fps_ <= 0.0 || videoStreamIndex_ < 0) {
      qWarning() << "[MediaPlayback] direct decode skipped: invalid state"
                 << "mediaSource_=" << (mediaSource_ && mediaSource_->isOpen() ? "open" : "closed")
                 << "videoDecoder_=" << (videoDecoder_ ? "ok" : "null")
                 << "fps_=" << fps_
                 << "videoStreamIndex_=" << videoStreamIndex_;
      return QImage();
    }

    std::lock_guard<std::mutex> lock(directDecodeMutex_);

    const bool wasPlaying = state_ == PlaybackState::Playing;

    const int64_t targetMs = static_cast<int64_t>((frameNumber / fps_) * 1000.0);
    if (!prepareFfmpegSeekReset(targetMs, true)) {
      qWarning() << "[MediaPlayback] direct decode seek failed:" << targetMs << "ms";
      restoreFfmpegReaderState(wasPlaying);
      return QImage();
    }

    if (auto ctx = mediaSource_->getFormatContext(); !ctx) {
      qWarning() << "[MediaPlayback] direct decode failed: no format context";
      restoreFfmpegReaderState(wasPlaying);
      return QImage();
    }

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
      qWarning() << "[MediaPlayback] direct decode failed: packet alloc";
      restoreFfmpegReaderState(wasPlaying);
      return QImage();
    }

    QImage result;
    AVFormatContext* ctx = mediaSource_->getFormatContext();
    const int maxPackets = 512;
    int packetsRead = 0;
    while (packetsRead < maxPackets) {
      if (av_read_frame(ctx, pkt) < 0) {
        break;
      }
      ++packetsRead;

      if (pkt->stream_index == videoStreamIndex_) {
        const int ret = videoDecoder_->sendPacket(pkt);
        av_packet_unref(pkt);
        if (ret < 0) {
          qWarning() << "[MediaPlayback] sendPacket failed:" << ret;
          break;
        }

        result = videoDecoder_->receiveFrame();
        if (!result.isNull()) {
          break;
        }
      } else {
        av_packet_unref(pkt);
      }
    }

    av_packet_free(&pkt);

    currentPositionMs_ = targetMs;
    currentFrame_ = frameNumber;

    restoreFfmpegReaderState(wasPlaying);

    if (result.isNull()) {
      qWarning() << "[MediaPlayback] direct decode failed for frame" << frameNumber;
    }
    return result;
  }

  void updatePlaybackInfo() {
    if (!mediaSource_ || !mediaSource_->isOpen()) {
      return;
    }
    AVFormatContext* ctx = mediaSource_->getFormatContext();
    if (!ctx) {
      return;
    }

    durationMs_ = ctx->duration / 1000;
    fps_ = 0.0;
    totalFrames_ = 0;
    for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
      if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        videoStreamIndex_ = i;
        videoTimeBase_ = ctx->streams[i]->time_base;
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

  void rebuildReader() {
    if (mediaReader_) {
      mediaReader_->stop();
      delete mediaReader_;
    }
    mediaReader_ = new MediaReader(mediaSource_);
  }

  void flushFfmpegDecoders() {
    if (videoDecoder_) {
      videoDecoder_->flush();
    }
    if (audioDecoder_) {
      audioDecoder_->flush();
    }
  }

  bool prepareFfmpegSeekReset(int64_t timestampMs, bool rebuildReaderAfterSeek) {
    if (!mediaSource_) {
      return false;
    }

    if (mediaReader_) {
      mediaReader_->stop();
    }

    if (!mediaSource_->seek(timestampMs)) {
      return false;
    }

    if (rebuildReaderAfterSeek) {
      rebuildReader();
    }

    flushFfmpegDecoders();
    return true;
  }

  void restoreFfmpegReaderState(bool wasPlaying) {
    if (wasPlaying && mediaReader_) {
      mediaReader_->start();
    }
  }

  void setMetadataFromMediaFoundation(const QString& url) {
    metadata_ = MediaMetaData();
    metadata_.filePath = url;
    QFileInfo fileInfo(url);
    metadata_.fileName = fileInfo.fileName();
    metadata_.fileExtension = fileInfo.suffix();
    if (fileInfo.exists()) {
      metadata_.fileSize = fileInfo.size();
    }

    if (!mfExtractor_ || !mfExtractor_->isOpen()) {
      return;
    }

    metadata_.duration = mfExtractor_->getDurationSeconds();
    metadata_.formatName = QStringLiteral("MediaFoundation");
    metadata_.formatLongName = QStringLiteral("Media Foundation direct frame extractor");

    StreamInfo streamInfo;
    streamInfo.index = 0;
    streamInfo.type = MediaType::Video;
    streamInfo.resolution = QSize(mfExtractor_->getWidth(), mfExtractor_->getHeight());
    streamInfo.frameRate = mfExtractor_->getFrameRate();
    streamInfo.frameCount = mfExtractor_->getTotalFrames();
    streamInfo.duration = mfExtractor_->getDurationSeconds();
    streamInfo.videoCodec.codecName = mfExtractor_->getCodecName();
    streamInfo.videoCodec.codecLongName = mfExtractor_->getCodecName();
    metadata_.streams.push_back(std::move(streamInfo));

    if (metadata_.duration > 0.0) {
      durationMs_ = static_cast<int64_t>(metadata_.duration * 1000.0);
    }
    fps_ = mfExtractor_->getFrameRate();
    totalFrames_ = mfExtractor_->getTotalFrames();
    if (totalFrames_ <= 0 && fps_ > 0.0 && durationMs_ > 0) {
      totalFrames_ = static_cast<int64_t>((durationMs_ / 1000.0) * fps_);
    }
  }

  void resetMetadata(const QString& url) {
    metadata_ = MediaMetaData();
    metadata_.filePath = url;
    QFileInfo fileInfo(url);
    metadata_.fileName = fileInfo.fileName();
    metadata_.fileExtension = fileInfo.suffix();
    if (fileInfo.exists()) {
      metadata_.fileSize = fileInfo.size();
    }

    if (!mediaSource_ || !mediaSource_->isOpen()) {
      return;
    }

    AVFormatContext* ctx = mediaSource_->getFormatContext();
    if (!ctx) {
      return;
    }

    metadata_.duration = ctx->duration > 0 ? static_cast<double>(ctx->duration) / AV_TIME_BASE : 0.0;
    metadata_.bitrate = ctx->bit_rate;
    if (ctx->iformat) {
      metadata_.formatName = QString::fromUtf8(ctx->iformat->name);
      metadata_.formatLongName = QString::fromUtf8(ctx->iformat->long_name ? ctx->iformat->long_name : "");
    }

    metadata_.streams.clear();
    metadata_.streams.reserve(ctx->nb_streams);
    for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
      const AVStream* stream = ctx->streams[i];
      const AVCodecParameters* params = stream->codecpar;
      StreamInfo streamInfo;
      streamInfo.index = static_cast<int>(i);
      streamInfo.duration = stream->duration > 0 ? stream->duration * av_q2d(stream->time_base) : metadata_.duration;
      streamInfo.bitrate = params->bit_rate;

      switch (params->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
          streamInfo.type = MediaType::Video;
          streamInfo.resolution = QSize(params->width, params->height);
          if (stream->avg_frame_rate.den > 0) {
            streamInfo.frameRate = av_q2d(stream->avg_frame_rate);
          }
          streamInfo.frameCount = stream->nb_frames;
          if (params->codec_id != AV_CODEC_ID_NONE) {
            const AVCodecDescriptor* desc = avcodec_descriptor_get(params->codec_id);
            if (desc) {
              streamInfo.videoCodec.codecName = QString::fromUtf8(desc->name);
              streamInfo.videoCodec.codecLongName = QString::fromUtf8(desc->long_name ? desc->long_name : "");
            }
          }
          break;
        case AVMEDIA_TYPE_AUDIO:
          streamInfo.type = MediaType::Audio;
          streamInfo.audioCodec.sampleRate = params->sample_rate;
          streamInfo.audioCodec.channels = params->ch_layout.nb_channels;
          streamInfo.audioCodec.bitrate = params->bit_rate;
          if (params->codec_id != AV_CODEC_ID_NONE) {
            const AVCodecDescriptor* desc = avcodec_descriptor_get(params->codec_id);
            if (desc) {
              streamInfo.audioCodec.codecName = QString::fromUtf8(desc->name);
              streamInfo.audioCodec.codecLongName = QString::fromUtf8(desc->long_name ? desc->long_name : "");
            }
          }
          break;
        default:
          streamInfo.type = MediaType::Unknown;
          break;
      }

      metadata_.streams.push_back(std::move(streamInfo));
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

class PlaybackBackend {
 public:
  virtual ~PlaybackBackend() = default;
  virtual DecoderBackend type() const = 0;
  virtual bool open(MediaPlaybackController::Impl& impl, const QString& url) = 0;
  virtual void close(MediaPlaybackController::Impl& impl) = 0;
  virtual bool isOpen(const MediaPlaybackController::Impl& impl) const = 0;
  virtual void seek(MediaPlaybackController::Impl& impl, int64_t timestampMs, SeekMode mode) = 0;
  virtual void seekToFrame(MediaPlaybackController::Impl& impl, int64_t frameNumber) = 0;
  virtual QImage getNextVideoFrame(MediaPlaybackController::Impl& impl) = 0;
  virtual QImage getVideoFrameAtFrameDirect(MediaPlaybackController::Impl& impl, int64_t frameNumber) = 0;
};

class FFmpegPlaybackBackend final : public PlaybackBackend {
 public:
  DecoderBackend type() const override { return DecoderBackend::FFmpeg; }

  bool open(MediaPlaybackController::Impl& impl, const QString& url) override;
  void close(MediaPlaybackController::Impl& impl) override;
  bool isOpen(const MediaPlaybackController::Impl& impl) const override;
  void seek(MediaPlaybackController::Impl& impl, int64_t timestampMs, SeekMode mode) override;
  void seekToFrame(MediaPlaybackController::Impl& impl, int64_t frameNumber) override;
  QImage getNextVideoFrame(MediaPlaybackController::Impl& impl) override;
  QImage getVideoFrameAtFrameDirect(MediaPlaybackController::Impl& impl, int64_t frameNumber) override;
};

class MFPlaybackBackend final : public PlaybackBackend {
 public:
  DecoderBackend type() const override { return DecoderBackend::MediaFoundation; }

  bool open(MediaPlaybackController::Impl& impl, const QString& url) override;
  void close(MediaPlaybackController::Impl& impl) override;
  bool isOpen(const MediaPlaybackController::Impl& impl) const override;
  void seek(MediaPlaybackController::Impl& impl, int64_t timestampMs, SeekMode mode) override;
  void seekToFrame(MediaPlaybackController::Impl& impl, int64_t frameNumber) override;
  QImage getNextVideoFrame(MediaPlaybackController::Impl& impl) override;
  QImage getVideoFrameAtFrameDirect(MediaPlaybackController::Impl& impl, int64_t frameNumber) override;
};

static PlaybackBackend& backendFor(DecoderBackend backend) {
  static FFmpegPlaybackBackend ffmpegBackend;
  static MFPlaybackBackend mfBackend;
  return backend == DecoderBackend::MediaFoundation ? static_cast<PlaybackBackend&>(mfBackend)
                                                    : static_cast<PlaybackBackend&>(ffmpegBackend);
}

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

  PlaybackBackend& preferred = backendFor(impl_->backend_);
  if (preferred.open(*impl_, url)) {
    impl_->notifyStateChanged(PlaybackState::Stopped);
    return true;
  }
  const QString preferredName = impl_->backend_ == DecoderBackend::MediaFoundation
      ? QStringLiteral("MediaFoundation")
      : QStringLiteral("FFmpeg");
  const QString preferredError = impl_->lastError_;

  PlaybackBackend& fallback = backendFor(impl_->backend_ == DecoderBackend::MediaFoundation
                                          ? DecoderBackend::FFmpeg
                                          : DecoderBackend::MediaFoundation);
  if (fallback.open(*impl_, url)) {
    impl_->backend_ = fallback.type();
    impl_->notifyStateChanged(PlaybackState::Stopped);
    return true;
  }

  impl_->notifyError(QStringLiteral("Failed to open media '%1' with %2 (error: %3)")
                     .arg(url, preferredName, preferredError));
  return false;
 }

 bool MediaPlaybackController::openMediaFile(const QString& filePath) {
  return openMedia(filePath);
 }

	 void MediaPlaybackController::closeMedia() {
	  if (!impl_) return;
	  stop();
	  backendFor(impl_->backend_).close(*impl_);
	  impl_->backend_ = DecoderBackend::MediaFoundation;
	  impl_->metadata_ = MediaMetaData();
	  impl_->currentPositionMs_ = 0;
  impl_->durationMs_ = 0;
  impl_->currentFrame_ = 0;
  impl_->totalFrames_ = 0;
  impl_->fps_ = 0.0;
 }

 bool MediaPlaybackController::isMediaOpen() const {
  if (!impl_) return false;
  return backendFor(impl_->backend_).isOpen(*impl_);
 }

 void MediaPlaybackController::play() {
  if (!impl_) return;
  if (impl_->state_ != PlaybackState::Playing) {
   if (impl_->backend_ == DecoderBackend::FFmpeg && impl_->mediaReader_) {
    impl_->mediaReader_->start();
   }
   impl_->notifyStateChanged(PlaybackState::Playing);
  }
 }

 void MediaPlaybackController::pause() {
  if (!impl_) return;
  if (impl_->backend_ == DecoderBackend::FFmpeg && impl_->mediaReader_) {
   impl_->mediaReader_->pause();
  }
  impl_->notifyStateChanged(PlaybackState::Paused);
 }

 void MediaPlaybackController::stop() {
  if (!impl_) return;
  if (impl_->backend_ == DecoderBackend::FFmpeg) {
   if (impl_->mediaReader_) impl_->mediaReader_->stop();
   if (impl_->videoDecoder_) impl_->videoDecoder_->flush();
   if (impl_->audioDecoder_) impl_->audioDecoder_->flush();
  }
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
	  if (!impl_) {
	   return;
	  }

	  backendFor(impl_->backend_).seek(*impl_, timestampMs, mode);
	 }

 void MediaPlaybackController::seekToSeconds(double seconds, SeekMode mode) {
  seek(static_cast<int64_t>(seconds * 1000.0), mode);
 }

 void MediaPlaybackController::seekToFrame(int64_t frameNumber) {
  if (!impl_) return;
  backendFor(impl_->backend_).seekToFrame(*impl_, frameNumber);
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

 void MediaPlaybackController::setDecoderBackend(DecoderBackend backend) {
  if (impl_) impl_->backend_ = backend;
 }

 DecoderBackend MediaPlaybackController::getDecoderBackend() const {
  return impl_ ? impl_->backend_ : DecoderBackend::FFmpeg;
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
  if (!impl_) return QImage();
  return backendFor(impl_->backend_).getNextVideoFrame(*impl_);
 }

 QImage MediaPlaybackController::getCurrentVideoFrame() {
  if (!impl_ || !impl_->videoDecoder_) {
   return QImage();
  }
  // ݈ʒũt[擾iĐʒui߂Ȃj
  // ۂ̎ł݂͌̃t[LbVKv
  return QImage();
 }

 QImage MediaPlaybackController::getVideoFrameAt(int64_t timestampMs) {
  if (!impl_) return QImage();
  const double fps = impl_->fps_;
  const int64_t frameNumber = fps > 0.0 ? static_cast<int64_t>((timestampMs / 1000.0) * fps) : 0;
  return backendFor(impl_->backend_).getVideoFrameAtFrameDirect(*impl_, frameNumber);
 }

 QImage MediaPlaybackController::getVideoFrameAtFrame(int64_t frameNumber) {
  if (!impl_) {
   return QImage();
  }

  if (impl_->backend_ == DecoderBackend::MediaFoundation) {
   return backendFor(impl_->backend_).getVideoFrameAtFrameDirect(*impl_, frameNumber);
  }

  if (impl_->fps_ <= 0) {
   qDebug() << "[MediaPlayback] getVideoFrameAtFrame: impl_ or fps invalid";
   return QImage();
  }

  double targetSec = frameNumber / impl_->fps_;
  int64_t targetMs = static_cast<int64_t>(targetSec * 1000.0);

  int64_t targetPts = 0;
  if (impl_->videoStreamIndex_ >= 0 && impl_->videoTimeBase_.den > 0) {
      double timebaseSec = static_cast<double>(impl_->videoTimeBase_.num) / impl_->videoTimeBase_.den;
      targetPts = static_cast<int64_t>(targetSec / timebaseSec);
  } else {
      targetPts = static_cast<int64_t>(targetSec * 90000.0);
  }

  qDebug() << "[MediaPlayback] getVideoFrameAtFrame:" << frameNumber
           << "targetSec=" << targetSec
           << "targetMs=" << targetMs
           << "targetPts=" << targetPts
           << "fps=" << impl_->fps_;

  seek(targetMs, SeekMode::Accurate);

  QImage result;
  const int maxDiscard = 300;
  int loopCount = 0;
  for (int i = 0; i < maxDiscard; ++i) {
      result = getNextVideoFrame();
      loopCount = i + 1;
      if (result.isNull()) {
          break;
      }
      
      int64_t currentPts = impl_->videoDecoder_->getLastDecodedPts();
      if (currentPts >= targetPts) {
          break;
      }
  }

  if (result.isNull()) {
   qDebug() << "[MediaPlayback] getVideoFrameAtFrame: NULL after" << loopCount << "iterations";
    result = backendFor(impl_->backend_).getVideoFrameAtFrameDirect(*impl_, frameNumber);
  } else {
   qDebug() << "[MediaPlayback] got frame after" << loopCount << "iterations, size=" << result.width() << "x" << result.height();
  }
  
  impl_->currentPositionMs_ = targetMs;
  impl_->currentFrame_ = frameNumber;
  
  return result;
 }

 QImage MediaPlaybackController::getVideoFrameAtFrameDirect(int64_t frameNumber) {
  if (!impl_) {
   return QImage();
  }
  return backendFor(impl_->backend_).getVideoFrameAtFrameDirect(*impl_, frameNumber);
 }

	 QByteArray MediaPlaybackController::getNextAudioFrame() {
  if (!impl_ || !impl_->mediaReader_ || !impl_->audioDecoder_) {
   return QByteArray();
  }

	  impl_->mediaReader_->start();
	  AVPacket* pkt = nullptr;
	  for (int attempt = 0; attempt < impl_->videoPacketWaitAttempts_ && !pkt; ++attempt) {
	   pkt = impl_->mediaReader_->getNextPacket(StreamType::Audio);
	   if (!pkt) {
	    std::this_thread::sleep_for(std::chrono::milliseconds(2));
	   }
	  }
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
  // obt@Oi̎MediaReaderɈˑ
  return 100.0;
 }

 QString MediaPlaybackController::getLastError() const {
  return impl_ ? impl_->lastError_ : QString();
 }

 QImage MediaPlaybackController::generateThumbnail(int64_t timestampMs, const QSize& size) {
  if (!impl_) return QImage();

  const double fps = impl_->fps_;
  const int64_t frameNumber = fps > 0.0 ? static_cast<int64_t>((timestampMs / 1000.0) * fps) : 0;
  QImage frame = backendFor(impl_->backend_).getVideoFrameAtFrameDirect(*impl_, frameNumber);
  
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

bool FFmpegPlaybackBackend::open(MediaPlaybackController::Impl& impl, const QString& url) {
  const bool opened = impl.mediaSource_ && impl.mediaSource_->open(url);
  if (!opened) {
    return false;
  }

  if (AVFormatContext* ctx = impl.mediaSource_->getFormatContext()) {
    impl.rebuildReader();
    const QFileInfo fileInfo(url);
    const qint64 fileSize = fileInfo.exists() ? fileInfo.size() : 0;
    impl.videoPacketWaitAttempts_ =
        std::clamp<int>(100 + static_cast<int>(fileSize / (10 * 1024 * 1024)), 100, 500);

    bool foundVideoStream = false;
    bool videoDecoderOk = false;
    bool audioDecoderOk = true;
    for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
      AVCodecParameters* params = ctx->streams[i]->codecpar;
      if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
        foundVideoStream = true;
        if (!impl.videoDecoder_->initialize(params)) {
          qWarning() << "[FFmpegBackend] Failed to initialize video decoder";
          impl.notifyError(QStringLiteral("Video decoder initialization failed for '%1'").arg(url));
        } else {
          videoDecoderOk = true;
        }
      } else if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
        if (!impl.audioDecoder_->initialize(params)) {
          qWarning() << "[FFmpegBackend] Failed to initialize audio decoder";
          impl.notifyError(QStringLiteral("Audio decoder initialization failed for '%1'").arg(url));
          audioDecoderOk = false;
        }
      }
    }
    impl.resetMetadata(url);
    impl.updatePlaybackInfo();
    // [Fix 2] updatePlaybackInfo 後の videoStreamIndex_ を確認。
    // -1 のままの場合 decodeVideoFrameDirectAtFrame が失敗する。
    qDebug() << "[FFmpegBackend] opened url=" << url
             << "videoStreamIndex_=" << impl.videoStreamIndex_
             << "fps_=" << impl.fps_
             << "totalFrames_=" << impl.totalFrames_
             << "waitAttempts=" << impl.videoPacketWaitAttempts_;
    if (!foundVideoStream || impl.videoStreamIndex_ < 0) {
      qWarning() << "[FFmpegBackend] No video stream found in" << url << "- open will fail.";
      impl.notifyError(QStringLiteral("No video stream found in '%1'").arg(url));
    }
    if (!foundVideoStream || impl.videoStreamIndex_ < 0 || !videoDecoderOk || !audioDecoderOk) {
      close(impl);
      return false;
    }
  }

  impl.backend_ = DecoderBackend::FFmpeg;
  impl.lastError_.clear();
  return true;
}

void FFmpegPlaybackBackend::close(MediaPlaybackController::Impl& impl) {
  if (impl.mediaReader_) {
    impl.mediaReader_->stop();
  }
  if (impl.mediaSource_) {
    impl.mediaSource_->close();
  }
  if (impl.videoDecoder_) {
    impl.videoDecoder_->flush();
  }
  if (impl.audioDecoder_) {
    impl.audioDecoder_->flush();
  }
}

bool FFmpegPlaybackBackend::isOpen(const MediaPlaybackController::Impl& impl) const {
  return impl.mediaSource_ && impl.mediaSource_->isOpen();
}

void FFmpegPlaybackBackend::seek(MediaPlaybackController::Impl& impl, int64_t timestampMs, SeekMode) {
  if (!impl.mediaSource_) {
    return;
  }
  const bool wasPlaying = impl.state_ == PlaybackState::Playing;
  if (!impl.prepareFfmpegSeekReset(timestampMs, true)) {
    impl.restoreFfmpegReaderState(wasPlaying);
    return;
  }
  impl.notifyPositionChanged(timestampMs);
  impl.restoreFfmpegReaderState(wasPlaying);
}

void FFmpegPlaybackBackend::seekToFrame(MediaPlaybackController::Impl& impl, int64_t frameNumber) {
  if (impl.fps_ <= 0.0) {
    return;
  }
  const double seconds = frameNumber / impl.fps_;
  seek(impl, static_cast<int64_t>(seconds * 1000.0), SeekMode::Accurate);
}

QImage FFmpegPlaybackBackend::getNextVideoFrame(MediaPlaybackController::Impl& impl) {
  if (!impl.mediaReader_ || !impl.videoDecoder_) {
    return QImage();
  }

  impl.mediaReader_->start();
  while (true) {
    AVPacket* pkt = nullptr;
    for (int attempt = 0; attempt < impl.videoPacketWaitAttempts_ && !pkt; ++attempt) {
      pkt = impl.mediaReader_->getNextPacket(StreamType::Video);
      if (!pkt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    }
    if (!pkt) {
      impl.notifyEndOfMedia();
      return QImage();
    }

    QImage img = impl.videoDecoder_->decodeFrame(pkt);
    av_packet_free(&pkt);
    if (!img.isNull()) {
      impl.notifyPositionChanged(impl.currentPositionMs_ + static_cast<int64_t>(1000.0 / std::max(0.0001, impl.fps_)));
      return img;
    }
  }
}

QImage FFmpegPlaybackBackend::getVideoFrameAtFrameDirect(MediaPlaybackController::Impl& impl, int64_t frameNumber) {
  return impl.decodeVideoFrameDirectAtFrame(frameNumber);
}

bool MFPlaybackBackend::open(MediaPlaybackController::Impl& impl, const QString& url) {
  if (!impl.mfExtractor_) {
    qWarning() << "[MFBackend] extractor not available for" << url;
    return false;
  }
  if (!impl.mfExtractor_->open(url)) {
    qWarning() << "[MFBackend] open failed for" << url
               << "reason=" << impl.mfExtractor_->lastError();
    return false;
  }
  impl.backend_ = DecoderBackend::MediaFoundation;
  impl.setMetadataFromMediaFoundation(url);
  impl.lastError_.clear();
  return true;
}

void MFPlaybackBackend::close(MediaPlaybackController::Impl& impl) {
  if (impl.mfExtractor_) {
    impl.mfExtractor_->close();
  }
}

bool MFPlaybackBackend::isOpen(const MediaPlaybackController::Impl& impl) const {
  return impl.mfExtractor_ && impl.mfExtractor_->isOpen();
}

void MFPlaybackBackend::seek(MediaPlaybackController::Impl& impl, int64_t timestampMs, SeekMode) {
  impl.currentPositionMs_ = std::max<int64_t>(0, timestampMs);
  if (impl.fps_ > 0.0) {
    impl.currentFrame_ = static_cast<int64_t>((impl.currentPositionMs_ / 1000.0) * impl.fps_);
  }
  impl.notifyPositionChanged(impl.currentPositionMs_);
}

void MFPlaybackBackend::seekToFrame(MediaPlaybackController::Impl& impl, int64_t frameNumber) {
  impl.currentFrame_ = std::max<int64_t>(0, frameNumber);
  if (impl.fps_ > 0.0) {
    impl.currentPositionMs_ = static_cast<int64_t>((impl.currentFrame_ / impl.fps_) * 1000.0);
  }
  impl.notifyPositionChanged(impl.currentPositionMs_);
}

QImage MFPlaybackBackend::getNextVideoFrame(MediaPlaybackController::Impl& impl) {
  const int64_t frameNumber = std::max<int64_t>(0, impl.currentFrame_);
  return getVideoFrameAtFrameDirect(impl, frameNumber);
}

QImage MFPlaybackBackend::getVideoFrameAtFrameDirect(MediaPlaybackController::Impl& impl, int64_t frameNumber) {
  if (!impl.mfExtractor_ || !impl.mfExtractor_->isOpen()) {
    qWarning() << "[MFBackend] frame extraction requested while extractor is closed"
               << "frame=" << frameNumber;
    return QImage();
  }
  auto frame = impl.mfExtractor_->extractFrameAtIndex(frameNumber);
  if (!frame || !frame->isValid()) {
    qWarning() << "[MFBackend] extractFrameAtIndex FAILED for frame" << frameNumber
               << "mfExtractor open=" << impl.mfExtractor_->isOpen()
               << "totalFrames=" << impl.mfExtractor_->getTotalFrames()
               << "reason=" << impl.mfExtractor_->lastError();
    return QImage();
  }
  QImage img(frame->data.data(), frame->width, frame->height, QImage::Format_RGBA8888);
  QImage result = img.copy();
  impl.currentFrame_ = frameNumber + 1;
  if (impl.fps_ > 0.0) {
    impl.currentPositionMs_ = static_cast<int64_t>((impl.currentFrame_ / impl.fps_) * 1000.0);
  }
  return result;
}

} // namespace ArtifactCore
