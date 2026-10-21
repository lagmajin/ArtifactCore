module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.DeEsser;
import Core.ArtifactString;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief De-Esser エフェクト(Core 側コア)
 * 高域サイドチェイン検出 + 高域帯のみの動的減衰で歯擦音を抑える。
 * 低域は無加工で通す。
 */
class LIBRARY_DLL_API AudioDeEsser : public AudioEffect {
public:
    AudioDeEsser();
    virtual ~AudioDeEsser() = default;

    String getName() const override { return "De-Esser"; }
    String effectType() const override { return "de_esser"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setFrequency(float v) {
        frequency_ = std::isfinite(v) ? std::clamp(v, 2000.0f, 12000.0f) : 6500.0f;
    }
    void setThresholdDb(float v) {
        thresholdDb_ = std::isfinite(v) ? std::clamp(v, -60.0f, 0.0f) : -30.0f;
    }
    void setReduction(float v) {
        reduction_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.6f;
    }

    float getFrequency() const { return frequency_; }
    float getThresholdDb() const { return thresholdDb_; }
    float getReduction() const { return reduction_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float frequency_ = 6500.0f;
    float thresholdDb_ = -30.0f;
    float reduction_ = 0.6f;

    // Instance-safe state (was thread_local 方式と同様、実体保持)。
    float splitState_[2] = {0.0f, 0.0f};
    float detectState_[2] = {0.0f, 0.0f};
    float envelope_[2] = {0.0f, 0.0f};
};

} // namespace ArtifactCore
