module;
#include <algorithm>
#include <cmath>
#include <limits>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.BassTreble;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float sanitizeBassTrebleSample(float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

}

AudioBassTreble::AudioBassTreble() {
    // デフォルト係数を初期化
    bassCoeff_.store(1.0f, std::memory_order_relaxed);
    trebleCoeff_.store(1.0f, std::memory_order_relaxed);
}

void AudioBassTreble::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    if (channels == 0 || frames == 0 || segment.sampleRate <= 0) return;

    const float bassDb = std::clamp(
        std::isfinite(bassDb_) ? bassDb_ : 0.0f, -24.0f, 24.0f);
    const float trebleDb = std::clamp(
        std::isfinite(trebleDb_) ? trebleDb_ : 0.0f, -24.0f, 24.0f);

    // dBから線形ゲインに変換（共通係数計算）
    const float bassGain = std::pow(10.0f, bassDb / 20.0f);
    const float trebleGain = std::pow(10.0f, trebleDb / 20.0f);

    if (stateSampleRate_ != segment.sampleRate) {
        stateSampleRate_ = segment.sampleRate;
        std::fill(statesInitialized_.begin(), statesInitialized_.end(), false);
    }
    if (static_cast<int>(lowStates_.size()) < channels) {
        lowStates_.resize(channels, 0.0f);
        highStates_.resize(channels, 0.0f);
        statesInitialized_.resize(channels, false);
    }

    // 簡易ローシェルフ/ハイシェルフ
    // Bass: 100Hz以下をブースト/カット
    // Treble: 8kHz以上をブースト/カット
    // 動的しきい値（サンプルレート依存）
    const float sampleRate = static_cast<float>(segment.sampleRate);
    const float bassFreq = 100.0f / (sampleRate * 0.5f);  // 正規化周波数
    const float trebleFreq = 8000.0f / (sampleRate * 0.5f);

    for (int ch = 0; ch < channels; ++ch) {
        if (ch >= segment.channelData.size()) break;
        const int samples = std::min(frames, segment.channelData[ch].size());
        if (samples <= 0) continue;
        float* data = segment.channelData[ch].data();
        
        // 1次IIRフィルタ係数（バタワースクローン本田デジタルフィルタ）
        // 簡略化: 固定係数でロー/ハイシェルフ
        const float lowCoeff = std::exp(-2.0f * 3.14159265f * bassFreq);
        const float highCoeff = std::exp(-2.0f * 3.14159265f * trebleFreq);
        
        float& lowState = lowStates_[ch];
        float& highState = highStates_[ch];
        if (!statesInitialized_[ch]) {
            lowState = std::isfinite(data[0]) ? data[0] : 0.0f;
            highState = lowState;
            statesInitialized_[ch] = true;
        }
        
        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;
            // ロー/ハイシェルフフィルタ
            lowState = sanitizeBassTrebleSample(
                lowCoeff * lowState + (1.0f - lowCoeff) * input);
            highState = sanitizeBassTrebleSample(
                highCoeff * highState + (1.0f - highCoeff) * input);
            
            // ベース+トレブル適用
            data[i] = sanitizeBassTrebleSample(
                (lowState * bassGain + highState * trebleGain) * 0.5f);
        }
    }
}

} // namespace ArtifactCore
