module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.StereoWidener;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateWidenerSample(const float sample)
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

AudioStereoWidener::AudioStereoWidener() {}

void AudioStereoWidener::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;
    // Mid/Side 処理のためステレオ必須。モノラルは素通しする。
    if (channels < 2 || segment.channelData.size() < 2) return;

    const float width = std::clamp(finiteOr(width_, 1.2f), 0.0f, 2.0f);
    const float bassMonoFreq = std::clamp(finiteOr(bassMonoFreq_, 120.0f), 0.0f, 500.0f);
    const float trimDb = std::clamp(finiteOr(trimDb_, 0.0f), -12.0f, 12.0f);
    const float trim = std::pow(10.0f, trimDb / 20.0f);

    const float sr = static_cast<float>(sampleRate);
    const float alpha = (bassMonoFreq > 0.0f)
        ? std::clamp(6.2831853f * bassMonoFreq / sr, 0.001f, 0.99f)
        : 0.0f;

    const int samples = std::min({frames,
        static_cast<int>(segment.channelData[0].size()),
        static_cast<int>(segment.channelData[1].size())});
    if (samples <= 0) return;
    float* left = segment.channelData[0].data();
    float* right = segment.channelData[1].data();

    for (int i = 0; i < samples; ++i) {
        const float inL = std::isfinite(left[i]) ? left[i] : 0.0f;
        const float inR = std::isfinite(right[i]) ? right[i] : 0.0f;
        const float mid = (inL + inR) * 0.5f;
        const float side = (inL - inR) * 0.5f;

        float sideLow = 0.0f;
        if (alpha > 0.0f) {
            sideLowState_ = saturateWidenerSample(
                sideLowState_ + alpha * (side - sideLowState_));
            sideLow = sideLowState_;
        }
        // 低域 Side はモノラルのまま、高域 Side だけ幅を掛ける。
        const float wideSide = saturateWidenerSample(
            sideLow + (side - sideLow) * width);
        left[i] = saturateWidenerSample((mid + wideSide) * trim);
        right[i] = saturateWidenerSample((mid - wideSide) * trim);
    }
}

std::vector<EffectParameter> AudioStereoWidener::getParameters() const
{
    return {
        {"width", "Width", 0.0f, 2.0f, 1.2f, width_},
        {"bass_mono_freq", "Bass Mono (Hz, 0=Off)", 0.0f, 500.0f, 120.0f, bassMonoFreq_},
        {"trim_db", "Trim (dB)", -12.0f, 12.0f, 0.0f, trimDb_}
    };
}

void AudioStereoWidener::setParameterValue(const String& id, float value)
{
    if (id == "width") setWidth(value);
    else if (id == "bass_mono_freq") setBassMonoFreq(value);
    else if (id == "trim_db") setTrimDb(value);
}

float AudioStereoWidener::getParameterValue(const String& id) const
{
    if (id == "width") return width_;
    if (id == "bass_mono_freq") return bassMonoFreq_;
    if (id == "trim_db") return trimDb_;
    return 0.0f;
}

QJsonObject AudioStereoWidener::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("stereo_widener");
    obj["width"] = width_;
    obj["bass_mono_freq"] = bassMonoFreq_;
    obj["trim_db"] = trimDb_;
    return obj;
}

void AudioStereoWidener::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setWidth(static_cast<float>(obj["width"].toDouble(1.2)));
    setBassMonoFreq(static_cast<float>(obj["bass_mono_freq"].toDouble(120.0)));
    setTrimDb(static_cast<float>(obj["trim_db"].toDouble(0.0)));
}

} // namespace ArtifactCore
