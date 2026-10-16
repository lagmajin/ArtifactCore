module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.TransientShaper;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Transient Shaper エフェクト(Core 側コア)
 * 高速/低速の 2 包絡の差でアタック成分を取り出し、attack_ で強調・抑制、
 * sustain_ で胴鳴り成分を調整する。ゲインは bounded に抑える。
 */
class LIBRARY_DLL_API AudioTransientShaper : public AudioEffect {
public:
    AudioTransientShaper();
    virtual ~AudioTransientShaper() = default;

    String getName() const override { return "Transient Shaper"; }
    String effectType() const override { return "transient_shaper"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setAttack(float v) {
        attack_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.3f;
    }
    void setSustain(float v) {
        sustain_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f;
    }
    void setSpeedMs(float v) {
        speedMs_ = std::isfinite(v) ? std::clamp(v, 1.0f, 100.0f) : 20.0f;
    }

    float getAttack() const { return attack_; }
    float getSustain() const { return sustain_; }
    float getSpeedMs() const { return speedMs_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float attack_ = 0.3f;
    float sustain_ = 0.0f;
    float speedMs_ = 20.0f;

    // Instance-safe envelope state (was thread_local 方式と同様、実体保持)。
    float fastEnv_[2] = {0.0f, 0.0f};
    float slowEnv_[2] = {0.0f, 0.0f};
};

} // namespace ArtifactCore
