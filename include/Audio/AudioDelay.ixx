module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.Delay;

import Audio.Effect;
import Audio.Segment;
import Audio.RingBuffer;

export namespace ArtifactCore {

/**
 * @brief Delay エフェクト
 * エコー・リヴァーブ効果を生成します。
 * After Effects Backwards/Delay 相当の基本ディレイエフェクト。
 */
class LIBRARY_DLL_API AudioDelay : public AudioEffect {
public:
    AudioDelay();
    virtual ~AudioDelay() = default;

    std::string getName() const override { return "Delay"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    // パラメータ
    void setDelayTimeMs(float ms) { delayTimeMs_ = ms; }
    void setFeedback(float fb) { feedback_ = fb; }       // 0.0 ~ 0.99
    void setMix(float mix) { mix_ = mix; }             // 0.0(dry) ~ 1.0(wet)
    void setBypassSideChain(bool enable) { bypassSideChain_ = enable; }

    float getDelayTimeMs() const { return delayTimeMs_; }
    float getFeedback() const { return feedback_; }
    float getMix() const { return mix_; }

private:
    float delayTimeMs_ = 300.0f;  // デフォルト300ms
    float feedback_ = 0.3f;
    float mix_ = 0.4f;
    bool bypassSideChain_ = false;
};

} // namespace ArtifactCore