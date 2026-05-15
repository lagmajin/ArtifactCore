module;
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Audio.DSP.DelayLine;

export namespace ArtifactCore {
namespace Audio {
namespace DSP {

    /**
     * @brief A high-performance fractional delay line for audio DSP based on linear interpolation.
     * Essential for Chorus, Flanger, and modulated Reverb tails (smoothly changing delay times).
     */
    class FractionalDelayLine {
    public:
        FractionalDelayLine() : writeIndex_(0), sampleRate_(48000.0f) {}

        void initialize(float maxDelaySeconds, float sampleRate) {
            sampleRate_ = sampleRate;
            size_t bufferSize = static_cast<size_t>(std::ceil(maxDelaySeconds * sampleRate)) + 4; // Add padding
            buffer_.assign(bufferSize, 0.0f);
            writeIndex_ = 0;
        }

        void clear() {
            std::fill(buffer_.begin(), buffer_.end(), 0.0f);
            writeIndex_ = 0;
        }

        /**
         * @brief Read a delayed sample with a fractional delay time in samples, using linear interpolation.
         */
        inline float read(float delayInSamples) const {
            if (buffer_.empty()) return 0.0f;

            // Ensure delay doesn't exceed buffer
            float maxDelay = static_cast<float>(buffer_.size() - 2);
            if (delayInSamples < 0.0f) delayInSamples = 0.0f;
            if (delayInSamples > maxDelay) delayInSamples = maxDelay;

            // Calculate exact read index
            int32_t intDelay = static_cast<int32_t>(delayInSamples);
            float fracDelay = delayInSamples - static_cast<float>(intDelay);

            int32_t readIndex1 = writeIndex_ - intDelay;
            if (readIndex1 < 0) readIndex1 += buffer_.size();

            int32_t readIndex2 = readIndex1 - 1;
            if (readIndex2 < 0) readIndex2 += buffer_.size();

            // Linear Interpolation
            return buffer_[readIndex1] * (1.0f - fracDelay) + buffer_[readIndex2] * fracDelay;
        }

        /**
         * @brief Read a delayed sample given a delay time in seconds.
         */
        inline float readSeconds(float delayInSeconds) const {
            return read(delayInSeconds * sampleRate_);
        }

        /**
         * @brief Write the next sample and advance the write pointer.
         */
        inline void write(float sample) {
            if (buffer_.empty()) return;
            
            buffer_[writeIndex_] = sample;
            writeIndex_++;
            if (writeIndex_ >= buffer_.size()) {
                writeIndex_ = 0;
            }
        }

    private:
        std::vector<float> buffer_;
        int32_t writeIndex_;
        float sampleRate_;
    };

} // namespace DSP
} // namespace Audio
} // namespace ArtifactCore
