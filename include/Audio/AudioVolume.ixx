module;
#include "../Define/DllExportMacro.hpp"

export module Audio.Volume;


namespace ArtifactCore
{
 class AudioVolumePrivate;

 class LIBRARY_DLL_API AudioVolume
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  AudioVolume();
  AudioVolume(const AudioVolume& volume);
  ~AudioVolume();
  float volume() const;
  void setVolume(float v);
  AudioVolume& operator=(const AudioVolume& volume);
 };

};