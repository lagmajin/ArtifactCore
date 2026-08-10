module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
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

    String getName() const override { return "Reverb"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setDecay(float d) {
        decay_ = std::isfinite(d) ? std::clamp(d, 0.0f, 1.0f) : 0.5f;
    }
    void setMix(float m) {
        mix_ = std::isfinite(m) ? std::clamp(m, 0.0f, 1.0f) : 0.3f;
    }
    void setSize(float s) {
        size_ = std::isfinite(s) ? std::clamp(s, 0.0f, 1.0f) : 0.7f;
    }

    float getDecay() const { return decay_; }
    float getMix() const { return mix_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float decay_ = 0.5f;
    float mix_ = 0.3f;
    float size_ = 0.7f;

    // Instance-safe buffers (was thread_local)
    std::vector<float> combBuffers_;
    int combBufSize_ = 0;
    int writePositions_[2] = {0, 0};
    std::vector<float> allpassBuffer_;
};

} // namespace ArtifactCore
