module;
#include "../Define/DllExportMacro.hpp"
#include <QString>

export module Audio.Panner;
import std;

import Audio.Segment;

export namespace ArtifactCore {
 
 export enum class PanningMode {
  StereoBalance, // シンプルなバランス (Linear)
  EqualPower,    // 定電力パンニング (Center での音量低下を防ぐ)
  VBAP,          // Vector Base Amplitude Panning (多チャンネル)
  Ambisonics,    // 全天球
  Binaural       // バイノーラル (HRTF)
 };
 
 export struct PanningGain {
  std::vector<float> channelGains; // 0.0 ~ 1.0 のリスト
 };

 class LIBRARY_DLL_API AudioPanner {
 private:
  class Impl;
  Impl* impl_;
 public:
  static PanningGain calculateConstantPowerGains(float pan); // pan: -1.0 to 1.0
  static QString modeName(PanningMode mode);
  AudioPanner();
  ~AudioPanner();

  void setMode(PanningMode mode);
  PanningMode getMode() const;

  PanningGain calculateGain(float azimuth, float elevation = 0.0f) const;
  void applyPanning(AudioSegment& segment, const PanningGain& gain);
 };
}
