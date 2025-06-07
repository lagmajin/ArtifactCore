module;

export module Halide;

import std;

namespace ArtifactCore {

 struct BlurParams { int samples; };
 struct GlowParams { float intensity; };

 struct EffectRequest {
  std::string name;   // "blur", "glow"‚È‚Ç
  std::any params;    // BlurParams‚È‚Ç‚ðŠi”[
 };

}