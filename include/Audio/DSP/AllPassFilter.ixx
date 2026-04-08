module;
#include <utility>
export module Audio.DSP.AllPassFilter;

import Audio.DSP.DelayLine;

export namespace ArtifactCore {
namespace Audio {
namespace DSP {

    /**
     * @brief Modulated All-Pass Filter. 
     * Creates density and diffusion in reverb tails without coloring the frequency response.
     */
    class AllPassFilter {
    public:
        AllPassFilter() : feedback_(0.5f), delayInSamples_(0.0f) {}

        void initialize(float maxDelaySeconds, float sampleRate) {
            delayLine_.initialize(maxDelaySeconds, sampleRate);
        }

        void clear() {
            delayLine_.clear();
        }

        void setParameters(float delayInSeconds, float feedback, float sampleRate) {
            delayInSamples_ = delayInSeconds * sampleRate;
            feedback_ = feedback;
        }

        void setParametersSamples(float delayInSamples, float feedback) {
            delayInSamples_ = delayInSamples;
            feedback_ = feedback;
        }

        /**
         * @brief Process a single sample through the all-pass network.
         * The modulation parameter allows for time-varying delays (reduces metallic ringing in reverb).
         */
        inline float process(float input, float modulationSamples = 0.0f) {
            float delayedDelaySamples = delayInSamples_ + modulationSamples;
            
            // 1. Read the delayed signal
            float delayedSignal = delayLine_.read(delayedDelaySamples);

            // 2. Calculate the feedback value
            float toDelayLine = input + (delayedSignal * feedback_);

            // 3. Write to the delay line
            delayLine_.write(toDelayLine);

            // 4. Output the all-pass response
            return -input + toDelayLine - (toDelayLine * feedback_) + delayedSignal;
        }

    private:
        FractionalDelayLine delayLine_;
        float feedback_;
        float delayInSamples_;
    };

} // namespace DSP
} // namespace Audio
} // namespace ArtifactCore
