module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.Delay;

import Audio.Effect;
import Audio.Segment;
import Audio.RingBuffer;

export namespace ArtifactCore {

/**
 * @brief Delay エフェクト
 * エコー・リヴァーブ効果を生成します。
 * After Effects Backwards/Delay 相当の基本ディレイエフェクト。
 */
class LIBRARY_DLL_API AudioDelay : public AudioEffect {
public:
    AudioDelay();
    virtual ~AudioDelay() = default;

    String getName() const override { return "Delay"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setDelayTimeMs(float ms) {
        delayTimeMs_ = std::isfinite(ms) ? std::clamp(ms, 1.0f, 2000.0f) : 300.0f;
    }
    void setFeedback(float fb) {
        feedback_ = std::isfinite(fb) ? std::clamp(fb, 0.0f, 0.99f) : 0.3f;
    }
    void setMix(float mix) {
        mix_ = std::isfinite(mix) ? std::clamp(mix, 0.0f, 1.0f) : 0.4f;
    }
    void setBypassSideChain(bool enable) { bypassSideChain_ = enable; }

    float getDelayTimeMs() const { return delayTimeMs_; }
    float getFeedback() const { return feedback_; }
    float getMix() const { return mix_; }

    String effectType() const override { return "delay"; }
    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float delayTimeMs_ = 300.0f;
    float feedback_ = 0.3f;
    float mix_ = 0.4f;
    bool bypassSideChain_ = false;

    // Instance-safe buffers (was thread_local)
    std::vector<std::vector<float>> delayBuffers_{2, std::vector<float>(48000)};
    int writePositions_[2] = {0, 0};
};

} // namespace ArtifactCore
