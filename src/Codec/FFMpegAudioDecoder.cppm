module;
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h> 
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
}
#include <QString>
#include <QVector>
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
module Media.Encoder.FFmpegAudioDecoder;




import Utils.String.UniString;
import Audio.RingBuffer;
import Audio.Segment;
import Audio.BufferQueue;

namespace ArtifactCore
{
 namespace {
  AVDictionary* makeSingleThreadStreamInfoOptions()
  {
   AVDictionary* opts = nullptr;
   av_dict_set(&opts, "threads", "1", 0);
   av_dict_set(&opts, "thread_type", "0", 0);
   return opts;
  }

  AVDictionary* makeSingleThreadCodecOpenOptions()
  {
   AVDictionary* opts = nullptr;
   av_dict_set(&opts, "threads", "1", 0);
   av_dict_set(&opts, "thread_type", "0", 0);
   return opts;
  }

  AudioChannelLayout mapLayoutFromChannels(const int channels)
  {
   switch (channels) {
    case 1: return AudioChannelLayout::Mono;
    case 2: return AudioChannelLayout::Stereo;
    case 6: return AudioChannelLayout::Surround51;
    case 8: return AudioChannelLayout::Surround71;
    case 10: return AudioChannelLayout::Custom10ch;
    default: return AudioChannelLayout::Stereo;
   }
  }
 }

 class FFmpegAudioDecoder::Impl {
 private:

  AVFormatContext* fmtCtx_ = nullptr;
  AVCodecContext* codecCtx_ = nullptr;
  int audioStreamIndex_ = -1;
  AVPacket* pkt_ = nullptr;
  AVFrame* frame_ = nullptr;
  SwrContext* swrCtx_ = nullptr;
  AVChannelLayout dstChannelLayout_{};
  bool dstChannelLayoutInitialized_ = false;
  AVSampleFormat dstSampleFormat_ = AV_SAMPLE_FMT_FLTP;
  int dstSampleRate_ = 48000;
  bool decoderDraining_ = false;
  bool decoderDrained_ = false;
  qint64 nextExpectedFrame_ = 0;
  qint64 seekTargetFrame_ = -1;
  QString openedPath_;

  bool receiveAndQueueFrames(AudioBufferQueue& queue);
  bool convertAndQueueFrame(AudioBufferQueue& queue, const AVFrame* srcFrame);
  bool drainResampler(AudioBufferQueue& queue);
  qint64 resolveStartFrame(const AVFrame* srcFrame) const;
 public:
  double seekTargetSeconds_ = -1.0;
  Impl();
  ~Impl();
  bool openFile(const QString& path);
  void closeFile();
  void setupResampler();
  void seek(double seek);
  bool decodeNextFrame(AudioBufferQueue& queue);
  void flush();
  bool isSameFile(const QString& path);
  bool isSameFile(const UniString& path);
 };

 FFmpegAudioDecoder::Impl::Impl()
 {
  pkt_ = av_packet_alloc();
  frame_ = av_frame_alloc();
 }

