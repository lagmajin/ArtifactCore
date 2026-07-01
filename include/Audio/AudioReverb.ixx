module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <QJsonObject>
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

    void setDecay(float d) { decay_ = d; }
    void setMix(float m) { mix_ = m; }
    void setSize(float s) { size_ = s; }

    float getDecay() const { return decay_; }
    float getMix() const { return mix_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const std::string& id, float value) override;
    float getParameterValue(const std::string& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float decay_ = 0.5f;
    float mix_ = 0.3f;
    float size_ = 0.7f;

    // Instance-safe buffers (was thread_local)
    std::vector<float> combBuffers_;
    int combBufSize_ = 0;
    std::vector<float> allpassBuffer_;
};

} // namespace ArtifactCore