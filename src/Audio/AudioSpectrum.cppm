module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Spectrum;

import Audio.Effect;
import Audio.Segment;
import Container.NamedVector;

namespace ArtifactCore {

namespace {

float sanitizeSpectrumSample(float value)
{
    if (std::isfinite(value)) return value;
    if (std::isnan(value)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), value);
}

qint64 saturatingFrameAdd(qint64 left, qint64 right)
{
    if (right > 0 && left > std::numeric_limits<qint64>::max() - right) {
        return std::numeric_limits<qint64>::max();
    }
    if (right < 0 && left < std::numeric_limits<qint64>::min() - right) {
        return std::numeric_limits<qint64>::min();
    }
    return left + right;
}

}

AudioSpectrum::AudioSpectrum() {
    spectrum_.assign(64, 0.0f);
    waveform_.assign(1024, 0.0f);
}

void AudioSpectrum::resetLoudnessMeasurement()
{
    momentaryLufs_ = -std::numeric_limits<float>::infinity();
    shortTermLufs_ = -std::numeric_limits<float>::infinity();
    integratedLufs_ = -std::numeric_limits<float>::infinity();
    loudnessRangeLufs_ = 0.0f;
    peakDb_ = -std::numeric_limits<float>::infinity();
    truePeakDb_ = -std::numeric_limits<float>::infinity();
    integratedEnergySum_ = 0.0;
    integratedFrameCount_ = 0;
    lastEndFrame_ = -1;
    loudnessWindows_.clear();
}

float AudioSpectrum::normalizationGainDb(float targetLufs) const
{
    if (!std::isfinite(targetLufs) || !std::isfinite(integratedLufs_)) {
        return 0.0f;
    }
    return targetLufs - integratedLufs_;
}

bool AudioSpectrum::normalizeToTargetLufs(AudioSegment& segment, float targetLufs)
{
    process(segment);
    const float gainDb = normalizationGainDb(targetLufs);
    if (!std::isfinite(gainDb)) {
        return false;
    }
    const float gain = std::pow(10.0f, gainDb / 20.0f);
    if (!std::isfinite(gain) || gain <= 0.0f) {
        return false;
    }
    for (auto& channel : segment.channelData) {
        for (float& sample : channel) {
            if (std::isfinite(sample)) {
                sample = sanitizeSpectrumSample(sample * gain);
            } else {
                sample = 0.0f;
            }
        }
    }
    return true;
}

void AudioSpectrum::computeFFT(const std::vector<float>& input, std::vector<float>& output) {
    // 簡易DFT実装（FFTはQtMultimedia::QAudioSpectrum や外部ライブラリを推奨）
    if (input.empty() || output.empty()) {
        std::fill(output.begin(), output.end(), 0.0f);
        return;
    }
    const int n = static_cast<int>(std::min(input.size(), output.size() * 2));
    if (n <= 0) {
        std::fill(output.begin(), output.end(), 0.0f);
        return;
    }
    const int halfN = n / 2;
    
    for (int k = 0; k < static_cast<int>(output.size()); ++k) {
        double magnitude = 0.0;
        for (int i = 0; i < n; ++i) {
            float angle = 2.0f * 3.14159265f * k * i / n;
            magnitude += std::abs(static_cast<double>(
                sanitizeSpectrumSample(input[i])) * std::cos(angle));
        }
        const double normalized = magnitude / n;
        output[k] = std::isfinite(normalized)
            ? static_cast<float>(std::min(
                normalized, static_cast<double>(std::numeric_limits<float>::max())))
            : std::numeric_limits<float>::max();
    }
}

