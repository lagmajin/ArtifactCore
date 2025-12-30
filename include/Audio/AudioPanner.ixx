module;
#include "../Define/DllExportMacro.hpp"
export module Audio.Panner;

import std;

export namespace ArtifactCore {
 
 export enum class PanningMode {
  StereoBalance, // シンプルな左右バランス
  VBAP,          // Vector Base Amplitude Panning (多チャンネル用)
  Ambisonics,    // 全天球音響用
  Binaural       // ヘッドフォン用立体音響
 };
 
 export struct PanningGain {
  std::vector<float> channelGains; // 0.0 ~ 1.0 のリスト
 };

 class AudioPanner {
 private:
  class Impl;
  Impl* impl_;
 public:
  AudioPanner();
  ~AudioPanner();

  void setMode(PanningMode mode);
 };

 
};