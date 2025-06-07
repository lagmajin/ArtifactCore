module;

export module Halide;

import std;

namespace ArtifactCore {

 struct BlurParams { int samples; };
 struct GlowParams { float intensity; };

 struct EffectRequest {
  std::string name;   // "blur", "glow"�Ȃ�
  std::any params;    // BlurParams�Ȃǂ��i�[
 };

}