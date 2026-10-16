module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include <QtGlobal>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.SpringReverb;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturateSpringSample(const float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

inline float ringRead(const std::vector<float>& buf, int writePos, int delaySamples)
{
    const int n = static_cast<int>(buf.size());
    if (n == 0) return 0.0f;
    int idx = (writePos - delaySamples) % n;
    if (idx < 0) idx += n;
    return buf[static_cast<std::size_t>(idx)];
}

inline void ringWrite(std::vector<float>& buf, int& writePos, float value)
{
    const int n = static_cast<int>(buf.size());
    if (n == 0) return;
    buf[static_cast<std::size_t>(writePos)] = value;
    writePos = (writePos + 1) % n;
}

// Core の AllPassFilter::process と同じ式の整数遅延版。
inline float allpassStep(std::vector<float>& buf, int& writePos,
                         int delaySamples, float input, float feedback)
{
    const float delayed = ringRead(buf, writePos, delaySamples);
    const float toDelay = saturateSpringSample(input + delayed * feedback);
    ringWrite(buf, writePos, toDelay);
    return saturateSpringSample(
        -input + toDelay - toDelay * feedback + delayed);
}

} // namespace

AudioSpringReverb::AudioSpringReverb() {}

void AudioSpringReverb::ensureCapacity(int sampleRate)
{
    const int sr = std::clamp(sampleRate, 4000, 192000);
    const float tension = std::clamp(finiteOr(tension_, 1.0f), 0.5f, 1.5f);

    int needSeries[6];
    for (int i = 0; i < 6; ++i) {
        needSeries[i] = std::max(
            8, static_cast<int>(kSeriesRefMs[i] * tension * sr / 1000.0f)) + 4;
    }
    int needLoop[2];
    for (int i = 0; i < 2; ++i) {
        needLoop[i] = std::max(
            16, static_cast<int>(kLoopRefMs[i] * tension * sr / 1000.0f)) + 8;
    }

    bool rebuild = (sr != builtSampleRate_) || (tension != builtTension_);
    if (!rebuild) {
        for (int i = 0; i < 6 && !rebuild; ++i) {
            if (static_cast<int>(seriesBuf_[i].size()) < needSeries[i]) rebuild = true;
        }
        for (int i = 0; i < 2 && !rebuild; ++i) {
            if (static_cast<int>(loopBuf_[i].size()) < needLoop[i]) rebuild = true;
        }
    }
    if (!rebuild) return;

    // コールドパスの bounded 再確保(最大 192kHz で約 100KB)。
    for (int i = 0; i < 6; ++i) {
        seriesBuf_[i].assign(static_cast<std::size_t>(needSeries[i]), 0.0f);
        seriesPos_[i] = 0;
    }
    for (int i = 0; i < 2; ++i) {
        loopBuf_[i].assign(static_cast<std::size_t>(needLoop[i]), 0.0f);
        loopPos_[i] = 0;
    }
    hpState_ = 0.0f;
    dampState_[0] = 0.0f;
    dampState_[1] = 0.0f;
    loopAccum_[0] = 0.0f;
    loopAccum_[1] = 0.0f;
    builtSampleRate_ = sr;
    builtTension_ = tension;
}

qint64 AudioSpringReverb::tailSamples() const
{
    const float decay = std::clamp(
        std::isfinite(decay_) ? decay_ : 0.8f, 0.0f, 0.99f);
    if (decay <= 0.0f) return 0;
    if (decay >= 1.0f) return std::numeric_limits<qint64>::max();
    const int sr = builtSampleRate_ > 0 ? builtSampleRate_ : 44100;
    const double tension = std::clamp(
        std::isfinite(tension_) ? tension_ : 1.0, 0.5, 1.5);
    // 最長ループが支配的。
    const double loopSamples = static_cast<double>(kLoopRefMs[1]) * tension
        * static_cast<double>(sr) / 1000.0;
    if (!(loopSamples > 0.0)) return 0;
    const double repeats = std::log(0.001) / std::log(static_cast<double>(decay));
    if (!(repeats > 0.0) || !std::isfinite(repeats)) return 0;
    const double tail = loopSamples * repeats;
    if (tail >= static_cast<double>(std::numeric_limits<qint64>::max())) {
        return std::numeric_limits<qint64>::max();
    }
    return static_cast<qint64>(tail);
}

