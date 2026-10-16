module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.TransientShaper;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateShaperSample(const float sample)
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

AudioTransientShaper::AudioTransientShaper() {}

void AudioTransientShaper::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float attack = std::clamp(finiteOr(attack_, 0.3f), -1.0f, 1.0f);
    const float sustain = std::clamp(finiteOr(sustain_, 0.0f), -1.0f, 1.0f);
    const float speedMs = std::clamp(finiteOr(speedMs_, 20.0f), 1.0f, 100.0f);

    if (attack == 0.0f && sustain == 0.0f) return;

    const float sr = static_cast<float>(sampleRate);
    const float fastCoeff = std::exp(-1.0f / (speedMs * 0.1f * 0.001f * sr));
    const float slowCoeff = std::exp(-1.0f / (speedMs * 0.001f * sr));

    const int chCount = std::min(channels, 2);
    for (int ch = 0; ch < chCount; ++ch) {
        if (ch >= segment.channelData.size()) break;
        const int samples = std::min(
            frames, static_cast<int>(segment.channelData[ch].size()));
        if (samples <= 0) continue;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;
            const float peak = std::abs(input);
            fastEnv_[ch] = saturateShaperSample(
                fastCoeff * fastEnv_[ch] + (1.0f - fastCoeff) * peak);
            slowEnv_[ch] = saturateShaperSample(
                slowCoeff * slowEnv_[ch] + (1.0f - slowCoeff) * peak);

            // アタック成分(0..1 に正規化)と胴鳴り成分(0..1 に正規化)。
            const float transientAmt = std::clamp(
                (fastEnv_[ch] - slowEnv_[ch]) / (fastEnv_[ch] + 1e-3f), 0.0f, 1.0f);
            const float bodyAmt = std::clamp(slowEnv_[ch] * 2.0f, 0.0f, 1.0f);
            const float gain = std::clamp(
                1.0f + attack * transientAmt * 2.0f + sustain * bodyAmt, 0.0f, 4.0f);
            data[i] = saturateShaperSample(input * gain);
        }
    }
}

std::vector<EffectParameter> AudioTransientShaper::getParameters() const
{
    return {
        {"attack", "Attack", -1.0f, 1.0f, 0.3f, attack_},
        {"sustain", "Sustain", -1.0f, 1.0f, 0.0f, sustain_},
        {"speed_ms", "Speed (ms)", 1.0f, 100.0f, 20.0f, speedMs_}
    };
}

void AudioTransientShaper::setParameterValue(const String& id, float value)
{
    if (id == "attack") setAttack(value);
    else if (id == "sustain") setSustain(value);
    else if (id == "speed_ms") setSpeedMs(value);
}

float AudioTransientShaper::getParameterValue(const String& id) const
{
    if (id == "attack") return attack_;
    if (id == "sustain") return sustain_;
    if (id == "speed_ms") return speedMs_;
    return 0.0f;
}

QJsonObject AudioTransientShaper::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("transient_shaper");
    obj["attack"] = attack_;
    obj["sustain"] = sustain_;
    obj["speed_ms"] = speedMs_;
    return obj;
}

void AudioTransientShaper::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setAttack(static_cast<float>(obj["attack"].toDouble(0.3)));
    setSustain(static_cast<float>(obj["sustain"].toDouble(0.0)));
    setSpeedMs(static_cast<float>(obj["speed_ms"].toDouble(20.0)));
}

} // namespace ArtifactCore
