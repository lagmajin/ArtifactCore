module;

#include <QtCore/QFile>


export module Codec:EncoderSettings;


export namespace ArtifactCore {

 enum class CodecType {
  H264,
  H265,
  VP8,
  AAC,
  MP3,
  None
 };

 enum class OutputMode {
  VideoOnly,
  AudioOnly,
  VideoAndAudio
 };

 class EncoderSettings {
 private:

 public:



 };


}