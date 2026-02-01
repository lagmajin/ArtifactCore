module;

#define QT_NO_KEYWORDS
#include <QDebug>
#include <QByteArray>
#include <QString>
#include <string>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

module MediaAudioDecoder;

import std;

namespace ArtifactCore {

 class MediaAudioDecoder::Impl {
 public:
  AVCodecContext* codecContext_ = nullptr;
  SwrContext* swrCtx_ = nullptr;
  bool initialized_ = false;
  bool resamplingEnabled_ = false;
  ResamplingConfig resamplingConfig_;
  DecoderStatistics statistics_;
  UniString lastError_;
  AudioInfo audioInfo_;

  Impl() = default;

  ~Impl() {
   cleanup();
  }

  void cleanup() {
   if (swrCtx_) {
    swr_free(&swrCtx_);
    swrCtx_ = nullptr;
   }
   if (codecContext_) {
    avcodec_free_context(&codecContext_);
    codecContext_ = nullptr;
   }
   initialized_ = false;
  }

  bool setupResampler() {
   if (swrCtx_) {
    swr_free(&swrCtx_);
   }

   swrCtx_ = swr_alloc();
   if (!swrCtx_) {
    lastError_ = UniString(std::string("Failed to allocate SwrContext"));
    return false;
   }

   av_opt_set_chlayout(swrCtx_, "in_chlayout", &codecContext_->ch_layout, 0);
   av_opt_set_int(swrCtx_, "in_sample_rate", codecContext_->sample_rate, 0);
   av_opt_set_sample_fmt(swrCtx_, "in_sample_fmt", codecContext_->sample_fmt, 0);

   AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
   int outSampleRate = 44100;
   AVSampleFormat outFormat = AV_SAMPLE_FMT_S16;

   if (resamplingEnabled_ && resamplingConfig_.targetSampleRate > 0) {
    outSampleRate = resamplingConfig_.targetSampleRate;
   }
   if (resamplingEnabled_ && resamplingConfig_.targetChannels > 0) {
    av_channel_layout_default(&outLayout, resamplingConfig_.targetChannels);
   }
   if (resamplingEnabled_ && resamplingConfig_.targetFormat != AudioSampleFormat::Unknown) {
    outFormat = toAVSampleFormat(resamplingConfig_.targetFormat);
   }

   av_opt_set_chlayout(swrCtx_, "out_chlayout", &outLayout, 0);
   av_opt_set_int(swrCtx_, "out_sample_rate", outSampleRate, 0);
   av_opt_set_sample_fmt(swrCtx_, "out_sample_fmt", outFormat, 0);

   if (swr_init(swrCtx_) < 0) {
    swr_free(&swrCtx_);
    swrCtx_ = nullptr;
    lastError_ = UniString(std::string("Failed to initialize SwrContext"));
    return false;
   }

   return true;
  }

  void updateAudioInfo() {
   if (!codecContext_) return;

   audioInfo_.sampleRate = codecContext_->sample_rate;
   audioInfo_.channels = codecContext_->ch_layout.nb_channels;
   audioInfo_.channelLayout = codecContext_->ch_layout.u.mask;
   audioInfo_.format = fromAVSampleFormat(codecContext_->sample_fmt);
   audioInfo_.bitrate = static_cast<int>(codecContext_->bit_rate);
   audioInfo_.bitsPerSample = av_get_bytes_per_sample(codecContext_->sample_fmt) * 8;

   if (codecContext_->codec) {
    audioInfo_.codecName = UniString(std::string(codecContext_->codec->name));
   }
  }

  static AudioSampleFormat fromAVSampleFormat(AVSampleFormat avFormat) {
   switch (avFormat) {
    case AV_SAMPLE_FMT_S16: return AudioSampleFormat::Int16;
    case AV_SAMPLE_FMT_S32: return AudioSampleFormat::Int32;
    case AV_SAMPLE_FMT_FLT: return AudioSampleFormat::Float;
    case AV_SAMPLE_FMT_DBL: return AudioSampleFormat::Double;
    case AV_SAMPLE_FMT_S16P: return AudioSampleFormat::Int16Planar;
    case AV_SAMPLE_FMT_S32P: return AudioSampleFormat::Int32Planar;
    case AV_SAMPLE_FMT_FLTP: return AudioSampleFormat::FloatPlanar;
    case AV_SAMPLE_FMT_DBLP: return AudioSampleFormat::DoublePlanar;
    default: return AudioSampleFormat::Unknown;
   }
  }

