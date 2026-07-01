module;
#include <algorithm>
#include <cmath>
#include <random>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Tone;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioTone::AudioTone() : phase_(0.0f) {}

void AudioTone::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const float sampleRate = static_cast<float>(segment.sampleRate);
    if (channels == 0 || frames == 0) return;

    for (int ch = 0; ch < std::min(channels, 2); ++ch) {
        if (ch >= segment.channelData.size()) break;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < frames; ++i) {
            float sample = 0.0f;
            switch (waveType_) {
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
            }
            data[i] += sample * amplitude_;

            phase_ += frequency_ / sampleRate;
            if (phase_ >= 1.0f) phase_ -= 1.0f;
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

void AudioTone::setParameterValue(const std::string& id, float value) {
    if (id == "frequency") frequency_ = value;
    else if (id == "amplitude") amplitude_ = value;
    else if (id == "wave_type") waveType_ = static_cast<WaveType>(static_cast<int>(value));
}

float AudioTone::getParameterValue(const std::string& id) const {
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
    frequency_ = obj["frequency"].toDouble(440.0);
    amplitude_ = obj["amplitude"].toDouble(0.2);
    waveType_ = static_cast<WaveType>(obj["wave_type"].toInt(0));
}

} // namespace ArtifactCore