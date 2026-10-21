module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.BitCrusher;
import Core.ArtifactString;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Bit Crusher エフェクト(Core 側コア)
 * 量子化ビット深度の削減 + ダウンサンプリング(ホールド)。
 * 状態はホールド値とカウンタのみ。
 */
class LIBRARY_DLL_API AudioBitCrusher : public AudioEffect {
public:
    AudioBitCrusher();
    virtual ~AudioBitCrusher() = default;

    String getName() const override { return "Bit Crusher"; }
    String effectType() const override { return "bit_crusher"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setBits(float v) {
        bits_ = std::isfinite(v) ? std::clamp(v, 2.0f, 16.0f) : 8.0f;
    }
    void setDownsample(float v) {
        downsample_ = std::isfinite(v) ? std::clamp(v, 1.0f, 40.0f) : 1.0f;
    }
    void setMix(float v) {
        mix_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
    }

    float getBits() const { return bits_; }
    float getDownsample() const { return downsample_; }
    float getMix() const { return mix_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float bits_ = 8.0f;
    float downsample_ = 1.0f;
    float mix_ = 1.0f;

    // Instance-safe hold state (was thread_local 方式と同様、実体保持)。
    float held_[2] = {0.0f, 0.0f};
    int holdCounter_[2] = {0, 0};
};

} // namespace ArtifactCore
