module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
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

    // パラメータ
    void setMode(Mode mode) { mode_ = mode; }
    void setRate(float rate) { rate_ = rate; }           // Hz (0.1 ~ 10.0)
    void setDepth(float depth) { depth_ = depth; }       // 0.0 ~ 1.0
    void setFeedback(float fb) { feedback_ = fb; }       // -0.99 ~ 0.99
    void setDelayMs(float ms) { baseDelayMs_ = ms; }     // 1.0 ~ 50.0

    Mode getMode() const { return mode_; }
    float getRate() const { return rate_; }
    float getDepth() const { return depth_; }

private:
    Mode mode_ = Mode::Chorus;
    float rate_ = 1.5f;
    float depth_ = 0.5f;
    float feedback_ = 0.3f;
    float baseDelayMs_ = 20.0f;
};

} // namespace ArtifactCore