module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.TapeSaturation;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateTapeSample(const float sample)
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

AudioTapeSaturation::AudioTapeSaturation() {}

void AudioTapeSaturation::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float driveDb = std::clamp(finiteOr(driveDb_, 6.0f), 0.0f, 24.0f);
    const float warmth = std::clamp(finiteOr(warmth_, 0.4f), 0.0f, 1.0f);
    const float outputDb = std::clamp(finiteOr(outputDb_, -6.0f), -12.0f, 12.0f);
    const float mix = std::clamp(finiteOr(mix_, 1.0f), 0.0f, 1.0f);

    if (mix <= 0.0f) return;

    const float sr = static_cast<float>(sampleRate);
    const float driveGain = std::pow(10.0f, driveDb / 20.0f);
    const float outputGain = std::pow(10.0f, outputDb / 20.0f);
    // warmth_ が高いほど高域を落とす(18kHz -> 3kHz)。
    const float dampFreq = 18000.0f - warmth * 15000.0f;
    const float alpha = std::clamp(6.2831853f * dampFreq / sr, 0.001f, 0.99f);

    const int chCount = std::min(channels, 2);
    for (int ch = 0; ch < chCount; ++ch) {
        if (ch >= segment.channelData.size()) break;
        const int samples = std::min(
            frames, static_cast<int>(segment.channelData[ch].size()));
        if (samples <= 0) continue;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;
            // 前段ダンピング -> 駆動 -> tanh -> 後段ダンピング。
            preState_[ch] = saturateTapeSample(
                preState_[ch] + alpha * (input - preState_[ch]));
            const float driven = saturateTapeSample(
                (input * (1.0f - warmth) + preState_[ch] * warmth) * driveGain);
            const float saturated = std::tanh(std::clamp(driven, -10.0f, 10.0f));
            postState_[ch] = saturateTapeSample(
                postState_[ch] + alpha * (saturated - postState_[ch]));
            const float wet = saturateTapeSample(
                (saturated * (1.0f - warmth) + postState_[ch] * warmth) * outputGain);
            data[i] = saturateTapeSample(input * (1.0f - mix) + wet * mix);
        }
    }
}

std::vector<EffectParameter> AudioTapeSaturation::getParameters() const
{
    return {
        {"drive_db", "Drive (dB)", 0.0f, 24.0f, 6.0f, driveDb_},
        {"warmth", "Warmth", 0.0f, 1.0f, 0.4f, warmth_},
        {"output_db", "Output (dB)", -12.0f, 12.0f, -6.0f, outputDb_},
        {"mix", "Mix", 0.0f, 1.0f, 1.0f, mix_}
    };
}

void AudioTapeSaturation::setParameterValue(const String& id, float value)
{
    if (id == "drive_db") setDriveDb(value);
    else if (id == "warmth") setWarmth(value);
    else if (id == "output_db") setOutputDb(value);
    else if (id == "mix") setMix(value);
}

float AudioTapeSaturation::getParameterValue(const String& id) const
{
    if (id == "drive_db") return driveDb_;
    if (id == "warmth") return warmth_;
    if (id == "output_db") return outputDb_;
    if (id == "mix") return mix_;
    return 0.0f;
}

QJsonObject AudioTapeSaturation::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("tape_saturation");
    obj["drive_db"] = driveDb_;
    obj["warmth"] = warmth_;
    obj["output_db"] = outputDb_;
    obj["mix"] = mix_;
    return obj;
}

void AudioTapeSaturation::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setDriveDb(static_cast<float>(obj["drive_db"].toDouble(6.0)));
    setWarmth(static_cast<float>(obj["warmth"].toDouble(0.4)));
    setOutputDb(static_cast<float>(obj["output_db"].toDouble(-6.0)));
    setMix(static_cast<float>(obj["mix"].toDouble(1.0)));
}

} // namespace ArtifactCore
