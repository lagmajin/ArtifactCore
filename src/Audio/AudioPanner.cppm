module;

module Audio.Panner;

import Audio.Segment;
import std;

namespace ArtifactCore {

class AudioPanner::Impl {
private:
  PanningMode mode_ = PanningMode::StereoBalance;

public:
  void setMode(PanningMode mode) {
    mode_ = mode;
  }

  PanningMode getMode() const {
    return mode_;
  }

  PanningGain calculateGain(float azimuth, float elevation = 0.0f) const {
    PanningGain gain;
    if (mode_ == PanningMode::StereoBalance) {
      const float pan = azimuth / 180.0f;
      const float leftGain = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
      const float rightGain = (pan >= 0.0f) ? 1.0f : (1.0f + pan);
      gain.channelGains = {leftGain, rightGain};
    } else {
      gain.channelGains = {1.0f, 1.0f};
    }
    (void)elevation;
    return gain;
  }

  void applyPanning(AudioSegment& segment, const PanningGain& gain) {
    const int numChannels = segment.channelCount();
    const int numFrames = segment.frameCount();
    for (int ch = 0; ch < numChannels && ch < static_cast<int>(gain.channelGains.size()); ++ch) {
      const float channelGain = gain.channelGains[ch];
      float* data = segment.channelData[ch].data();
      for (int frameIndex = 0; frameIndex < numFrames; ++frameIndex) {
        data[frameIndex] *= channelGain;
      }
    }
  }
};

PanningGain AudioPanner::calculateConstantPowerGains(float pan) {
    constexpr float kPi = 3.1415926535f;
    const float angle = (pan + 1.0f) * (kPi / 4.0f);
    return {{std::cos(angle), std::sin(angle)}};
}

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

} // namespace ArtifactCore
