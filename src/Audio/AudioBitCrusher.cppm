module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.BitCrusher;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateCrusherSample(const float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

} // namespace

AudioBitCrusher::AudioBitCrusher() {}

void AudioBitCrusher::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    if (channels == 0 || frames == 0) return;

    const float bits = std::clamp(finiteOr(bits_, 8.0f), 2.0f, 16.0f);
    const float mix = std::clamp(finiteOr(mix_, 1.0f), 0.0f, 1.0f);
    int factor = static_cast<int>(finiteOr(downsample_, 1.0f) + 0.5f);
    factor = std::clamp(factor, 1, 40);

    if (mix <= 0.0f) return;
    // 16bit + factor 1 は恒等に近いが、量子化誤差が残るため素通しする。
    if (bits >= 16.0f && factor <= 1) return;

    const float levels = std::pow(2.0f, bits - 1.0f);

    const int chCount = std::min(channels, 2);
    for (int ch = 0; ch < chCount; ++ch) {
        if (ch >= segment.channelData.size()) break;
        const int samples = std::min(
            frames, static_cast<int>(segment.channelData[ch].size()));
        if (samples <= 0) continue;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;
            if (holdCounter_[ch] <= 0) {
                const float clamped = std::clamp(input, -1.0f, 1.0f);
                held_[ch] = std::round(clamped * levels) / levels;
                holdCounter_[ch] = factor;
            }
            --holdCounter_[ch];
            data[i] = saturateCrusherSample(
                input * (1.0f - mix) + held_[ch] * mix);
        }
    }
}

std::vector<EffectParameter> AudioBitCrusher::getParameters() const
{
    return {
        {"bits", "Bits", 2.0f, 16.0f, 8.0f, bits_},
        {"downsample", "Downsample", 1.0f, 40.0f, 1.0f, downsample_},
        {"mix", "Mix", 0.0f, 1.0f, 1.0f, mix_}
    };
}

void AudioBitCrusher::setParameterValue(const String& id, float value)
{
    if (id == "bits") setBits(value);
    else if (id == "downsample") setDownsample(value);
    else if (id == "mix") setMix(value);
}

float AudioBitCrusher::getParameterValue(const String& id) const
{
    if (id == "bits") return bits_;
    if (id == "downsample") return downsample_;
    if (id == "mix") return mix_;
    return 0.0f;
}

QJsonObject AudioBitCrusher::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("bit_crusher");
    obj["bits"] = bits_;
    obj["downsample"] = downsample_;
    obj["mix"] = mix_;
    return obj;
}

void AudioBitCrusher::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setBits(static_cast<float>(obj["bits"].toDouble(8.0)));
    setDownsample(static_cast<float>(obj["downsample"].toDouble(1.0)));
    setMix(static_cast<float>(obj["mix"].toDouble(1.0)));
}

} // namespace ArtifactCore
