module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include <QtGlobal>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.PlateReverb;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

// 非有限は無音化、オーバーフローは最大値に飽和させる(姉妹版と同契約)。
float saturatePlateSample(const float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

// 整数遅延リングの読み出し。writePos は次回書き込み位置。
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

// Schroeder オールパス 1 段。Core の AllPassFilter::process と
// 同じ式の整数遅延版 (|feedback| < 1 で安定)。
inline float allpassStep(std::vector<float>& buf, int& writePos,
                         int delaySamples, float input, float feedback)
{
    const float delayed = ringRead(buf, writePos, delaySamples);
    const float toDelay = saturatePlateSample(input + delayed * feedback);
    ringWrite(buf, writePos, toDelay);
    return saturatePlateSample(
        -input + toDelay - toDelay * feedback + delayed);
}

} // namespace

AudioPlateReverb::AudioPlateReverb() {}

void AudioPlateReverb::ensureCapacity(int sampleRate)
{
    const int sr = std::clamp(sampleRate, 4000, 192000);
    const float size = std::clamp(finiteOr(size_, 1.0f), 0.5f, 2.0f);
    const float factor = size * (static_cast<float>(sr) / kRefSampleRate);

    auto need = [&](float refSamples, int margin) {
        const int delay = std::max(8, static_cast<int>(refSamples * factor));
        return delay + margin;
    };

    const int preDelaySamples = std::max(
        0, static_cast<int>(finiteOr(preDelayMs_, 10.0f) * 0.001f * sr));
    const int needPre = preDelaySamples + 8;
    int needDiff[4];
    for (int i = 0; i < 4; ++i) needDiff[i] = need(kInDiffRef[i], 8);
    int needTank[2];
    for (int i = 0; i < 2; ++i) needTank[i] = need(kTankRef[i], 24);
    int needAp[2];
    for (int i = 0; i < 2; ++i) needAp[i] = need(kTankApRef[i], 8);

    bool rebuild = (sr != builtSampleRate_) || (size != builtSize_);
    if (!rebuild) {
        if (static_cast<int>(preBuf_.size()) < needPre) rebuild = true;
        for (int i = 0; i < 4 && !rebuild; ++i) {
            if (static_cast<int>(diffBuf_[i].size()) < needDiff[i]) rebuild = true;
        }
        for (int i = 0; i < 2 && !rebuild; ++i) {
            if (static_cast<int>(tankBuf_[i].size()) < needTank[i]) rebuild = true;
            if (static_cast<int>(tankApBuf_[i].size()) < needAp[i]) rebuild = true;
        }
    }
    if (!rebuild) return;

    // コールドパスの bounded 再確保。合計は最大でも
    // (100ms + diffusion + tank + AP) * 192kHz < 200KB 程度。
    preBuf_.assign(static_cast<std::size_t>(needPre), 0.0f);
    prePos_ = 0;
    for (int i = 0; i < 4; ++i) {
        diffBuf_[i].assign(static_cast<std::size_t>(needDiff[i]), 0.0f);
        diffPos_[i] = 0;
    }
    for (int i = 0; i < 2; ++i) {
        tankBuf_[i].assign(static_cast<std::size_t>(needTank[i]), 0.0f);
        tankPos_[i] = 0;
        tankApBuf_[i].assign(static_cast<std::size_t>(needAp[i]), 0.0f);
        tankApPos_[i] = 0;
    }
    lfoPhase_[0] = 0.0f;
    lfoPhase_[1] = 0.0f;
    dampState_[0] = 0.0f;
    dampState_[1] = 0.0f;
    tankAccum_[0] = 0.0f;
    tankAccum_[1] = 0.0f;
    builtSampleRate_ = sr;
    builtSize_ = size;
}

