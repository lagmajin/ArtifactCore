module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.StereoWidener;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Stereo Widener エフェクト(Core 側コア)
 * Mid/Side 処理による幅調整。bassMonoFreq_ 以下の Side は
 * モノラルのままにして低域の位相崩れを防ぐ。
 */
class LIBRARY_DLL_API AudioStereoWidener : public AudioEffect {
public:
    AudioStereoWidener();
    virtual ~AudioStereoWidener() = default;

    String getName() const override { return "Stereo Widener"; }
    String effectType() const override { return "stereo_widener"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setWidth(float v) {
        width_ = std::isfinite(v) ? std::clamp(v, 0.0f, 2.0f) : 1.2f;
    }
    void setBassMonoFreq(float v) {
        bassMonoFreq_ = std::isfinite(v) ? std::clamp(v, 0.0f, 500.0f) : 120.0f;
    }
    void setTrimDb(float v) {
        trimDb_ = std::isfinite(v) ? std::clamp(v, -12.0f, 12.0f) : 0.0f;
    }

    float getWidth() const { return width_; }
    float getBassMonoFreq() const { return bassMonoFreq_; }
    float getTrimDb() const { return trimDb_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float width_ = 1.2f;
    float bassMonoFreq_ = 120.0f;
    float trimDb_ = 0.0f;

    // Instance-safe lowpass state for the Side signal.
    float sideLowState_ = 0.0f;
};

} // namespace ArtifactCore
