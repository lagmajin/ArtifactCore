module;
class tst_QList;
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QVector>
#include <QString>
export module Codec.Encoder.Setting;

import Utils.String.UniString;


export namespace ArtifactCore {

 enum class CodecType {
  H264,
  H265,
  VP8,
  AAC,
  MP3,
  AppleProRes,
  None
 };
 
 enum class VideoCodec { H264, H265, VP8, AppleProRes, None };
 enum class AudioCodec { AAC, MP3, OPUS, None };
 
 enum class VideoProfile {
  None,
  ProRes_Proxy,
  ProRes_LT,
  ProRes_422,
  ProRes_422HQ,
  ProRes_4444,
  ProRes_4444XQ,
  H264_High,
  H264_Main
 };

 enum class OutputMode {
  VideoOnly,
  AudioOnly,
  VideoAndAudio
 };

 enum class OutputFormat {
  VideoFile,
  ImageSequence
 };

 enum class ImageFormat {
  PNG,
  JPEG,
  TIFF,
  EXR,
  BMP
 };

 class EncoderSettings {
 private:
  class Impl;
  Impl* impl_;

 public:
  EncoderSettings();
  ~EncoderSettings();
  EncoderSettings(const EncoderSettings& other);
  EncoderSettings& operator=(const EncoderSettings& other);

  // --- Output Mode ---
  void setOutputMode(OutputMode m);
  OutputMode getOutputMode() const;

  void setOutputFormat(OutputFormat format);
  OutputFormat getOutputFormat() const;

  // --- Video Settings ---
  void setVideoCodec(VideoCodec codec);
  VideoCodec getVideoCodec() const;

  void setVideoProfile(VideoProfile profile);
  VideoProfile getVideoProfile() const;

  void setWidth(int width);
  int getWidth() const;

  void setHeight(int height);
  int getHeight() const;

  void setFrameRate(double fps);
  double getFrameRate() const;

  void setBitrate(int bitrateMbps);
  int getBitrate() const;

  void setQuality(int quality);  // 0-100 XC_[
  int getQuality() const;

  // --- Audio Settings ---
  void setAudioCodec(AudioCodec codec);
  AudioCodec getAudioCodec() const;

  void setAudioBitrate(int bitrateMbps);
  int getAudioBitrate() const;

  void setAudioSampleRate(int sampleRate);
  int getAudioSampleRate() const;

  // --- Image Sequence Settings ---
  void setImageFormat(ImageFormat format);
  ImageFormat getImageFormat() const;

  void setImageSequencePrefix(const QString& prefix);
  QString getImageSequencePrefix() const;

  void setImageSequencePadding(int digits);  // 4, 5, 6
  int getImageSequencePadding() const;

  QString generateImageSequencePath(int frameNumber) const;
  QString getImageFormatExtension() const;
  static UniString getImageFormatName(ImageFormat format);
  
  QVector<ImageFormat> getAvailableImageFormats() const;

  // --- vZbg@\ ---
  enum class Preset {
    YouTube_1080p_60fps,
    YouTube_4K_60fps,
    ProRes_422_FullHD,
    ProRes_4444_UltraHD,
    WebH264_High,
    WebH265_High
  };

  void applyPreset(Preset preset);
  static UniString getPresetName(Preset preset);

  // --- rbg[gvZ ---
  int calculateBitrate() const;  // 𑜓xfps琄lvZ
  int calculateAudioBitrate() const;

  // ---  ---
  bool isValid() const;
  QVector<UniString> getValidationErrors() const;

  // --- VACY ---
  UniString serialize() const;
  bool deserialize(const UniString& data);

  // --- UI ---
  static UniString getCodecName(VideoCodec codec);
  static UniString getProfileName(VideoProfile profile);
  
  QVector<VideoCodec> getAvailableVideoCodecs() const;
  QVector<VideoProfile> getAvailableProfiles() const;
  QVector<int> getStandardFrameRates() const;
  
  bool isProfileValidForCodec(VideoCodec codec, VideoProfile profile) const;
  
  int getRecommendedBitrate() const;
  int getRecommendedAudioBitrate() const;
 };


}
