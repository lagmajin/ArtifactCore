module;
#include <QtMultiMedia/QAudioFormat>

export module Audio.Converter.Qt;

import Audio.Frame;

export namespace ArtifactCore {


 class AudioFrameToQAudioFormat {
 public:
  static QAudioFormat convert(const AudioFrame& frame);
 };







};