module;
#include <algorithm>
#include <cmath>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Chorus;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioChorus::AudioChorus() {}

void AudioChorus::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0) return;

    const int maxDelay = static_cast<int>(baseDelayMs_ * sampleRate / 1000.0) + 1;
    int needed = frames + maxDelay + 64;
    if (delayBufSize_ < needed) {
        delayBuffer_.resize(needed, 0.0f);
        delayBufSize_ = needed;
    }

    for (int ch = 0; ch < std::min(channels, 2); ++ch) {
        if (ch >= segment.channelData.size()) break;

        float* inData = segment.channelData[ch].data();

        for (int i = 0; i < frames; ++i) {
            delayBuffer_[writePos_] = inData[i];

            float lfo = std::sin(2.0f * 3.14159265f * rate_ * i / sampleRate);
            int delaySamples = static_cast<int>(baseDelayMs_ * sampleRate / 1000.0f)
                             + static_cast<int>(lfo * depth_ * maxDelay);
            delaySamples = std::clamp(delaySamples, 1, needed - 1);

            int readPos = (writePos_ - delaySamples + needed) % needed;
            float delayed = delayBuffer_[readPos];

            inData[i] = inData[i] * 0.7f + delayed * 0.3f;

            writePos_ = (writePos_ + 1) % needed;
        }
    }
}

std::vector<EffectParameter> AudioChorus::getParameters() const {
    return {
        {"rate", "Rate (Hz)", 0.1f, 10.0f, 1.5f, rate_},
        {"depth", "Depth", 0.0f, 1.0f, 0.5f, depth_},
        {"feedback", "Feedback", -0.99f, 0.99f, 0.3f, feedback_},
        {"delay_ms", "Delay (ms)", 1.0f, 50.0f, 20.0f, baseDelayMs_}
    };
}

void AudioChorus::setParameterValue(const std::string& id, float value) {
    if (id == "rate") rate_ = value;
    else if (id == "depth") depth_ = value;
    else if (id == "feedback") feedback_ = value;
    else if (id == "delay_ms") baseDelayMs_ = value;
}

float AudioChorus::getParameterValue(const std::string& id) const {
    if (id == "rate") return rate_;
    if (id == "depth") return depth_;
    if (id == "feedback") return feedback_;
    if (id == "delay_ms") return baseDelayMs_;
    return 0.0f;
}

QJsonObject AudioChorus::toJson() const {
    QJsonObject obj = AudioEffect::toJson();
    obj["rate"] = rate_;
    obj["depth"] = depth_;
    obj["feedback"] = feedback_;
    obj["delay_ms"] = baseDelayMs_;
    obj["mode"] = (mode_ == Mode::Flanger) ? "flanger" : "chorus";
    return obj;
}

void AudioChorus::fromJson(const QJsonObject& obj) {
    AudioEffect::fromJson(obj);
    rate_ = obj["rate"].toDouble(1.5);
    depth_ = obj["depth"].toDouble(0.5);
    feedback_ = obj["feedback"].toDouble(0.3);
    baseDelayMs_ = obj["delay_ms"].toDouble(20.0);
    mode_ = (obj["mode"].toString() == "flanger") ? Mode::Flanger : Mode::Chorus;
}

} // namespace ArtifactCore