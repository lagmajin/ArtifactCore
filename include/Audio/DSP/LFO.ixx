module;
#include <utility>
#include <cmath>

export module Audio.DSP.LFO;

export namespace ArtifactCore {
namespace Audio {
namespace DSP {

    /**
     * @brief A simple Low Frequency Oscillator (Sine Wave) for modulating delay lines in Reverb/Chorus.
     */
    class LFO {
    public:
        LFO() : phase_(0.0f), phaseIncrement_(0.0f), sampleRate_(48000.0f) {}

        void initialize(float frequency, float sampleRate) {
            sampleRate_ = std::isfinite(sampleRate) && sampleRate > 0.0f
                ? sampleRate : 48000.0f;
            setFrequency(frequency);
            phase_ = 0.0f;
        }

        void setFrequency(float frequency) {
            const float safeFrequency = std::isfinite(frequency) ? frequency : 0.0f;
            phaseIncrement_ = (2.0f * 3.14159265358979323846f * safeFrequency) /
                              sampleRate_;
            if (!std::isfinite(phaseIncrement_)) {
                phaseIncrement_ = 0.0f;
            }
        }

        inline float process() {
            float out = std::sin(phase_);
            phase_ += phaseIncrement_;
            constexpr float twoPi = 2.0f * 3.14159265358979323846f;
            if (!std::isfinite(phase_)) {
                phase_ = 0.0f;
            } else if (phase_ >= twoPi || phase_ < 0.0f) {
                phase_ = std::fmod(phase_, twoPi);
                if (phase_ < 0.0f) phase_ += twoPi;
            }
            return out;
        }

    private:
        float phase_;
        float phaseIncrement_;
        float sampleRate_;
    };

} // namespace DSP
} // namespace Audio
} // namespace ArtifactCore
