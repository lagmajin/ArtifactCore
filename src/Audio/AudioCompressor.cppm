module;
#include <algorithm>
#include <cmath>

module Audio.Effect.Compressor;

import Audio.Segment;

namespace ArtifactCore {

AudioCompressor::AudioCompressor() {}

void AudioCompressor::process(AudioSegment& segment, const AudioSegment* sideChain) {
    if (bypass_) return;

    int channels = segment.channelCount();
    int frames = segment.frameCount();
    float sampleRate = static_cast<float>(segment.sampleRate);
    
    // アタック・リリースの係数計算
    float attackCoef = std::exp(-1.0f / (attackMs_ * 0.001f * sampleRate));
    float releaseCoef = std::exp(-1.0f / (releaseMs_ * 0.001f * sampleRate));

    float thresholdLinear = std::pow(10.0f, thresholdDb_ / 20.0f);

    for (int i = 0; i < frames; ++i) {
        float detectorVal = 0.0f;
        
        const AudioSegment* source = (sideChainEnabled_ && sideChain) ? sideChain : &segment;
        int detectorChannels = source->channelCount();

        for (int c = 0; c < detectorChannels; ++c) {
            detectorVal = std::max(detectorVal, std::abs(source->channelData[c][i]));
        }

        // envelope_ を読み書き（process は同一スレッド内で連続呼ばれるためローカルコピーで競合回避）
        float envelope = envelope_;
        if (detectorVal > envelope) {
            envelope = attackCoef * envelope + (1.0f - attackCoef) * detectorVal;
        } else {
            envelope = releaseCoef * envelope + (1.0f - releaseCoef) * detectorVal;
        }
        envelope_ = envelope;

        // ゲイン計算
        float gain = 1.0f;
        if (envelope > thresholdLinear) {
            float envDb = 20.0f * std::log10(envelope);
            float overDb = envDb - thresholdDb_;
            float newDb = thresholdDb_ + overDb / ratio_;
            gain = std::pow(10.0f, (newDb - envDb) / 20.0f);
        }

        // ゲイン適用
        for (int c = 0; c < channels; ++c) {
            segment.channelData[c][i] *= gain;
        }

        currentGainReduction_ = gain;
    }
}

float AudioCompressor::getGainReduction() const {
    return currentGainReduction_;
}

} // namespace ArtifactCore
