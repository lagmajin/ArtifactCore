module;

#include <cmath>
#include <QString>

module Audio.Volume;

import std;
import Utils.String.UniString;
import Audio.Decibels;

namespace ArtifactCore
{
 class AudioVolume::Impl
 {
 public:
  float volume = 1.0f;
  bool muted = false;

  explicit Impl(float vol = 1.0f) : volume(std::clamp(vol, 0.0f, 1.0f)), muted(false) {}
  ~Impl() = default;
 };

 // --- コンストラクタ/デストラクタ ---
 AudioVolume::AudioVolume() 
   : impl_(new Impl())
 {
 }

 AudioVolume::AudioVolume(float volume) 
   : impl_(new Impl(volume))
 {
 }

 AudioVolume::AudioVolume(const AudioVolume& other) 
   : impl_(new Impl(*other.impl_))
 {
 }

 AudioVolume::AudioVolume(AudioVolume&& other) noexcept 
   : impl_(other.impl_)
 {
   other.impl_ = nullptr;
 }

 AudioVolume::~AudioVolume()
 {
   delete impl_;
 }

 // --- ゲッター/セッター ---
 float AudioVolume::volume() const
 {
   return impl_->muted ? 0.0f : impl_->volume;
 }

 void AudioVolume::setVolume(float v)
 {
   impl_->volume = std::clamp(v, 0.0f, 1.0f);
 }

 // --- dB変換 ---
 float AudioVolume::toDecibels() const
 {
   float vol = volume();
   if (vol <= 0.0f) {
     return -std::numeric_limits<float>::infinity();
   }
   return 20.0f * std::log10(vol);
 }

 AudioVolume AudioVolume::fromDecibels(float dB)
 {
   // dB = 20 * log10(volume)
   // volume = 10^(dB/20)
   float linearVolume = std::pow(10.0f, dB / 20.0f);
   return AudioVolume(linearVolume);
 }

AudioDecibels AudioVolume::toDecibelsObject() const
{
  return AudioDecibels::fromLinearValue(volume());
}

AudioVolume AudioVolume::fromDecibelsObject(const AudioDecibels& dB)
{
  return AudioVolume(dB.toLinearValue());
}

 // --- 補間 ---
 AudioVolume AudioVolume::interpolate(const AudioVolume& target, float t) const
 {
   t = std::clamp(t, 0.0f, 1.0f);
   float interpolated = impl_->volume + (target.impl_->volume - impl_->volume) * t;
   return AudioVolume(interpolated);
 }

 AudioVolume AudioVolume::linearFade(const AudioVolume& from, const AudioVolume& to, float t)
 {
   return from.interpolate(to, t);
 }

 // --- バリデーション ---
 bool AudioVolume::isValid() const
 {
   return impl_->volume >= 0.0f && impl_->volume <= 1.0f;
 }

 bool AudioVolume::isMuted() const
 {
   return impl_->muted || impl_->volume < EPSILON;
 }

 bool AudioVolume::isNormalized() const
 {
   return std::abs(impl_->volume - 1.0f) < EPSILON;
 }

 void AudioVolume::clamp()
 {
   impl_->volume = std::clamp(impl_->volume, 0.0f, 1.0f);
 }

 // --- プリセット ---
 AudioVolume AudioVolume::createPreset(Preset preset)
 {
   switch (preset) {
   case Preset::Mute:
     return AudioVolume(0.0f);
   case Preset::VeryQuiet:
     return AudioVolume(0.1f);
   case Preset::Quiet:
     return AudioVolume(0.3f);
   case Preset::Normal:
     return AudioVolume(1.0f);
   case Preset::Loud:
     return AudioVolume(1.5f);  // 正規化範囲外（増幅）
   case Preset::Maximum:
     return AudioVolume(2.0f);  // 正規化範囲外（増幅）
   default:
     return AudioVolume(1.0f);
   }
 }