  static AVSampleFormat toAVSampleFormat(AudioSampleFormat format) {
   switch (format) {
    case AudioSampleFormat::Int16: return AV_SAMPLE_FMT_S16;
    case AudioSampleFormat::Int32: return AV_SAMPLE_FMT_S32;
    case AudioSampleFormat::Float: return AV_SAMPLE_FMT_FLT;
    case AudioSampleFormat::Double: return AV_SAMPLE_FMT_DBL;
    case AudioSampleFormat::Int16Planar: return AV_SAMPLE_FMT_S16P;
    case AudioSampleFormat::Int32Planar: return AV_SAMPLE_FMT_S32P;
    case AudioSampleFormat::FloatPlanar: return AV_SAMPLE_FMT_FLTP;
    case AudioSampleFormat::DoublePlanar: return AV_SAMPLE_FMT_DBLP;
    default: return AV_SAMPLE_FMT_NONE;
   }
  }
 };

 MediaAudioDecoder::MediaAudioDecoder() : impl_(new Impl()) {}

 MediaAudioDecoder::~MediaAudioDecoder() {
  delete impl_;
 }

 MediaAudioDecoder::MediaAudioDecoder(MediaAudioDecoder&& other) noexcept
  : impl_(other.impl_) {
  other.impl_ = nullptr;
 }

 MediaAudioDecoder& MediaAudioDecoder::operator=(MediaAudioDecoder&& other) noexcept {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 bool MediaAudioDecoder::initialize(AVCodecParameters* codecParams) {
  if (!impl_) return false;

  impl_->cleanup();

  const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
  if (!codec) {
   impl_->lastError_ = UniString(std::string("Decoder not found"));
   return false;
  }

  impl_->codecContext_ = avcodec_alloc_context3(codec);
  if (!impl_->codecContext_) {
   impl_->lastError_ = UniString(std::string("Failed to allocate codec context"));
   return false;
  }

  if (avcodec_parameters_to_context(impl_->codecContext_, codecParams) < 0) {
   avcodec_free_context(&impl_->codecContext_);
   impl_->lastError_ = UniString(std::string("Failed to copy codec parameters"));
   return false;
  }

  if (avcodec_open2(impl_->codecContext_, codec, nullptr) < 0) {
   avcodec_free_context(&impl_->codecContext_);
   impl_->lastError_ = UniString(std::string("Failed to open codec"));
   return false;
  }

  if (!impl_->setupResampler()) {
   avcodec_free_context(&impl_->codecContext_);
   return false;
  }

  impl_->updateAudioInfo();
  impl_->initialized_ = true;
  return true;
 }

 bool MediaAudioDecoder::initialize(AVCodecContext* codecContext) {
  if (!impl_ || !codecContext) return false;

  impl_->cleanup();
  impl_->codecContext_ = codecContext;
  
  if (!impl_->setupResampler()) {
   impl_->codecContext_ = nullptr;
   return false;
  }

  impl_->updateAudioInfo();
  impl_->initialized_ = true;
  return true;
 }

 bool MediaAudioDecoder::initializeByCodecName(const UniString& codecName) {
  if (!impl_) return false;

  impl_->cleanup();

  QString name = codecName.toQString();
  const AVCodec* codec = avcodec_find_decoder_by_name(name.toUtf8().constData());
  if (!codec) {
   impl_->lastError_ = UniString(std::string("Decoder not found: ") + name.toStdString());
   return false;
  }

  impl_->codecContext_ = avcodec_alloc_context3(codec);
  if (!impl_->codecContext_) {
   impl_->lastError_ = UniString(std::string("Failed to allocate codec context"));
   return false;
  }

  if (avcodec_open2(impl_->codecContext_, codec, nullptr) < 0) {
   avcodec_free_context(&impl_->codecContext_);
   impl_->lastError_ = UniString(std::string("Failed to open codec"));
   return false;
  }

  impl_->updateAudioInfo();
  impl_->initialized_ = true;
  return true;
 }

 void MediaAudioDecoder::reset() {
  if (impl_) {
   impl_->cleanup();
   impl_->statistics_ = DecoderStatistics();
  }
 }

 bool MediaAudioDecoder::isInitialized() const {
  return impl_ && impl_->initialized_;
 }

 QByteArray MediaAudioDecoder::decodeFrame(AVPacket* packet) {
  auto result = decodeFrameDetailed(packet);
  return result.data;
 }

 AudioDecodeResult MediaAudioDecoder::decodeFrameDetailed(AVPacket* packet) {
  AudioDecodeResult result;

  if (!impl_ || !impl_->codecContext_ || !impl_->swrCtx_) {
   result.errorMessage = UniString(std::string("Decoder not initialized"));
   return result;
  }

  auto startTime = std::chrono::high_resolution_clock::now();

  AVFrame* frame = av_frame_alloc();
  if (!frame) {
   result.errorMessage = UniString(std::string("Failed to allocate frame"));
   impl_->statistics_.errors++;
   return result;
  }

  int ret = avcodec_send_packet(impl_->codecContext_, packet);
  if (ret < 0) {
   av_frame_free(&frame);
   result.errorMessage = UniString(std::string("Failed to send packet"));
   impl_->statistics_.errors++;
   return result;
  }

  ret = avcodec_receive_frame(impl_->codecContext_, frame);
  if (ret < 0) {
   av_frame_free(&frame);
   if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    result.success = true;
   } else {
    result.errorMessage = UniString(std::string("Failed to receive frame"));
    impl_->statistics_.errors++;
   }
   return result;
  }

  int outSamples = swr_get_delay(impl_->swrCtx_, 44100) + frame->nb_samples;
  int bytesPerSample = 2 * 2; // S16 stereo
  uint8_t* outBuffer = new uint8_t[outSamples * bytesPerSample];
  uint8_t* outPtr[1] = { outBuffer };

  int converted = swr_convert(impl_->swrCtx_, outPtr, outSamples,
                              (const uint8_t**)frame->data, frame->nb_samples);

  if (converted > 0) {
   result.data = QByteArray((char*)outBuffer, converted * bytesPerSample);
   result.samplesDecoded = converted;
   result.timestamp = frame->pts;
   result.success = true;

   impl_->statistics_.framesDecoded++;
   impl_->statistics_.samplesDecoded += converted;
   impl_->statistics_.bytesDecoded += result.data.size();
  }

  delete[] outBuffer;
  av_frame_free(&frame);

  auto endTime = std::chrono::high_resolution_clock::now();
  double decodeTime = std::chrono::duration<double>(endTime - startTime).count();
  impl_->statistics_.totalDecodeTime += decodeTime;
  if (impl_->statistics_.framesDecoded > 0) {
   impl_->statistics_.averageDecodeTime =
    (impl_->statistics_.totalDecodeTime / impl_->statistics_.framesDecoded) * 1000.0;
  }

  return result;
 }

