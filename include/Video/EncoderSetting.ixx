module;
#include <QVector>
#include <QString>
export module Codec.Encoder.Setting;

import std;
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

  void setQuality(int quality);  // 0-100 スライダー向け
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

  // --- プリセット機能 ---
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

  // --- ビットレート自動計算 ---
  int calculateBitrate() const;  // 解像度とfpsから推奨値を計算
  int calculateAudioBitrate() const;

  // --- 検証 ---
  bool isValid() const;
  QVector<UniString> getValidationErrors() const;

  // --- シリアライズ ---
  UniString serialize() const;
  bool deserialize(const UniString& data);

  // --- UI統合 ---
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