void AudioSpringReverb::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float decay = std::clamp(finiteOr(decay_, 0.8f), 0.0f, 0.99f);
    const float brightness = std::clamp(finiteOr(brightness_, 0.5f), 0.0f, 1.0f);
    const float mix = std::clamp(finiteOr(mix_, 0.3f), 0.0f, 1.0f);

    ensureCapacity(sampleRate);
    if (mix <= 0.0f) return;

    const int sr = builtSampleRate_ > 0 ? builtSampleRate_ : sampleRate;
    const float tension = builtTension_ > 0.0f ? builtTension_ : 1.0f;
    // brightness_ が高いほど高域を残す(2kHz -> 16kHz)。
    const float dampFreq = 2000.0f + brightness * 14000.0f;
    const float dampAlpha = std::clamp(6.2831853f * dampFreq / sr, 0.001f, 0.99f);
    // 入力ハイパス 300Hz (ぼやけ防止)。
    const float hpAlpha = std::clamp(6.2831853f * 300.0f / sr, 0.001f, 0.99f);

    const bool stereo = channels > 1
        && segment.channelData.size() > 1
        && !segment.channelData[1].isEmpty();
    float* ch0 = segment.channelData[0].data();
    float* ch1 = stereo ? segment.channelData[1].data() : nullptr;
    const int samples0 = std::min(
        frames, static_cast<int>(segment.channelData[0].size()));
    const int samples = (ch1 != nullptr) ? std::min(samples0, std::min(
        frames, static_cast<int>(segment.channelData[1].size()))) : samples0;
    if (samples <= 0) return;

    int seriesDelay[6];
    for (int i = 0; i < 6; ++i) {
        seriesDelay[i] = std::max(
            1, static_cast<int>(kSeriesRefMs[i] * tension * sr / 1000.0f));
    }
    int loopDelay[2];
    for (int i = 0; i < 2; ++i) {
        loopDelay[i] = std::max(
            2, static_cast<int>(kLoopRefMs[i] * tension * sr / 1000.0f));
    }

    for (int i = 0; i < samples; ++i) {
        const float inL = std::isfinite(ch0[i]) ? ch0[i] : 0.0f;
        const float inR = (ch1 != nullptr && std::isfinite(ch1[i])) ? ch1[i] : inL;
        const float monoIn = saturateSpringSample((inL + inR) * 0.5f);

        // 入力ハイパス(1 ポール)。
        hpState_ = saturateSpringSample(hpState_ + hpAlpha * (monoIn - hpState_));
        float d = saturateSpringSample(monoIn - hpState_);

        // 6 直列オールパス(スプリングの分散近似)。
        static constexpr float kSeriesFb[6] =
            {0.5f, -0.5f, 0.5f, -0.5f, 0.5f, -0.5f};
        for (int s = 0; s < 6; ++s) {
            d = allpassStep(seriesBuf_[s], seriesPos_[s], seriesDelay[s],
                            d, kSeriesFb[s]);
        }

        // 2 並列フィードバックループ。
        float wet[2];
        for (int t = 0; t < 2; ++t) {
            const float loopIn = saturateSpringSample(
                d * 0.5f + loopAccum_[t] * decay);
            ringWrite(loopBuf_[t], loopPos_[t], loopIn);
            const float raw = ringRead(loopBuf_[t], loopPos_[t], loopDelay[t]);
            dampState_[t] = saturateSpringSample(
                dampState_[t] + dampAlpha * (raw - dampState_[t]));
            wet[t] = saturateSpringSample(
                raw * (1.0f - brightness) + dampState_[t] * brightness);
            loopAccum_[t] = saturateSpringSample(wet[t]);
        }

        const float wetL = saturateSpringSample(wet[0] * decay);
        const float wetR = saturateSpringSample(wet[1] * decay);
        ch0[i] = saturateSpringSample(inL * (1.0f - mix) + wetL * mix);
        if (ch1 != nullptr) {
            ch1[i] = saturateSpringSample(inR * (1.0f - mix) + wetR * mix);
        } else if (channels == 1) {
            ch0[i] = saturateSpringSample(
                inL * (1.0f - mix) + (wetL + wetR) * 0.5f * mix);
        }
    }
}

std::vector<EffectParameter> AudioSpringReverb::getParameters() const
{
    return {
        {"decay", "Decay", 0.0f, 0.99f, 0.8f, decay_},
        {"brightness", "Brightness", 0.0f, 1.0f, 0.5f, brightness_},
        {"tension", "Tension", 0.5f, 1.5f, 1.0f, tension_},
        {"mix", "Mix", 0.0f, 1.0f, 0.3f, mix_}
    };
}

void AudioSpringReverb::setParameterValue(const String& id, float value)
{
    if (id == "decay") setDecay(value);
    else if (id == "brightness") setBrightness(value);
    else if (id == "tension") setTension(value);
    else if (id == "mix") setMix(value);
}

float AudioSpringReverb::getParameterValue(const String& id) const
{
    if (id == "decay") return decay_;
    if (id == "brightness") return brightness_;
    if (id == "tension") return tension_;
    if (id == "mix") return mix_;
    return 0.0f;
}

QJsonObject AudioSpringReverb::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("spring_reverb");
    obj["decay"] = decay_;
    obj["brightness"] = brightness_;
    obj["tension"] = tension_;
    obj["mix"] = mix_;
    return obj;
}

void AudioSpringReverb::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setDecay(static_cast<float>(obj["decay"].toDouble(0.8)));
    setBrightness(static_cast<float>(obj["brightness"].toDouble(0.5)));
    setTension(static_cast<float>(obj["tension"].toDouble(1.0)));
    setMix(static_cast<float>(obj["mix"].toDouble(0.3)));
}

} // namespace ArtifactCore
