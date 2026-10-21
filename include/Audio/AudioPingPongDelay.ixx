module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include <QtGlobal>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.PingPongDelay;
import Core.ArtifactString;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Ping-Pong Delay エフェクト(Core 側コア)
 * 左右クロスフィードバックのステレオディレイ。
 * モノラル入力は両 ch へ同一に入る。
 */
class LIBRARY_DLL_API AudioPingPongDelay : public AudioEffect {
public:
    AudioPingPongDelay();
    virtual ~AudioPingPongDelay() = default;

    String getName() const override { return "Ping-Pong Delay"; }
    String effectType() const override { return "ping_pong_delay"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override;

    void setTimeMs(float v) {
        timeMs_ = std::isfinite(v) ? std::clamp(v, 1.0f, 2000.0f) : 375.0f;
    }
    void setFeedback(float v) {
        feedback_ = std::isfinite(v) ? std::clamp(v, 0.0f, 0.95f) : 0.4f;
    }
    void setMix(float v) {
        mix_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.35f;
    }

    float getTimeMs() const { return timeMs_; }
    float getFeedback() const { return feedback_; }
    float getMix() const { return mix_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    void ensureCapacity(int sampleRate);

    float timeMs_ = 375.0f;
    float feedback_ = 0.4f;
    float mix_ = 0.35f;

    // Instance-safe buffers (was thread_local 方式と同様、実体保持)。
    std::vector<float> delayBuf_[2];
    int writePos_[2] = {0, 0};
    int builtSampleRate_ = -1;
};

} // namespace ArtifactCore
