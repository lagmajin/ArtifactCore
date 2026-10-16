module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.DCBlocker;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief DC Blocker エフェクト(Core 側コア)
 * 直流成分の除去。y = x - x1 + R*y1 の古典形。遅延なし。
 */
class LIBRARY_DLL_API AudioDCBlocker : public AudioEffect {
public:
    AudioDCBlocker();
    virtual ~AudioDCBlocker() = default;

    String getName() const override { return "DC Blocker"; }
    String effectType() const override { return "dc_blocker"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setFrequency(float v) {
        frequency_ = std::isfinite(v) ? std::clamp(v, 2.0f, 100.0f) : 10.0f;
    }
    void setMix(float v) {
        mix_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
    }

    float getFrequency() const { return frequency_; }
    float getMix() const { return mix_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float frequency_ = 10.0f;
    float mix_ = 1.0f;

    // Instance-safe state (was thread_local 方式と同様、実体保持)。
    float prevInput_[2] = {0.0f, 0.0f};
    float prevOutput_[2] = {0.0f, 0.0f};
};

} // namespace ArtifactCore
