module;
#include <algorithm>
#include <cmath>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.BassTreble;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioBassTreble::AudioBassTreble() {
    // デフォルト係数を初期化
    bassCoeff_.store(1.0f, std::memory_order_relaxed);
    trebleCoeff_.store(1.0f, std::memory_order_relaxed);
}

void AudioBassTreble::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    if (channels == 0 || frames == 0) return;

    // dBから線形ゲインに変換（共通係数計算）
    const float bassGain = std::pow(10.0f, bassDb_ / 20.0f);
    const float trebleGain = std::pow(10.0f, trebleDb_ / 20.0f);

    // 簡易ローシェルフ/ハイシェルフ
    // Bass: 100Hz以下をブースト/カット
    // Treble: 8kHz以上をブースト/カット
    // 動的しきい値（サンプルレート依存）
    const float sampleRate = static_cast<float>(segment.sampleRate);
    const float bassFreq = 100.0f / (sampleRate * 0.5f);  // 正規化周波数
    const float trebleFreq = 8000.0f / (sampleRate * 0.5f);

    for (int ch = 0; ch < channels; ++ch) {
        if (ch >= segment.channelData.size()) break;
        float* data = segment.channelData[ch].data();
        
        // 1次IIRフィルタ係数（バタワースクローン本田デジタルフィルタ）
        // 簡略化: 固定係数でロー/ハイシェルフ
        const float lowCoeff = std::exp(-2.0f * 3.14159265f * bassFreq);
        const float highCoeff = std::exp(-2.0f * 3.14159265f * trebleFreq);
        
        float lowState = data[0];
        float highState = data[0];
        
        for (int i = 0; i < frames; ++i) {
            // ロー/ハイシェルフフィルタ
            lowState = lowCoeff * lowState + (1.0f - lowCoeff) * data[i];
            highState = highCoeff * highState + (1.0f - highCoeff) * data[i];
            
            // ベース+トレブル適用
            data[i] = (lowState * bassGain + highState * trebleGain) * 0.5f;
        }
    }
}

} // namespace ArtifactCore