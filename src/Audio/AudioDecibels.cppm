module;

#include <QString>
#include <cmath>
#include <format>

module Audio.Decibels;

import std;
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

 // --- コンストラクタ/デストラクタ ---
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

 // --- ゲッター/セッター ---
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

 // --- 補間 ---
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

 // --- バリデーション ---
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

 // --- プリセット ---
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
     return UniString(QString("Mute (-∞ dB)"));
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

 // --- シリアライズ ---
 UniString AudioDecibels::serialize() const
 {
   std::string json = std::format(R"({{"dB":{}}})", impl_->dB);
   return UniString(QString::fromStdString(json));
 }

 AudioDecibels AudioDecibels::deserialize(const UniString& data)
 {
   // 簡易JSON パーサー
   float dB = 0.0f;
   // 正規表現などでパースする
   return AudioDecibels(dB);
 }

 // --- 比較（エプシロン考慮） ---
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

 // --- 演算子（デシベル計算用） ---
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

 // --- 代入演算子 ---
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
