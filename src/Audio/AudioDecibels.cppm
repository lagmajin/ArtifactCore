module;

#include <QString>
#include <cmath>
#include <format>

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
module Audio.Decibels;




import Utils.String.UniString;

namespace ArtifactCore
{
class AudioDecibels::Impl
 {
 public:
  float dB = 0.0f;

  explicit Impl(float decibels = 0.0f)
      : dB(std::clamp(decibels, AudioDecibels::MIN_DB, AudioDecibels::MAX_DB))
  {
  }

  ~Impl() = default;
 };

 // --- RXgN^/fXgN^ ---
 AudioDecibels::AudioDecibels() 
   : impl_(new Impl())
 {
 }

 AudioDecibels::AudioDecibels(float dB) 
   : impl_(new Impl(dB))
 {
 }

 AudioDecibels::AudioDecibels(const AudioDecibels& other) 
   : impl_(new Impl(*other.impl_))
 {
 }

 AudioDecibels::AudioDecibels(AudioDecibels&& other) noexcept 
   : impl_(other.impl_)
 {
   other.impl_ = nullptr;
 }

 AudioDecibels::~AudioDecibels()
 {
   delete impl_;
 }

 // --- Qb^[/Zb^[ ---
 float AudioDecibels::decibels() const
 {
   return impl_->dB;
 }

 void AudioDecibels::setDecibels(float dB)
 {
   impl_->dB = std::clamp(dB, MIN_DB, MAX_DB);
 }

float AudioDecibels::toLinearValue() const
{
  if (impl_->dB <= MIN_DB) {
    return 0.0f;
  }
  return std::pow(10.0f, impl_->dB / 20.0f);
}

AudioDecibels AudioDecibels::fromLinearValue(float linear)
{
  if (linear <= 0.0f) {
    return AudioDecibels(MIN_DB);
  }
  float dB = 20.0f * std::log10(linear);
  return AudioDecibels(dB);
}

 // ---  ---
 AudioDecibels AudioDecibels::interpolate(const AudioDecibels& target, float t) const
 {
   t = std::clamp(t, 0.0f, 1.0f);
   float interpolated = impl_->dB + (target.impl_->dB - impl_->dB) * t;
   return AudioDecibels(interpolated);
 }

 AudioDecibels AudioDecibels::linearFade(const AudioDecibels& from, const AudioDecibels& to, float t)
 {
   return from.interpolate(to, t);
 }

 // --- of[V ---
 bool AudioDecibels::isValid() const
 {
   return impl_->dB >= MIN_DB && impl_->dB <= MAX_DB;
 }

 bool AudioDecibels::isMute() const
 {
   return impl_->dB <= MIN_DB + EPSILON;
 }

 bool AudioDecibels::isSilent() const
 {
   return impl_->dB <= -40.0f;
 }

 bool AudioDecibels::isNormalized() const
 {
   return std::abs(impl_->dB) < EPSILON;
 }

 void AudioDecibels::clamp()
 {
   impl_->dB = std::clamp(impl_->dB, MIN_DB, MAX_DB);
 }

 // --- vZbg ---
 AudioDecibels AudioDecibels::createPreset(Preset preset)
 {
   switch (preset) {
   case Preset::Mute:
     return AudioDecibels(MIN_DB);
   case Preset::VeryQuiet:
     return AudioDecibels(-40.0f);
   case Preset::Quiet:
     return AudioDecibels(-20.0f);
   case Preset::Normal:
     return AudioDecibels(0.0f);
   case Preset::Loud:
     return AudioDecibels(6.0f);
   case Preset::VeryLoud:
     return AudioDecibels(12.0f);
   case Preset::Maximum:
     return AudioDecibels(MAX_DB);
   default:
     return AudioDecibels(0.0f);
   }
 }

