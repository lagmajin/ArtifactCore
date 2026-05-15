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
            sampleRate_ = sampleRate;
            setFrequency(frequency);
            phase_ = 0.0f;
        }

        void setFrequency(float frequency) {
            phaseIncrement_ = (2.0f * 3.14159265358979323846f * frequency) / sampleRate_;
        }

        inline float process() {
            float out = std::sin(phase_);
            phase_ += phaseIncrement_;
            if (phase_ >= 2.0f * 3.14159265358979323846f) {
                phase_ -= 2.0f * 3.14159265358979323846f;
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
