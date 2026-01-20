module;
#include <QList>
module Audio.Panner;

import Audio.Segment;

namespace ArtifactCore {

 class AudioPanner::Impl {
 private:
  PanningMode mode_ = PanningMode::StereoBalance;
 public:
  Impl() {}
  ~Impl() {}

  void setMode(PanningMode mode) {
    mode_ = mode;
  }

  PanningMode getMode() const {
    return mode_;
  }

  PanningGain calculateGain(float azimuth, float elevation = 0.0f) const {
    PanningGain gain;
    if (mode_ == PanningMode::StereoBalance) {
      // Simple stereo balance: azimuth from -180 to 180
      float pan = azimuth / 180.0f; // -1 to 1
      float leftGain = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
      float rightGain = (pan >= 0.0f) ? 1.0f : (1.0f + pan);
      gain.channelGains = {leftGain, rightGain};
    } else {
      // For other modes, default to equal gain
      gain.channelGains = {1.0f, 1.0f};
    }
    return gain;
  }

  void applyPanning(AudioSegment& segment, const PanningGain& gain) {
    int numChannels = segment.channelCount();
    int numFrames = segment.frameCount();
    for (int ch = 0; ch < numChannels && ch < static_cast<int>(gain.channelGains.size()); ++ch) {
      float g = gain.channelGains[ch];
      for (int f = 0; f < numFrames; ++f) {
        segment.channelData[ch][f] *= g;
      }
    }
  }
 };

 AudioPanner::AudioPanner() : impl_(new Impl())
 {

 }

 AudioPanner::~AudioPanner()
 {
  delete impl_;
 }

 void AudioPanner::setMode(PanningMode mode)
 {
  impl_->setMode(mode);
 }

 PanningMode AudioPanner::getMode() const
 {
  return impl_->getMode();
 }

 PanningGain AudioPanner::calculateGain(float azimuth, float elevation) const
 {
  return impl_->calculateGain(azimuth, elevation);
 }

 void AudioPanner::applyPanning(AudioSegment& segment, const PanningGain& gain)
 {
  impl_->applyPanning(segment, gain);
 }

}