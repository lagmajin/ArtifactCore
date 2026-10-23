module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Phaser;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturatePhaserSample(const float sample)
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

AudioPhaser::AudioPhaser() {}

void AudioPhaser::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float rate = std::clamp(finiteOr(rate_, 0.5f), 0.05f, 10.0f);
    const float depth = std::clamp(finiteOr(depth_, 0.6f), 0.0f, 1.0f);
    const float center = std::clamp(finiteOr(center_, 800.0f), 200.0f, 4000.0f);
    const float feedback = std::clamp(finiteOr(feedback_, 0.4f), 0.0f, 0.9f);
    const float mix = std::clamp(finiteOr(mix_, 0.5f), 0.0f, 1.0f);
    int stages = static_cast<int>(finiteOr(stages_, 4.0f) + 0.5f);
    stages = std::clamp(stages, 2, 8);

    if (mix <= 0.0f) return;

    const float sr = static_cast<float>(sampleRate);
    constexpr float kTwoPi = 6.2831853f;
    constexpr float kPi = 3.14159265f;

    const int chCount = std::min(channels, 2);
    for (int ch = 0; ch < chCount; ++ch) {
        if (ch >= segment.channelData.size()) break;
        const int samples = std::min(
            frames, static_cast<int>(segment.channelData[ch].size()));
        if (samples <= 0) continue;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;

            // LFO でノッチ中心を上下へ掃引(最大 ±2.2 oct)。
            const float lfo = std::sin(lfoPhase_);
            lfoPhase_ += kTwoPi * rate / sr;
            if (lfoPhase_ >= kTwoPi) lfoPhase_ -= kTwoPi;
            const float fc = std::clamp(
                center * std::pow(2.0f, depth * 2.2f * lfo), 60.0f, 9000.0f);
            const float t = std::tan(kPi * fc / sr);
            const float coeff = std::clamp((1.0f - t) / (1.0f + t), -0.99f, 0.99f);

            // 1 次オールパス DF1: y = -c*x + s; s = x + c*y。
            float v = saturatePhaserSample(input + feedbackState_[ch] * feedback);
            for (int s = 0; s < stages; ++s) {
                const float y = saturatePhaserSample(
                    -coeff * v + stageState_[ch][s]);
                stageState_[ch][s] = saturatePhaserSample(v + coeff * y);
                v = y;
            }
            feedbackState_[ch] = saturatePhaserSample(v);
            data[i] = saturatePhaserSample(input * (1.0f - mix) + v * mix);
        }
    }
}

std::vector<EffectParameter> AudioPhaser::getParameters() const
{
    return {
        {"rate", "Rate (Hz)", 0.05f, 10.0f, 0.5f, rate_},
        {"depth", "Depth", 0.0f, 1.0f, 0.6f, depth_},
        {"center", "Center (Hz)", 200.0f, 4000.0f, 800.0f, center_},
        {"feedback", "Feedback", 0.0f, 0.9f, 0.4f, feedback_},
        {"stages", "Stages", 2.0f, 8.0f, 4.0f, stages_},
        {"mix", "Mix", 0.0f, 1.0f, 0.5f, mix_}
    };
}

void AudioPhaser::setParameterValue(const String& id, float value)
{
    if (id == "rate") setRate(value);
    else if (id == "depth") setDepth(value);
    else if (id == "center") setCenter(value);
    else if (id == "feedback") setFeedback(value);
    else if (id == "stages") setStages(value);
    else if (id == "mix") setMix(value);
}

float AudioPhaser::getParameterValue(const String& id) const
{
    if (id == "rate") return rate_;
    if (id == "depth") return depth_;
    if (id == "center") return center_;
    if (id == "feedback") return feedback_;
    if (id == "stages") return stages_;
    if (id == "mix") return mix_;
    return 0.0f;
}

QJsonObject AudioPhaser::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("phaser");
    obj["rate"] = rate_;
    obj["depth"] = depth_;
    obj["center"] = center_;
    obj["feedback"] = feedback_;
    obj["stages"] = stages_;
    obj["mix"] = mix_;
    return obj;
}

void AudioPhaser::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setRate(static_cast<float>(obj["rate"].toDouble(0.5)));
    setDepth(static_cast<float>(obj["depth"].toDouble(0.6)));
    setCenter(static_cast<float>(obj["center"].toDouble(800.0)));
    setFeedback(static_cast<float>(obj["feedback"].toDouble(0.4)));
    setStages(static_cast<float>(obj["stages"].toDouble(4.0)));
    setMix(static_cast<float>(obj["mix"].toDouble(0.5)));
}

} // namespace ArtifactCore
