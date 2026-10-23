module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.DeEsser;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateDeEsserSample(const float sample)
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

AudioDeEsser::AudioDeEsser() {}

void AudioDeEsser::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float frequency = std::clamp(finiteOr(frequency_, 6500.0f), 2000.0f, 12000.0f);
    const float thresholdDb = std::clamp(finiteOr(thresholdDb_, -30.0f), -60.0f, 0.0f);
    const float reduction = std::clamp(finiteOr(reduction_, 0.6f), 0.0f, 1.0f);

    if (reduction <= 0.0f) return;

    const float sr = static_cast<float>(sampleRate);
    // 1 ポール LP の平滑係数。split と検出で共用する。
    const float alpha = std::clamp(
        6.2831853f * frequency / sr, 0.001f, 0.99f);
    const float thresholdLin = std::pow(10.0f, thresholdDb / 20.0f);
    // 立ち上がり 1ms / 立ち下がり 40ms の検出エンベロープ。
    const float attackCoeff = std::exp(-1.0f / (0.001f * sr));
    const float releaseCoeff = std::exp(-1.0f / (0.04f * sr));

    const int chCount = std::min(channels, 2);
    for (int ch = 0; ch < chCount; ++ch) {
        if (ch >= segment.channelData.size()) break;
        const int samples = std::min(
            frames, static_cast<int>(segment.channelData[ch].size()));
        if (samples <= 0) continue;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;

            // 帯域分割: 低域は無加工、高域だけ減衰対象。
            splitState_[ch] = saturateDeEsserSample(
                splitState_[ch] + alpha * (input - splitState_[ch]));
            const float highBand = saturateDeEsserSample(input - splitState_[ch]);

            // 検出: 高域の絶対値を平滑して包絡にする。
            detectState_[ch] = saturateDeEsserSample(
                detectState_[ch] + alpha * (std::abs(highBand) - detectState_[ch]));
            const float coeff = (detectState_[ch] > envelope_[ch])
                ? attackCoeff : releaseCoeff;
            envelope_[ch] = saturateDeEsserSample(
                coeff * envelope_[ch] + (1.0f - coeff) * detectState_[ch]);

            // しきい値超過ぶんだけ高域を抑える(最大 12dB スケール)。
            const float over = std::clamp(
                (envelope_[ch] - thresholdLin) / (thresholdLin * 3.0f + 1e-6f),
                0.0f, 1.0f);
            const float hfGain = 1.0f - reduction * over;
            data[i] = saturateDeEsserSample(
                splitState_[ch] + highBand * hfGain);
        }
    }
}

std::vector<EffectParameter> AudioDeEsser::getParameters() const
{
    return {
        {"frequency", "Frequency (Hz)", 2000.0f, 12000.0f, 6500.0f, frequency_},
        {"threshold_db", "Threshold (dB)", -60.0f, 0.0f, -30.0f, thresholdDb_},
        {"reduction", "Reduction", 0.0f, 1.0f, 0.6f, reduction_}
    };
}

void AudioDeEsser::setParameterValue(const String& id, float value)
{
    if (id == "frequency") setFrequency(value);
    else if (id == "threshold_db") setThresholdDb(value);
    else if (id == "reduction") setReduction(value);
}

float AudioDeEsser::getParameterValue(const String& id) const
{
    if (id == "frequency") return frequency_;
    if (id == "threshold_db") return thresholdDb_;
    if (id == "reduction") return reduction_;
    return 0.0f;
}

QJsonObject AudioDeEsser::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("de_esser");
    obj["frequency"] = frequency_;
    obj["threshold_db"] = thresholdDb_;
    obj["reduction"] = reduction_;
    return obj;
}

void AudioDeEsser::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setFrequency(static_cast<float>(obj["frequency"].toDouble(6500.0)));
    setThresholdDb(static_cast<float>(obj["threshold_db"].toDouble(-30.0)));
    setReduction(static_cast<float>(obj["reduction"].toDouble(0.6)));
}

} // namespace ArtifactCore
