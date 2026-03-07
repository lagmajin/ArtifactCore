module;
#include "../Define/DllExportMacro.hpp"
export module Audio.Panner;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>



import Audio.Segment;

export namespace ArtifactCore {
 
 export enum class PanningMode {
  StereoBalance, // VvȍEoX
  VBAP,          // Vector Base Amplitude Panning (`lp)
  Ambisonics,    // SVp
  Binaural       // wbhtHp̉
 };
 
 export struct PanningGain {
  std::vector<float> channelGains; // 0.0 ~ 1.0 ̃Xg
 };

 class AudioPanner {
 private:
  class Impl;
  Impl* impl_;
 public:
  AudioPanner();
  ~AudioPanner();

  void setMode(PanningMode mode);
  PanningMode getMode() const;

  PanningGain calculateGain(float azimuth, float elevation = 0.0f) const;
  void applyPanning(AudioSegment& segment, const PanningGain& gain);
 };
}