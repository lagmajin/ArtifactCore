module;
#include "../Define/DllExportMacro.hpp"

export module Audio.Decibels;

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



import Utils.String.UniString;

export namespace ArtifactCore
{
 class LIBRARY_DLL_API AudioDecibels final
 {
 private:
  class Impl;
  Impl* impl_;

  static constexpr float EPSILON = 1e-6f;
  static constexpr float MIN_DB = -60.0f;
  static constexpr float MAX_DB = 20.0f;

 public:
  // RXgN^/fXgN^
  AudioDecibels();
  explicit AudioDecibels(float dB);
  AudioDecibels(const AudioDecibels& other);
  AudioDecibels(AudioDecibels&& other) noexcept;
  ~AudioDecibels();

  // Qb^[/Zb^[
  float decibels() const;
  void setDecibels(float dB);

  // AudioVolumeƂ̑ݕϊ
  float toLinearValue() const;
  static AudioDecibels fromLinearValue(float linear);

  // ԁitF[hpj
  AudioDecibels interpolate(const AudioDecibels& target, float t) const;
  static AudioDecibels linearFade(const AudioDecibels& from, const AudioDecibels& to, float t);

  // of[V
  bool isValid() const;
  bool isMute() const;
  bool isSilent() const;  // -40dBȉ
  bool isNormalized() const;  // 0dB
  void clamp();

  // vZbg
  enum class Preset {
    Mute,          // - dB
    VeryQuiet,     // -40 dB
    Quiet,         // -20 dB
    Normal,        // 0 dB
    Loud,          // +6 dB
    VeryLoud,      // +12 dB
    Maximum        // +20 dB
  };

  static AudioDecibels createPreset(Preset preset);
  static UniString getPresetName(Preset preset);

  // VACY
  UniString serialize() const;
  static AudioDecibels deserialize(const UniString& data);

  // riGvVlj
  bool equals(const AudioDecibels& other) const;
  bool operator==(const AudioDecibels& other) const;
  bool operator!=(const AudioDecibels& other) const;
  bool operator<(const AudioDecibels& other) const;
  bool operator<=(const AudioDecibels& other) const;
  bool operator>(const AudioDecibels& other) const;
  bool operator>=(const AudioDecibels& other) const;

  // ZqifVxvZpj
  AudioDecibels operator+(float dB) const;
  AudioDecibels operator-(float dB) const;
  AudioDecibels operator*(float scalar) const;
  AudioDecibels operator/(float scalar) const;

  AudioDecibels& operator+=(float dB);
  AudioDecibels& operator-=(float dB);
  AudioDecibels& operator*=(float scalar);
  AudioDecibels& operator/=(float scalar);

  // Zq
  AudioDecibels& operator=(const AudioDecibels& other);
  AudioDecibels& operator=(AudioDecibels&& other) noexcept;
 };

};
