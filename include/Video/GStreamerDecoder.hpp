#pragma once

import std.core;

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

