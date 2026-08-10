module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.HighLowPass;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief High-Low Pass Filter エフェクト
 * ローパス/ハイパスフィルターです。
 * After Effects High-Low Pass 相当のエフェクト。
 */
class LIBRARY_DLL_API AudioHighLowPass : public AudioEffect {
public:
    AudioHighLowPass();
    virtual ~AudioHighLowPass() = default;

    String getName() const override { return "High-Low Pass"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    // パラメータ
    void setLowPassFreq(float hz) { lowPassFreq_ = std::isfinite(hz) ? std::clamp(hz, 0.0f, 24000.0f) : 0.0f; }    // Hz (20 ~ 20000)
    void setHighPassFreq(float hz) { highPassFreq_ = std::isfinite(hz) ? std::clamp(hz, 0.0f, 24000.0f) : 0.0f; }  // Hz
    void setLowPassResonance(float r) { lowPassRes_ = std::isfinite(r) ? r : 0.0f; }
    void setHighPassResonance(float r) { highPassRes_ = std::isfinite(r) ? r : 0.0f; }

    float getLowPassFreq() const { return lowPassFreq_; }
    float getHighPassFreq() const { return highPassFreq_; }

private:
    float lowPassFreq_ = 0.0f;    // 0で無効
    float highPassFreq_ = 0.0f;     // 0で無効
    float lowPassRes_ = 0.0f;
    float highPassRes_ = 0.0f;
};

} // namespace ArtifactCore
