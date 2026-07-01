module;
#include <algorithm>
#include <cmath>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Delay;

import Audio.Effect;
import Audio.Segment;
import Audio.RingBuffer;

namespace ArtifactCore {

AudioDelay::AudioDelay() {}

void AudioDelay::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0) return;

    const int delaySamples = static_cast<int>(delayTimeMs_ * sampleRate / 1000.0);

    // Ensure per-instance buffers sized correctly
    int needed = std::min(delaySamples * 2, 48000);
    if (static_cast<int>(delayBuffers_[0].size()) < needed) {
        for (auto& buf : delayBuffers_) buf.resize(needed);
    }

    for (int ch = 0; ch < std::min(channels, 2); ++ch) {
        if (ch >= segment.channelData.size()) break;
        if (ch >= static_cast<int>(delayBuffers_.size())) break;

        float* inData = segment.channelData[ch].data();
        auto& buf = delayBuffers_[ch];
        int bufSize = static_cast<int>(buf.size());
        float dry = mix_;
        float wet = mix_;

        for (int i = 0; i < frames; ++i) {
            int idx = i % bufSize;
            float delayed = (idx + delaySamples < bufSize)
                ? buf[(idx + delaySamples) % bufSize]
                : 0.0f;
            float out = inData[i] + delayed * feedback_;
            buf[idx] = out;
            inData[i] = inData[i] * (1.0f - wet) + delayed * wet;
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

void AudioDelay::setParameterValue(const std::string& id, float value) {
    if (id == "delay_ms") delayTimeMs_ = value;
    else if (id == "feedback") feedback_ = value;
    else if (id == "mix") mix_ = value;
}

float AudioDelay::getParameterValue(const std::string& id) const {
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
    delayTimeMs_ = obj["delay_ms"].toDouble(300.0);
    feedback_ = obj["feedback"].toDouble(0.3);
    mix_ = obj["mix"].toDouble(0.4);
}

} // namespace ArtifactCore