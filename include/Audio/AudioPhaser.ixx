module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.Phaser;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Phaser エフェクト(Core 側コア)
 * 1 次オールパス最大 8 段のカスケード + LFO によるノッチ掃引。
 * 係数はサンプルごとに中心周波数から求めるため遅延バッファを持たない。
 */
class LIBRARY_DLL_API AudioPhaser : public AudioEffect {
public:
    AudioPhaser();
    virtual ~AudioPhaser() = default;

    String getName() const override { return "Phaser"; }
    String effectType() const override { return "phaser"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setRate(float v) {
        rate_ = std::isfinite(v) ? std::clamp(v, 0.05f, 10.0f) : 0.5f;
    }
    void setDepth(float v) {
        depth_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.6f;
    }
    void setCenter(float v) {
        center_ = std::isfinite(v) ? std::clamp(v, 200.0f, 4000.0f) : 800.0f;
    }
    void setFeedback(float v) {
        feedback_ = std::isfinite(v) ? std::clamp(v, 0.0f, 0.9f) : 0.4f;
    }
    void setStages(float v) {
        stages_ = std::isfinite(v) ? std::clamp(v, 2.0f, 8.0f) : 4.0f;
    }
    void setMix(float v) {
        mix_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
    }

    float getRate() const { return rate_; }
    float getDepth() const { return depth_; }
    float getCenter() const { return center_; }
    float getFeedback() const { return feedback_; }
    float getStages() const { return stages_; }
    float getMix() const { return mix_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float rate_ = 0.5f;
    float depth_ = 0.6f;
    float center_ = 800.0f;
    float feedback_ = 0.4f;
    float stages_ = 4.0f;
    float mix_ = 0.5f;

    // Instance-safe state (was thread_local 方式と同様、実体保持)。
    float stageState_[2][8] = {};
    float feedbackState_[2] = {0.0f, 0.0f};
    float lfoPhase_ = 0.0f;
};

} // namespace ArtifactCore
