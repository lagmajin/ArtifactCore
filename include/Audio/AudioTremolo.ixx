module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.Tremolo;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Tremolo / Auto-Pan エフェクト(Core 側コア)
 * LFO による振幅変調。width_ が 0 より大きいと Rch の位相がずれ、
 * ステレオでは Auto-Pan として働く。遅延バッファを持たない。
 */
class LIBRARY_DLL_API AudioTremolo : public AudioEffect {
public:
    AudioTremolo();
    virtual ~AudioTremolo() = default;

    String getName() const override { return "Tremolo"; }
    String effectType() const override { return "tremolo"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setRate(float v) {
        rate_ = std::isfinite(v) ? std::clamp(v, 0.1f, 20.0f) : 5.0f;
    }
    void setDepth(float v) {
        depth_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.8f;
    }
    void setShape(float v) {
        shape_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
    }
    void setWidth(float v) {
        width_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
    }
    void setMix(float v) {
        mix_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
    }

    float getRate() const { return rate_; }
    float getDepth() const { return depth_; }
    float getShape() const { return shape_; }
    float getWidth() const { return width_; }
    float getMix() const { return mix_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float rate_ = 5.0f;
    float depth_ = 0.8f;
    float shape_ = 0.0f;
    float width_ = 0.0f;
    float mix_ = 1.0f;

    float lfoPhase_ = 0.0f;
};

} // namespace ArtifactCore
