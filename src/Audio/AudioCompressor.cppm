module;
#include <algorithm>
#include <cmath>

module Audio.Effect.Compressor;

import Audio.Segment;
import Audio.Bus;

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

    // サイドチェーンバッファがあるか確認し、なければ自分を参照
    // 本来は AudioBus の情報を参照する必要があるが、ここでは簡易化
    // ※ 実際の実装では AudioBus::process 内で sideChainBuffer をセットする想定
    
    for (int i = 0; i < frames; ++i) {
        // 制御用レベルの検出 (サイドチェーン or メイン)
        float detectorVal = 0.0f;
        
        const AudioSegment* source = (sideChainEnabled_ && sideChain) ? sideChain : &segment;
        int detectorChannels = source->channelCount();

        for (int c = 0; c < detectorChannels; ++c) {
            detectorVal = std::max(detectorVal, std::abs(source->channelData[c][i]));
        }

        // エンベロープ・フォロワー
        if (detectorVal > envelope_) {
            envelope_ = attackCoef * envelope_ + (1.0f - attackCoef) * detectorVal;
        } else {
            envelope_ = releaseCoef * envelope_ + (1.0f - releaseCoef) * detectorVal;
        }

        // ゲイン計算
        float gain = 1.0f;
        if (envelope_ > thresholdLinear) {
            // dB空間での計算
            float envDb = 20.0f * std::log10(envelope_);
            float overDb = envDb - thresholdDb_;
            float newDb = thresholdDb_ + overDb / ratio_;
            gain = std::pow(10.0f, (newDb - envDb) / 20.0f);
        }

        // ゲイン適用
        for (int c = 0; c < channels; ++c) {
            segment.channelData[c][i] *= gain;
        }

        currentGainReduction_ = gain; // 可視化用に保存
    }
}

} // namespace ArtifactCore
