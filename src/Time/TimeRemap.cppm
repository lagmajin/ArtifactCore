module;

export module Time.TimeRemap;

import std;
import Frame.Rate;

import <cmath>;
import <algorithm>;

namespace ArtifactCore {

// ==================== TimeRemapProcessor ====================

TimeRemapProcessor::TimeRemapProcessor() = default;
TimeRemapProcessor::~TimeRemapProcessor() = default;

void TimeRemapProcessor::setSourceDuration(double seconds) {
    sourceDuration_ = seconds;
    sourceFrameCount_ = static_cast<int>(seconds * frameRate_.framerate());
}

void TimeRemapProcessor::setSourceFrameCount(int frames) {
    sourceFrameCount_ = frames;
    sourceDuration_ = static_cast<double>(frames) / frameRate_.framerate();
}

void TimeRemapProcessor::setFrameRate(const FrameRate& rate) {
    double oldDuration = sourceDuration_;
    frameRate_ = rate;
    // Recalculate frames
    sourceFrameCount_ = static_cast<int>(oldDuration * frameRate_.framerate());
}

void TimeRemapProcessor::addKeyframe(const TimeRemapKeyframe& keyframe) {
    keyframes_.push_back(keyframe);
    // Sort by output time
    std::sort(keyframes_.begin(), keyframes_.end(), 
        [](const TimeRemapKeyframe& a, const TimeRemapKeyframe& b) {
            return a.outputTime < b.outputTime;
        });
}

void TimeRemapProcessor::removeKeyframe(int index) {
    if (index >= 0 && index < keyframes_.size()) {
        keyframes_.remove(index);
    }
}

void TimeRemapProcessor::clearKeyframes() {
    keyframes_.clear();
}

double TimeRemapProcessor::interpolateKeyframes(double outputTime) const {
    if (keyframes_.isEmpty()) {
        // Default: linear mapping
        return outputTime * sourceDuration_ / 10.0; // Assuming 10s output
    }
    
    if (keyframes_.size() == 1) {
        return keyframes_[0].sourceTime;
    }
    
    // Find surrounding keyframes
    int idx = 0;
    for (int i = 0; i < keyframes_.size() - 1; i++) {
        if (outputTime >= keyframes_[i].outputTime && 
            outputTime <= keyframes_[i + 1].outputTime) {
            idx = i;
            break;
        }
    }
    
    const auto& k0 = keyframes_[idx];
    const auto& k1 = keyframes_[idx + 1];
    
    // Normalized time between keyframes
    double t = (outputTime - k0.outputTime) / (k1.outputTime - k0.outputTime);
    t = std::clamp(t, 0.0, 1.0);
    
    // Apply easing
    t = applyEasing(t, k0.interpolation);
    
    // Interpolate source time
    return k0.sourceTime + t * (k1.sourceTime - k0.sourceTime);
}

double TimeRemapProcessor::applyEasing(double t, TimeRemapKeyframe::Interpolation interp) const {
    switch (interp) {
        case TimeRemapKeyframe::Interpolation::Linear:
            return t;
            
        case TimeRemapKeyframe::Interpolation::EaseIn:
            return t * t;
            
        case TimeRemapKeyframe::Interpolation::EaseOut:
            return 1 - (1 - t) * (1 - t);
            
        case TimeRemapKeyframe::Interpolation::EaseInOut:
            return t < 0.5 ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
            
        case TimeRemapKeyframe::Interpolation::Hold:
            return t < 0.5 ? 0.0 : 1.0;
            
        case TimeRemapKeyframe::Interpolation::Bezier:
        default:
            return t;
    }
}

double TimeRemapProcessor::mapOutputToSource(double outputTime) const {
    return interpolateKeyframes(outputTime);
}

double TimeRemapProcessor::mapSourceToOutput(double sourceTime) const {
    // Inverse mapping - binary search
    if (keyframes_.isEmpty()) {
        // Linear: output = source * 10 / duration
        return sourceTime * 10.0 / sourceDuration_;
    }
    
    double low = 0;
    double high = keyframes_.last().outputTime;
    
    for (int iter = 0; iter < 20; iter++) {
        double mid = (low + high) / 2;
        double mapped = interpolateKeyframes(mid);
        
        if (mapped < sourceTime) {
            low = mid;
        } else {
            high = mid;
        }
    }
    
    return (low + high) / 2;
}

int TimeRemapProcessor::getSourceFrameIndex(double outputTime) const {
    double sourceTime = mapOutputToSource(outputTime);
    return static_cast<int>(sourceTime * frameRate_.framerate());
}

void TimeRemapProcessor::setFrameBlendMode(FrameBlendMode mode) {
    blendMode_ = mode;
}

void TimeRemapProcessor::setFrameBlendAmount(float amount) {
    blendAmount_ = std::clamp(amount, 0.0f, 1.0f);
}

double TimeRemapProcessor::getSpeedAtTime(double outputTime) const {
    double delta = 0.001; // Small time step
    double t1 = mapOutputToSource(outputTime - delta);
    double t2 = mapOutputToSource(outputTime + delta);
    
    // Speed = change in source time / change in output time
    return (t2 - t1) / (2 * delta);
}

void TimeRemapProcessor::convertTimesToFrames() {
    for (auto& kf : keyframes_) {
        kf.outputTime = kf.outputTime * frameRate_.framerate();
        kf.sourceTime = kf.sourceTime * frameRate_.framerate();
    }
}

void TimeRemapProcessor::convertFramesToTimes() {
    float invRate = 1.0f / frameRate_.framerate();
    for (auto& kf : keyframes_) {
        kf.outputTime = kf.outputTime * invRate;
        kf.sourceTime = kf.sourceTime * invRate;
    }
}

TimeRemapProcessor TimeRemapProcessor::createConstantSpeed(double speed) {
    TimeRemapProcessor proc;
    TimeRemapKeyframe kf1;
    kf1.outputTime = 0;
    kf1.sourceTime = 0;
    kf1.interpolation = TimeRemapKeyframe::Interpolation::Linear;
    proc.addKeyframe(kf1);
    
    TimeRemapKeyframe kf2;
    kf2.outputTime = 10.0; // 10 seconds
    kf2.sourceTime = 10.0 / speed; // Duration = duration / speed
    kf2.interpolation = TimeRemapKeyframe::Interpolation::Linear;
    proc.addKeyframe(kf2);
    
    return proc;
}

TimeRemapProcessor TimeRemapProcessor::createRampUp(double startSpeed, double endSpeed) {
    TimeRemapProcessor proc;
    
    // Add keyframes at various points
    for (int i = 0; i <= 10; i++) {
        TimeRemapKeyframe kf;
        kf.outputTime = i;
        double t = i / 10.0;
        // Integrate speed over time
        double speed = startSpeed + (endSpeed - startSpeed) * t;
        // Approximate source time
        kf.sourceTime = i * speed;
        kf.interpolation = TimeRemapKeyframe::Interpolation::EaseInOut;
        proc.addKeyframe(kf);
    }
    
    return proc;
}

TimeRemapProcessor TimeRemapProcessor::createRampDown(double startSpeed, double endSpeed) {
    return createRampUp(endSpeed, startSpeed);
}

TimeRemapProcessor TimeRemapProcessor::createHoldAt(double time, double holdDuration) {
    TimeRemapProcessor proc;
    
    TimeRemapKeyframe kf1;
    kf1.outputTime = time;
    kf1.sourceTime = time;
    kf1.interpolation = TimeRemapKeyframe::Interpolation::Linear;
    proc.addKeyframe(kf1);
    
    TimeRemapKeyframe kf2;
    kf2.outputTime = time + holdDuration;
    kf2.sourceTime = time; // Hold at same source time
    kf2.interpolation = TimeRemapKeyframe::Interpolation::Hold;
    proc.addKeyframe(kf2);
    
    TimeRemapKeyframe kf3;
    kf3.outputTime = time + holdDuration + 1.0;
    kf3.sourceTime = time + 1.0;
    kf3.interpolation = TimeRemapKeyframe::Interpolation::Linear;
    proc.addKeyframe(kf3);
    
    return proc;
}

TimeRemapProcessor TimeRemapProcessor::createReverse() {
    TimeRemapProcessor proc;
    proc.setSourceDuration(10.0);
    
    TimeRemapKeyframe kf1;
    kf1.outputTime = 0;
    kf1.sourceTime = 10.0;
    kf1.interpolation = TimeRemapKeyframe::Interpolation::Linear;
    proc.addKeyframe(kf1);
    
    TimeRemapKeyframe kf2;
    kf2.outputTime = 10.0;
    kf2.sourceTime = 0;
    kf2.interpolation = TimeRemapKeyframe::Interpolation::Linear;
    proc.addKeyframe(kf2);
    
    return proc;
}

void TimeRemapProcessor::reset() {
    keyframes_.clear();
    sourceDuration_ = 10.0;
    sourceFrameCount_ = 300;
    frameRate_ = FrameRate(30);
    blendMode_ = FrameBlendMode::None;
    blendAmount_ = 0.5f;
}

// ==================== AudioTimeStretchProcessor ====================

AudioTimeStretchProcessor::AudioTimeStretchProcessor() = default;
AudioTimeStretchProcessor::~AudioTimeStretchProcessor() = default;

void AudioTimeStretchProcessor::setSampleRate(int sampleRate) {
    sampleRate_ = sampleRate;
}

void AudioTimeStretchProcessor::setChannels(int channels) {
    channels_ = channels;
}

void AudioTimeStretchProcessor::setPreservePitch(bool preserve) {
    preservePitch_ = preserve;
}

int AudioTimeStretchProcessor::processTimeStretch(
    const float* inputSamples,
    int inputSampleCount,
    double timeStretchRatio,
    float* outputSamples
) {
    if (preservePitch_) {
        return processTimeStretchFFT(inputSamples, inputSampleCount, timeStretchRatio, outputSamples);
    } else {
        return processTimeStretchSimple(inputSamples, inputSampleCount, timeStretchRatio, outputSamples);
    }
}

int AudioTimeStretchProcessor::processTimeStretchSimple(
    const float* inputSamples,
    int inputSampleCount,
    double timeStretchRatio,
    float* outputSamples
) {
    // Simple rate conversion
    int outputSampleCount = static_cast<int>(inputSampleCount / timeStretchRatio);
    
    for (int i = 0; i < outputSampleCount; i++) {
        double inputIndex = i * timeStretchRatio;
        int idx = static_cast<int>(inputIndex);
        float frac = static_cast<float>(inputIndex - idx);
        
        // Linear interpolation between samples
        if (idx + 1 < inputSampleCount) {
            for (int ch = 0; ch < channels_; ch++) {
                outputSamples[i * channels_ + ch] = 
                    inputSamples[idx * channels_ + ch] * (1.0f - frac) +
                    inputSamples[(idx + 1) * channels_ + ch] * frac;
            }
        } else if (idx < inputSampleCount) {
            for (int ch = 0; ch < channels_; ch++) {
                outputSamples[i * channels_ + ch] = inputSamples[idx * channels_ + ch];
            }
        }
    }
    
    return outputSampleCount;
}

int AudioTimeStretchProcessor::processTimeStretchFFT(
    const float* inputSamples,
    int inputSampleCount,
    double timeStretchRatio,
    float* outputSamples
) {
    // FFT-based time stretching (simplified implementation)
    // For production, use libraries like SoundTouch or Rubber Band
    
    int outputSampleCount = static_cast<int>(inputSampleCount / timeStretchRatio);
    
    // Window size for FFT
    const int windowSize = 2048;
    const int hopSize = windowSize / 4;
    
    // Process in windows
    int inputPos = 0;
    int outputPos = 0;
    
    while (inputPos + windowSize < inputSampleCount) {
        // Apply Hann window (simplified)
        std::vector<float> windowedInput(windowSize * channels_);
        
        for (int i = 0; i < windowSize; i++) {
            float window = 0.5f * (1.0f - std::cos(2.0f * 3.14159f * i / (windowSize - 1)));
            for (int ch = 0; ch < channels_; ch++) {
                windowedInput[i * channels_ + ch] = 
                    inputSamples[(inputPos + i) * channels_ + ch] * window;
            }
        }
        
        // Copy to output (simplified - real implementation would do FFT processing)
        for (int i = 0; i < windowSize && outputPos < outputSampleCount; i++) {
            float fade = 1.0f;
            if (i < hopSize) {
                fade = static_cast<float>(i) / hopSize;
            } else if (i > windowSize - hopSize) {
                fade = static_cast<float>(windowSize - i) / hopSize;
            }
            
            for (int ch = 0; ch < channels_; ch++) {
                outputSamples[outputPos * channels_ + ch] = 
                    windowedInput[i * channels_ + ch] * fade;
            }
            outputPos++;
        }
        
        inputPos += hopSize;
    }
    
    // Copy remaining samples
    while (inputPos < inputSampleCount && outputPos < outputSampleCount) {
        for (int ch = 0; ch < channels_; ch++) {
            outputSamples[outputPos * channels_ + ch] = 
                inputSamples[inputPos * channels_ + ch];
        }
        outputPos++;
        inputPos += static_cast<int>(timeStretchRatio);
    }
    
    return outputSampleCount;
}

// ==================== TimeRemapEffect ====================

TimeRemapEffect::TimeRemapEffect() = default;
TimeRemapEffect::~TimeRemapEffect() = default;

void TimeRemapEffect::setEnabled(bool enabled) {
    enabled_ = enabled;
}

void TimeRemapEffect::setHasAudio(bool hasAudio) {
    hasAudio_ = hasAudio;
}

int TimeRemapEffect::processFrame(
    double outputTime,
    float& blendForward,
    float& blendBackward
) {
    blendForward = 0.0f;
    blendBackward = 0.0f;
    
    if (!enabled_) {
        return static_cast<int>(outputTime * remap_.frameRate().framerate());
    }
    
    // Get source time
    double sourceTime = remap_.mapOutputToSource(outputTime);
    
    // Get frame index
    int frameIndex = static_cast<int>(sourceTime * remap_.frameRate().framerate());
    
    // Handle frame blending for slow motion
    if (remap_.frameBlendMode() == FrameBlendMode::FrameMix) {
        // Calculate fractional part
        float frac = static_cast<float>(sourceTime * remap_.frameRate().framerate() - frameIndex);
        
        // Blend with adjacent frames
        blendForward = frac * remap_.frameBlendAmount();
        blendBackward = (1.0f - frac) * remap_.frameBlendAmount();
    }
    
    return frameIndex;
}

double TimeRemapEffect::getTimeStretchRatio(double outputTime) const {
    double speed = remap_.getSpeedAtTime(outputTime);
    return 1.0 / speed; // Speed > 1 means slow motion (stretch ratio < 1)
}

void TimeRemapEffect::reset() {
    remap_.reset();
    enabled_ = true;
    hasAudio_ = false;
}

} // namespace ArtifactCore
