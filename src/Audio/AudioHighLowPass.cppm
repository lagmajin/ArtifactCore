module;
#include <algorithm>
#include <cmath>
#include <limits>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.HighLowPass;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float sanitizeHighLowSample(float value)
{
    if (std::isfinite(value)) return value;
    if (std::isnan(value)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), value);
}

}

AudioHighLowPass::AudioHighLowPass() {}

void AudioHighLowPass::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const float sampleRate = static_cast<float>(segment.sampleRate);

    if (channels == 0 || frames == 0 || segment.sampleRate <= 0) return;

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

    if (stateSampleRate_ != segment.sampleRate) {
        stateSampleRate_ = segment.sampleRate;
        std::fill(lowPassStatesInitialized_.begin(),
                  lowPassStatesInitialized_.end(), false);
        std::fill(highPassStatesInitialized_.begin(),
                  highPassStatesInitialized_.end(), false);
    }
    if (static_cast<int>(lowPassStates_.size()) < channels) {
        lowPassStates_.resize(channels, 0.0f);
        highPassStates_.resize(channels, 0.0f);
        lowPassStatesInitialized_.resize(channels, false);
        highPassStatesInitialized_.resize(channels, false);
    }
    const bool lowPassEnabled = lowPassFreq_ > 0.0f;
    const bool highPassEnabled = highPassFreq_ > 0.0f;

    for (int ch = 0; ch < channels; ++ch) {
        if (ch >= segment.channelData.size()) break;
        const int samples = std::min(frames, segment.channelData[ch].size());
        if (samples <= 0) continue;
        float* data = segment.channelData[ch].data();
        
        float& lpState = lowPassStates_[ch];
        float& hpState = highPassStates_[ch];
        const float initialState = std::isfinite(data[0]) ? data[0] : 0.0f;
        if (!lowPassStatesInitialized_[ch] || !lowPassEnabled) {
            lpState = initialState;
            lowPassStatesInitialized_[ch] = lowPassEnabled;
        }
        if (!highPassStatesInitialized_[ch] || !highPassEnabled) {
            hpState = initialState;
            highPassStatesInitialized_[ch] = highPassEnabled;
        }

        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;
            // ローパス
            if (lowPassEnabled) {
                lpState = sanitizeHighLowSample(
                    lpCoef * lpState + (1.0f - lpCoef) * input);
            }
            
            // ハイパス
            if (highPassEnabled) {
                float hpInput = sanitizeHighLowSample(input - hpState);
                hpState = sanitizeHighLowSample(hpCoef * hpState + hpInput);
                data[i] = hpInput;
            }
            
            // ローパス適用
            if (lowPassEnabled) {
                data[i] = sanitizeHighLowSample(lpState);
            } else {
                data[i] = sanitizeHighLowSample(data[i]);
            }
        }
    }
}

} // namespace ArtifactCore
