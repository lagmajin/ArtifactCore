module;
#include "../Define/DllExportMacro.hpp"

export module Audio.Volume;

import std;
import Utils.String.Like;

namespace ArtifactCore
{
 


 class LIBRARY_DLL_API AudioVolume final
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  AudioVolume();
  explicit AudioVolume(float volume);
  AudioVolume(const AudioVolume& volume);
  AudioVolume(AudioVolume&& other) noexcept;
  ~AudioVolume();
  float volume() const;
  void setVolume(float v);
  AudioVolume& operator=(const AudioVolume& volume);
  AudioVolume& operator=(AudioVolume&& other) noexcept;
 };



};