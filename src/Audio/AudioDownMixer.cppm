module;
#include <utility>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
module Audio.DownMixer;

import Audio.Segment;

namespace ArtifactCore {

namespace {

float sanitizeMixedSample(const float sample)
{
    if (std::isfinite(sample)) return sample;
    if (std::isnan(sample)) return 0.0f;
    return std::copysign(std::numeric_limits<float>::max(), sample);
}

}

struct AudioDownMixer::Impl {
    AudioChannelLayout targetLayout_ = AudioChannelLayout::Stereo;
    float centerMixLevel_ = 0.7071f;
    float lfeMixLevel_ = 0.7071f;
    float surroundMixLevel_ = 0.5f;
    float backMixLevel_ = 0.5f;  // 7.1 back channel mix (-6dB default)
};

AudioDownMixer::AudioDownMixer() : impl_(new Impl()) {}

AudioDownMixer::~AudioDownMixer() {
    delete impl_;
}

void AudioDownMixer::setTargetLayout(AudioChannelLayout target) {
    switch (target) {
    case AudioChannelLayout::Mono:
    case AudioChannelLayout::Stereo:
    case AudioChannelLayout::Surround51:
    case AudioChannelLayout::Surround71:
    case AudioChannelLayout::Custom10ch:
        impl_->targetLayout_ = target;
        break;
    default:
        impl_->targetLayout_ = AudioChannelLayout::Stereo;
        break;
    }
}

AudioChannelLayout AudioDownMixer::targetLayout() const {
    return impl_->targetLayout_;
}

void AudioDownMixer::setCenterMixLevel(float level) {
    impl_->centerMixLevel_ = std::isfinite(level) ? level : 0.7071f;
}

void AudioDownMixer::setLFEMixLevel(float level) {
    impl_->lfeMixLevel_ = std::isfinite(level) ? level : 0.7071f;
}

void AudioDownMixer::setSurroundMixLevel(float level) {
    impl_->surroundMixLevel_ = std::isfinite(level) ? level : 0.5f;
}

void AudioDownMixer::setBackMixLevel(float level) {
    impl_->backMixLevel_ = std::isfinite(level) ? level : 0.5f;
}

float AudioDownMixer::backMixLevel() const {
    return impl_->backMixLevel_;
}

AudioSegment AudioDownMixer::processChannelMap(
    const AudioSegment& source,
    const QVector<int>& sourceChannelForOutput) const
{
    AudioSegment output;
    output.sampleRate = source.sampleRate;
    output.layout = impl_->targetLayout_;
    output.startFrame = source.startFrame;
    const int frames = source.frameCount();
    if (frames <= 0 || sourceChannelForOutput.isEmpty()) {
        return output;
    }
    output.channelData.resize(sourceChannelForOutput.size());
    for (int outputChannel = 0;
         outputChannel < sourceChannelForOutput.size();
         ++outputChannel) {
        auto& destination = output.channelData[outputChannel];
        destination.fill(0.0f, frames);
        const int sourceChannel = sourceChannelForOutput[outputChannel];
        if (sourceChannel < 0 || sourceChannel >= source.channelCount()) {
            continue;
        }
        const auto& input = source.channelData[sourceChannel];
        const int copyFrames = std::min(frames, static_cast<int>(input.size()));
        for (int frame = 0; frame < copyFrames; ++frame) {
            destination[frame] = std::isfinite(input[frame]) ? input[frame] : 0.0f;
        }
    }
    return output;
}

AudioSegment AudioDownMixer::process(const AudioSegment& source) const {
    const auto expectedChannelCount = [](AudioChannelLayout layout) {
        switch (layout) {
        case AudioChannelLayout::Mono: return 1;
        case AudioChannelLayout::Stereo: return 2;
        case AudioChannelLayout::Surround51: return 6;
        case AudioChannelLayout::Surround71: return 8;
        case AudioChannelLayout::Custom10ch: return 10;
        case AudioChannelLayout::Ambisonics: return 0;
        }
        return 0;
    };
    const int expectedChannels = expectedChannelCount(impl_->targetLayout_);
    const int sourceFrames = source.frameCount();
    const bool uniformFrameShape = std::all_of(
        source.channelData.cbegin(), source.channelData.cend(),
        [sourceFrames](const auto& channel) {
            return static_cast<int>(channel.size()) == sourceFrames;
        });
    if (source.layout == impl_->targetLayout_ &&
        (expectedChannels == 0 || source.channelCount() == expectedChannels) &&
        uniformFrameShape) {
        AudioSegment output = source;
        for (auto& channel : output.channelData) {
            for (float& sample : channel) {
                sample = sanitizeMixedSample(sample);
            }
        }
        return output; // No conversion needed
    }

    AudioSegment output;
    output.sampleRate = source.sampleRate;
    output.layout = impl_->targetLayout_;
    output.startFrame = source.startFrame;

    int frames = sourceFrames;
    if (frames <= 0) return output;
    const auto sampleAt = [](const auto& channel, int frame) {
        if (frame < 0 || frame >= static_cast<int>(channel.size())) {
            return 0.0f;
        }
        return std::isfinite(channel[frame]) ? channel[frame] : 0.0f;
    };

    if (impl_->targetLayout_ == AudioChannelLayout::Stereo) {
        output.channelData.resize(2);
        output.channelData[0].resize(frames); // L
        output.channelData[1].resize(frames); // R
        output.channelData[0].fill(0.0f);
        output.channelData[1].fill(0.0f);

        if (source.layout == AudioChannelLayout::Surround51 && source.channelCount() >= 6) {
            float* outL = output.channelData[0].data();
            float* outR = output.channelData[1].data();

            for (int i = 0; i < frames; ++i) {
                float center = sampleAt(source.channelData[2], i) * impl_->centerMixLevel_;
                float lfeSample = sampleAt(source.channelData[3], i) * impl_->lfeMixLevel_;
                outL[i] = sampleAt(source.channelData[0], i) + center + lfeSample +
                          (sampleAt(source.channelData[4], i) * impl_->surroundMixLevel_);
                outR[i] = sampleAt(source.channelData[1], i) + center + lfeSample +
                          (sampleAt(source.channelData[5], i) * impl_->surroundMixLevel_);
            }
        } 
        else if (source.layout == AudioChannelLayout::Mono && source.channelCount() >= 1) {
            // Mono to Stereo: Dual mono
            for (int i = 0; i < frames; ++i) {
                const float sample = sampleAt(source.channelData[0], i);
                output.channelData[0][i] = sample;
                output.channelData[1][i] = sample;
            }
        }
        else if (source.layout == AudioChannelLayout::Surround71 && source.channelCount() >= 8) {
            // 7.1 -> Stereo (ITU-R BS.775)
            float* outL = output.channelData[0].data();
            float* outR = output.channelData[1].data();

            for (int i = 0; i < frames; ++i) {
                float center = sampleAt(source.channelData[2], i) * impl_->centerMixLevel_;
                float lfeSample = sampleAt(source.channelData[3], i) * impl_->lfeMixLevel_;
                outL[i] = sampleAt(source.channelData[0], i) + center + lfeSample
                       + (sampleAt(source.channelData[4], i) * impl_->surroundMixLevel_)
                       + (sampleAt(source.channelData[6], i) * impl_->backMixLevel_);
                outR[i] = sampleAt(source.channelData[1], i) + center + lfeSample
                       + (sampleAt(source.channelData[5], i) * impl_->surroundMixLevel_)
                       + (sampleAt(source.channelData[7], i) * impl_->backMixLevel_);
            }
        }
        else {
            // Fallback: Copy first two channels if available, or fill with zero
            for (int ch = 0; ch < 2; ++ch) {
                if (ch < source.channelCount()) {
                    for (int i = 0; i < frames; ++i) {
                        output.channelData[ch][i] = sampleAt(source.channelData[ch], i);
                    }
                } else {
                    std::fill(output.channelData[ch].begin(), output.channelData[ch].end(), 0.0f);
                }
            }
        }
    } 
    else if (impl_->targetLayout_ == AudioChannelLayout::Mono) {
        output.channelData.resize(1);
        output.channelData[0].resize(frames);
        
        float* outMono = output.channelData[0].data();
        std::fill(outMono, outMono + frames, 0.0f);

        int inChannels = source.channelCount();
        if (inChannels > 0) {
            float weight = 1.0f / inChannels;
            for (int ch = 0; ch < inChannels; ++ch) {
                for (int i = 0; i < frames; ++i) {
                    outMono[i] += sampleAt(source.channelData[ch], i) * weight;
                }
            }
        }
    }
    else if (impl_->targetLayout_ == AudioChannelLayout::Surround51 ||
             impl_->targetLayout_ == AudioChannelLayout::Surround71) {
        const int targetChannels =
            impl_->targetLayout_ == AudioChannelLayout::Surround71 ? 8 : 6;
        output.channelData.resize(targetChannels);
        for (auto& channel : output.channelData) {
            channel.fill(0.0f, frames);
        }

        // Preserve the standard L/R/C/LFE/surround ordering when promoting
        // stereo/5.1 input to a larger multichannel bus.
        if (impl_->targetLayout_ == AudioChannelLayout::Surround51 &&
            source.layout == AudioChannelLayout::Surround71 &&
            source.channelCount() >= 8) {
            for (int channel = 0; channel < 4; ++channel) {
                const auto& input = source.channelData[channel];
                const int copyFrames = std::min(
                    frames, static_cast<int>(input.size()));
                if (copyFrames > 0) {
                    for (int frame = 0; frame < copyFrames; ++frame) {
                        output.channelData[channel][frame] =
                            std::isfinite(input[frame]) ? input[frame] : 0.0f;
                    }
                }
            }
            const auto& leftSurround = source.channelData[4];
            const auto& rightSurround = source.channelData[5];
            const auto& leftBack = source.channelData[6];
            const auto& rightBack = source.channelData[7];
            const int copyFrames = std::min({
                frames, static_cast<int>(leftSurround.size()),
                static_cast<int>(rightSurround.size()),
                static_cast<int>(leftBack.size()),
                static_cast<int>(rightBack.size())});
            for (int i = 0; i < copyFrames; ++i) {
                output.channelData[4][i] =
                    leftSurround[i] + leftBack[i] * impl_->backMixLevel_;
                output.channelData[5][i] =
                    rightSurround[i] + rightBack[i] * impl_->backMixLevel_;
            }
        } else if (source.layout == AudioChannelLayout::Mono &&
                   source.channelCount() >= 1) {
            for (int i = 0; i < frames; ++i) {
                const float sample = sampleAt(source.channelData[0], i);
                output.channelData[0][i] = sample;
                output.channelData[1][i] = sample;
            }
        } else {
            const int copyChannels = std::min(
                targetChannels, source.channelCount());
            for (int channel = 0; channel < copyChannels; ++channel) {
                const auto& input = source.channelData[channel];
                const int copyFrames = std::min(
                    frames, static_cast<int>(input.size()));
                if (copyFrames > 0) {
                    for (int frame = 0; frame < copyFrames; ++frame) {
                        output.channelData[channel][frame] =
                            std::isfinite(input[frame]) ? input[frame] : 0.0f;
                    }
                }
            }
        }
    } else if (impl_->targetLayout_ == AudioChannelLayout::Custom10ch) {
        // Preserve the channels that are present when promoting a malformed
        // or differently shaped source into the explicit 10-channel layout.
        constexpr int targetChannels = 10;
        output.channelData.resize(targetChannels);
        for (auto& channel : output.channelData) {
            channel.fill(0.0f, frames);
        }

        const int copyChannels = std::min(targetChannels, source.channelCount());
        for (int channel = 0; channel < copyChannels; ++channel) {
            const auto& input = source.channelData[channel];
            const int copyFrames = std::min(frames, static_cast<int>(input.size()));
            for (int frame = 0; frame < copyFrames; ++frame) {
                output.channelData[channel][frame] =
                    std::isfinite(input[frame]) ? input[frame] : 0.0f;
            }
        }
    }

    for (auto& channel : output.channelData) {
        for (float& sample : channel) {
            sample = sanitizeMixedSample(sample);
        }
    }
    return output;
}

} // namespace ArtifactCore
