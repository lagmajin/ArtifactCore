module;
#include <algorithm>
#include <cmath>
#include <QJsonArray>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.ParametricEQ;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioParametricEQ::AudioParametricEQ() {
    bands_.resize(4);  // デフォルト4バンド
    bands_[0] = {100.0f, 0.0f, 1.0f, true};   // Low
    bands_[1] = {500.0f, 0.0f, 1.0f, true};   // Low-Mid
    bands_[2] = {2000.0f, 0.0f, 1.0f, true};  // High-Mid
    bands_[3] = {8000.0f, 0.0f, 1.0f, true};  // High
}

void AudioParametricEQ::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const float sampleRate = std::max(1.0f, static_cast<float>(segment.sampleRate));

    if (channels == 0 || frames == 0) return;

    const size_t stateCount = static_cast<size_t>(channels) * bands_.size() * 4;
    if (delayedState_.size() != stateCount) {
        delayedState_.assign(stateCount, 0.0f);
    }

    for (int ch = 0; ch < channels; ++ch) {
        if (ch >= segment.channelData.size()) break;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < frames; ++i) {
            float sample = data[i];
            for (size_t bandIndex = 0; bandIndex < bands_.size(); ++bandIndex) {
                const auto& band = bands_[bandIndex];
                if (!band.enabled) continue;

                const float frequency = std::clamp(band.frequency, 1.0f,
                                                   std::max(1.0f, sampleRate * 0.49f));
                const float q = std::clamp(band.qFactor, 0.1f, 10.0f);
                const float amplitude = std::pow(10.0f, band.gainDb / 40.0f);
                const float omega = 2.0f * 3.14159265358979323846f * frequency / sampleRate;
                const float alpha = std::sin(omega) / (2.0f * q);
                const float cosOmega = std::cos(omega);
                const float b0 = 1.0f + alpha * amplitude;
                const float b1 = -2.0f * cosOmega;
                const float b2 = 1.0f - alpha * amplitude;
                const float a0 = 1.0f + alpha / amplitude;
                const float a1 = -2.0f * cosOmega;
                const float a2 = 1.0f - alpha / amplitude;
                const float normalizedB0 = b0 / a0;
                const float normalizedB1 = b1 / a0;
                const float normalizedB2 = b2 / a0;
                const float normalizedA1 = a1 / a0;
                const float normalizedA2 = a2 / a0;
                const size_t state = (static_cast<size_t>(ch) * bands_.size() + bandIndex) * 4;
                const float x1 = delayedState_[state];
                const float x2 = delayedState_[state + 1];
                const float y1 = delayedState_[state + 2];
                const float y2 = delayedState_[state + 3];
                const float filtered = normalizedB0 * sample + normalizedB1 * x1 + normalizedB2 * x2
                                     - normalizedA1 * y1 - normalizedA2 * y2;
                delayedState_[state] = sample;
                delayedState_[state + 1] = x1;
                delayedState_[state + 2] = filtered;
                delayedState_[state + 3] = y1;
                sample = std::isfinite(filtered) ? filtered : sample;
            }
            data[i] = sample;
        }
    }
}

std::vector<EffectParameter> AudioParametricEQ::getParameters() const {
    std::vector<EffectParameter> parameters;
    parameters.reserve(bands_.size() * 3);
    for (size_t index = 0; index < bands_.size(); ++index) {
        const auto& band = bands_[index];
        const std::string prefix = "band" + std::to_string(index) + ".";
        parameters.push_back({String(prefix + "frequency"), String(prefix + "Frequency"), 1.0f, 24000.0f, band.frequency, band.frequency});
        parameters.push_back({String(prefix + "gainDb"), String(prefix + "Gain (dB)"), -48.0f, 48.0f, band.gainDb, band.gainDb});
        parameters.push_back({String(prefix + "qFactor"), String(prefix + "Q"), 0.1f, 10.0f, 1.0f, band.qFactor});
    }
    return parameters;
}

void AudioParametricEQ::setParameterValue(const String& id, float value) {
    const std::string key = toStdString(id);
    const std::string prefix = "band";
    if (key.rfind(prefix, 0) != 0) return;
    const size_t separator = key.find('.');
    if (separator == std::string::npos) return;
    int index = -1;
    try {
        index = std::stoi(key.substr(prefix.size(), separator - prefix.size()));
    } catch (...) {
        return;
    }
    if (index < 0 || index >= static_cast<int>(bands_.size())) return;
    auto& band = bands_[static_cast<size_t>(index)];
    const std::string property = key.substr(separator + 1);
    if (property == "frequency") band.frequency = std::clamp(value, 1.0f, 24000.0f);
    else if (property == "gainDb") band.gainDb = std::clamp(value, -48.0f, 48.0f);
    else if (property == "qFactor") band.qFactor = std::clamp(value, 0.1f, 10.0f);
}

QJsonObject AudioParametricEQ::toJson() const {
    QJsonObject object = AudioEffect::toJson();
    QJsonArray bands;
    for (const auto& band : bands_) {
        QJsonObject bandObject;
        bandObject["frequency"] = band.frequency;
        bandObject["gainDb"] = band.gainDb;
        bandObject["qFactor"] = band.qFactor;
        bandObject["enabled"] = band.enabled;
        bands.append(bandObject);
    }
    object["bands"] = bands;
    return object;
}

void AudioParametricEQ::fromJson(const QJsonObject& object) {
    AudioEffect::fromJson(object);
    const QJsonArray bands = object.value("bands").toArray();
    if (bands.isEmpty()) return;
    bands_.clear();
    bands_.reserve(bands.size());
    for (const auto& value : bands) {
        const QJsonObject bandObject = value.toObject();
        Band band;
        band.frequency = std::clamp(static_cast<float>(bandObject.value("frequency").toDouble(1000.0)), 1.0f, 24000.0f);
        band.gainDb = std::clamp(static_cast<float>(bandObject.value("gainDb").toDouble(0.0)), -48.0f, 48.0f);
        band.qFactor = std::clamp(static_cast<float>(bandObject.value("qFactor").toDouble(1.0)), 0.1f, 10.0f);
        band.enabled = bandObject.value("enabled").toBool(true);
        if (!std::isfinite(band.frequency)) band.frequency = 1000.0f;
        if (!std::isfinite(band.gainDb)) band.gainDb = 0.0f;
        if (!std::isfinite(band.qFactor)) band.qFactor = 1.0f;
        bands_.push_back(band);
    }
    delayedState_.clear();
}

} // namespace ArtifactCore
