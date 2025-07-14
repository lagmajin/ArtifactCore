module;

export module Codec.GStreamerDecoder;

import std;

namespace ArtifactCore {

 class GStreamerDecoderPrivate;

 class __declspec(dllexport) GStreamerDecoder {
 private:
  
 public:
  static void InitGstreamer();
  GStreamerDecoder();
  ~GStreamerDecoder();
  void Play();

 };








}