void AudioSpectrum::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    if (channels == 0 || frames == 0) {
        spectrum_.assign(std::max(1, bins_), 0.0f);
        waveform_.assign(1024, 0.0f);
        momentaryLufs_ = -std::numeric_limits<float>::infinity();
        shortTermLufs_ = momentaryLufs_;
        integratedLufs_ = momentaryLufs_;
        loudnessRangeLufs_ = 0.0f;
        peakDb_ = momentaryLufs_;
        truePeakDb_ = momentaryLufs_;
        integratedEnergySum_ = 0.0;
        integratedFrameCount_ = 0;
        lastEndFrame_ = -1;
        loudnessWindows_.clear();
        spectrumReady_.store(false, std::memory_order_release);
        return;
    }

    // Compute a deterministic loudness estimate from the non-LFE channels.
    // This keeps the meter useful for all existing AudioSegment layouts while
    // avoiding a second audio traversal in clients.  The -0.691 LU offset is
    // the RMS-to-LUFS reference used by BS.1770-style meters; filtering and
    // gating remain explicit future extensions of this core path.
    double sumSquared = 0.0;
    float peak = 0.0f;
    float truePeak = 0.0f;
    int contributingChannels = 0;
    const bool hasLfe = segment.layout == AudioChannelLayout::Surround51 ||
                        segment.layout == AudioChannelLayout::Surround71;
    for (int c = 0; c < channels; ++c) {
        if (hasLfe && c == 3) continue;
        if (c >= segment.channelData.size()) continue;
        const auto& channel = segment.channelData[c];
        if (channel.isEmpty()) continue;
        double channelSum = 0.0;
        float previousSample = 0.0f;
        bool hasPreviousSample = false;
        for (const float sample : channel) {
            if (std::isfinite(sample)) {
                const double square = static_cast<double>(sample) * sample;
                channelSum = std::isfinite(channelSum + square)
                    ? channelSum + square
                    : std::numeric_limits<double>::max();
                const float absoluteSample = std::abs(sample);
                peak = std::max(peak, absoluteSample);
                // True-peak interpolation must retain the original sample
                // peak too; a one-sample impulse has no adjacent transition
                // from which the oversampling loop can recover it.
                truePeak = std::max(truePeak, absoluteSample);
                if (hasPreviousSample) {
                    for (int subSample = 1; subSample < 4; ++subSample) {
                        const float t = static_cast<float>(subSample) / 4.0f;
                        const float interpolated = sanitizeSpectrumSample(
                            previousSample + (sample - previousSample) * t);
                        truePeak = std::max(truePeak, std::abs(interpolated));
                    }
                }
                previousSample = sample;
                hasPreviousSample = true;
            }
        }
        sumSquared = std::isfinite(
            sumSquared + channelSum / static_cast<double>(channel.size()))
            ? sumSquared + channelSum / static_cast<double>(channel.size())
            : std::numeric_limits<double>::max();
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
            loudnessWindows_.clear();
        }
        const double energyContribution = safeMeanSquare * static_cast<double>(frames);
        integratedEnergySum_ = std::isfinite(integratedEnergySum_ + energyContribution)
            ? integratedEnergySum_ + energyContribution
            : std::numeric_limits<double>::max();
        integratedFrameCount_ = saturatingFrameAdd(integratedFrameCount_, frames);
        lastEndFrame_ = saturatingFrameAdd(segment.startFrame, frames);
        const double integratedMeanSquare =
            integratedEnergySum_ / static_cast<double>(std::max<qint64>(1, integratedFrameCount_));
        integratedLufs_ = static_cast<float>(
            -0.691 + 10.0 * std::log10(std::max(integratedMeanSquare, 1.0e-12)));

        // Keep a bounded, sample-weighted 3-second window for short-term
        // loudness. This is intentionally explicit about its approximation:
        // BS.1770 K-weighting and gating are future filter stages, while the
        // time-domain contract is already useful to the mixer and preflight.
        loudnessWindows_.push_back(LoudnessWindow{
            segment.startFrame, frames, safeMeanSquare, lufs});
        const qint64 windowFrames = static_cast<qint64>(std::max(1, segment.sampleRate)) * 3;
        const qint64 windowEnd = saturatingFrameAdd(segment.startFrame, frames);
        while (!loudnessWindows_.empty() &&
               saturatingFrameAdd(loudnessWindows_.front().startFrame,
                                  loudnessWindows_.front().frameCount) <
                   windowEnd - windowFrames) {
            loudnessWindows_.erase(loudnessWindows_.begin());
        }
        double shortTermEnergy = 0.0;
        qint64 shortTermFrames = 0;
        NamedVector<float> windowLufs;
        windowLufs.reserve(loudnessWindows_.size());
        for (const auto& window : loudnessWindows_) {
            const qint64 windowStart = std::max(window.startFrame, windowEnd - windowFrames);
            const qint64 windowStop = std::min(
                saturatingFrameAdd(window.startFrame, window.frameCount), windowEnd);
            const qint64 overlap = std::max<qint64>(0, windowStop - windowStart);
            if (overlap <= 0) continue;
            shortTermEnergy += window.meanSquare * static_cast<double>(overlap);
            shortTermFrames = saturatingFrameAdd(shortTermFrames, overlap);
            if (std::isfinite(window.lufs)) windowLufs.push_back(window.lufs);
        }
        if (shortTermFrames > 0) {
            const double energy = std::max(
                shortTermEnergy / static_cast<double>(shortTermFrames), 1.0e-12);
            shortTermLufs_ = static_cast<float>(-0.691 + 10.0 * std::log10(energy));
        }
        if (windowLufs.size() >= 2) {
            std::sort(windowLufs.begin(), windowLufs.end());
            const auto percentile = [&windowLufs](double p) {
                const double index = p * static_cast<double>(windowLufs.size() - 1);
                const auto lower = static_cast<size_t>(std::floor(index));
                const auto upper = std::min(lower + 1, windowLufs.size() - 1);
                const float fraction = static_cast<float>(index - static_cast<double>(lower));
                return windowLufs[lower] +
                    (windowLufs[upper] - windowLufs[lower]) * fraction;
            };
            loudnessRangeLufs_ = std::max(0.0f, percentile(0.95) - percentile(0.10));
        }
        peakDb_ = peak > 1.0e-12f
            ? static_cast<float>(20.0 * std::log10(peak))
            : -std::numeric_limits<float>::infinity();
        truePeakDb_ = truePeak > 1.0e-12f
            ? static_cast<float>(20.0 * std::log10(truePeak))
            : -std::numeric_limits<float>::infinity();
    } else {
        momentaryLufs_ = -std::numeric_limits<float>::infinity();
        shortTermLufs_ = momentaryLufs_;
        integratedLufs_ = momentaryLufs_;
        loudnessRangeLufs_ = 0.0f;
        peakDb_ = momentaryLufs_;
        truePeakDb_ = momentaryLufs_;
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
                    sum = sanitizeSpectrumSample(
                        sum + std::abs(sanitizeSpectrumSample(
                            segment.channelData[c][idx])));
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
                downmixed[i] = sanitizeSpectrumSample(
                    (sanitizeSpectrumSample(segment.channelData[0][i]) +
                     sanitizeSpectrumSample(segment.channelData[1][i])) * 0.5f);
            }
        }
    } else {
        downmixed.resize(segment.channelData[0].size());
        for (int i = 0; i < downmixed.size(); ++i) {
            downmixed[i] = sanitizeSpectrumSample(segment.channelData[0][i]);
        }
    }

    spectrum_.resize(bins_);
    computeFFT(downmixed, spectrum_);
    spectrumReady_.store(true, std::memory_order_release);
}

} // namespace ArtifactCore
