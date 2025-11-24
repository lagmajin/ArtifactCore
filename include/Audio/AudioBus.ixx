module;
export module Audio.Bus;

import std;
import Utils.Id;
import Utils.String.Like;

export namespace ArtifactCore {


 class AudioBus {
 private:
  class Impl;
  Impl* impl_;
 public:
  AudioBus();
  virtual ~AudioBus();

 };



}