module;
#include <algorithm>
#include <cmath>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Reverb;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioReverb::AudioReverb() {}

void AudioReverb::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    if (channels == 0 || frames == 0) return;

    // Schroederアルゴリズム: 4コンバライザ + 1ディフューザ
    static thread_local std::vector<float> combBuffers(4 * 1024);
    static thread_local std::vector<float> allpassBuffer(2048);

    for (int ch = 0; ch < std::min(channels, 2); ++ch) {
        if (ch >= segment.channelData.size()) break;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < frames; ++i) {
            float input = data[i];
            float output = 0.0f;

            // 4コンバライザ
            int combDelays[4] = {151, 307, 613, 1225};
            for (int c = 0; c < 4; ++c) {
                int idx = i % 1024;
                if (idx < 0) idx = 0;
                float bufOut = (idx + combDelays[c] < 1024) ? combBuffers[c * 1024 + (idx + combDelays[c]) % 1024] : 0.0f;
                combBuffers[c * 1024 + idx] = bufOut + input * decay_;
                output += bufOut;
            }
            output *= 0.25f;

            data[i] = data[i] * (1.0f - mix_) + output * mix_;
        }
    }
}

} // namespace ArtifactCore