void AudioPlateReverb::processPlateSample(float monoIn, float& wetL, float& wetR)
{
    const float sr = static_cast<float>(builtSampleRate_ > 0 ? builtSampleRate_ : 44100);
    const float factor = builtSize_ * (sr / kRefSampleRate);
    const float diffusion = std::clamp(finiteOr(diffusion_, 0.7f), 0.0f, 1.0f);
    const float decay = std::clamp(finiteOr(decay_, 0.75f), 0.0f, 0.99f);
    const float damping = std::clamp(finiteOr(damping_, 0.4f), 0.0f, 1.0f);

    // Pre-delay (Wet 経路のみ)。
    const int preDelaySamples = std::max(
        0, static_cast<int>(finiteOr(preDelayMs_, 10.0f) * 0.001f * sr));
    ringWrite(preBuf_, prePos_, saturatePlateSample(monoIn));
    const float pre = ringRead(preBuf_, prePos_, preDelaySamples);

    // LFO (金属的共振の抑止用。親 ReverbEffect と同じ正弦 2 相)。
    constexpr float kTwoPi = 6.2831853f;
    const float lfo0 = std::sin(lfoPhase_[0]);
    const float lfo1 = std::sin(lfoPhase_[1]);
    lfoPhase_[0] += kTwoPi * modRate_ / sr;
    lfoPhase_[1] += kTwoPi * modRate_ * 1.07f / sr;
    if (lfoPhase_[0] >= kTwoPi) lfoPhase_[0] -= kTwoPi;
    if (lfoPhase_[1] >= kTwoPi) lfoPhase_[1] -= kTwoPi;
    const int modDiff = static_cast<int>(lfo0 * modDepth_ * 2.0f);
    const int modTankL = static_cast<int>(lfo0 * modDepth_ * 6.0f);
    const int modTankR = static_cast<int>(lfo1 * modDepth_ * 6.0f);

    // 入力ディフュージョン 4 直列。
    static constexpr float kDiffScale[4] = {0.75f, 0.75f, 0.625f, 0.625f};
    float d = pre;
    for (int i = 0; i < 4; ++i) {
        const int delay = std::max(
            1, static_cast<int>(kInDiffRef[i] * factor) + modDiff);
        d = allpassStep(diffBuf_[i], diffPos_[i], delay,
                        d, diffusion * kDiffScale[i]);
    }

    // クロスカップルドタンク。循環ゲインは decay_ 1 箇所に集約し、
    // タンク内 AP は拡散専(-0.65 固定)とすることでテール予測を単純化する。
    // decay_ = 0 は Wet 全体を 0 にする(無残響 = 完全ドライ)。
    constexpr float kTankApFb = -0.65f;
    constexpr float kDampUpdate = 0.4f;
    const float tankInL = saturatePlateSample(d + tankAccum_[1] * decay);
    const float tankInR = saturatePlateSample(d + tankAccum_[0] * decay);

    float tankOut[2];
    const int mods[2] = {modTankL, modTankR};
    for (int t = 0; t < 2; ++t) {
        ringWrite(tankBuf_[t], tankPos_[t],
                  t == 0 ? tankInL : tankInR);
        const int delay = std::max(
            1, static_cast<int>(kTankRef[t] * factor) + mods[t]);
        const float raw = ringRead(tankBuf_[t], tankPos_[t], delay);
        dampState_[t] = saturatePlateSample(
            dampState_[t] + kDampUpdate * (raw - dampState_[t]));
        const float apIn = saturatePlateSample(
            raw * (1.0f - damping) + dampState_[t] * damping);
        const int apDelay = std::max(
            1, static_cast<int>(kTankApRef[t] * factor) + mods[t]);
        tankOut[t] = allpassStep(tankApBuf_[t], tankApPos_[t], apDelay,
                                 apIn, kTankApFb);
    }
    tankAccum_[0] = saturatePlateSample(tankOut[0]);
    tankAccum_[1] = saturatePlateSample(tankOut[1]);

    // ステレオ幅のクロスミックスと decay ゲート。
    const float w = std::clamp(finiteOr(width_, 0.8f), 0.0f, 1.0f);
    wetL = saturatePlateSample(
        (tankOut[0] * (1.0f - w * 0.5f) + tankOut[1] * w * 0.5f) * decay);
    wetR = saturatePlateSample(
        (tankOut[1] * (1.0f - w * 0.5f) + tankOut[0] * w * 0.5f) * decay);
}

void AudioPlateReverb::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float mix = std::clamp(
        finiteOr(mix_, 0.3f), 0.0f, 1.0f);

    ensureCapacity(sampleRate);

    // 完全ドライは入力をbit一致で残す(姉妹テストと同契約)。
    if (mix <= 0.0f) return;

    const int chCount = std::min(channels, 2);
    auto* ch0 = segment.channelData[0].data();
    float* ch1 = (chCount > 1) ? segment.channelData[1].data() : nullptr;
    const int samples = std::min(
        frames, static_cast<int>(segment.channelData[0].size()));
    if (samples <= 0) return;
    if (ch1 != nullptr) {
        const int samples1 = std::min(
            frames, static_cast<int>(segment.channelData[1].size()));
        if (samples1 <= 0) ch1 = nullptr;
    }

    for (int i = 0; i < samples; ++i) {
        const float inL = std::isfinite(ch0[i]) ? ch0[i] : 0.0f;
        float inR = inL;
        if (ch1 != nullptr && i < static_cast<int>(segment.channelData[1].size())) {
            inR = std::isfinite(ch1[i]) ? ch1[i] : inL;
        }
        // プレート実機どおりタンク前でモノラル化する。
        const float monoIn = saturatePlateSample((inL + inR) * 0.5f);

        float wetL = 0.0f, wetR = 0.0f;
        processPlateSample(monoIn, wetL, wetR);

        if (ch1 != nullptr) {
            ch0[i] = saturatePlateSample(inL * (1.0f - mix) + wetL * mix);
            ch1[i] = saturatePlateSample(inR * (1.0f - mix) + wetR * mix);
        } else {
            // モノラル出力はステレオ Wet の平均で受ける。
            ch0[i] = saturatePlateSample(
                inL * (1.0f - mix) + (wetL + wetR) * 0.5f * mix);
        }
    }
}

