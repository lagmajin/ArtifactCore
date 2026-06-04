module;
#include <algorithm>
#include <cmath>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Chorus;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioChorus::AudioChorus() {}

void AudioChorus::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;

    if (channels == 0 || frames == 0) return;

    // LFOと遅延バッファ
    static thread_local std::vector<float> delayBuffer(48000 * 2);
    const int maxDelay = static_cast<int>(baseDelayMs_ * sampleRate / 1000.0) + 1;

    for (int ch = 0; ch < std::min(channels, 2); ++ch) {
        if (ch >= segment.channelData.size()) break;
        
        float* inData = segment.channelData[ch].data();
        
        for (int i = 0; i < frames; ++i) {
            float lfo = std::sin(2.0f * 3.14159265f * rate_ * i / sampleRate);
            int delaySamples = baseDelayMs_ + static_cast<int>(lfo * depth_ * maxDelay);
            
            int readIdx = i - delaySamples;
            float delayed = (readIdx >= 0) ? inData[readIdx] : 0.0f;
            
            inData[i] = inData[i] * 0.7f + delayed * 0.3f;
        }
    }
}

} // namespace ArtifactCore