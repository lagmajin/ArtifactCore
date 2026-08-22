module;
#include <algorithm>
#include <cmath>
#include <cstdint>

export module Audio.Modulation.Modulator;

namespace ArtifactCore {
namespace Audio {
namespace Modulation {
namespace Detail {

    class XorShift32 {
    public:
        explicit XorShift32(std::uint32_t seed = 2463534242u)
            : state_(seed != 0u ? seed : 2463534242u) {}

        void seed(std::uint32_t seed) {
            state_ = seed != 0u ? seed : 2463534242u;
        }

        float nextSigned() {
            state_ ^= state_ << 13;
            state_ ^= state_ >> 17;
            state_ ^= state_ << 5;
            const float unit = static_cast<float>(state_) * (1.0f / 4294967296.0f);
            return unit * 2.0f - 1.0f;
        }

    private:
        std::uint32_t state_;
    };

    inline float sanitizedSampleRate(float sampleRate) {
        return (std::isfinite(sampleRate) && sampleRate > 0.0f) ? sampleRate : 48000.0f;
    }

    inline std::uint32_t nextAutoSeed() {
        static std::uint32_t counter = 0x9E3779B9u;
        counter = counter * 1664525u + 1013904223u;
        return counter;
    }

} // namespace Detail
} // namespace Modulation
} // namespace Audio
} // namespace ArtifactCore

export namespace ArtifactCore {
namespace Audio {
namespace Modulation {

    enum class LfoWaveform : std::uint8_t {
        Sine,
        Triangle,
        Sawtooth,
        Square,
        SampleAndHold
    };

    class IModulatorSource {
    public:
        virtual ~IModulatorSource() = default;

        virtual void setSampleRate(float sampleRate) = 0;
        virtual void reset() = 0;

        virtual float process(std::uint32_t numFrames) = 0;

        virtual bool bipolar() const = 0;
    };

    class LfoSource final : public IModulatorSource {
    public:
        LfoSource() = default;
        explicit LfoSource(LfoWaveform waveform) : waveform_(waveform) {}

        void setWaveform(LfoWaveform waveform) { waveform_ = waveform; }
        LfoWaveform waveform() const { return waveform_; }

        void setFrequency(float hertz) {
            frequency_ = std::isfinite(hertz) && hertz > 0.0f
                ? std::min(hertz, sampleRate_ * 0.5f) : 0.0f;
            updateIncrement();
        }
        float frequency() const { return frequency_; }

        void setPhase(float phase01) {
            if (!std::isfinite(phase01)) return;
            phaseOffset_ = phase01 - std::floor(phase01);
            phase_ = phaseOffset_;
        }
        float phase() const { return phase_; }

        void setPulseWidth(float width01) {
            pulseWidth_ = std::isfinite(width01)
                ? std::clamp(width01, 0.01f, 0.99f) : 0.5f;
        }
        float pulseWidth() const { return pulseWidth_; }

        void setUnipolar(bool unipolar) { unipolar_ = unipolar; }
        bool unipolar() const { return unipolar_; }

        void setSampleRate(float sampleRate) override {
            sampleRate_ = Detail::sanitizedSampleRate(sampleRate);
            updateIncrement();
        }

        void reset() override {
            phase_ = phaseOffset_;
            heldValue_ = 0.0f;
        }

        float process(std::uint32_t numFrames) override {
            const float previousPhase = phase_;
            phase_ += phaseIncrement_ * static_cast<float>(numFrames);

            float value = 0.0f;
            if (waveform_ == LfoWaveform::SampleAndHold) {
                if (std::floor(phase_) != std::floor(previousPhase)) {
                    heldValue_ = random_.nextSigned();
                }
                value = heldValue_;
            } else {
                float wrapped = phase_ - std::floor(phase_);
                value = evaluate(wrapped);
            }

            phase_ -= std::floor(phase_);
            return unipolar_ ? value * 0.5f + 0.5f : value;
        }

        bool bipolar() const override { return !unipolar_; }

    private:
        float evaluate(float phase01) const {
            switch (waveform_) {
                case LfoWaveform::Sine:
                    return std::sin(phase01 * 6.28318530717958647692f);
                case LfoWaveform::Triangle:
                    return phase01 < 0.5f ? (4.0f * phase01 - 1.0f)
                                          : (3.0f - 4.0f * phase01);
                case LfoWaveform::Sawtooth:
                    return 2.0f * phase01 - 1.0f;
                case LfoWaveform::Square:
                    return phase01 < pulseWidth_ ? 1.0f : -1.0f;
                case LfoWaveform::SampleAndHold:
                    break;
            }
            return 0.0f;
        }

        void updateIncrement() {
            phaseIncrement_ = frequency_ / sampleRate_;
            if (!std::isfinite(phaseIncrement_) || phaseIncrement_ < 0.0f) {
                phaseIncrement_ = 0.0f;
            }
        }

        LfoWaveform waveform_{LfoWaveform::Sine};
        float frequency_{1.0f};
        float sampleRate_{48000.0f};
        float phaseIncrement_{frequency_ / sampleRate_};
        float phase_{0.0f};
        float phaseOffset_{0.0f};
        float pulseWidth_{0.5f};
        float heldValue_{0.0f};
        bool unipolar_{false};
        Detail::XorShift32 random_{};
    };