qint64 AudioPlateReverb::tailSamples() const
{
    const float decay = std::clamp(
        std::isfinite(decay_) ? decay_ : 0.75f, 0.0f, 0.99f);
    if (decay <= 0.0f) return 0;
    // 循環ゲインは decay_ にダンピング損失を織り込んだ近似。
    // タンク内 AP はオールパス( magnitude ~= 1 )のため減衰に寄与しない。
    const double damping = std::clamp(
        std::isfinite(damping_) ? damping_ : 0.4, 0.0, 1.0);
    const double gain = static_cast<double>(decay) * (1.0 - 0.15 * damping);
    if (!(gain > 0.0) || !std::isfinite(gain)) return 0;
    if (gain >= 1.0) return std::numeric_limits<qint64>::max();
    const int sr = builtSampleRate_ > 0 ? builtSampleRate_ : 44100;
    const double size = std::clamp(
        std::isfinite(size_) ? size_ : 1.0, 0.5, 2.0);
    // 最長ループ = tank + tankAP (kTankRef[1] + kTankApRef[0])。
    const double loopSamples =
        static_cast<double>(kTankRef[1] + kTankApRef[0]) * size *
        (static_cast<double>(sr) / kRefSampleRate);
    if (!(loopSamples > 0.0)) return 0;
    const double repeats = std::log(0.001) / std::log(gain);
    if (!(repeats > 0.0) || !std::isfinite(repeats)) return 0;
    const double tail = loopSamples * repeats;
    if (tail >= static_cast<double>(std::numeric_limits<qint64>::max())) {
        return std::numeric_limits<qint64>::max();
    }
    return static_cast<qint64>(tail);
}

std::vector<EffectParameter> AudioPlateReverb::getParameters() const
{
    return {
        {"pre_delay_ms", "Pre-Delay (ms)", 0.0f, 100.0f, 10.0f, preDelayMs_},
        {"decay", "Decay", 0.0f, 0.99f, 0.75f, decay_},
        {"damping", "Damping", 0.0f, 1.0f, 0.4f, damping_},
        {"diffusion", "Diffusion", 0.0f, 1.0f, 0.7f, diffusion_},
        {"size", "Size", 0.5f, 2.0f, 1.0f, size_},
        {"mod_depth", "Mod Depth", 0.0f, 1.0f, 0.3f, modDepth_},
        {"mod_rate", "Mod Rate (Hz)", 0.1f, 5.0f, 0.8f, modRate_},
        {"width", "Stereo Width", 0.0f, 1.0f, 0.8f, width_},
        {"mix", "Mix", 0.0f, 1.0f, 0.3f, mix_}
    };
}

void AudioPlateReverb::setParameterValue(const String& id, float value)
{
    if (id == "pre_delay_ms") setPreDelayMs(value);
    else if (id == "decay") setDecay(value);
    else if (id == "damping") setDamping(value);
    else if (id == "diffusion") setDiffusion(value);
    else if (id == "size") setSize(value);
    else if (id == "mod_depth") setModDepth(value);
    else if (id == "mod_rate") setModRate(value);
    else if (id == "width") setWidth(value);
    else if (id == "mix") setMix(value);
}

float AudioPlateReverb::getParameterValue(const String& id) const
{
    if (id == "pre_delay_ms") return preDelayMs_;
    if (id == "decay") return decay_;
    if (id == "damping") return damping_;
    if (id == "diffusion") return diffusion_;
    if (id == "size") return size_;
    if (id == "mod_depth") return modDepth_;
    if (id == "mod_rate") return modRate_;
    if (id == "width") return width_;
    if (id == "mix") return mix_;
    return 0.0f;
}

QJsonObject AudioPlateReverb::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("plate_reverb");
    obj["pre_delay_ms"] = preDelayMs_;
    obj["decay"] = decay_;
    obj["damping"] = damping_;
    obj["diffusion"] = diffusion_;
    obj["size"] = size_;
    obj["mod_depth"] = modDepth_;
    obj["mod_rate"] = modRate_;
    obj["width"] = width_;
    obj["mix"] = mix_;
    return obj;
}

void AudioPlateReverb::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setPreDelayMs(static_cast<float>(obj["pre_delay_ms"].toDouble(10.0)));
    setDecay(static_cast<float>(obj["decay"].toDouble(0.75)));
    setDamping(static_cast<float>(obj["damping"].toDouble(0.4)));
    setDiffusion(static_cast<float>(obj["diffusion"].toDouble(0.7)));
    setSize(static_cast<float>(obj["size"].toDouble(1.0)));
    setModDepth(static_cast<float>(obj["mod_depth"].toDouble(0.3)));
    setModRate(static_cast<float>(obj["mod_rate"].toDouble(0.8)));
    setWidth(static_cast<float>(obj["width"].toDouble(0.8)));
    setMix(static_cast<float>(obj["mix"].toDouble(0.3)));
}

} // namespace ArtifactCore
