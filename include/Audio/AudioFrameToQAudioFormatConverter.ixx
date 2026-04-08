module;
#include <utility>

export module Audio.Converter.Qt;

#include <QtMultiMedia/QAudioFormat>

import Audio.Frame;

export namespace ArtifactCore {


 class AudioFrameToQAudioFormat {
 public:
  static QAudioFormat convert(const AudioFrame& frame);
 };







};
