module;
#include <algorithm>
#include <cmath>
#include <limits>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.StereoMixer;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float sanitizeStereoSample(float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

}

AudioStereoMixer::AudioStereoMixer() {}

void AudioStereoMixer::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    if (channels < 2 || frames == 0) return;

    const int samples = std::min({frames,
                                  segment.channelData[0].size(),
                                  segment.channelData[1].size()});
    if (samples <= 0) return;

    float* left = segment.channelData[0].data();
    float* right = segment.channelData[1].data();

    // 左右バランス
    float leftGain = (leftRight_ <= 0.0f) ? 1.0f : (1.0f - leftRight_);
    float rightGain = (leftRight_ >= 0.0f) ? 1.0f : (1.0f + leftRight_);

    for (int i = 0; i < samples; ++i) {
        // LRバランス適用
        left[i] = sanitizeStereoSample(
            (std::isfinite(left[i]) ? left[i] : 0.0f) * leftGain);
        right[i] = sanitizeStereoSample(
            (std::isfinite(right[i]) ? right[i] : 0.0f) * rightGain);
    }
}

} // namespace ArtifactCore
