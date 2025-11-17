module;

module Audio.Volume;

import std;

namespace ArtifactCore
{

 class AudioVolume::Impl
 {
 private:
 
 public:
  Impl();
  ~Impl();
  float volume = 1.0f;
  bool muted = false;
 };

 AudioVolume::Impl::Impl()
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

};