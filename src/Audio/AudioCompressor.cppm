module;
#include <algorithm>
#include <cmath>
#include <QJsonObject>

module Audio.Effect.Compressor;

import Audio.Segment;

namespace ArtifactCore {

AudioCompressor::AudioCompressor() {}

void AudioCompressor::process(AudioSegment& segment, const AudioSegment* sideChain) {
    if (bypass_) return;

    int channels = segment.channelCount();
    int frames = segment.frameCount();
    float sampleRate = static_cast<float>(segment.sampleRate);

    if (sampleRate != lastSampleRate_) {
        envelope_ = 0.0f;
        lastSampleRate_ = sampleRate;
    }

    float attackCoef = std::exp(-1.0f / (attackMs_ * 0.001f * sampleRate));
    float releaseCoef = std::exp(-1.0f / (releaseMs_ * 0.001f * sampleRate));
    float thresholdLinear = std::pow(10.0f, thresholdDb_ / 20.0f);

    for (int i = 0; i < frames; ++i) {
        float detectorVal = 0.0f;

        const AudioSegment* source = (sideChainEnabled_ && sideChain) ? sideChain : &segment;
        int detectorChannels = source->channelCount();

        for (int c = 0; c < detectorChannels; ++c) {
            detectorVal = std::max(detectorVal, std::abs(source->channelData[c][i]));
        }

        float envelope = envelope_;
        if (detectorVal > envelope) {
            envelope = attackCoef * envelope + (1.0f - attackCoef) * detectorVal;
        } else {
            envelope = releaseCoef * envelope + (1.0f - releaseCoef) * detectorVal;
        }
        envelope_ = envelope;

        float gain = 1.0f;
        if (envelope > thresholdLinear) {
            float envDb = 20.0f * std::log10(envelope);
            float overDb = envDb - thresholdDb_;
            float newDb = thresholdDb_ + overDb / ratio_;
            gain = std::pow(10.0f, (newDb - envDb) / 20.0f);
        }

        for (int c = 0; c < channels; ++c) {
            segment.channelData[c][i] *= gain;
        }

        currentGainReduction_ = gain;
    }
}

float AudioCompressor::getGainReduction() const {
    return currentGainReduction_;
}

std::vector<EffectParameter> AudioCompressor::getParameters() const {
    return {
        {"threshold", "Threshold (dB)", -60.0f, 0.0f, -20.0f, thresholdDb_},
        {"ratio", "Ratio", 1.0f, 20.0f, 4.0f, ratio_},
        {"attack_ms", "Attack (ms)", 0.1f, 100.0f, 10.0f, attackMs_},
        {"release_ms", "Release (ms)", 1.0f, 1000.0f, 100.0f, releaseMs_},
        {"sidechain", "Sidechain", 0.0f, 1.0f, 0.0f, sideChainEnabled_ ? 1.0f : 0.0f}
    };
}

void AudioCompressor::setParameterValue(const std::string& id, float value) {
    if (id == "threshold") thresholdDb_ = value;
    else if (id == "ratio") ratio_ = value;
    else if (id == "attack_ms") attackMs_ = value;
    else if (id == "release_ms") releaseMs_ = value;
    else if (id == "sidechain") sideChainEnabled_ = (value > 0.5f);
}

float AudioCompressor::getParameterValue(const std::string& id) const {
    if (id == "threshold") return thresholdDb_;
    if (id == "ratio") return ratio_;
    if (id == "attack_ms") return attackMs_;
    if (id == "release_ms") return releaseMs_;
    if (id == "sidechain") return sideChainEnabled_ ? 1.0f : 0.0f;
    return 0.0f;
}

QJsonObject AudioCompressor::toJson() const {
    QJsonObject obj = AudioEffect::toJson();
    obj["threshold"] = thresholdDb_;
    obj["ratio"] = ratio_;
    obj["attack_ms"] = attackMs_;
    obj["release_ms"] = releaseMs_;
    obj["sidechain"] = sideChainEnabled_;
    return obj;
}

void AudioCompressor::fromJson(const QJsonObject& obj) {
    AudioEffect::fromJson(obj);
    thresholdDb_ = obj["threshold"].toDouble(-20.0);
    ratio_ = obj["ratio"].toDouble(4.0);
    attackMs_ = obj["attack_ms"].toDouble(10.0);
    releaseMs_ = obj["release_ms"].toDouble(100.0);
    sideChainEnabled_ = obj["sidechain"].toBool(false);
}

} // namespace ArtifactCore