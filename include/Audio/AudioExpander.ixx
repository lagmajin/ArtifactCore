module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.Expander;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Expander エフェクト(Core 側コア)
 * 下向きエキスパンダ。しきい値以下の低レベル信号をレシオに応じて減衰し、
 * ノイズフロアやマイクかぶりを抑える。ピーク検出 + attack/release 平滑。
 */
class LIBRARY_DLL_API AudioExpander : public AudioEffect {
public:
    AudioExpander();
    virtual ~AudioExpander() = default;

    String getName() const override { return "Expander"; }
    String effectType() const override { return "expander"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setThresholdDb(float v) {
        thresholdDb_ = std::isfinite(v) ? std::clamp(v, -60.0f, 0.0f) : -40.0f;
    }
    void setRatio(float v) {
        ratio_ = std::isfinite(v) ? std::clamp(v, 1.0f, 10.0f) : 2.0f;
    }
    void setAttackMs(float v) {
        attackMs_ = std::isfinite(v) ? std::clamp(v, 0.1f, 100.0f) : 10.0f;
    }
    void setReleaseMs(float v) {
        releaseMs_ = std::isfinite(v) ? std::clamp(v, 10.0f, 1000.0f) : 150.0f;
    }

    float getThresholdDb() const { return thresholdDb_; }
    float getRatio() const { return ratio_; }
    float getAttackMs() const { return attackMs_; }
    float getReleaseMs() const { return releaseMs_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float thresholdDb_ = -40.0f;
    float ratio_ = 2.0f;
    float attackMs_ = 10.0f;
    float releaseMs_ = 150.0f;

    // Instance-safe envelope state (was thread_local 方式と同様、実体保持)。
    float envelope_[2] = {0.0f, 0.0f};
};

} // namespace ArtifactCore
