module;
#include <wobjectdefs.h>

export module AudioRenderer;

export namespace ArtifactCore
{

 class AudioRenderer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  AudioRenderer();
  ~AudioRenderer();
 };











}