    class AdsrSource final : public IModulatorSource {
    public:
        enum class Stage : std::uint8_t {
            Idle,
            Attack,
            Decay,
            Sustain,
            Release
        };

        void setAttack(float seconds) {
            attackTime_ = finitePositive(seconds);
        }
        void setDecay(float seconds) {
            decayTime_ = finitePositive(seconds);
        }
        void setSustain(float level01) {
            sustainLevel_ = std::isfinite(level01)
                ? std::clamp(level01, 0.0f, 1.0f) : 0.0f;
        }
        void setRelease(float seconds) {
            releaseTime_ = finitePositive(seconds);
        }

        float attack() const { return attackTime_; }
        float decay() const { return decayTime_; }
        float sustain() const { return sustainLevel_; }
        float release() const { return releaseTime_; }
        Stage stage() const { return stage_; }

        void gate(bool on) {
            if (on) {
                stage_ = Stage::Attack;
            } else if (stage_ != Stage::Idle) {
                stage_ = Stage::Release;
            }
        }

        void setSampleRate(float sampleRate) override {
            sampleRate_ = Detail::sanitizedSampleRate(sampleRate);
        }

        void reset() override {
            level_ = 0.0f;
            stage_ = Stage::Idle;
        }

        float process(std::uint32_t numFrames) override {
            const float deltaSeconds =
                static_cast<float>(numFrames) / sampleRate_;

            switch (stage_) {
                case Stage::Attack:
                    level_ += deltaSeconds / attackTime_;
                    if (level_ >= 1.0f) {
                        level_ = 1.0f;
                        stage_ = Stage::Decay;
                    }
                    break;
                case Stage::Decay:
                    level_ -= deltaSeconds / decayTime_;
                    if (level_ <= sustainLevel_) {
                        level_ = sustainLevel_;
                        stage_ = Stage::Sustain;
                    }
                    break;
                case Stage::Sustain:
                    level_ = sustainLevel_;
                    break;
                case Stage::Release:
                    level_ -= deltaSeconds / releaseTime_;
                    if (level_ <= 0.0f) {
                        level_ = 0.0f;
                        stage_ = Stage::Idle;
                    }
                    break;
                case Stage::Idle:
                    break;
            }

            return level_;
        }

        bool bipolar() const override { return false; }

    private:
        static float finitePositive(float seconds) {
            return (std::isfinite(seconds) && seconds > 0.0f)
                ? seconds : 0.000001f;
        }

        float attackTime_{0.01f};
        float decayTime_{0.1f};
        float sustainLevel_{0.7f};
        float releaseTime_{0.2f};
        float sampleRate_{48000.0f};
        float level_{0.0f};
        Stage stage_{Stage::Idle};
    };

    class RandomSource final : public IModulatorSource {
    public:
        void setRate(float hertz) {
            rate_ = std::isfinite(hertz) && hertz > 0.0f
                ? std::min(hertz, 1000.0f) : 0.0f;
        }
        float rate() const { return rate_; }

        void setSmoothing(float seconds) {
            smoothingTime_ = std::isfinite(seconds) && seconds > 0.0f
                ? seconds : 0.0f;
        }
        float smoothing() const { return smoothingTime_; }

        void setSeed(std::uint32_t seed) { random_.seed(seed); }

        void setUnipolar(bool unipolar) { unipolar_ = unipolar; }
        bool unipolar() const { return unipolar_; }

        void setSampleRate(float sampleRate) override {
            sampleRate_ = Detail::sanitizedSampleRate(sampleRate);
        }

        void reset() override {
            heldValue_ = random_.nextSigned();
            value_ = heldValue_;
            framesUntilDraw_ = 0.0;
        }

        float process(std::uint32_t numFrames) override {
            if (rate_ > 0.0f) {
                framesUntilDraw_ -= static_cast<double>(numFrames);
                const double periodSamples =
                    static_cast<double>(sampleRate_) / static_cast<double>(rate_);
                while (framesUntilDraw_ <= 0.0) {
                    heldValue_ = random_.nextSigned();
                    framesUntilDraw_ += periodSamples;
                }
            }

            const float deltaSeconds =
                static_cast<float>(numFrames) / sampleRate_;
            const float coefficient = smoothingTime_ > 0.0f
                ? 1.0f - std::exp(-deltaSeconds / smoothingTime_)
                : 1.0f;
            value_ += (heldValue_ - value_) * coefficient;

            return unipolar_ ? value_ * 0.5f + 0.5f : value_;
        }

        bool bipolar() const override { return !unipolar_; }

    private:
        float rate_{1.0f};
        float smoothingTime_{0.005f};
        float sampleRate_{48000.0f};
        float heldValue_{0.0f};
        float value_{0.0f};
        double framesUntilDraw_{0.0};
        bool unipolar_{false};
        Detail::XorShift32 random_{Detail::nextAutoSeed()};
    };

} // namespace Modulation
} // namespace Audio
} // namespace ArtifactCore
