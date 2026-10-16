module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include <QtGlobal>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Vibrato;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateVibratoSample(const float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

// 線形補間つき遅延読み出し。
float interpRead(const std::vector<float>& buf, int writePos, float delaySamples)
{
    const int n = static_cast<int>(buf.size());
    if (n == 0) return 0.0f;
    const float clamped = std::clamp(
        std::isfinite(delaySamples) ? delaySamples : 0.0f,
        0.0f, static_cast<float>(n - 1));
    const int intDelay = static_cast<int>(clamped);
    const float frac = clamped - static_cast<float>(intDelay);
    int idx1 = (writePos - intDelay) % n;
    if (idx1 < 0) idx1 += n;
    int idx2 = idx1 - 1;
    if (idx2 < 0) idx2 += n;
    return buf[static_cast<std::size_t>(idx1)] * (1.0f - frac)
         + buf[static_cast<std::size_t>(idx2)] * frac;
}

} // namespace

AudioVibrato::AudioVibrato() {}

void AudioVibrato::ensureCapacity(int sampleRate)
{
    const int sr = std::clamp(sampleRate, 4000, 192000);
    // base(最大 10ms) + depth(最大 10ms) + 余裕。
    const int need = static_cast<int>(0.024f * sr) + 8;
    if (sr == builtSampleRate_
        && static_cast<int>(delayBuf_[0].size()) >= need
        && static_cast<int>(delayBuf_[1].size()) >= need) {
        return;
    }
    // コールドパスの bounded 再確保(最大 192kHz で約 18KB/ch)。
    delayBuf_[0].assign(static_cast<std::size_t>(need), 0.0f);
    delayBuf_[1].assign(static_cast<std::size_t>(need), 0.0f);
    writePos_[0] = 0;
    writePos_[1] = 0;
    builtSampleRate_ = sr;
}

qint64 AudioVibrato::latencySamples() const
{
    const int sr = builtSampleRate_ > 0 ? builtSampleRate_ : 44100;
    const double samples = (static_cast<double>(finiteOr(baseMs_, 5.0f))
        + static_cast<double>(finiteOr(depthMs_, 2.0f))) * 0.001 * sr;
    if (samples >= static_cast<double>(std::numeric_limits<qint64>::max())) {
        return std::numeric_limits<qint64>::max();
    }
    return static_cast<qint64>(std::max(0.0, samples));
}

void AudioVibrato::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float rate = std::clamp(finiteOr(rate_, 6.0f), 1.0f, 15.0f);
    const float depthMs = std::clamp(finiteOr(depthMs_, 2.0f), 0.0f, 10.0f);
    const float baseMs = std::clamp(finiteOr(baseMs_, 5.0f), 3.0f, 10.0f);
    const float mix = std::clamp(finiteOr(mix_, 1.0f), 0.0f, 1.0f);

    ensureCapacity(sampleRate);
    if (mix <= 0.0f) return;

    const float sr = static_cast<float>(builtSampleRate_ > 0 ? builtSampleRate_ : sampleRate);
    constexpr float kTwoPi = 6.2831853f;

    const int chCount = std::min(channels, 2);
    for (int ch = 0; ch < chCount; ++ch) {
        if (ch >= segment.channelData.size()) break;
        const int samples = std::min(
            frames, static_cast<int>(segment.channelData[ch].size()));
        if (samples <= 0) continue;
        float* data = segment.channelData[ch].data();
        // Rch は逆相で揺らすとステレオ感が出る。
        const float phaseOffset = (ch == 1) ? 3.14159265f : 0.0f;

        for (int i = 0; i < samples; ++i) {
            const float input = std::isfinite(data[i]) ? data[i] : 0.0f;
            if (static_cast<int>(delayBuf_[ch].size()) > 0) {
                delayBuf_[ch][static_cast<std::size_t>(writePos_[ch])] = input;
                writePos_[ch] = (writePos_[ch] + 1)
                    % static_cast<int>(delayBuf_[ch].size());
            }
            const float delayMs = baseMs
                + depthMs * std::sin(lfoPhase_ + phaseOffset);
            const float wet = interpRead(
                delayBuf_[ch], writePos_[ch], std::max(0.0f, delayMs) * 0.001f * sr);
            data[i] = saturateVibratoSample(input * (1.0f - mix) + wet * mix);

            if (ch == 0) {
                lfoPhase_ += kTwoPi * rate / sr;
                if (lfoPhase_ >= kTwoPi) lfoPhase_ -= kTwoPi;
            }
        }
    }
}

std::vector<EffectParameter> AudioVibrato::getParameters() const
{
    return {
        {"rate", "Rate (Hz)", 1.0f, 15.0f, 6.0f, rate_},
        {"depth_ms", "Depth (ms)", 0.0f, 10.0f, 2.0f, depthMs_},
        {"base_ms", "Base Delay (ms)", 3.0f, 10.0f, 5.0f, baseMs_},
        {"mix", "Mix", 0.0f, 1.0f, 1.0f, mix_}
    };
}

void AudioVibrato::setParameterValue(const String& id, float value)
{
    if (id == "rate") setRate(value);
    else if (id == "depth_ms") setDepthMs(value);
    else if (id == "base_ms") setBaseMs(value);
    else if (id == "mix") setMix(value);
}

float AudioVibrato::getParameterValue(const String& id) const
{
    if (id == "rate") return rate_;
    if (id == "depth_ms") return depthMs_;
    if (id == "base_ms") return baseMs_;
    if (id == "mix") return mix_;
    return 0.0f;
}

QJsonObject AudioVibrato::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("vibrato");
    obj["rate"] = rate_;
    obj["depth_ms"] = depthMs_;
    obj["base_ms"] = baseMs_;
    obj["mix"] = mix_;
    return obj;
}

void AudioVibrato::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setRate(static_cast<float>(obj["rate"].toDouble(6.0)));
    setDepthMs(static_cast<float>(obj["depth_ms"].toDouble(2.0)));
    setBaseMs(static_cast<float>(obj["base_ms"].toDouble(5.0)));
    setMix(static_cast<float>(obj["mix"].toDouble(1.0)));
}

} // namespace ArtifactCore
