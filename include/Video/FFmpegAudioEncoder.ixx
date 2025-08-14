module;


export module Media.Encoder.FFMpegAudioEncoder;



export namespace ArtifactCore
{
 enum class AudioFormat
 {
  WAV,
  MP3,
  AAC,
  FLAC,
  // 必要なフォーマットをここに列挙
 };


 const char* ToFfmpegFormatName(AudioFormat format)
 {
  switch (format)
  {
  case AudioFormat::WAV:  return "wav";
  case AudioFormat::MP3:  return "mp3";
  case AudioFormat::AAC:  return "aac";
  case AudioFormat::FLAC: return "flac";
  default:               return nullptr;
  }
 }




 class FFMpegAudioEncoder
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  FFMpegAudioEncoder();
  ~FFMpegAudioEncoder();
 };










};