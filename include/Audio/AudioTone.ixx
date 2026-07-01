module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <random>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Audio.Effect.Tone;

import Audio.Effect;
import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Tone エフェクト
 * トーンジェネレータ（正弦波/ノイズ）。
 * After Effects Tone 相当のエフェクト。
 */
class LIBRARY_DLL_API AudioTone : public AudioEffect {
public:
    enum class WaveType { Sine, Square, Sawtooth, Noise };

    AudioTone();
    virtual ~AudioTone() = default;

    std::string getName() const override { return "Tone"; }
    void process(AudioSegment& segment, const AudioSegment* sideChain = nullptr) override;

    void setFrequency(float hz) { frequency_ = hz; }
    void setAmplitude(float amp) { amplitude_ = amp; }
    void setWaveType(WaveType type) { waveType_ = type; }

    float getFrequency() const { return frequency_; }
    float getAmplitude() const { return amplitude_; }

    std::vector<EffectParameter> getParameters() const override;
    void setParameterValue(const std::string& id, float value) override;
    float getParameterValue(const std::string& id) const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& obj) override;

private:
    float frequency_ = 440.0f;
    float amplitude_ = 0.2f;
    WaveType waveType_ = WaveType::Sine;
    float phase_ = 0.0f;

    // Instance-safe rng (was thread_local)
    class RngHolder {
    public:
        RngHolder() : rng(std::random_device{}()) {}
        std::mt19937 rng;
        std::uniform_real_distribution<float> dist{-1.0f, 1.0f};
    } rng_;
};

} // namespace ArtifactCore