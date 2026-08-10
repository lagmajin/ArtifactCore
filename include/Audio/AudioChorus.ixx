module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <cmath>
#include <algorithm>
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

    String getName() const override { return "Chorus"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setMode(Mode mode) { mode_ = mode; }
    void setRate(float rate) {
        rate_ = std::isfinite(rate) ? std::clamp(rate, 0.1f, 10.0f) : 1.5f;
    }
    void setDepth(float depth) {
        depth_ = std::isfinite(depth) ? std::clamp(depth, 0.0f, 1.0f) : 0.5f;
    }
    void setFeedback(float fb) {
        feedback_ = std::isfinite(fb) ? std::clamp(fb, -0.99f, 0.99f) : 0.3f;
    }
    void setDelayMs(float ms) {
        baseDelayMs_ = std::isfinite(ms) ? std::clamp(ms, 1.0f, 50.0f) : 20.0f;
    }

    Mode getMode() const { return mode_; }
    float getRate() const { return rate_; }
    float getDepth() const { return depth_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const String& id, float value) override;
    float getParameterValue(const String& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    Mode mode_ = Mode::Chorus;
    float rate_ = 1.5f;
    float depth_ = 0.5f;
    float feedback_ = 0.3f;
    float baseDelayMs_ = 20.0f;

    // Instance-safe buffers (was thread_local)
    // Keep independent delay history and write positions for L/R. Sharing a
    // single ring between channels leaks one channel's delayed signal into
    // the other whenever a stereo segment is processed.
    std::vector<float> delayBuffer_;
    int delayBufSize_ = 0;
    int writePositions_[2] = {0, 0};
};

} // namespace ArtifactCore
