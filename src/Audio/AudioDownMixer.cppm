module;
#include <algorithm>
#include <vector>
#include <QList>
module Audio.DownMixer;

import Audio.Segment;

namespace ArtifactCore {

struct AudioDownMixer::Impl {
    AudioChannelLayout targetLayout_ = AudioChannelLayout::Stereo;
    float centerMixLevel_ = 0.7071f;
    float lfeMixLevel_ = 0.7071f;
    float surroundMixLevel_ = 0.5f;
};

AudioDownMixer::AudioDownMixer() : impl_(new Impl()) {}

AudioDownMixer::~AudioDownMixer() {
    delete impl_;
}

void AudioDownMixer::setTargetLayout(AudioChannelLayout target) {
    impl_->targetLayout_ = target;
}

AudioChannelLayout AudioDownMixer::targetLayout() const {
    return impl_->targetLayout_;
}

void AudioDownMixer::setCenterMixLevel(float level) {
    impl_->centerMixLevel_ = level;
}

void AudioDownMixer::setLFEMixLevel(float level) {
    impl_->lfeMixLevel_ = level;
}

void AudioDownMixer::setSurroundMixLevel(float level) {
    impl_->surroundMixLevel_ = level;
}

AudioSegment AudioDownMixer::process(const AudioSegment& source) const {
    if (source.layout == impl_->targetLayout_) {
        return source; // No conversion needed
    }

    AudioSegment output;
    output.sampleRate = source.sampleRate;
    output.layout = impl_->targetLayout_;
    output.startFrame = source.startFrame;

    int frames = source.frameCount();
    if (frames <= 0) return output;

    if (impl_->targetLayout_ == AudioChannelLayout::Stereo) {
        output.channelData.resize(2);
        output.channelData[0].resize(frames); // L
        output.channelData[1].resize(frames); // R

        if (source.layout == AudioChannelLayout::Surround51 && source.channelCount() >= 6) {
            const float* l = source.constData(0);
            const float* r = source.constData(1);
            const float* c = source.constData(2);
            const float* lfe = source.constData(3);
            const float* ls = source.constData(4);
            const float* rs = source.constData(5);

            float* outL = output.channelData[0].data();
            float* outR = output.channelData[1].data();

            for (int i = 0; i < frames; ++i) {
                float center = c[i] * impl_->centerMixLevel_;
                float lfeSample = lfe[i] * impl_->lfeMixLevel_;
                outL[i] = l[i] + center + lfeSample + (ls[i] * impl_->surroundMixLevel_);
                outR[i] = r[i] + center + lfeSample + (rs[i] * impl_->surroundMixLevel_);
            }
        } 
        else if (source.layout == AudioChannelLayout::Mono && source.channelCount() >= 1) {
            // Mono to Stereo: Dual mono
            const float* mono = source.constData(0);
            std::copy(mono, mono + frames, output.channelData[0].begin());
            std::copy(mono, mono + frames, output.channelData[1].begin());
        }
        else {
            // Fallback: Copy first two channels if available, or fill with zero
            for (int ch = 0; ch < 2; ++ch) {
                if (ch < source.channelCount()) {
                    std::copy(source.constData(ch), source.constData(ch) + frames, output.channelData[ch].begin());
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
                const float* in = source.constData(ch);
                for (int i = 0; i < frames; ++i) {
                    outMono[i] += in[i] * weight;
                }
            }
        }
    }

    return output;
}

} // namespace ArtifactCore