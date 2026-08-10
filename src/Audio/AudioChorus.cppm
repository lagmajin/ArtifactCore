module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Chorus;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateChorusSample(const float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

}

AudioChorus::AudioChorus() {}

void AudioChorus::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float baseDelayMs = std::clamp(
        std::isfinite(baseDelayMs_) ? baseDelayMs_ : 20.0f, 0.0f, 50.0f);
    const float rate = std::clamp(
        std::isfinite(rate_) ? rate_ : 1.5f, 0.0f, 20.0f);
    const float depth = std::clamp(
        std::isfinite(depth_) ? depth_ : 0.5f, 0.0f, 1.0f);
    const double requestedMaxDelay =
        static_cast<double>(baseDelayMs) * sampleRate / 1000.0;
    const int maxDelay = static_cast<int>(std::clamp(
        requestedMaxDelay, 0.0, 47999.0)) + 1;
    if (frames > std::numeric_limits<int>::max() - maxDelay - 64) {
        return;
    }
    int needed = frames + maxDelay + 64;
    if (needed > std::numeric_limits<int>::max() / 2) {
        return;
    }
    if (delayBufSize_ < needed) {
        delayBuffer_.assign(static_cast<std::size_t>(needed) * 2, 0.0f);
        delayBufSize_ = needed;
        writePositions_[0] = 0;
        writePositions_[1] = 0;
    }

    for (int ch = 0; ch < std::min(channels, 2); ++ch) {
        if (ch >= segment.channelData.size()) break;

        const int samples = std::min(frames, segment.channelData[ch].size());
        if (samples <= 0) continue;
        float* inData = segment.channelData[ch].data();
        int& writePos = writePositions_[ch];
        writePos %= needed;
        const std::size_t channelOffset = static_cast<std::size_t>(ch) *
                                          static_cast<std::size_t>(needed);

        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(inData[i]) ? inData[i] : 0.0f;
            delayBuffer_[channelOffset + static_cast<std::size_t>(writePos)] = input;

            float lfo = std::sin(2.0f * 3.14159265f * rate * i / sampleRate);
            int delaySamples = static_cast<int>(baseDelayMs * sampleRate / 1000.0f)
                             + static_cast<int>(lfo * depth * maxDelay);
            delaySamples = std::clamp(delaySamples, 1, needed - 1);

            int readPos = (writePos - delaySamples + needed) % needed;
            const float delayed = saturateChorusSample(
                delayBuffer_[channelOffset + static_cast<std::size_t>(readPos)]);

            inData[i] = saturateChorusSample(input * 0.7f + delayed * 0.3f);

            writePos = (writePos + 1) % needed;
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

void AudioChorus::setParameterValue(const String& id, float value) {
    if (id == "rate") setRate(value);
    else if (id == "depth") setDepth(value);
    else if (id == "feedback") setFeedback(value);
    else if (id == "delay_ms") setDelayMs(value);
}

float AudioChorus::getParameterValue(const String& id) const {
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
    setRate(static_cast<float>(obj["rate"].toDouble(1.5)));
    setDepth(static_cast<float>(obj["depth"].toDouble(0.5)));
    setFeedback(static_cast<float>(obj["feedback"].toDouble(0.3)));
    setDelayMs(static_cast<float>(obj["delay_ms"].toDouble(20.0)));
    mode_ = (obj["mode"].toString() == "flanger") ? Mode::Flanger : Mode::Chorus;
}

} // namespace ArtifactCore
