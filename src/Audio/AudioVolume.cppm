module;

module Audio.Volume;

import std;

namespace ArtifactCore
{

 class AudioVolume::Impl
 {
 private:
 
 public:
  explicit Impl(float volume=1.0f);
  ~Impl();
  float volume = 1.0f;
  bool muted = false;
 };

 AudioVolume::Impl::Impl(float volume/*=1.0f*/)
 {

 }

 AudioVolume::Impl::~Impl()
 {

 }

 AudioVolume::AudioVolume() :impl_(new Impl())
 {

 }

 AudioVolume::AudioVolume(const AudioVolume& volume) :impl_(new Impl())
 {

 }

 AudioVolume::AudioVolume(AudioVolume&& other) noexcept :impl_(new Impl())
 {

 }

 AudioVolume::AudioVolume(float volume) :impl_(new Impl())
 {

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

 AudioVolume& AudioVolume::operator=(const AudioVolume& volume)
 {

  return *this;
 }
AudioVolume& AudioVolume::operator=(AudioVolume&& other) noexcept
 {
 return *this;
 }



};