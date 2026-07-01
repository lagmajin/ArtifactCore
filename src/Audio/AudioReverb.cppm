module;
#include <algorithm>
#include <cmath>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Reverb;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioReverb::AudioReverb() {}

void AudioReverb::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    if (channels == 0 || frames == 0) return;

    int combDelays[4] = {151, 307, 613, 1225};
    int maxDelay = 1225;
    int needed = std::max(frames, maxDelay) + 64;
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
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < frames; ++i) {
            float input = data[i];
            float output = 0.0f;

            for (int c = 0; c < 4; ++c) {
                int idx = i % needed;
                float bufOut = combBuffers_[c * needed + (idx + combDelays[c]) % needed];
                combBuffers_[c * needed + idx] = bufOut + input * decay_;
                output += bufOut;
            }
            output *= 0.25f;

            data[i] = data[i] * (1.0f - mix_) + output * mix_;
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

void AudioReverb::setParameterValue(const std::string& id, float value) {
    if (id == "decay") decay_ = value;
    else if (id == "mix") mix_ = value;
    else if (id == "size") size_ = value;
}

float AudioReverb::getParameterValue(const std::string& id) const {
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
    decay_ = obj["decay"].toDouble(0.5);
    mix_ = obj["mix"].toDouble(0.3);
    size_ = obj["size"].toDouble(0.7);
}

} // namespace ArtifactCore