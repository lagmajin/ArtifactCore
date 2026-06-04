module;
#include <algorithm>
#include <cmath>
#include <QVector>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Delay;

import Audio.Effect;
import Audio.Segment;
import Audio.RingBuffer;

namespace ArtifactCore {

AudioDelay::AudioDelay() {}

void AudioDelay::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    
    if (channels == 0 || frames == 0) return;

    // 遅延サンプル数を計算
    const int delaySamples = static_cast<int>(delayTimeMs_ * sampleRate / 1000.0);

    // チャンネルごとに遅延バッファ
    static thread_local std::vector<std::vector<float>> delayBuffers(2, std::vector<float>(48000)); // 1秒分

    for (int ch = 0; ch < std::min(channels, 2); ++ch) {
        if (ch >= segment.channelData.size()) break;
        
        const int delayBufSize = std::min(delaySamples * 2, 48000);
        if (delayBuffers[ch].size() < static_cast<size_t>(delayBufSize)) {
            delayBuffers[ch].resize(delayBufSize);
        }

        float* inData = segment.channelData[ch].data();
        float dry = mix_;
        float wet = mix_;

        for (int i = 0; i < frames; ++i) {
            int delayIdx = i % delayBufSize;
            
            // 遅延読み取り
            float delayed = (delayIdx + delaySamples < delayBufSize) 
                ? delayBuffers[ch][(delayIdx + delaySamples) % delayBufSize] 
                : 0.0f;
            
            // フィードバック込みで書き込み
            float out = inData[i] + delayed * feedback_;
            delayBuffers[ch][delayIdx] = out;

            // ミックス出力
            inData[i] = inData[i] * (1.0f - wet) + delayed * wet;
        }
    }
}

} // namespace ArtifactCore