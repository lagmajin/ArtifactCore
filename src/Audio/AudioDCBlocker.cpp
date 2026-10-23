module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.DCBlocker;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateDCBlockerSample(const float sample)
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

AudioDCBlocker::AudioDCBlocker() {}

void AudioDCBlocker::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float frequency = std::clamp(finiteOr(frequency_, 10.0f), 2.0f, 100.0f);
    const float mix = std::clamp(finiteOr(mix_, 1.0f), 0.0f, 1.0f);

    if (mix <= 0.0f) return;

    const float sr = static_cast<float>(sampleRate);
    const float coeff = std::clamp(
        1.0f - 6.2831853f * frequency / sr, 0.9f, 0.9999f);

    const int chCount = std::min(channels, 2);
    for (int ch = 0; ch < chCount; ++ch) {
        if (ch >= segment.channelData.size()) break;
        const int samples = std::min(
            frames, static_cast<int>(segment.channelData[ch].size()));
        if (samples <= 0) continue;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;
            const float wet = saturateDCBlockerSample(
                input - prevInput_[ch] + coeff * prevOutput_[ch]);
            prevInput_[ch] = input;
            prevOutput_[ch] = wet;
            data[i] = saturateDCBlockerSample(input * (1.0f - mix) + wet * mix);
        }
    }
}

std::vector<EffectParameter> AudioDCBlocker::getParameters() const
{
    return {
        {"frequency", "Frequency (Hz)", 2.0f, 100.0f, 10.0f, frequency_},
        {"mix", "Mix", 0.0f, 1.0f, 1.0f, mix_}
    };
}

void AudioDCBlocker::setParameterValue(const String& id, float value)
{
    if (id == "frequency") setFrequency(value);
    else if (id == "mix") setMix(value);
}

float AudioDCBlocker::getParameterValue(const String& id) const
{
    if (id == "frequency") return frequency_;
    if (id == "mix") return mix_;
    return 0.0f;
}

QJsonObject AudioDCBlocker::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("dc_blocker");
    obj["frequency"] = frequency_;
    obj["mix"] = mix_;
    return obj;
}

void AudioDCBlocker::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setFrequency(static_cast<float>(obj["frequency"].toDouble(10.0)));
    setMix(static_cast<float>(obj["mix"].toDouble(1.0)));
}

} // namespace ArtifactCore
