module;
#include <QString>
#include <algorithm>
#include <cmath>
#include <limits>

module Audio.Volume;

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

AudioVolume::AudioVolume() : impl_(new Impl())
{
}

AudioVolume::AudioVolume(float volume) : impl_(new Impl(volume))
{
}

AudioVolume::AudioVolume(const AudioVolume& other) : impl_(new Impl(*other.impl_))
{
}

AudioVolume::AudioVolume(AudioVolume&& other) noexcept : impl_(other.impl_)
{
 other.impl_ = nullptr;
}

AudioVolume::~AudioVolume()
{
 delete impl_;
}

float AudioVolume::volume() const
{
 return impl_->muted ? 0.0f : impl_->volume;
}

void AudioVolume::setVolume(float v)
{
 impl_->volume = std::clamp(v, 0.0f, 1.0f);
}

float AudioVolume::toDecibels() const
{
 const float vol = volume();
 if (vol <= 0.0f) {
  return -std::numeric_limits<float>::infinity();
 }
 return 20.0f * std::log10(vol);
}

AudioVolume AudioVolume::fromDecibels(float dB)
{
 const float linearVolume = std::pow(10.0f, dB / 20.0f);
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

AudioVolume AudioVolume::interpolate(const AudioVolume& target, float t) const
{
 t = std::clamp(t, 0.0f, 1.0f);
 const float interpolated = impl_->volume + (target.impl_->volume - impl_->volume) * t;
 return AudioVolume(interpolated);
}

AudioVolume AudioVolume::linearFade(const AudioVolume& from, const AudioVolume& to, float t)
{
 return from.interpolate(to, t);
}

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

AudioVolume AudioVolume::createPreset(Preset preset)
{
 switch (preset) {
 case Preset::Mute: return AudioVolume(0.0f);
 case Preset::VeryQuiet: return AudioVolume(0.1f);
 case Preset::Quiet: return AudioVolume(0.3f);
 case Preset::Normal: return AudioVolume(1.0f);
 case Preset::Loud: return AudioVolume(1.5f);
 case Preset::Maximum: return AudioVolume(2.0f);
 default: return AudioVolume(1.0f);
 }
}

UniString AudioVolume::getPresetName(Preset preset)
{
 switch (preset) {
 case Preset::Mute: return UniString(QString("Mute"));
 case Preset::VeryQuiet: return UniString(QString("Very Quiet"));
 case Preset::Quiet: return UniString(QString("Quiet"));
 case Preset::Normal: return UniString(QString("Normal"));
 case Preset::Loud: return UniString(QString("Loud"));
 case Preset::Maximum: return UniString(QString("Maximum"));
 default: return UniString(QString("Unknown"));
 }
}

UniString AudioVolume::serialize() const
{
 const QString json = QString("{\"volume\":")
                    + QString::number(impl_->volume, 'g', 9)
                    + QString(",\"muted\":")
                    + (impl_->muted ? QString("true") : QString("false"))
                    + QString("}");
 return UniString(json);
}

AudioVolume AudioVolume::deserialize(const UniString&)
{
 return AudioVolume(1.0f);
}

bool AudioVolume::equals(const AudioVolume& other) const
{
 return std::abs(impl_->volume - other.impl_->volume) < EPSILON;
}

bool AudioVolume::operator==(const AudioVolume& other) const { return equals(other); }
bool AudioVolume::operator!=(const AudioVolume& other) const { return !equals(other); }
bool AudioVolume::operator<(const AudioVolume& other) const { return impl_->volume < other.impl_->volume - EPSILON; }
bool AudioVolume::operator<=(const AudioVolume& other) const { return impl_->volume <= other.impl_->volume + EPSILON; }
bool AudioVolume::operator>(const AudioVolume& other) const { return impl_->volume > other.impl_->volume + EPSILON; }
bool AudioVolume::operator>=(const AudioVolume& other) const { return impl_->volume >= other.impl_->volume - EPSILON; }
AudioVolume AudioVolume::operator+(const AudioVolume& other) const { return AudioVolume(impl_->volume + other.impl_->volume); }
AudioVolume AudioVolume::operator-(const AudioVolume& other) const { return AudioVolume(impl_->volume - other.impl_->volume); }
AudioVolume AudioVolume::operator*(float scalar) const { return AudioVolume(impl_->volume * scalar); }

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

}
