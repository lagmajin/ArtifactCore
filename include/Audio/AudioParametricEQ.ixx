module;
#include <utility>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <memory>
#include <atomic>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.ParametricEQ;

import Audio.Effect;
import Audio.Segment;
import Container.NamedVector;

export namespace ArtifactCore {

/**
 * @brief Parametric EQ エフェクト
 * カスタマイズ可能なマルチバンドイコライザです。
 * After Effects Parametric EQ 相当のエフェクト。
 */
class LIBRARY_DLL_API AudioParametricEQ : public AudioEffect {
public:
    struct Band {
        float frequency = 1000.0f;  // Hz
        float gainDb = 0.0f;        // dB
        float qFactor = 1.0f;       // Q値 (0.1 ~ 10.0)
        bool enabled = true;
    };

    AudioParametricEQ();
    virtual ~AudioParametricEQ() = default;

    String getName() const override { return "Parametric EQ"; }
    String effectType() const override { return String("parametric_eq"); }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;
    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

    // バンド操作
    void setBand(int index, const Band& band) {
        if (index >= 0 && index < static_cast<int>(bands_.size())) {
            Band sanitized = band;
            if (!std::isfinite(sanitized.frequency)) sanitized.frequency = 1000.0f;
            if (!std::isfinite(sanitized.gainDb)) sanitized.gainDb = 0.0f;
            if (!std::isfinite(sanitized.qFactor)) sanitized.qFactor = 1.0f;
            sanitized.frequency = std::clamp(sanitized.frequency, 1.0f, 24000.0f);
            sanitized.gainDb = std::clamp(sanitized.gainDb, -48.0f, 48.0f);
            sanitized.qFactor = std::clamp(sanitized.qFactor, 0.1f, 10.0f);
            bands_[index] = sanitized;
        }
    }
    Band getBand(int index) const {
        if (index >= 0 && index < static_cast<int>(bands_.size())) {
            return bands_[index];
        }
        return {};
    }
    void setBandCount(int count) {
        bands_.resize(static_cast<size_t>(std::clamp(count, 0, 64)));
    }
    int getBandCount() const { return static_cast<int>(bands_.size()); }

private:
    NamedVector<Band> bands_{
        makeNamedVector<Band>(ContainerName{"AudioParametricEQBands"})};
    std::vector<float> delayedState_; // フィルタ状態保持
};

} // namespace ArtifactCore
