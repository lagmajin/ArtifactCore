module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Delay;


import Audio.Effect;
import Audio.Segment;
import Audio.RingBuffer;

namespace ArtifactCore {

namespace {

float saturateDelaySample(const float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

}

AudioDelay::AudioDelay() {}

void AudioDelay::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const double requestedDelaySamples =
        std::isfinite(delayTimeMs_)
            ? static_cast<double>(delayTimeMs_) * sampleRate / 1000.0
            : 0.0;
    const int delaySamples = static_cast<int>(std::clamp(
        requestedDelaySamples, 0.0, 47999.0));
    const float feedback = std::clamp(
        std::isfinite(feedback_) ? feedback_ : 0.0f, 0.0f, 0.99f);
    const float mix = std::clamp(
        std::isfinite(mix_) ? mix_ : 0.0f, 0.0f, 1.0f);

    // Ensure per-instance buffers sized correctly
    int needed = std::max(1, std::min(delaySamples * 2, 48000));
    if (static_cast<int>(delayBuffers_[0].size()) < needed) {
        for (auto& buf : delayBuffers_) buf.resize(needed);
    }

    for (int ch = 0; ch < std::min(channels, 2); ++ch) {
        if (ch >= segment.channelData.size()) break;
        if (ch >= static_cast<int>(delayBuffers_.size())) break;

        const int samples = std::min(frames, segment.channelData[ch].size());
        if (samples <= 0) continue;
        float* inData = segment.channelData[ch].data();
        auto& buf = delayBuffers_[ch];
        int bufSize = static_cast<int>(buf.size());
        const float wet = mix;

        for (int i = 0; i < samples; ++i) {
            int idx = i % bufSize;
            const float delayed = (idx + delaySamples < bufSize)
                ? saturateDelaySample(buf[(idx + delaySamples) % bufSize])
                : 0.0f;
            const float input = std::isfinite(inData[i]) ? inData[i] : 0.0f;
            const float out = saturateDelaySample(input + delayed * feedback);
            buf[idx] = out;
            inData[i] = saturateDelaySample(
                input * (1.0f - wet) + delayed * wet);
        }
    }
}

std::vector<EffectParameter> AudioDelay::getParameters() const {
    return {
        {"delay_ms", "Delay (ms)", 1.0f, 2000.0f, 300.0f, delayTimeMs_},
        {"feedback", "Feedback", 0.0f, 0.99f, 0.3f, feedback_},
        {"mix", "Mix", 0.0f, 1.0f, 0.4f, mix_}
    };
}

void AudioDelay::setParameterValue(const String& id, float value) {
    if (!std::isfinite(value)) return;
    if (id == "delay_ms") delayTimeMs_ = value;
    else if (id == "feedback") feedback_ = value;
    else if (id == "mix") mix_ = value;
}

float AudioDelay::getParameterValue(const String& id) const {
    if (id == "delay_ms") return delayTimeMs_;
    if (id == "feedback") return feedback_;
    if (id == "mix") return mix_;
    return 0.0f;
}

QJsonObject AudioDelay::toJson() const {
    QJsonObject obj = AudioEffect::toJson();
    obj["delay_ms"] = delayTimeMs_;
    obj["feedback"] = feedback_;
    obj["mix"] = mix_;
    return obj;
}

void AudioDelay::fromJson(const QJsonObject& obj) {
    AudioEffect::fromJson(obj);
    setDelayTimeMs(static_cast<float>(obj["delay_ms"].toDouble(300.0)));
    setFeedback(static_cast<float>(obj["feedback"].toDouble(0.3)));
    setMix(static_cast<float>(obj["mix"].toDouble(0.4)));
}

} // namespace ArtifactCore
