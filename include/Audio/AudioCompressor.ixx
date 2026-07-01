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
    void setRatio(float ratio) { ratio_ = ratio; }
    void setAttack(float ms) { attackMs_ = ms; }
    void setRelease(float ms) { releaseMs_ = ms; }
    void setSideChain(bool enable) { sideChainEnabled_ = enable; }
    
    float getGainReduction() const;

    std::string effectType() const override { return "compressor"; }
    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const std::string& id, float value) override;
    float getParameterValue(const std::string& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float thresholdDb_ = -20.0f;
    float ratio_ = 4.0f;
    float attackMs_ = 10.0f;
    float releaseMs_ = 100.0f;
    bool sideChainEnabled_ = false;

    float envelope_ = 0.0f; // process() ループ内で連続書き換え（同一スレッド）
    float lastSampleRate_ = 0.0f; // サンプルレート変化検出用（バス間リーク防止）
    std::atomic<float> currentGainReduction_{1.0f}; // UI スレッドからの読み込み用
};

} // namespace ArtifactCore
