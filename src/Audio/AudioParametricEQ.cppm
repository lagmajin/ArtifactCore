module;
#include <algorithm>
#include <cmath>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.ParametricEQ;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioParametricEQ::AudioParametricEQ() {
    bands_.resize(4);  // デフォルト4バンド
    bands_[0] = {100.0f, 0.0f, 1.0f, true};   // Low
    bands_[1] = {500.0f, 0.0f, 1.0f, true};   // Low-Mid
    bands_[2] = {2000.0f, 0.0f, 1.0f, true};  // High-Mid
    bands_[3] = {8000.0f, 0.0f, 1.0f, true};  // High
}

void AudioParametricEQ::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const float sampleRate = static_cast<float>(segment.sampleRate);

    if (channels == 0 || frames == 0) return;

    for (int ch = 0; ch < channels; ++ch) {
        if (ch >= segment.channelData.size()) break;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < frames; ++i) {
            float sample = data[i];
            for (const auto& band : bands_) {
                if (!band.enabled) continue;

                float freq = band.frequency / (sampleRate * 0.5f);
                float theta = 2.0f * 3.14159265f * freq;
                
                // ピークフィルタシミュレーション（簡易版）
                float gain = std::pow(10.0f, band.gainDb / 20.0f);
                sample *= gain;
            }
            data[i] = sample;
        }
    }
}

} // namespace ArtifactCore