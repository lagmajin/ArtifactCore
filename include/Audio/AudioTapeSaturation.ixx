module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.TapeSaturation;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Tape Saturation エフェクト(Core 側コア)
 * tanh 整形 + 高域ダンピングによるテープ風飽和。状態は 1 ポールのみ。
 */
class LIBRARY_DLL_API AudioTapeSaturation : public AudioEffect {
public:
    AudioTapeSaturation();
    virtual ~AudioTapeSaturation() = default;

    String getName() const override { return "Tape Saturation"; }
    String effectType() const override { return "tape_saturation"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setDriveDb(float v) {
        driveDb_ = std::isfinite(v) ? std::clamp(v, 0.0f, 24.0f) : 6.0f;
    }
    void setWarmth(float v) {
        warmth_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.4f;
    }
    void setOutputDb(float v) {
        outputDb_ = std::isfinite(v) ? std::clamp(v, -12.0f, 12.0f) : -6.0f;
    }
    void setMix(float v) {
        mix_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
    }

    float getDriveDb() const { return driveDb_; }
    float getWarmth() const { return warmth_; }
    float getOutputDb() const { return outputDb_; }
    float getMix() const { return mix_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float driveDb_ = 6.0f;
    float warmth_ = 0.4f;
    float outputDb_ = -6.0f;
    float mix_ = 1.0f;

    // Instance-safe filter state (was thread_local 方式と同様、実体保持)。
    float preState_[2] = {0.0f, 0.0f};
    float postState_[2] = {0.0f, 0.0f};
};

} // namespace ArtifactCore
