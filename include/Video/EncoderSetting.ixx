module;

#include <QtCore/QFile>


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

 class EncoderSettings {
 private:
  class Impl;
  Impl* impl_;

  OutputMode mode = OutputMode::VideoAndAudio;
  VideoCodec vCodec = VideoCodec::H264;
  AudioCodec aCodec = AudioCodec::AAC;
  VideoProfile vProfile = VideoProfile::H264_High;
 public:
  EncoderSettings() = default;

  // --- Setter / Getter ---
  void setOutputMode(OutputMode m);
  OutputMode getOutputMode() const { return mode; }

  void setVideoCodec(VideoCodec codec);
  VideoCodec getVideoCodec() const { return vCodec; }

  void setAudioCodec(AudioCodec codec) { aCodec = codec; }
  AudioCodec getAudioCodec() const { return aCodec; }

  void setVideoProfile(VideoProfile profile) { vProfile = profile; }
  VideoProfile getVideoProfile() const { return vProfile; }
  bool isValid() const {
   if (mode == OutputMode::AudioOnly) return aCodec != AudioCodec::None;

   // ProRes選択時にH264のプロファイルが設定されていないか等のチェック
   if (vCodec == VideoCodec::AppleProRes) {
	return (vProfile >= VideoProfile::ProRes_Proxy && vProfile <= VideoProfile::ProRes_4444XQ);
   }
   if (vCodec == VideoCodec::H264) {
	return (vProfile == VideoProfile::H264_High || vProfile == VideoProfile::H264_Main);
   }
   return true;
  }

 };


}