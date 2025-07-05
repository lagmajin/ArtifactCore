
module;
#include <cstdint>
export module Layer.LayerStrip;

import std;

export namespace ArtifactCore {

 class LayerStrip {
 private:
  class Impl;
  Impl* impl_;
 public:
  LayerStrip();
  ~LayerStrip();
  void SetStartFrame(int32_t frame);
  void SetEndFrame(int32_t frame);
 };











};
