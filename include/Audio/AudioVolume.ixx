module;
#include "../Define/DllExportMacro.hpp"

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
export module Audio.Volume;




import Utils.String.UniString;
import Utils.String.Like;
import Audio.Decibels;

export namespace ArtifactCore
{
 


class LIBRARY_DLL_API AudioVolume final
 {
 private:
  class Impl;
  Impl* impl_;
  static constexpr float EPSILON = 1e-6f;

 public:
  AudioVolume();
  explicit AudioVolume(float volume);
  AudioVolume(const AudioVolume& volume);
  AudioVolume(AudioVolume&& other) noexcept;
  ~AudioVolume();

  float volume() const;
  void setVolume(float v);

  float toDecibels() const;
  static AudioVolume fromDecibels(float dB);
  AudioDecibels toDecibelsObject() const;
  static AudioVolume fromDecibelsObject(const AudioDecibels& dB);

  AudioVolume interpolate(const AudioVolume& target, float t) const;
  static AudioVolume linearFade(const AudioVolume& from, const AudioVolume& to, float t);

  bool isValid() const;
  bool isMuted() const;
  bool isNormalized() const;
  void clamp();

  enum class Preset { Mute, VeryQuiet, Quiet, Normal, Loud, Maximum };
  static AudioVolume createPreset(Preset preset);
  static UniString getPresetName(Preset preset);

  UniString serialize() const;
  static AudioVolume deserialize(const UniString& data);

  bool equals(const AudioVolume& other) const;
  bool operator==(const AudioVolume& other) const;
  bool operator!=(const AudioVolume& other) const;
  bool operator<(const AudioVolume& other) const;
  bool operator<=(const AudioVolume& other) const;
  bool operator>(const AudioVolume& other) const;
  bool operator>=(const AudioVolume& other) const;

  AudioVolume operator+(const AudioVolume& other) const;
  AudioVolume operator-(const AudioVolume& other) const;
  AudioVolume operator*(float scalar) const;
  AudioVolume operator/(float scalar) const;

  AudioVolume& operator+=(const AudioVolume& other);
  AudioVolume& operator-=(const AudioVolume& other);
  AudioVolume& operator*=(float scalar);
  AudioVolume& operator/=(float scalar);

  AudioVolume& operator=(const AudioVolume& volume);
  AudioVolume& operator=(AudioVolume&& other) noexcept;
 };



};