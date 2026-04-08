module;
#include <utility>
#include <string>
#include <memory>
#include <vector>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.Compressor;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief サイドチェーン対応のコンプレッサー
 * 音量を一定レベル以下に抑え、ダイナミクスを制御します。
 */
class LIBRARY_DLL_API AudioCompressor : public AudioEffect {
public:
    AudioCompressor();
    virtual ~AudioCompressor() = default;

    std::string getName() const override { return "Compressor"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    // パラメータ
    void setThreshold(float db) { thresholdDb_ = db; }
    void setRatio(float ratio) { ratio_ = ratio; } // 1.0 ~ 20.0 (Inf)
    void setAttack(float ms) { attackMs_ = ms; }
    void setRelease(float ms) { releaseMs_ = ms; }
    void setSideChain(bool enable) { sideChainEnabled_ = enable; }
    
    float getGainReduction() const { return currentGainReduction_; }

private:
    float thresholdDb_ = -20.0f;
    float ratio_ = 4.0f;
    float attackMs_ = 10.0f;
    float releaseMs_ = 100.0f;
    bool sideChainEnabled_ = false;

    float envelope_ = 0.0f;
    float currentGainReduction_ = 1.0f; // 0.0 ~ 1.0
};

} // namespace ArtifactCore