 std::vector<QByteArray> MediaAudioDecoder::decodeFrames(const std::vector<AVPacket*>& packets) {
  std::vector<QByteArray> results;
  results.reserve(packets.size());
  for (auto* packet : packets) {
   results.push_back(decodeFrame(packet));
  }
  return results;
 }

 void MediaAudioDecoder::flush() {
  if (impl_ && impl_->codecContext_) {
   avcodec_flush_buffers(impl_->codecContext_);
  }
 }

 QByteArray MediaAudioDecoder::flushAndGetRemaining() {
  if (!impl_ || !impl_->swrCtx_) return QByteArray();

  int remaining = swr_get_delay(impl_->swrCtx_, 44100);
  if (remaining <= 0) return QByteArray();

  int bytesPerSample = 2 * 2;
  uint8_t* outBuffer = new uint8_t[remaining * bytesPerSample];
  uint8_t* outPtr[1] = { outBuffer };

  int converted = swr_convert(impl_->swrCtx_, outPtr, remaining, nullptr, 0);
  QByteArray data;
  if (converted > 0) {
   data = QByteArray((char*)outBuffer, converted * bytesPerSample);
  }

  delete[] outBuffer;
  return data;
 }

 bool MediaAudioDecoder::enableResampling(const ResamplingConfig& config) {
  if (!impl_) return false;
  impl_->resamplingConfig_ = config;
  impl_->resamplingEnabled_ = true;
  if (impl_->initialized_) {
   return impl_->setupResampler();
  }
  return true;
 }

 void MediaAudioDecoder::disableResampling() {
  if (impl_) {
   impl_->resamplingEnabled_ = false;
   if (impl_->initialized_) {
    impl_->setupResampler();
   }
  }
 }

 bool MediaAudioDecoder::isResamplingEnabled() const {
  return impl_ && impl_->resamplingEnabled_;
 }

