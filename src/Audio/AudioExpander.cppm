module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Expander;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateExpanderSample(const float sample)
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

AudioExpander::AudioExpander() {}

void AudioExpander::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float thresholdDb = std::clamp(finiteOr(thresholdDb_, -40.0f), -60.0f, 0.0f);
    const float ratio = std::clamp(finiteOr(ratio_, 2.0f), 1.0f, 10.0f);
    const float attackMs = std::clamp(finiteOr(attackMs_, 10.0f), 0.1f, 100.0f);
    const float releaseMs = std::clamp(finiteOr(releaseMs_, 150.0f), 10.0f, 1000.0f);

    if (ratio <= 1.0f) return;

    const float sr = static_cast<float>(sampleRate);
    const float attackCoeff = std::exp(-1.0f / (attackMs * 0.001f * sr));
    const float releaseCoeff = std::exp(-1.0f / (releaseMs * 0.001f * sr));
    const float thresholdLin = std::pow(10.0f, thresholdDb / 20.0f);

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
            // 立ち上がりは attack、立ち下がりは release で追従する。
            const float coeff = (peak > envelope_[ch]) ? attackCoeff : releaseCoeff;
            envelope_[ch] = saturateExpanderSample(
                coeff * envelope_[ch] + (1.0f - coeff) * peak);

            float gain = 1.0f;
            if (envelope_[ch] < thresholdLin && envelope_[ch] > 0.0f) {
                const float envDb = 20.0f * std::log10(envelope_[ch]);
                const float outDb = thresholdDb + (envDb - thresholdDb) * ratio;
                gain = std::pow(10.0f, (outDb - envDb) / 20.0f);
                gain = std::clamp(gain, 0.0f, 1.0f);
            } else if (envelope_[ch] <= 0.0f) {
                gain = 0.0f;
            }
            data[i] = saturateExpanderSample(input * gain);
        }
    }
}

std::vector<EffectParameter> AudioExpander::getParameters() const
{
    return {
        {"threshold_db", "Threshold (dB)", -60.0f, 0.0f, -40.0f, thresholdDb_},
        {"ratio", "Ratio", 1.0f, 10.0f, 2.0f, ratio_},
        {"attack_ms", "Attack (ms)", 0.1f, 100.0f, 10.0f, attackMs_},
        {"release_ms", "Release (ms)", 10.0f, 1000.0f, 150.0f, releaseMs_}
    };
}

void AudioExpander::setParameterValue(const String& id, float value)
{
    if (id == "threshold_db") setThresholdDb(value);
    else if (id == "ratio") setRatio(value);
    else if (id == "attack_ms") setAttackMs(value);
    else if (id == "release_ms") setReleaseMs(value);
}

float AudioExpander::getParameterValue(const String& id) const
{
    if (id == "threshold_db") return thresholdDb_;
    if (id == "ratio") return ratio_;
    if (id == "attack_ms") return attackMs_;
    if (id == "release_ms") return releaseMs_;
    return 0.0f;
}

QJsonObject AudioExpander::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("expander");
    obj["threshold_db"] = thresholdDb_;
    obj["ratio"] = ratio_;
    obj["attack_ms"] = attackMs_;
    obj["release_ms"] = releaseMs_;
    return obj;
}

void AudioExpander::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setThresholdDb(static_cast<float>(obj["threshold_db"].toDouble(-40.0)));
    setRatio(static_cast<float>(obj["ratio"].toDouble(2.0)));
    setAttackMs(static_cast<float>(obj["attack_ms"].toDouble(10.0)));
    setReleaseMs(static_cast<float>(obj["release_ms"].toDouble(150.0)));
}

} // namespace ArtifactCore
