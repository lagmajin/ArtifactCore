module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include "../Define/DllExportMacro.hpp"

module Audio.Effect.Waveform;

import Audio.Effect;
import Audio.Segment;

namespace ArtifactCore {

AudioWaveform::AudioWaveform() {
    waveformData_.resize(resolution_, 0.0f);
}

void AudioWaveform::process(AudioSegment& segment, const AudioSegment* /*sideChain*/) {
    if (bypass_) return;

    const int channels = segment.channelCount();
    const int frames = segment.frameCount();
    if (channels == 0 || frames == 0) return;

    const int resolution = std::min(
        std::clamp(resolution_, 1, 1 << 20), frames);
    waveformData_.resize(resolution);
    
    // RMS 波形抽出
    const int step = std::max(1, frames / resolution);
    for (int i = 0; i < resolution; ++i) {
        double sumSq = 0.0;
        int count = 0;
        const std::size_t base = static_cast<std::size_t>(i) *
                                 static_cast<std::size_t>(step);
        for (int j = 0; j < step && base + static_cast<std::size_t>(j) <
                                      static_cast<std::size_t>(frames); ++j) {
            const int frame = static_cast<int>(base + static_cast<std::size_t>(j));
            for (int c = 0; c < channels; ++c) {
                if (c < segment.channelData.size() && frame < segment.channelData[c].size()) {
                    const float s = segment.channelData[c][frame];
                    if (!std::isfinite(s)) continue;
                    sumSq += static_cast<double>(s) * s;
                    ++count;
                }
            }
        }
        const double rms = count > 0 ? std::sqrt(sumSq / count) : 0.0;
        waveformData_[i] = std::isfinite(rms)
            ? static_cast<float>(std::min(
                  rms, static_cast<double>(std::numeric_limits<float>::max())))
            : std::numeric_limits<float>::max();
    }
}

} // namespace ArtifactCore
