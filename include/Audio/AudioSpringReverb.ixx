module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <QJsonObject>
#include <QtGlobal>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.SpringReverb;
import Core.ArtifactString;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Spring Reverb エフェクト(Core 側コア)
 * 簡易スプリング。入力ハイパス + 6 直列オールパス +
 * 2 並列のローパス付きフィードバックループで金属的な残響を作る。
 */
class LIBRARY_DLL_API AudioSpringReverb : public AudioEffect {
public:
    AudioSpringReverb();
    virtual ~AudioSpringReverb() = default;

    String getName() const override { return "Spring Reverb"; }
    String effectType() const override { return "spring_reverb"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    qint64 latencySamples() const override { return 0; }
    qint64 tailSamples() const override;

    void setDecay(float v) {
        decay_ = std::isfinite(v) ? std::clamp(v, 0.0f, 0.99f) : 0.8f;
    }
    void setBrightness(float v) {
        brightness_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
    }
    void setTension(float v) {
        tension_ = std::isfinite(v) ? std::clamp(v, 0.5f, 1.5f) : 1.0f;
    }
    void setMix(float v) {
        mix_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.3f;
    }

    float getDecay() const { return decay_; }
    float getBrightness() const { return brightness_; }
    float getTension() const { return tension_; }
    float getMix() const { return mix_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    void ensureCapacity(int sampleRate);

    float decay_ = 0.8f;
    float brightness_ = 0.5f;
    float tension_ = 1.0f;
    float mix_ = 0.3f;

    // 基準遅延(ms)。実遅延 = ref * tension_ * sr / 1000。
    static constexpr float kSeriesRefMs[6] = {1.1f, 1.5f, 2.3f, 3.1f, 4.2f, 5.6f};
    static constexpr float kLoopRefMs[2] = {23.0f, 31.0f};

    // Instance-safe buffers (was thread_local 方式と同様、実体保持)。
    std::vector<float> seriesBuf_[6];
    int seriesPos_[6] = {0, 0, 0, 0, 0, 0};
    std::vector<float> loopBuf_[2];
    int loopPos_[2] = {0, 0};

    float hpState_ = 0.0f;
    float dampState_[2] = {0.0f, 0.0f};
    float loopAccum_[2] = {0.0f, 0.0f};

    int builtSampleRate_ = -1;
    float builtTension_ = -1.0f;
};

} // namespace ArtifactCore
