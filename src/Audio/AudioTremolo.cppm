module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Tremolo;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateTremoloSample(const float sample)
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

AudioTremolo::AudioTremolo() {}

void AudioTremolo::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float rate = std::clamp(finiteOr(rate_, 5.0f), 0.1f, 20.0f);
    const float depth = std::clamp(finiteOr(depth_, 0.8f), 0.0f, 1.0f);
    const float shape = std::clamp(finiteOr(shape_, 0.0f), 0.0f, 1.0f);
    const float width = std::clamp(finiteOr(width_, 0.0f), 0.0f, 1.0f);
    const float mix = std::clamp(finiteOr(mix_, 1.0f), 0.0f, 1.0f);

    if (mix <= 0.0f || depth <= 0.0f) return;

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
        // Rch は width_ に応じて位相をずらす(1.0 で逆相 = Auto-Pan)。
        const float phaseOffset = (ch == 1) ? width_ * kPi : 0.0f;

        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;
            const float sine = std::sin(lfoPhase_ + phaseOffset);
            // shape_: sine(0) -> square(1) へのモーフ。
            const float morphed = (1.0f - shape) * sine
                + shape * std::tanh(3.0f * sine);
            const float gain = (1.0f - depth) + depth * (morphed * 0.5f + 0.5f);
            data[i] = saturateTremoloSample(input * (1.0f - mix) + input * gain * mix);

            if (ch == 0) {
                lfoPhase_ += kTwoPi * rate / sr;
                if (lfoPhase_ >= kTwoPi) lfoPhase_ -= kTwoPi;
            }
        }
    }
}

std::vector<EffectParameter> AudioTremolo::getParameters() const
{
    return {
        {"rate", "Rate (Hz)", 0.1f, 20.0f, 5.0f, rate_},
        {"depth", "Depth", 0.0f, 1.0f, 0.8f, depth_},
        {"shape", "Shape (Sine-Square)", 0.0f, 1.0f, 0.0f, shape_},
        {"width", "Stereo Width (Auto-Pan)", 0.0f, 1.0f, 0.0f, width_},
        {"mix", "Mix", 0.0f, 1.0f, 1.0f, mix_}
    };
}

void AudioTremolo::setParameterValue(const String& id, float value)
{
    if (id == "rate") setRate(value);
    else if (id == "depth") setDepth(value);
    else if (id == "shape") setShape(value);
    else if (id == "width") setWidth(value);
    else if (id == "mix") setMix(value);
}

float AudioTremolo::getParameterValue(const String& id) const
{
    if (id == "rate") return rate_;
    if (id == "depth") return depth_;
    if (id == "shape") return shape_;
    if (id == "width") return width_;
    if (id == "mix") return mix_;
    return 0.0f;
}

QJsonObject AudioTremolo::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("tremolo");
    obj["rate"] = rate_;
    obj["depth"] = depth_;
    obj["shape"] = shape_;
    obj["width"] = width_;
    obj["mix"] = mix_;
    return obj;
}

void AudioTremolo::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setRate(static_cast<float>(obj["rate"].toDouble(5.0)));
    setDepth(static_cast<float>(obj["depth"].toDouble(0.8)));
    setShape(static_cast<float>(obj["shape"].toDouble(0.0)));
    setWidth(static_cast<float>(obj["width"].toDouble(0.0)));
    setMix(static_cast<float>(obj["mix"].toDouble(1.0)));
}

} // namespace ArtifactCore
