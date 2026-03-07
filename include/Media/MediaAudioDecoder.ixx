module;
#define QT_NO_KEYWORDS
#include <QByteArray>
#include <QString>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
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
export module MediaAudioDecoder;




import Utils.String.UniString;

export namespace ArtifactCore {

 // I[fBITvtH[}bg
 enum class AudioSampleFormat {
  Unknown,
  Int16,          // 16-bit signed integer
  Int32,          // 32-bit signed integer
  Float,          // 32-bit float
  Double,         // 64-bit double
  Int16Planar,    // 16-bit signed integer (planar)
  Int32Planar,    // 32-bit signed integer (planar)
  FloatPlanar,    // 32-bit float (planar)
  DoublePlanar    // 64-bit double (planar)
 };

 // I[fBI
 struct AudioInfo {
  int sampleRate = 0;              // Tv[giHzj
  int channels = 0;                // `l
  int64_t channelLayout = 0;       // `lCAEg
  AudioSampleFormat format = AudioSampleFormat::Unknown;
  UniString codecName;             // R[fbN
  int bitrate = 0;                 // rbg[gibpsj
  int bitsPerSample = 0;           // Tṽrbg
  int64_t totalSamples = 0;        // Tv
  double duration = 0.0;           // f[Vibj
 };

 // fR[h
 struct AudioDecodeResult {
  bool success = false;
  QByteArray data;                 // fR[hꂽPCMf[^
  int samplesDecoded = 0;          // fR[hꂽTv
  int64_t timestamp = 0;           // ^CX^vi}CNbj
  UniString errorMessage;
 };

 // TvOݒ
 struct ResamplingConfig {
  int targetSampleRate = 0;        // ڕWTv[gi0=ϊȂj
  int targetChannels = 0;          // ڕW`li0=ϊȂj
  AudioSampleFormat targetFormat = AudioSampleFormat::Unknown;
  int64_t targetChannelLayout = 0;
 };

 // fR[_[v
 struct DecoderStatistics {
  int64_t framesDecoded = 0;
  int64_t samplesDecoded = 0;
  int64_t bytesDecoded = 0;
  double totalDecodeTime = 0.0;    // b
  double averageDecodeTime = 0.0;  // t[i~bj
  int errors = 0;
 };

 class MediaAudioDecoder {
 private:
  class Impl;
  Impl* impl_;

 public:
  MediaAudioDecoder();
  ~MediaAudioDecoder();

  // Rs[/[u
  MediaAudioDecoder(const MediaAudioDecoder&) = delete;
  MediaAudioDecoder& operator=(const MediaAudioDecoder&) = delete;
  MediaAudioDecoder(MediaAudioDecoder&&) noexcept;
  MediaAudioDecoder& operator=(MediaAudioDecoder&&) noexcept;

  // ----  ----

  // R[fbNp[^珉
  bool initialize(AVCodecParameters* codecParams);
  
  // R[fbNReLXg珉
  bool initialize(AVCodecContext* codecContext);
  
  // R[fbN珉
  bool initializeByCodecName(const UniString& codecName);
  
  // Zbgiďj
  void reset();
  
  // ς݂
  bool isInitialized() const;

  // ---- fR[h ----

  // pPbgfR[h
  QByteArray decodeFrame(AVPacket* packet);
  
  // pPbgfR[hiڍׂȌʁj
  AudioDecodeResult decodeFrameDetailed(AVPacket* packet);
  
  // pPbgfR[h
  std::vector<QByteArray> decodeFrames(const std::vector<AVPacket*>& packets);
  
  // obt@tbV
  void flush();
  
  // obt@ɎcĂt[擾
  QByteArray flushAndGetRemaining();

  // ---- TvO ----

  // TvOL
  bool enableResampling(const ResamplingConfig& config);
  
  // TvO𖳌
  void disableResampling();
  
  // TvOL
  bool isResamplingEnabled() const;
  
  // ݂̃TvOݒ
  ResamplingConfig getResamplingConfig() const;

  // ---- I[fBI ----

  // I[fBI擾
  AudioInfo getAudioInfo() const;
  
  // Tv[g
  int getSampleRate() const;
  
  // `l
  int getChannels() const;
  
  // `lCAEg
  int64_t getChannelLayout() const;
  
  // TvtH[}bg
  AudioSampleFormat getSampleFormat() const;
  
  // R[fbN
  UniString getCodecName() const;
  
  // rbg[g
  int getBitrate() const;

  // ---- v ----

  // v擾
  DecoderStatistics getStatistics() const;
  
  // vZbg
  void resetStatistics();
  
  // fR[hς݃t[
  int64_t getDecodedFrameCount() const;
  
  // fR[hς݃Tv
  int64_t getDecodedSampleCount() const;

  // ---- G[nhO ----

  // Ō̃G[bZ[W
  UniString getLastError() const;
  
  // G[邩
  bool hasError() const;
  
  // G[NA
  void clearError();

  // ---- [eBeB ----

  // TvtH[}bg𕶎ɕϊ
  static UniString sampleFormatToString(AudioSampleFormat format);
  
  // AVSampleFormatAudioSampleFormatɕϊ
  static AudioSampleFormat fromAVSampleFormat(AVSampleFormat avFormat);
  
  // AudioSampleFormatAVSampleFormatɕϊ
  static AVSampleFormat toAVSampleFormat(AudioSampleFormat format);
  
  // `lCAEg𕶎ɕϊ
  static UniString channelLayoutToString(int64_t layout);
  
  // TvoCgvZ
  static int64_t samplesToBytes(int64_t samples, int channels, AudioSampleFormat format);
  
  // oCgTvvZ
  static int64_t bytesToSamples(int64_t bytes, int channels, AudioSampleFormat format);

  // ---- FFmpegڃANZXi㋉Ҍj ----

  // AVCodecContext擾
  AVCodecContext* getCodecContext() const;
  
  // SwrContext擾
  SwrContext* getSwrContext() const;
 };

} // namespace ArtifactCore