 bool FFmpegAudioDecoder::Impl::openFile(const QString& path)
 {
  closeFile();
  if (avformat_open_input(&fmtCtx_, path.toUtf8().constData(), nullptr, nullptr) != 0)
  {
   return false;
  }
  AVDictionary* streamInfoOpts = makeSingleThreadStreamInfoOptions();
  if (avformat_find_stream_info(fmtCtx_, &streamInfoOpts) < 0)
  {
   av_dict_free(&streamInfoOpts);
   return false;
  }
  av_dict_free(&streamInfoOpts);
  const AVCodec* codec = nullptr;
  audioStreamIndex_ = av_find_best_stream(fmtCtx_, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
  if (audioStreamIndex_ < 0) return false;

  codecCtx_ = avcodec_alloc_context3(codec);
  if (!codecCtx_) return false;
  if (avcodec_parameters_to_context(codecCtx_, fmtCtx_->streams[audioStreamIndex_]->codecpar) < 0) {
   closeFile();
   return false;
  }

  codecCtx_->thread_count = 1;
  codecCtx_->thread_type = 0;

  AVDictionary* codecOpts = makeSingleThreadCodecOpenOptions();
  if (avcodec_open2(codecCtx_, codec, &codecOpts) < 0)
  {
   av_dict_free(&codecOpts);
   closeFile();
   return false;
  }
  av_dict_free(&codecOpts);

  if (!pkt_) pkt_ = av_packet_alloc();
  if (!frame_) frame_ = av_frame_alloc();
  if (!pkt_ || !frame_) {
   closeFile();
   return false;
  }

  setupResampler();
  if (!swrCtx_) {
   closeFile();
   return false;
  }

  decoderDraining_ = false;
  decoderDrained_ = false;
  nextExpectedFrame_ = 0;
  seekTargetFrame_ = -1;
  seekTargetSeconds_ = -1.0;
  openedPath_ = path;
  return true;
 }

 void FFmpegAudioDecoder::Impl::closeFile()
 {
  if (swrCtx_) {
   swr_free(&swrCtx_);
   swrCtx_ = nullptr;
  }
  if (dstChannelLayoutInitialized_) {
   av_channel_layout_uninit(&dstChannelLayout_);
   dstChannelLayoutInitialized_ = false;
  }
  if (pkt_) av_packet_unref(pkt_);
  if (frame_) av_frame_unref(frame_);

  if (codecCtx_) {
   avcodec_free_context(&codecCtx_);
   codecCtx_ = nullptr;
  }
  if (fmtCtx_) {
   avformat_close_input(&fmtCtx_);
   fmtCtx_ = nullptr;
  }
  audioStreamIndex_ = -1;
  decoderDraining_ = false;
  decoderDrained_ = false;
  nextExpectedFrame_ = 0;
  seekTargetFrame_ = -1;
  seekTargetSeconds_ = -1.0;
  openedPath_.clear();
 }

 void FFmpegAudioDecoder::Impl::seek(double seconds) {
  if (!fmtCtx_ || !codecCtx_ || audioStreamIndex_ < 0) return;

  AVRational tb = fmtCtx_->streams[audioStreamIndex_]->time_base;
  int64_t target_ts = av_rescale_q(
   static_cast<int64_t>(seconds * static_cast<double>(AV_TIME_BASE)),
   AVRational{1, AV_TIME_BASE},
   tb
  );

  if (av_seek_frame(fmtCtx_, audioStreamIndex_, target_ts, AVSEEK_FLAG_BACKWARD) >= 0) {
   flush();
   seekTargetSeconds_ = seconds;
   seekTargetFrame_ = static_cast<qint64>(std::llround(std::max(0.0, seconds) * dstSampleRate_));
   nextExpectedFrame_ = seekTargetFrame_;
  }
 }

 void FFmpegAudioDecoder::Impl::setupResampler()
 {
  if (!codecCtx_) return;
  if (swrCtx_) {
   swr_free(&swrCtx_);
   swrCtx_ = nullptr;
  }
  if (dstChannelLayoutInitialized_) {
   av_channel_layout_uninit(&dstChannelLayout_);
   dstChannelLayoutInitialized_ = false;
  }

  av_channel_layout_default(&dstChannelLayout_, 2);
  dstChannelLayoutInitialized_ = true;

  swr_alloc_set_opts2(&swrCtx_,
   &dstChannelLayout_, dstSampleFormat_, dstSampleRate_,
   &codecCtx_->ch_layout, codecCtx_->sample_fmt, codecCtx_->sample_rate,
   0, nullptr);
  if (!swrCtx_) return;

  if (swr_init(swrCtx_) < 0) {
   swr_free(&swrCtx_);
   swrCtx_ = nullptr;
  }
 }

 bool FFmpegAudioDecoder::Impl::decodeNextFrame(AudioBufferQueue& queue)
 {
  if (!fmtCtx_ || !codecCtx_ || audioStreamIndex_ < 0 || !pkt_ || !frame_ || !swrCtx_) {
   return false;
  }

  while (true) {
   if (decoderDrained_) {
    return drainResampler(queue);
   }

   int readRet = av_read_frame(fmtCtx_, pkt_);
   if (readRet == AVERROR_EOF) {
    if (!decoderDraining_) {
     const int sendRet = avcodec_send_packet(codecCtx_, nullptr);
     if (sendRet < 0 && sendRet != AVERROR_EOF && sendRet != AVERROR(EAGAIN)) {
      decoderDrained_ = true;
      return false;
     }
     decoderDraining_ = true;
    }
    const bool queued = receiveAndQueueFrames(queue);
    if (queued) return true;
    if (decoderDrained_) return drainResampler(queue);
    continue;
   }

   if (readRet < 0) {
    return false;
   }

   if (pkt_->stream_index != audioStreamIndex_) {
    av_packet_unref(pkt_);
    continue;
   }

   const int sendRet = avcodec_send_packet(codecCtx_, pkt_);
   av_packet_unref(pkt_);

   if (sendRet == AVERROR(EAGAIN)) {
    const bool queued = receiveAndQueueFrames(queue);
    if (queued) return true;
    continue;
   }
   if (sendRet < 0) {
    continue;
   }

   const bool queued = receiveAndQueueFrames(queue);
   if (queued) return true;
  }
 }

 FFmpegAudioDecoder::Impl::~Impl()
 {
  closeFile();
  if (pkt_) av_packet_free(&pkt_);
  if (frame_) av_frame_free(&frame_);
 }

 void FFmpegAudioDecoder::Impl::flush()
 {
  if (codecCtx_) {
   avcodec_flush_buffers(codecCtx_);
  }
  if (swrCtx_) {
   swr_close(swrCtx_);
   swr_init(swrCtx_);
  }
  if (pkt_) av_packet_unref(pkt_);
  if (frame_) av_frame_unref(frame_);
  decoderDraining_ = false;
  decoderDrained_ = false;
  nextExpectedFrame_ = 0;
  seekTargetSeconds_ = -1.0;
 }

 bool FFmpegAudioDecoder::Impl::isSameFile(const QString& path)
 {
  if (openedPath_.isEmpty()) return false;
  return openedPath_ == path;
 }

 bool FFmpegAudioDecoder::Impl::isSameFile(const UniString& path)
 {
  return isSameFile(path.toQString());
 }

 bool FFmpegAudioDecoder::Impl::receiveAndQueueFrames(AudioBufferQueue& queue)
 {
  bool queuedAny = false;

  while (true) {
   const int ret = avcodec_receive_frame(codecCtx_, frame_);
   if (ret == AVERROR(EAGAIN)) {
    return queuedAny;
   }
   if (ret == AVERROR_EOF) {
    decoderDrained_ = true;
    return queuedAny;
   }
   if (ret < 0) {
    return queuedAny;
   }

   if (convertAndQueueFrame(queue, frame_)) {
    queuedAny = true;
   }
   av_frame_unref(frame_);
  }
 }

 bool FFmpegAudioDecoder::Impl::convertAndQueueFrame(AudioBufferQueue& queue, const AVFrame* srcFrame)
 {
  if (!srcFrame || !swrCtx_ || !codecCtx_) return false;
  const int channels = dstChannelLayout_.nb_channels;
  if (channels <= 0 || channels > 64) return false;

  const int dstNbSamples = static_cast<int>(av_rescale_rnd(
   swr_get_delay(swrCtx_, codecCtx_->sample_rate) + srcFrame->nb_samples,
   dstSampleRate_,
   codecCtx_->sample_rate,
   AV_ROUND_UP
  ));
  if (dstNbSamples <= 0) return false;

  AudioSegment segment;
  segment.sampleRate = dstSampleRate_;
  segment.layout = mapLayoutFromChannels(channels);
  segment.channelData.resize(channels);
  for (int ch = 0; ch < channels; ++ch) {
   segment.channelData[ch].resize(dstNbSamples);
  }

  uint8_t* outPtrs[64] = {};
  for (int ch = 0; ch < channels; ++ch) {
   outPtrs[ch] = reinterpret_cast<uint8_t*>(segment.channelData[ch].data());
  }

  const int converted = swr_convert(
   swrCtx_,
   outPtrs,
   dstNbSamples,
   const_cast<const uint8_t**>(srcFrame->extended_data),
   srcFrame->nb_samples
  );

  if (converted <= 0) return false;
  for (int ch = 0; ch < channels; ++ch) {
   segment.channelData[ch].resize(converted);
  }

  qint64 startFrame = resolveStartFrame(srcFrame);
  qint64 endFrame = startFrame + converted;

  if (seekTargetFrame_ >= 0) {
   if (endFrame <= seekTargetFrame_) {
    nextExpectedFrame_ = endFrame;
    return false;
   }
   if (startFrame < seekTargetFrame_) {
    const int trim = static_cast<int>(seekTargetFrame_ - startFrame);
    if (trim >= converted) {
     nextExpectedFrame_ = endFrame;
     return false;
    }
    for (int ch = 0; ch < channels; ++ch) {
     segment.channelData[ch] = segment.channelData[ch].mid(trim);
    }
    startFrame = seekTargetFrame_;
   }
   seekTargetFrame_ = -1;
  }

  segment.startFrame = startFrame;
  queue.push(segment);
  nextExpectedFrame_ = segment.startFrame + segment.frameCount();
  return true;
 }

 bool FFmpegAudioDecoder::Impl::drainResampler(AudioBufferQueue& queue)
 {
  if (!swrCtx_ || !codecCtx_) return false;

  const int pendingIn = static_cast<int>(swr_get_delay(swrCtx_, codecCtx_->sample_rate));
  if (pendingIn <= 0) return false;

  const int channels = dstChannelLayout_.nb_channels;
  if (channels <= 0 || channels > 64) return false;

  const int dstNbSamples = static_cast<int>(av_rescale_rnd(
   pendingIn,
   dstSampleRate_,
   codecCtx_->sample_rate,
   AV_ROUND_UP
  ));
  if (dstNbSamples <= 0) return false;

  AudioSegment segment;
  segment.sampleRate = dstSampleRate_;
  segment.layout = mapLayoutFromChannels(channels);
  segment.startFrame = nextExpectedFrame_;
  segment.channelData.resize(channels);
  for (int ch = 0; ch < channels; ++ch) {
   segment.channelData[ch].resize(dstNbSamples);
  }

  uint8_t* outPtrs[64] = {};
  for (int ch = 0; ch < channels; ++ch) {
   outPtrs[ch] = reinterpret_cast<uint8_t*>(segment.channelData[ch].data());
  }

  const int converted = swr_convert(swrCtx_, outPtrs, dstNbSamples, nullptr, 0);
  if (converted <= 0) return false;

  for (int ch = 0; ch < channels; ++ch) {
   segment.channelData[ch].resize(converted);
  }
  queue.push(segment);
  nextExpectedFrame_ += converted;
  return true;
 }

 qint64 FFmpegAudioDecoder::Impl::resolveStartFrame(const AVFrame* srcFrame) const
 {
  if (!srcFrame || !fmtCtx_ || audioStreamIndex_ < 0) {
   return nextExpectedFrame_;
  }

  int64_t pts = srcFrame->best_effort_timestamp;
  if (pts == AV_NOPTS_VALUE) pts = srcFrame->pts;
  if (pts == AV_NOPTS_VALUE) {
   return nextExpectedFrame_;
  }

  const AVRational srcTb = fmtCtx_->streams[audioStreamIndex_]->time_base;
  const int64_t framePos = av_rescale_q(pts, srcTb, AVRational{1, dstSampleRate_});
  if (framePos < nextExpectedFrame_) {
   return nextExpectedFrame_;
  }
  return framePos;
 }

 FFmpegAudioDecoder::FFmpegAudioDecoder() :impl_(new Impl())
 {

 }

 FFmpegAudioDecoder::~FFmpegAudioDecoder()
 {
  delete impl_;
 }

 bool FFmpegAudioDecoder::openFile(const QString& path)
 {
  return impl_->openFile(path);
 }

 void FFmpegAudioDecoder::closeFile()
 {
  impl_->closeFile();
 }

 void FFmpegAudioDecoder::seek(double seek)
 {
  impl_->seek(seek);
 }

 void FFmpegAudioDecoder::fillCacheAsync(double start, double end)
 {
  (void)start;
  (void)end;
 }

 void FFmpegAudioDecoder::flush()
 {
  impl_->flush();
 }

 bool FFmpegAudioDecoder::isSameFile(const UniString& path) const
 {
  return impl_->isSameFile(path);
 }

};