 UniString AudioVolume::getPresetName(Preset preset)
 {
   switch (preset) {
   case Preset::Mute:
     return UniString(QString("Mute"));
   case Preset::VeryQuiet:
     return UniString(QString("Very Quiet"));
   case Preset::Quiet:
     return UniString(QString("Quiet"));
   case Preset::Normal:
     return UniString(QString("Normal"));
   case Preset::Loud:
     return UniString(QString("Loud"));
   case Preset::Maximum:
     return UniString(QString("Maximum"));
   default:
     return UniString(QString("Unknown"));
   }
 }

 // --- シリアライズ ---
 UniString AudioVolume::serialize() const
 {
   std::string json = std::format(
     R"({{"volume":{}, "muted":{}}})",
     impl_->volume, impl_->muted ? "true" : "false"
   );
   return UniString(QString::fromStdString(json));
 }

 AudioVolume AudioVolume::deserialize(const UniString& data)
 {
   // 簡易JSON パーサー
   // 実装例：{"volume":0.8, "muted":false}
   float volume = 1.0f;
   // 正規表現などでパースする
   return AudioVolume(volume);
 }

 // --- 比較（エプシロン考慮） ---
 bool AudioVolume::equals(const AudioVolume& other) const
 {
   return std::abs(impl_->volume - other.impl_->volume) < EPSILON;
 }

 bool AudioVolume::operator==(const AudioVolume& other) const
 {
   return equals(other);
 }

 bool AudioVolume::operator!=(const AudioVolume& other) const
 {
   return !equals(other);
 }

 bool AudioVolume::operator<(const AudioVolume& other) const
 {
   return impl_->volume < other.impl_->volume - EPSILON;
 }

 bool AudioVolume::operator<=(const AudioVolume& other) const
 {
   return impl_->volume <= other.impl_->volume + EPSILON;
 }

 bool AudioVolume::operator>(const AudioVolume& other) const
 {
   return impl_->volume > other.impl_->volume + EPSILON;
 }

 bool AudioVolume::operator>=(const AudioVolume& other) const
 {
   return impl_->volume >= other.impl_->volume - EPSILON;
 }

 // --- 演算子 ---
 AudioVolume AudioVolume::operator+(const AudioVolume& other) const
 {
   return AudioVolume(impl_->volume + other.impl_->volume);
 }

 AudioVolume AudioVolume::operator-(const AudioVolume& other) const
 {
   return AudioVolume(impl_->volume - other.impl_->volume);
 }

 AudioVolume AudioVolume::operator*(float scalar) const
 {
   return AudioVolume(impl_->volume * scalar);
 }

 AudioVolume AudioVolume::operator/(float scalar) const
 {
   if (std::abs(scalar) < EPSILON) {
     return AudioVolume(0.0f);
   }
   return AudioVolume(impl_->volume / scalar);
 }

 AudioVolume& AudioVolume::operator+=(const AudioVolume& other)
 {
   impl_->volume = std::clamp(impl_->volume + other.impl_->volume, 0.0f, 2.0f);
   return *this;
 }

 AudioVolume& AudioVolume::operator-=(const AudioVolume& other)
 {
   impl_->volume = std::clamp(impl_->volume - other.impl_->volume, 0.0f, 2.0f);
   return *this;
 }

 AudioVolume& AudioVolume::operator*=(float scalar)
 {
   impl_->volume = std::clamp(impl_->volume * scalar, 0.0f, 2.0f);
   return *this;
 }

 AudioVolume& AudioVolume::operator/=(float scalar)
 {
   if (std::abs(scalar) > EPSILON) {
     impl_->volume = std::clamp(impl_->volume / scalar, 0.0f, 2.0f);
   }
   return *this;
 }

 // --- 代入演算子 ---
 AudioVolume& AudioVolume::operator=(const AudioVolume& other)
 {
   if (this != &other) {
     *impl_ = *other.impl_;
   }
   return *this;
 }

 AudioVolume& AudioVolume::operator=(AudioVolume&& other) noexcept
 {
   if (this != &other) {
     delete impl_;
     impl_ = other.impl_;
     other.impl_ = nullptr;
   }
   return *this;
 }

};
