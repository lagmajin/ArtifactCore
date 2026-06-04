module;
#include <algorithm>
#include <cmath>
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

    waveformData_.resize(resolution_);
    
    // RMS 波形抽出
    const int step = std::max(1, frames / resolution_);
    for (int i = 0; i < resolution_; ++i) {
        float sumSq = 0.0f;
        int count = 0;
        for (int j = 0; j < step && (i * step + j) < frames; ++j) {
            for (int c = 0; c < channels; ++c) {
                if (c < segment.channelData.size() && (i * step + j) < segment.channelData[c].size()) {
                    float s = segment.channelData[c][i * step + j];
                    sumSq += s * s;
                    ++count;
                }
            }
        }
        waveformData_[i] = (count > 0) ? std::sqrt(sumSq / count) : 0.0f;
    }
}

} // namespace ArtifactCore