 UniString AudioDecibels::getPresetName(Preset preset)
 {
   switch (preset) {
   case Preset::Mute:
     return UniString(QString("Mute (- dB)"));
   case Preset::VeryQuiet:
     return UniString(QString("Very Quiet (-40 dB)"));
   case Preset::Quiet:
     return UniString(QString("Quiet (-20 dB)"));
   case Preset::Normal:
     return UniString(QString("Normal (0 dB)"));
   case Preset::Loud:
     return UniString(QString("Loud (+6 dB)"));
   case Preset::VeryLoud:
     return UniString(QString("Very Loud (+12 dB)"));
   case Preset::Maximum:
     return UniString(QString("Maximum (+20 dB)"));
   default:
     return UniString(QString("Unknown"));
   }
 }

 // --- VACY ---
 UniString AudioDecibels::serialize() const
 {
   std::string json = std::format(R"({{"dB":{}}})", impl_->dB);
   return UniString(QString::fromStdString(json));
 }

 AudioDecibels AudioDecibels::deserialize(const UniString& data)
 {
   // ȈJSON p[T[
   float dB = 0.0f;
   // K\ȂǂŃp[X
   return AudioDecibels(dB);
 }

 // --- riGvVlj ---
 bool AudioDecibels::equals(const AudioDecibels& other) const
 {
   return std::abs(impl_->dB - other.impl_->dB) < EPSILON;
 }

 bool AudioDecibels::operator==(const AudioDecibels& other) const
 {
   return equals(other);
 }

 bool AudioDecibels::operator!=(const AudioDecibels& other) const
 {
   return !equals(other);
 }

 bool AudioDecibels::operator<(const AudioDecibels& other) const
 {
   return impl_->dB < other.impl_->dB - EPSILON;
 }

 bool AudioDecibels::operator<=(const AudioDecibels& other) const
 {
   return impl_->dB <= other.impl_->dB + EPSILON;
 }

 bool AudioDecibels::operator>(const AudioDecibels& other) const
 {
   return impl_->dB > other.impl_->dB + EPSILON;
 }

 bool AudioDecibels::operator>=(const AudioDecibels& other) const
 {
   return impl_->dB >= other.impl_->dB - EPSILON;
 }

 // --- ZqifVxvZpj ---
 AudioDecibels AudioDecibels::operator+(float dB) const
 {
   return AudioDecibels(impl_->dB + dB);
 }

 AudioDecibels AudioDecibels::operator-(float dB) const
 {
   return AudioDecibels(impl_->dB - dB);
 }

 AudioDecibels AudioDecibels::operator*(float scalar) const
 {
   return AudioDecibels(impl_->dB * scalar);
 }

 AudioDecibels AudioDecibels::operator/(float scalar) const
 {
   if (std::abs(scalar) < EPSILON) {
     return AudioDecibels(0.0f);
   }
   return AudioDecibels(impl_->dB / scalar);
 }

 AudioDecibels& AudioDecibels::operator+=(float dB)
 {
   impl_->dB = std::clamp(impl_->dB + dB, MIN_DB, MAX_DB);
   return *this;
 }

 AudioDecibels& AudioDecibels::operator-=(float dB)
 {
   impl_->dB = std::clamp(impl_->dB - dB, MIN_DB, MAX_DB);
   return *this;
 }

 AudioDecibels& AudioDecibels::operator*=(float scalar)
 {
   impl_->dB = std::clamp(impl_->dB * scalar, MIN_DB, MAX_DB);
   return *this;
 }

 AudioDecibels& AudioDecibels::operator/=(float scalar)
 {
   if (std::abs(scalar) > EPSILON) {
     impl_->dB = std::clamp(impl_->dB / scalar, MIN_DB, MAX_DB);
   }
   return *this;
 }

 // --- Zq ---
 AudioDecibels& AudioDecibels::operator=(const AudioDecibels& other)
 {
   if (this != &other) {
     *impl_ = *other.impl_;
   }
   return *this;
 }

 AudioDecibels& AudioDecibels::operator=(AudioDecibels&& other) noexcept
 {
   if (this != &other) {
     delete impl_;
     impl_ = other.impl_;
     other.impl_ = nullptr;
   }
   return *this;
 }

};
