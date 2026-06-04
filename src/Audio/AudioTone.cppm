module;
#include <algorithm>
#include <cmath>
#include <random>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Tone;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioTone::AudioTone() {
    phase_ = 0.0f;
}

void AudioTone::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    const float sampleRate = static_cast<float>(segment.sampleRate);

    if (channels == 0 || frames == 0) return;

    static thread_local std::mt19937 rng{std::random_device{}()};
    static thread_local std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);

    for (int ch = 0; ch < std::min(channels, 2); ++ch) {
        if (ch >= segment.channelData.size()) break;
        float* data = segment.channelData[ch].data();

        for (int i = 0; i < frames; ++i) {
            float sample = 0.0f;
            switch (waveType_) {
                case WaveType::Sine:
                    sample = std::sin(phase_ * 2.0f * 3.14159265f);
                    break;
                case WaveType::Square:
                    sample = (std::sin(phase_ * 2.0f * 3.14159265f) > 0) ? 1.0f : -1.0f;
                    break;
                case WaveType::Sawtooth:
                    sample = 2.0f * (phase_ - std::floor(phase_ + 0.5f));
                    break;
                case WaveType::Noise:
                    sample = noiseDist(rng);
                    break;
            }
            data[i] += sample * amplitude_;
            
            phase_ += frequency_ / sampleRate;
            if (phase_ >= 1.0f) phase_ -= 1.0f;
        }
    }
}

} // namespace ArtifactCore