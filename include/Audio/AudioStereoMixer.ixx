module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.StereoMixer;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Stereo Mixer エフェクト
 * ステレオバランス・幅コントロールを行います。
 * After Effects Stereo Mixer 相当のエフェクト。
 */
class LIBRARY_DLL_API AudioStereoMixer : public AudioEffect {
public:
    AudioStereoMixer();
    virtual ~AudioStereoMixer() = default;

    std::string getName() const override { return "Stereo Mixer"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    // パラメータ
    void setLeftRight(float lr) { leftRight_ = lr; }     // -1.0(left) ~ 1.0(right)
    void setCenter(float c) { center_ = c; }             // -1.0 ~ 1.0
    void setLeftDelay(float d) { leftDelay_ = d; }       // ms
    void setRightDelay(float d) { rightDelay_ = d; }     // ms

    float getLeftRight() const { return leftRight_; }

private:
    float leftRight_ = 0.0f;    // LRバランス
    float center_ = 0.0f;       // センター取得
    float leftDelay_ = 0.0f;
    float rightDelay_ = 0.0f;
};

} // namespace ArtifactCore