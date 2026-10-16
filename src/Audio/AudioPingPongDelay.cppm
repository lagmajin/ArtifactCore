module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <QString>
#include <QJsonObject>
#include <QtGlobal>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.PingPongDelay;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

namespace {

float saturatePingPongSample(const float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

float finiteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

} // namespace

AudioPingPongDelay::AudioPingPongDelay() {}

void AudioPingPongDelay::ensureCapacity(int sampleRate)
{
    const int sr = std::clamp(sampleRate, 4000, 192000);
    // 最大 2000ms + 余裕。
    const int need = static_cast<int>(2.05f * sr) + 8;
    if (sr == builtSampleRate_
        && static_cast<int>(delayBuf_[0].size()) >= need
        && static_cast<int>(delayBuf_[1].size()) >= need) {
        return;
    }
    // コールドパスの bounded 再確保(最大 192kHz で約 3.1MB)。
    delayBuf_[0].assign(static_cast<std::size_t>(need), 0.0f);
    delayBuf_[1].assign(static_cast<std::size_t>(need), 0.0f);
    writePos_[0] = 0;
    writePos_[1] = 0;
    builtSampleRate_ = sr;
}

qint64 AudioPingPongDelay::tailSamples() const
{
    const float feedback = std::clamp(
        std::isfinite(feedback_) ? feedback_ : 0.4f, 0.0f, 0.95f);
    if (feedback <= 0.0f) return 0;
    if (feedback >= 1.0f) return std::numeric_limits<qint64>::max();
    const int sr = builtSampleRate_ > 0 ? builtSampleRate_ : 44100;
    const double loopSamples = static_cast<double>(std::clamp(
        std::isfinite(timeMs_) ? timeMs_ : 375.0f, 1.0f, 2000.0f))
        * 0.001 * static_cast<double>(sr);
    if (!(loopSamples > 0.0)) return 0;
    const double repeats = std::log(0.001) / std::log(static_cast<double>(feedback));
    if (!(repeats > 0.0) || !std::isfinite(repeats)) return 0;
    const double tail = loopSamples * repeats;
    if (tail >= static_cast<double>(std::numeric_limits<qint64>::max())) {
        return std::numeric_limits<qint64>::max();
    }
    return static_cast<qint64>(tail);
}

void AudioPingPongDelay::process(AudioSegment& segment, const AudioSegment* /*sideChain*/)
{
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const int sampleRate = segment.sampleRate;
    if (channels == 0 || frames == 0 || sampleRate <= 0) return;

    const float timeMs = std::clamp(finiteOr(timeMs_, 375.0f), 1.0f, 2000.0f);
    const float feedback = std::clamp(finiteOr(feedback_, 0.4f), 0.0f, 0.95f);
    const float mix = std::clamp(finiteOr(mix_, 0.35f), 0.0f, 1.0f);

    ensureCapacity(sampleRate);
    if (mix <= 0.0f) return;

    const int sr = builtSampleRate_ > 0 ? builtSampleRate_ : sampleRate;
    const int delaySamples = std::max(
        1, static_cast<int>(timeMs * 0.001f * static_cast<float>(sr)));
    const int bufSize = static_cast<int>(delayBuf_[0].size());
    if (bufSize <= delaySamples) return;

    const bool stereo = channels > 1
        && segment.channelData.size() > 1
        && !segment.channelData[1].isEmpty();
    float* ch0 = segment.channelData[0].data();
    float* ch1 = stereo ? segment.channelData[1].data() : nullptr;
    const int samples0 = std::min(
        frames, static_cast<int>(segment.channelData[0].size()));
    const int samples1 = stereo ? std::min(
        frames, static_cast<int>(segment.channelData[1].size())) : 0;
    const int samples = ch1 != nullptr ? std::min(samples0, samples1) : samples0;
    if (samples <= 0) return;

    for (int i = 0; i < samples; ++i) {
        const float inL = std::isfinite(ch0[i]) ? ch0[i] : 0.0f;
        const float inR = (ch1 != nullptr && std::isfinite(ch1[i])) ? ch1[i] : inL;

        // クロスフィードバック: L <- R の過去、R <- L の過去。
        int readL = (writePos_[0] - delaySamples) % bufSize;
        if (readL < 0) readL += bufSize;
        int readR = (writePos_[1] - delaySamples) % bufSize;
        if (readR < 0) readR += bufSize;
        const float echoL = delayBuf_[0][static_cast<std::size_t>(readL)];
        const float echoR = delayBuf_[1][static_cast<std::size_t>(readR)];

        delayBuf_[0][static_cast<std::size_t>(writePos_[0])] =
            saturatePingPongSample(inL + echoR * feedback);
        delayBuf_[1][static_cast<std::size_t>(writePos_[1])] =
            saturatePingPongSample(inR + echoL * feedback);
        writePos_[0] = (writePos_[0] + 1) % bufSize;
        writePos_[1] = (writePos_[1] + 1) % bufSize;

        ch0[i] = saturatePingPongSample(inL * (1.0f - mix) + echoL * mix);
        if (ch1 != nullptr) {
            ch1[i] = saturatePingPongSample(inR * (1.0f - mix) + echoR * mix);
        } else if (channels == 1) {
            ch0[i] = saturatePingPongSample(
                inL * (1.0f - mix) + (echoL + echoR) * 0.5f * mix);
        }
    }
}

std::vector<EffectParameter> AudioPingPongDelay::getParameters() const
{
    return {
        {"time_ms", "Time (ms)", 1.0f, 2000.0f, 375.0f, timeMs_},
        {"feedback", "Feedback", 0.0f, 0.95f, 0.4f, feedback_},
        {"mix", "Mix", 0.0f, 1.0f, 0.35f, mix_}
    };
}

void AudioPingPongDelay::setParameterValue(const String& id, float value)
{
    if (id == "time_ms") setTimeMs(value);
    else if (id == "feedback") setFeedback(value);
    else if (id == "mix") setMix(value);
}

float AudioPingPongDelay::getParameterValue(const String& id) const
{
    if (id == "time_ms") return timeMs_;
    if (id == "feedback") return feedback_;
    if (id == "mix") return mix_;
    return 0.0f;
}

QJsonObject AudioPingPongDelay::toJson() const
{
    QJsonObject obj = AudioEffect::toJson();
    obj["type"] = QStringLiteral("ping_pong_delay");
    obj["time_ms"] = timeMs_;
    obj["feedback"] = feedback_;
    obj["mix"] = mix_;
    return obj;
}

void AudioPingPongDelay::fromJson(const QJsonObject& obj)
{
    AudioEffect::fromJson(obj);
    setTimeMs(static_cast<float>(obj["time_ms"].toDouble(375.0)));
    setFeedback(static_cast<float>(obj["feedback"].toDouble(0.4)));
    setMix(static_cast<float>(obj["mix"].toDouble(0.35)));
}

} // namespace ArtifactCore
