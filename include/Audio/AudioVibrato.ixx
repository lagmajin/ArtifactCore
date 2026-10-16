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

export module Audio.Effect.Vibrato;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Vibrato エフェクト(Core 側コア)
 * 短遅延の周期的変調によるピッチビブラート。線形補間で読み出す。
 */
class LIBRARY_DLL_API AudioVibrato : public AudioEffect {
public:
    AudioVibrato();
    virtual ~AudioVibrato() = default;

    String getName() const override { return "Vibrato"; }
    String effectType() const override { return "vibrato"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    qint64 latencySamples() const override;
    qint64 tailSamples() const override { return 0; }

    void setRate(float v) {
        rate_ = std::isfinite(v) ? std::clamp(v, 1.0f, 15.0f) : 6.0f;
    }
    void setDepthMs(float v) {
        depthMs_ = std::isfinite(v) ? std::clamp(v, 0.0f, 10.0f) : 2.0f;
    }
    void setBaseMs(float v) {
        baseMs_ = std::isfinite(v) ? std::clamp(v, 3.0f, 10.0f) : 5.0f;
    }
    void setMix(float v) {
        mix_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
    }

    float getRate() const { return rate_; }
    float getDepthMs() const { return depthMs_; }
    float getBaseMs() const { return baseMs_; }
    float getMix() const { return mix_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    void ensureCapacity(int sampleRate);

    float rate_ = 6.0f;
    float depthMs_ = 2.0f;
    float baseMs_ = 5.0f;
    float mix_ = 1.0f;

    // Instance-safe buffers (was thread_local 方式と同様、実体保持)。
    std::vector<float> delayBuf_[2];
    int writePos_[2] = {0, 0};
    float lfoPhase_ = 0.0f;
    int builtSampleRate_ = -1;
};

} // namespace ArtifactCore