 ResamplingConfig MediaAudioDecoder::getResamplingConfig() const {
  if (!impl_) return ResamplingConfig();
  return impl_->resamplingConfig_;
 }

 AudioInfo MediaAudioDecoder::getAudioInfo() const {
  if (!impl_) return AudioInfo();
  return impl_->audioInfo_;
 }

 int MediaAudioDecoder::getSampleRate() const {
  return impl_ ? impl_->audioInfo_.sampleRate : 0;
 }

 int MediaAudioDecoder::getChannels() const {
  return impl_ ? impl_->audioInfo_.channels : 0;
 }

 int64_t MediaAudioDecoder::getChannelLayout() const {
  return impl_ ? impl_->audioInfo_.channelLayout : 0;
 }

 AudioSampleFormat MediaAudioDecoder::getSampleFormat() const {
  return impl_ ? impl_->audioInfo_.format : AudioSampleFormat::Unknown;
 }

 UniString MediaAudioDecoder::getCodecName() const {
  return impl_ ? impl_->audioInfo_.codecName : UniString();
 }

 int MediaAudioDecoder::getBitrate() const {
  return impl_ ? impl_->audioInfo_.bitrate : 0;
 }

 DecoderStatistics MediaAudioDecoder::getStatistics() const {
  if (!impl_) return DecoderStatistics();
  return impl_->statistics_;
 }

 void MediaAudioDecoder::resetStatistics() {
  if (impl_) {
   impl_->statistics_ = DecoderStatistics();
  }
 }

 int64_t MediaAudioDecoder::getDecodedFrameCount() const {
  return impl_ ? impl_->statistics_.framesDecoded : 0;
 }

 int64_t MediaAudioDecoder::getDecodedSampleCount() const {
  return impl_ ? impl_->statistics_.samplesDecoded : 0;
 }

 UniString MediaAudioDecoder::getLastError() const {
  return impl_ ? impl_->lastError_ : UniString();
 }

 bool MediaAudioDecoder::hasError() const {
  return impl_ && impl_->lastError_.length() > 0;
 }

 void MediaAudioDecoder::clearError() {
  if (impl_) {
   impl_->lastError_ = UniString();
  }
 }

 UniString MediaAudioDecoder::sampleFormatToString(AudioSampleFormat format) {
  switch (format) {
   case AudioSampleFormat::Int16: return UniString(std::string("s16"));
   case AudioSampleFormat::Int32: return UniString(std::string("s32"));
   case AudioSampleFormat::Float: return UniString(std::string("flt"));
   case AudioSampleFormat::Double: return UniString(std::string("dbl"));
   case AudioSampleFormat::Int16Planar: return UniString(std::string("s16p"));
   case AudioSampleFormat::Int32Planar: return UniString(std::string("s32p"));
   case AudioSampleFormat::FloatPlanar: return UniString(std::string("fltp"));
   case AudioSampleFormat::DoublePlanar: return UniString(std::string("dblp"));
   default: return UniString(std::string("unknown"));
  }
 }

 AudioSampleFormat MediaAudioDecoder::fromAVSampleFormat(AVSampleFormat avFormat) {
  return Impl::fromAVSampleFormat(avFormat);
 }

 AVSampleFormat MediaAudioDecoder::toAVSampleFormat(AudioSampleFormat format) {
  return Impl::toAVSampleFormat(format);
 }

 UniString MediaAudioDecoder::channelLayoutToString(int64_t layout) {
  char buf[64] = {0};
  AVChannelLayout chLayout;
  av_channel_layout_from_mask(&chLayout, layout);
  av_channel_layout_describe(&chLayout, buf, sizeof(buf));
  av_channel_layout_uninit(&chLayout);
  return UniString(std::string(buf));
 }

 int64_t MediaAudioDecoder::samplesToBytes(int64_t samples, int channels, AudioSampleFormat format) {
  int bytesPerSample = 0;
  switch (format) {
   case AudioSampleFormat::Int16:
   case AudioSampleFormat::Int16Planar:
    bytesPerSample = 2;
    break;
   case AudioSampleFormat::Int32:
   case AudioSampleFormat::Int32Planar:
   case AudioSampleFormat::Float:
   case AudioSampleFormat::FloatPlanar:
    bytesPerSample = 4;
    break;
   case AudioSampleFormat::Double:
   case AudioSampleFormat::DoublePlanar:
    bytesPerSample = 8;
    break;
   default:
    return 0;
  }
  return samples * channels * bytesPerSample;
 }

} // namespace ArtifactCore
