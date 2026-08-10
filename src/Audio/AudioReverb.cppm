module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Reverb;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateReverbSample(const float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

}

AudioReverb::AudioReverb() {}

void AudioReverb::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    if (channels == 0 || frames == 0) return;

    const float decay = std::clamp(
        std::isfinite(decay_) ? decay_ : 0.5f, 0.0f, 1.0f);
    const float mix = std::clamp(
        std::isfinite(mix_) ? mix_ : 0.3f, 0.0f, 1.0f);

    int combDelays[4] = {151, 307, 613, 1225};
    int maxDelay = 1225;
    const int baseNeeded = std::max(frames, maxDelay);
    if (baseNeeded > std::numeric_limits<int>::max() - 64) {
        return;
    }
    const int needed = baseNeeded + 64;
    if (needed > std::numeric_limits<int>::max() / 4) {
        return;
    }
    int totalComb = needed * 4;
    if (combBufSize_ < totalComb) {
        combBuffers_.assign(totalComb, 0.0f);
        combBufSize_ = totalComb;
    }
    if (static_cast<int>(allpassBuffer_.size()) < needed) {
        allpassBuffer_.assign(needed, 0.0f);
    }

    for (int ch = 0; ch < std::min(channels, 2); ++ch) {
        if (ch >= segment.channelData.size()) break;
        const int samples = std::min(frames, segment.channelData[ch].size());
        if (samples <= 0) continue;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;
            float output = 0.0f;

            for (int c = 0; c < 4; ++c) {
                int idx = i % needed;
                const float bufOut = saturateReverbSample(
                    combBuffers_[c * needed + (idx + combDelays[c]) % needed]);
                combBuffers_[c * needed + idx] = saturateReverbSample(
                    bufOut + input * decay);
                output = saturateReverbSample(output + bufOut);
            }
            output *= 0.25f;

            data[i] = saturateReverbSample(
                input * (1.0f - mix) + output * mix);
        }
    }
}

std::vector<EffectParameter> AudioReverb::getParameters() const {
    return {
        {"decay", "Decay", 0.0f, 1.0f, 0.5f, decay_},
        {"mix", "Mix", 0.0f, 1.0f, 0.3f, mix_},
        {"size", "Size", 0.0f, 1.0f, 0.7f, size_}
    };
}

void AudioReverb::setParameterValue(const String& id, float value) {
    if (id == "decay") setDecay(value);
    else if (id == "mix") setMix(value);
    else if (id == "size") setSize(value);
}

float AudioReverb::getParameterValue(const String& id) const {
    if (id == "decay") return decay_;
    if (id == "mix") return mix_;
    if (id == "size") return size_;
    return 0.0f;
}

QJsonObject AudioReverb::toJson() const {
    QJsonObject obj = AudioEffect::toJson();
    obj["decay"] = decay_;
    obj["mix"] = mix_;
    obj["size"] = size_;
    return obj;
}

void AudioReverb::fromJson(const QJsonObject& obj) {
    AudioEffect::fromJson(obj);
    setDecay(static_cast<float>(obj["decay"].toDouble(0.5)));
    setMix(static_cast<float>(obj["mix"].toDouble(0.3)));
    setSize(static_cast<float>(obj["size"].toDouble(0.7)));
}

} // namespace ArtifactCore
