module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Spectrum;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioSpectrum::AudioSpectrum() {
    spectrum_.assign(64, 0.0f);
    waveform_.assign(1024, 0.0f);
}

void AudioSpectrum::computeFFT(const std::vector<float>& input, std::vector<float>& output) {
    // 簡易DFT実装（FFTはQtMultimedia::QAudioSpectrum や外部ライブラリを推奨）
    const int n = static_cast<int>(std::min(input.size(), output.size() * 2));
    const int halfN = n / 2;
    
    for (int k = 0; k < static_cast<int>(output.size()); ++k) {
        float magnitude = 0.0f;
        for (int i = 0; i < n; ++i) {
            float angle = 2.0f * 3.14159265f * k * i / n;
            magnitude += std::abs(input[i] * std::cos(angle));
        }
        output[k] = magnitude / n;
    }
}

void AudioSpectrum::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    if (channels == 0 || frames == 0) return;

    // 波形取得（ダウンサンプル）
    int waveSamples = std::min(frames, 1024);
    waveform_.resize(waveSamples);
    for (int i = 0; i < waveSamples; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) {
            if (c < segment.channelData.size()) {
                int idx = i * (frames / waveSamples);
                if (idx < segment.channelData[c].size()) {
                    sum += std::abs(segment.channelData[c][idx]);
                }
            }
        }
        waveform_[i] = sum / channels;
    }

    // スペクトラム計算
    std::vector<float> downmixed;
    if (channels == 2) {
        downmixed.resize(frames);
        for (int i = 0; i < frames; ++i) {
            if (i < segment.channelData[0].size() && i < segment.channelData[1].size()) {
                downmixed[i] = (segment.channelData[0][i] + segment.channelData[1][i]) * 0.5f;
            }
        }
    } else {
        downmixed.assign(segment.channelData[0].cbegin(), segment.channelData[0].cend());
    }

    spectrum_.resize(bins_);
    computeFFT(downmixed, spectrum_);
    spectrumReady_.store(true, std::memory_order_release);
}

} // namespace ArtifactCore
