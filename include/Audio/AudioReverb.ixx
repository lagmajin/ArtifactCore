module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.Reverb;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Reverb エフェクト
 * 簡易Schroederリバーブ（コンバライザ+ディフュージョン）
 * After Effects Reverb 相当のエフェクト。
 */
class LIBRARY_DLL_API AudioReverb : public AudioEffect {
public:
    AudioReverb();
    virtual ~AudioReverb() = default;

    std::string getName() const override { return "Reverb"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    // パラメータ
    void setDecay(float d) { decay_ = d; }      // 0.0 ~ 1.0
    void setMix(float m) { mix_ = m; }           // 0.0 ~ 1.0
    void setSize(float s) { size_ = s; }         // 0.0 ~ 1.0

    float getDecay() const { return decay_; }
    float getMix() const { return mix_; }

private:
    float decay_ = 0.5f;
    float mix_ = 0.3f;
    float size_ = 0.7f;
};

} // namespace ArtifactCore