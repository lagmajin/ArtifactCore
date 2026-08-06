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

float AudioSpectrum::normalizationGainDb(float targetLufs) const
{
    if (!std::isfinite(targetLufs) || !std::isfinite(integratedLufs_)) {
        return 0.0f;
    }
    return targetLufs - integratedLufs_;
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

    // Compute a deterministic loudness estimate from the non-LFE channels.
    // This keeps the meter useful for all existing AudioSegment layouts while
    // avoiding a second audio traversal in clients.  The -0.691 LU offset is
    // the RMS-to-LUFS reference used by BS.1770-style meters; filtering and
    // gating remain explicit future extensions of this core path.
    double sumSquared = 0.0;
    float peak = 0.0f;
    int contributingChannels = 0;
    const bool hasLfe = segment.layout == AudioChannelLayout::Surround51 ||
                        segment.layout == AudioChannelLayout::Surround71;
    for (int c = 0; c < channels; ++c) {
        if (hasLfe && c == 3) continue;
        if (c >= segment.channelData.size()) continue;
        const auto& channel = segment.channelData[c];
        if (channel.isEmpty()) continue;
        double channelSum = 0.0;
        for (const float sample : channel) {
            if (std::isfinite(sample)) {
                channelSum += static_cast<double>(sample) * sample;
                peak = std::max(peak, std::abs(sample));
            }
        }
        sumSquared += channelSum / static_cast<double>(channel.size());
        ++contributingChannels;
    }
    if (contributingChannels > 0) {
        const double meanSquare = sumSquared / static_cast<double>(contributingChannels);
        const double safeMeanSquare = std::max(meanSquare, 1.0e-12);
        const float lufs = static_cast<float>(-0.691 + 10.0 * std::log10(safeMeanSquare));
        momentaryLufs_ = lufs;
        // Keep a continuous energy average across adjacent segments.  A seek
        // or discontinuity starts a new integrated measurement window.
        if (lastEndFrame_ >= 0 && segment.startFrame != lastEndFrame_) {
            integratedEnergySum_ = 0.0;
            integratedFrameCount_ = 0;
        }
        integratedEnergySum_ += safeMeanSquare * static_cast<double>(frames);
        integratedFrameCount_ += frames;
        lastEndFrame_ = segment.startFrame + frames;
        const double integratedMeanSquare =
            integratedEnergySum_ / static_cast<double>(std::max<qint64>(1, integratedFrameCount_));
        integratedLufs_ = static_cast<float>(
            -0.691 + 10.0 * std::log10(std::max(integratedMeanSquare, 1.0e-12)));
        peakDb_ = peak > 1.0e-12f
            ? static_cast<float>(20.0 * std::log10(peak))
            : -std::numeric_limits<float>::infinity();
    } else {
        momentaryLufs_ = -std::numeric_limits<float>::infinity();
        integratedLufs_ = momentaryLufs_;
        peakDb_ = momentaryLufs_;
    }

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
