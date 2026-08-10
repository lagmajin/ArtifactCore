module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Tone;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float sanitizeToneSample(float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

}

AudioTone::AudioTone() : phase_(0.0f) {}

void AudioTone::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const float sampleRate = static_cast<float>(segment.sampleRate);
    if (channels == 0 || frames == 0 || sampleRate <= 0.0f) return;

    const float frequency = std::clamp(
        std::isfinite(frequency_) ? frequency_ : 0.0f, 0.0f, 96000.0f);
    const float amplitude = std::isfinite(amplitude_) ? amplitude_ : 0.0f;
    const int waveType = static_cast<int>(waveType_);

    for (int ch = 0; ch < std::min(channels, 2); ++ch) {
        if (ch >= segment.channelData.size()) break;
        const int samples = std::min(frames, segment.channelData[ch].size());
        if (samples <= 0) continue;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < samples; ++i) {
            float sample = 0.0f;
            switch (waveType) {
                case WaveType::Sine:
                    sample = std::sin(phase_ * 2.0f * 3.14159265f);
                    break;
                case WaveType::Square:
                    sample = (std::sin(phase_ * 2.0f * 3.14159265f) > 0) ? 1.0f : -1.0f;
                    break;
                case WaveType::Sawtooth:
                    sample = 2.0f * (phase_ - std::floor(phase_ + 0.5f));
                    break;
                case WaveType::Noise:
                    sample = rng_.dist(rng_.rng);
                    break;
                default:
                    sample = 0.0f;
                    break;
            }
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;
            data[i] = sanitizeToneSample(
                input + sanitizeToneSample(sample * amplitude));

            phase_ += frequency / sampleRate;
            phase_ -= std::floor(phase_);
        }
    }
}

std::vector<EffectParameter> AudioTone::getParameters() const {
    return {
        {"frequency", "Frequency (Hz)", 20.0f, 20000.0f, 440.0f, frequency_},
        {"amplitude", "Amplitude", 0.0f, 1.0f, 0.2f, amplitude_},
        {"wave_type", "Wave Type", 0.0f, 3.0f, 0.0f, static_cast<float>(waveType_)}
    };
}

void AudioTone::setParameterValue(const String& id, float value) {
    if (id == "frequency") setFrequency(value);
    else if (id == "amplitude") setAmplitude(value);
    else if (id == "wave_type" && std::isfinite(value)) {
        const int waveType = static_cast<int>(std::clamp(value, 0.0f, 3.0f));
        waveType_ = static_cast<WaveType>(waveType);
    }
}

float AudioTone::getParameterValue(const String& id) const {
    if (id == "frequency") return frequency_;
    if (id == "amplitude") return amplitude_;
    if (id == "wave_type") return static_cast<float>(waveType_);
    return 0.0f;
}

QJsonObject AudioTone::toJson() const {
    QJsonObject obj = AudioEffect::toJson();
    obj["frequency"] = frequency_;
    obj["amplitude"] = amplitude_;
    obj["wave_type"] = static_cast<int>(waveType_);
    return obj;
}

void AudioTone::fromJson(const QJsonObject& obj) {
    AudioEffect::fromJson(obj);
    setFrequency(static_cast<float>(obj["frequency"].toDouble(440.0)));
    setAmplitude(static_cast<float>(obj["amplitude"].toDouble(0.2)));
    const int waveType = std::clamp(obj["wave_type"].toInt(0), 0, 3);
    waveType_ = static_cast<WaveType>(waveType);
}

} // namespace ArtifactCore
