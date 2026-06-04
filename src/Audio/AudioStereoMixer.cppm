module;
#include <algorithm>
#include <cmath>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.StereoMixer;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioStereoMixer::AudioStereoMixer() {}

void AudioStereoMixer::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    if (channels < 2 || frames == 0) return;

    float* left = segment.channelData[0].data();
    float* right = segment.channelData[1].data();

    // 左右バランス
    float leftGain = (leftRight_ <= 0.0f) ? 1.0f : (1.0f - leftRight_);
    float rightGain = (leftRight_ >= 0.0f) ? 1.0f : (1.0f + leftRight_);

    for (int i = 0; i < frames; ++i) {
        // LRバランス適用
        left[i] *= leftGain;
        right[i] *= rightGain;
    }
}

} // namespace ArtifactCore