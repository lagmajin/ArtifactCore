module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.ParametricEQ;

import Audio.Effect;
import Audio.Segment;

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

    std::string getName() const override { return "Parametric EQ"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    // バンド操作
    void setBand(int index, const Band& band) {
        if (index >= 0 && index < static_cast<int>(bands_.size())) {
            bands_[index] = band;
        }
    }
    Band getBand(int index) const {
        if (index >= 0 && index < static_cast<int>(bands_.size())) {
            return bands_[index];
        }
        return {};
    }
    void setBandCount(int count) {
        bands_.resize(count);
    }
    int getBandCount() const { return static_cast<int>(bands_.size()); }

private:
    std::vector<Band> bands_;
    std::vector<float> delayedState_; // フィルタ状態保持
};

} // namespace ArtifactCore