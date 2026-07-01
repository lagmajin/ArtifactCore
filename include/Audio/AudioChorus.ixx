module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.Chorus;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Chorus / Flange エフェクト
 * LFOでモジュレーションし、コーラス/フレージ効果を生成します。
 * After Effects Flange & Chorus 相当のエフェクト。
 */
class LIBRARY_DLL_API AudioChorus : public AudioEffect {
public:
    enum class Mode { Chorus, Flanger };

    AudioChorus();
    virtual ~AudioChorus() = default;

    std::string getName() const override { return "Chorus"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setMode(Mode mode) { mode_ = mode; }
    void setRate(float rate) { rate_ = rate; }
    void setDepth(float depth) { depth_ = depth; }
    void setFeedback(float fb) { feedback_ = fb; }
    void setDelayMs(float ms) { baseDelayMs_ = ms; }

    Mode getMode() const { return mode_; }
    float getRate() const { return rate_; }
    float getDepth() const { return depth_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const std::string& id, float value) override;
    float getParameterValue(const std::string& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    Mode mode_ = Mode::Chorus;
    float rate_ = 1.5f;
    float depth_ = 0.5f;
    float feedback_ = 0.3f;
    float baseDelayMs_ = 20.0f;

    // Instance-safe buffers (was thread_local)
    std::vector<float> delayBuffer_;
    int delayBufSize_ = 0;
    int writePos_ = 0;
};

} // namespace ArtifactCore