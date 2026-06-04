module;
#include <algorithm>
#include <cmath>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.HighLowPass;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioHighLowPass::AudioHighLowPass() {}

void AudioHighLowPass::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const float sampleRate = static_cast<float>(segment.sampleRate);

    if (channels == 0 || frames == 0) return;

    const float pi = 3.14159265f;
    
    // ローパス係数（12dB/octave）
    float lpCoef = 0.0f;
    if (lowPassFreq_ > 0.0f) {
        float w = 2.0f * pi * lowPassFreq_ / sampleRate;
        lpCoef = std::exp(-w);
    }
    
    // ハイパス係数
    float hpCoef = 0.0f;
    if (highPassFreq_ > 0.0f) {
        float w = 2.0f * pi * highPassFreq_ / sampleRate;
        hpCoef = std::exp(-w);
    }

    for (int ch = 0; ch < channels; ++ch) {
        if (ch >= segment.channelData.size()) break;
        float* data = segment.channelData[ch].data();
        
        float lpState = data[0];
        float hpState = data[0];

        for (int i = 0; i < frames; ++i) {
            // ローパス
            if (lowPassFreq_ > 0.0f) {
                lpState = lpCoef * lpState + (1.0f - lpCoef) * data[i];
            }
            
            // ハイパス
            if (highPassFreq_ > 0.0f) {
                float hpInput = data[i] - hpState;
                hpState = hpCoef * hpState + hpInput;
                data[i] = hpInput;
            }
            
            // ローパス適用
            if (lowPassFreq_ > 0.0f) {
                data[i] = lpState;
            }
        }
    }
}

} // namespace ArtifactCore