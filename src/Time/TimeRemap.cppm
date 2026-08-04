module;
#include <utility>

#include <algorithm>
#include <cmath>
#include <compare>
#include <QtCore/QList>
#include <QtCore/QPointF>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
module Time.TimeRemap;

import Frame.Rate;
import Analyze.OpticalFlow;
namespace ArtifactCore {

namespace {

constexpr float kPi = 3.14159265358979323846f;

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

// ==================== TimeRemapProcessor ====================

TimeRemapProcessor::TimeRemapProcessor() = default;
TimeRemapProcessor::~TimeRemapProcessor() = default;

void TimeRemapProcessor::setSourceDuration(double seconds) {
    const double safeDuration = std::isfinite(seconds) ? std::max(0.0, seconds) : 0.0;
    const double safeRate = std::isfinite(frameRate_.framerate()) &&
                            frameRate_.framerate() > 0.0
        ? frameRate_.framerate() : 30.0;
    sourceDuration_ = safeDuration;
    sourceFrameCount_ = std::max(0, static_cast<int>(std::ceil(safeDuration * safeRate)));
}

void TimeRemapProcessor::setSourceFrameCount(int frames) {
    const int safeFrames = std::max(0, frames);
    const double safeRate = std::isfinite(frameRate_.framerate()) &&
                            frameRate_.framerate() > 0.0
        ? frameRate_.framerate() : 30.0;
    sourceFrameCount_ = safeFrames;
    sourceDuration_ = static_cast<double>(safeFrames) / safeRate;
}

void TimeRemapProcessor::setFrameRate(const FrameRate& rate) {
    double oldDuration = sourceDuration_;
    frameRate_ = rate;
    const double safeRate = std::isfinite(frameRate_.framerate()) &&
                            frameRate_.framerate() > 0.0
        ? frameRate_.framerate() : 30.0;
    sourceFrameCount_ = std::max(0, static_cast<int>(oldDuration * safeRate));
}

void TimeRemapProcessor::addKeyframe(const TimeRemapKeyframe& keyframe) {
    if (!std::isfinite(keyframe.outputTime) || !std::isfinite(keyframe.sourceTime)) {
        return;
    }
    for (auto& existing : keyframes_) {
        if (existing.outputTime == keyframe.outputTime) {
            existing = keyframe;
            return;
        }
    }
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

void TimeRemapProcessor::setKeyframes(const QVector<TimeRemapKeyframe>& frames) {
    keyframes_.clear();
    for (const auto& keyframe : frames) {
        addKeyframe(keyframe);
    }
}

double TimeRemapProcessor::interpolateKeyframes(double outputTime) const {
    if (keyframes_.isEmpty()) {
        // Default: linear mapping
        return outputTime * sourceDuration_ / 10.0; // Assuming 10s output
    }
    
    if (keyframes_.size() == 1) {
        return keyframes_[0].sourceTime;
    }
    
    if (outputTime <= keyframes_.front().outputTime) {
        return keyframes_.front().sourceTime;
    }
    if (outputTime >= keyframes_.back().outputTime) {
        return keyframes_.back().sourceTime;
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
    const double outputSpan = k1.outputTime - k0.outputTime;
    if (outputSpan <= 0.0) {
        return k1.sourceTime;
    }
    double t = (outputTime - k0.outputTime) / outputSpan;
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
        return sourceDuration_ > 0.0 ? sourceTime * 10.0 / sourceDuration_ : 0.0;
    }

    double low = 0;
    double high = keyframes_.last().outputTime;
    const bool increasing = keyframes_.last().sourceTime >= keyframes_.front().sourceTime;
    
    for (int iter = 0; iter < 20; iter++) {
        double mid = (low + high) / 2;
        double mapped = interpolateKeyframes(mid);
        
        if ((increasing && mapped < sourceTime) || (!increasing && mapped > sourceTime)) {
            low = mid;
        } else {
            high = mid;
        }
    }
    
    return (low + high) / 2;
}

int TimeRemapProcessor::getSourceFrameIndex(double outputTime) const {
    double sourceTime = mapOutputToSource(outputTime);
    const double safeRate = std::isfinite(frameRate_.framerate()) &&
                            frameRate_.framerate() > 0.0
        ? frameRate_.framerate() : 30.0;
    if (!std::isfinite(sourceTime) || sourceFrameCount_ <= 0) {
        return 0;
    }
    const int frameIndex = static_cast<int>(std::floor(
        std::max(0.0, sourceTime) * safeRate));
    return std::clamp(frameIndex, 0, std::max(0, sourceFrameCount_ - 1));
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
    const double safeRate = std::isfinite(frameRate_.framerate()) &&
                            frameRate_.framerate() > 0.0
        ? frameRate_.framerate() : 30.0;
    for (auto& kf : keyframes_) {
        kf.outputTime *= safeRate;
        kf.sourceTime *= safeRate;
    }
}

void TimeRemapProcessor::convertFramesToTimes() {
    const double safeRate = std::isfinite(frameRate_.framerate()) &&
                            frameRate_.framerate() > 0.0
        ? frameRate_.framerate() : 30.0;
    const double invRate = 1.0 / safeRate;
    for (auto& kf : keyframes_) {
        kf.outputTime = kf.outputTime * invRate;
        kf.sourceTime = kf.sourceTime * invRate;
    }
}

TimeRemapProcessor TimeRemapProcessor::createConstantSpeed(double speed) {
    TimeRemapProcessor proc;
    const double safeSpeed = std::isfinite(speed) && std::abs(speed) > 1.0e-6
        ? speed : 1.0;
    TimeRemapKeyframe kf1;
    kf1.outputTime = 0;
    kf1.sourceTime = 0;
    kf1.interpolation = TimeRemapKeyframe::Interpolation::Linear;
    proc.addKeyframe(kf1);
    
    TimeRemapKeyframe kf2;
    kf2.outputTime = 10.0; // 10 seconds
    kf2.sourceTime = 10.0 / safeSpeed; // Duration = duration / speed
    kf2.interpolation = TimeRemapKeyframe::Interpolation::Linear;
    proc.addKeyframe(kf2);
    
    return proc;
}

TimeRemapProcessor TimeRemapProcessor::createRampUp(double startSpeed, double endSpeed) {
    TimeRemapProcessor proc;
    const double safeStart = std::isfinite(startSpeed) ? startSpeed : 1.0;
    const double safeEnd = std::isfinite(endSpeed) ? endSpeed : safeStart;
    
    // Add keyframes at various points
    for (int i = 0; i <= 10; i++) {
        TimeRemapKeyframe kf;
        kf.outputTime = i;
        double t = i / 10.0;
        // Integrate speed over time
        // Integral of the linear speed ramp from 0 to output time i.
        kf.sourceTime = safeStart * i + 0.5 * (safeEnd - safeStart) * i * t;
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

TimeRemapProcessor TimeRemapProcessor::createSuperSlowMotion(double speedScale, double durationSeconds) {
    TimeRemapProcessor proc;
    const double safeSpeed = std::max(speedScale, 0.01);
    const double safeDuration = std::max(durationSeconds, 0.0);

    proc.setSourceDuration(safeDuration);
    proc.setFrameBlendMode(FrameBlendMode::FrameMix);
    proc.setFrameBlendAmount(0.85f);

    TimeRemapKeyframe start;
    start.outputTime = 0.0;
    start.sourceTime = 0.0;
    start.interpolation = TimeRemapKeyframe::Interpolation::Linear;
    proc.addKeyframe(start);

    TimeRemapKeyframe end;
    end.outputTime = safeDuration / safeSpeed;
    end.sourceTime = safeDuration;
    end.interpolation = TimeRemapKeyframe::Interpolation::EaseInOut;
    proc.addKeyframe(end);

    return proc;
}

// ==================== AudioTimeStretchProcessor ====================

AudioTimeStretchProcessor::AudioTimeStretchProcessor() = default;
AudioTimeStretchProcessor::~AudioTimeStretchProcessor() = default;

void AudioTimeStretchProcessor::setSampleRate(int sampleRate) {
    sampleRate_ = std::max(1, sampleRate);
}

void AudioTimeStretchProcessor::setChannels(int channels) {
    channels_ = std::max(1, channels);
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
    if (!inputSamples || !outputSamples || inputSampleCount <= 0 || channels_ <= 0 ||
        !std::isfinite(timeStretchRatio) || timeStretchRatio <= 0.0) {
        return 0;
    }
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
    const int outputSampleCount = std::max(1, static_cast<int>(std::ceil(
        static_cast<double>(inputSampleCount) / timeStretchRatio)));
    
    for (int i = 0; i < outputSampleCount; i++) {
        double inputIndex = i * timeStretchRatio;
        const int idx = std::min(inputSampleCount - 1,
                                 static_cast<int>(inputIndex));
        float frac = static_cast<float>(inputIndex - idx);
        
        // Linear interpolation between samples
        if (idx + 1 < inputSampleCount) {
            for (int ch = 0; ch < channels_; ch++) {
                outputSamples[i * channels_ + ch] = 
                    inputSamples[idx * channels_ + ch] * (1.0f - frac) +
                    inputSamples[(idx + 1) * channels_ + ch] * frac;
            }
        } else if (idx >= 0 && idx < inputSampleCount) {
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
    if (!inputSamples || !outputSamples || inputSampleCount <= 0 || channels_ <= 0 ||
        !std::isfinite(timeStretchRatio) || timeStretchRatio <= 0.0) {
        return 0;
    }

    const int outputSampleCount = std::max(1, static_cast<int>(
        std::ceil(static_cast<double>(inputSampleCount) / timeStretchRatio)));
    constexpr int windowSize = 2048;
    constexpr int analysisHop = windowSize / 4;
    const int synthesisHop = std::max(1, static_cast<int>(
        std::lround(static_cast<double>(analysisHop) / timeStretchRatio)));
    const double pi = 3.14159265358979323846;

    std::fill(outputSamples, outputSamples + outputSampleCount * channels_, 0.0f);
    std::vector<float> overlapWeight(static_cast<std::size_t>(outputSampleCount), 0.0f);
    std::vector<float> grain(static_cast<std::size_t>(windowSize * channels_), 0.0f);

    for (int inputPos = 0, outputPos = 0;
         inputPos < inputSampleCount && outputPos < outputSampleCount;
         inputPos += analysisHop, outputPos += synthesisHop) {
        for (int i = 0; i < windowSize; ++i) {
            const int sourceIndex = std::min(inputSampleCount - 1, inputPos + i);
            const float phase = static_cast<float>(i) / static_cast<float>(windowSize - 1);
            const float window = 0.5f - 0.5f * std::cos(static_cast<float>(2.0 * pi) * phase);
            for (int ch = 0; ch < channels_; ++ch) {
                grain[static_cast<std::size_t>(i * channels_ + ch)] =
                    inputSamples[sourceIndex * channels_ + ch] * window;
            }
        }

        const int writable = std::min(windowSize, outputSampleCount - outputPos);
        for (int i = 0; i < writable; ++i) {
            const int destination = outputPos + i;
            const float phase = static_cast<float>(i) / static_cast<float>(windowSize - 1);
            const float window = 0.5f - 0.5f * std::cos(static_cast<float>(2.0 * pi) * phase);
            overlapWeight[static_cast<std::size_t>(destination)] += window;
            for (int ch = 0; ch < channels_; ++ch) {
                outputSamples[destination * channels_ + ch] +=
                    grain[static_cast<std::size_t>(i * channels_ + ch)];
            }
        }
    }

    for (int frame = 0; frame < outputSampleCount; ++frame) {
        const float weight = overlapWeight[static_cast<std::size_t>(frame)];
        if (weight > 1.0e-6f) {
            for (int ch = 0; ch < channels_; ++ch) {
                outputSamples[frame * channels_ + ch] /= weight;
            }
        }
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

    const double safeRate = std::isfinite(remap_.frameRate().framerate()) &&
                            remap_.frameRate().framerate() > 0.0
        ? remap_.frameRate().framerate() : 30.0;
    const double safeOutputTime = std::isfinite(outputTime) ? outputTime : 0.0;
    
    if (!enabled_) {
        return std::max(0, static_cast<int>(safeOutputTime * safeRate));
    }
    
    // Get source time
    double sourceTime = remap_.mapOutputToSource(safeOutputTime);
    if (!std::isfinite(sourceTime)) {
        sourceTime = 0.0;
    }
    
    // Get frame index
    const int frameIndex = std::max(0, static_cast<int>(sourceTime * safeRate));
    
    // Handle frame blending for slow motion
    if (remap_.frameBlendMode() == FrameBlendMode::FrameMix) {
        // Calculate fractional part
        const float frac = std::clamp(static_cast<float>(
            sourceTime * safeRate - frameIndex), 0.0f, 1.0f);
        
        // Blend with adjacent frames
        blendForward = frac * remap_.frameBlendAmount();
        blendBackward = (1.0f - frac) * remap_.frameBlendAmount();
    }
    
    return frameIndex;
}

bool TimeRemapEffect::needsFrameBlending(double outputTime) const {
    if (!enabled_) {
        return false;
    }
    
    FrameBlendMode mode = remap_.frameBlendMode();
    if (mode == FrameBlendMode::None) {
        return false;
    }
    
    // Check if we're in slow motion (time stretch > 1.0)
    const double speed = remap_.getSpeedAtTime(
        std::isfinite(outputTime) ? outputTime : 0.0);
    if (!std::isfinite(speed)) {
        return false;
    }
    if (std::abs(speed) < 1.0) {
        // Slow motion - blending needed
        return true;
    }
    
    return false;
}

QImage TimeRemapEffect::processFrameBlending(
    double outputTime,
    const QImage& currentFrame,
    const QImage& nextFrame,
    const QImage& prevFrame,
    int64_t frameNumber
) {
    if (currentFrame.isNull()) {
        return currentFrame;
    }
    
    if (!enabled_) {
        return currentFrame;
    }
    
    FrameBlendMode mode = remap_.frameBlendMode();
    if (mode == FrameBlendMode::None) {
        return currentFrame;
    }
    
    // Get blend factors
    float blendForward = 0.0f;
    float blendBackward = 0.0f;
    int frameIndex = processFrame(outputTime, blendForward, blendBackward);
    (void)frameIndex;
    
    if (mode == FrameBlendMode::FrameMix) {
        // Simple frame mixing
        if (blendForward <= 0.0f && blendBackward <= 0.0f) {
            return currentFrame;
        }
        
        // Blend current frame with adjacent frames
        QImage result = currentFrame.copy();
        QPainter painter(&result);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        
        if (blendForward > 0.0f && !nextFrame.isNull()) {
            painter.setOpacity(blendForward);
            painter.drawImage(0, 0, nextFrame);
        }
        
        if (blendBackward > 0.0f && !prevFrame.isNull()) {
            painter.setOpacity(blendBackward);
            painter.drawImage(0, 0, prevFrame);
        }
        
        return result;
    }
    
    if (mode == FrameBlendMode::MotionBlur) {
        // Estimate image-space translation from adjacent frames when no
        // transform velocity is available.  This keeps motion blur useful
        // for footage and raster layers as well as animated transforms.
        const QImage reference = !prevFrame.isNull() &&
                prevFrame.size() == currentFrame.size()
            ? prevFrame
            : nextFrame;
        if (reference.isNull() || reference.size() != currentFrame.size()) {
            return currentFrame;
        }

        QImage referenceRgba = reference.convertToFormat(QImage::Format_RGBA8888);
        QImage currentRgba = currentFrame.convertToFormat(QImage::Format_RGBA8888);
        cv::Mat referenceMat(referenceRgba.height(), referenceRgba.width(), CV_8UC4,
                             referenceRgba.bits(), referenceRgba.bytesPerLine());
        cv::Mat currentMat(currentRgba.height(), currentRgba.width(), CV_8UC4,
                           currentRgba.bits(), currentRgba.bytesPerLine());
        cv::Mat referenceGray;
        cv::Mat currentGray;
        cv::cvtColor(referenceMat, referenceGray, cv::COLOR_RGBA2GRAY);
        cv::cvtColor(currentMat, currentGray, cv::COLOR_RGBA2GRAY);
        referenceGray.convertTo(referenceGray, CV_32F);
        currentGray.convertTo(currentGray, CV_32F);

        const cv::Point2d shift = cv::phaseCorrelate(referenceGray, currentGray);
        if (!std::isfinite(shift.x) || !std::isfinite(shift.y)) {
            return currentFrame;
        }
        const float direction = !prevFrame.isNull() ? 1.0f : -1.0f;
        const QPointF velocity(direction * shift.x, direction * shift.y);
        return applyMotionBlur(currentFrame, velocity,
                               std::clamp(remap_.frameBlendAmount(), 0.0f, 1.0f),
                               8);
    }
    
    if (mode == FrameBlendMode::OpticalFlow) {
        if (prevFrame.isNull() && nextFrame.isNull()) {
            return currentFrame;
        }

        bool hasPrev = !prevFrame.isNull() && prevFrame.size() == currentFrame.size();
        bool hasNext = !nextFrame.isNull() && nextFrame.size() == currentFrame.size();
        if (!hasPrev && !hasNext) {
            return currentFrame;
        }

        try {
            auto qimgToCv = [](const QImage& img) -> cv::Mat {
                QImage conv = img.convertToFormat(QImage::Format_RGBA8888);
                cv::Mat rgba(conv.height(), conv.width(), CV_8UC4,
                             const_cast<uchar*>(conv.bits()), conv.bytesPerLine());
                cv::Mat bgr;
                cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
                return bgr;
            };

            auto cvToQImage = [](const cv::Mat& bgr) -> QImage {
                cv::Mat rgba;
                cv::cvtColor(bgr, rgba, cv::COLOR_BGR2RGBA);
                return QImage(rgba.data, rgba.cols, rgba.rows,
                              static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
            };

            OpticalFlowEngine flowEngine;
            flowEngine.pyr_scale = 0.5;
            flowEngine.levels = 3;
            flowEngine.winsize = 15;
            flowEngine.iterations = 3;
            flowEngine.poly_n = 5;
            flowEngine.poly_sigma = 1.2;

            cv::Mat currCv = qimgToCv(currentFrame);
            int w = currCv.cols;
            int h = currCv.rows;

            cv::Mat accumCv = cv::Mat::zeros(h, w, CV_32FC3);
            float totalWeight = 0.0f;

            // Warp prev frame forward using backward flow (prev->curr)
            if (hasPrev && blendBackward > 0.0f) {
                cv::Mat prevCv = qimgToCv(prevFrame);
                OpticalFlowResult flowResult = flowEngine.compute(prevCv, currCv);
                const cv::Mat& flow = flowResult.getFlowMat();

                cv::Mat warped(h, w, CV_32FC3, cv::Scalar(0, 0, 0));
                for (int y = 0; y < h; ++y) {
                    const auto* flowRow = flow.ptr<cv::Vec2f>(y);
                    auto* warpedRow = warped.ptr<cv::Vec3f>(y);
                    for (int x = 0; x < w; ++x) {
                        cv::Vec2f fv = flowRow[x];
                        float sx = static_cast<float>(x) + fv[0];
                        float sy = static_cast<float>(y) + fv[1];
                        int ix = std::clamp(static_cast<int>(sx), 0, w - 1);
                        int iy = std::clamp(static_cast<int>(sy), 0, h - 1);
                        const cv::Vec3b p = currCv.ptr<cv::Vec3b>(iy)[ix];
                        warpedRow[x] = cv::Vec3f(p[0], p[1], p[2]);
                    }
                }

                for (int y = 0; y < h; ++y) {
                    const auto* warpedRow = warped.ptr<cv::Vec3f>(y);
                    auto* accumRow = accumCv.ptr<cv::Vec3f>(y);
                    for (int x = 0; x < w; ++x) {
                        accumRow[x] += warpedRow[x] * blendBackward;
                    }
                }
                totalWeight += blendBackward;
            }

            // Warp next frame backward using forward flow (curr->next)
            if (hasNext && blendForward > 0.0f) {
                cv::Mat nextCv = qimgToCv(nextFrame);
                OpticalFlowResult flowResult = flowEngine.compute(currCv, nextCv);
                const cv::Mat& flow = flowResult.getFlowMat();

                cv::Mat warped(h, w, CV_32FC3, cv::Scalar(0, 0, 0));
                for (int y = 0; y < h; ++y) {
                    const auto* flowRow = flow.ptr<cv::Vec2f>(y);
                    auto* warpedRow = warped.ptr<cv::Vec3f>(y);
                    for (int x = 0; x < w; ++x) {
                        cv::Vec2f fv = flowRow[x];
                        float sx = static_cast<float>(x) + fv[0];
                        float sy = static_cast<float>(y) + fv[1];
                        int ix = std::clamp(static_cast<int>(sx), 0, w - 1);
                        int iy = std::clamp(static_cast<int>(sy), 0, h - 1);
                        const cv::Vec3b p = nextCv.ptr<cv::Vec3b>(iy)[ix];
                        warpedRow[x] = cv::Vec3f(p[0], p[1], p[2]);
                    }
                }

                for (int y = 0; y < h; ++y) {
                    const auto* warpedRow = warped.ptr<cv::Vec3f>(y);
                    auto* accumRow = accumCv.ptr<cv::Vec3f>(y);
                    for (int x = 0; x < w; ++x) {
                        accumRow[x] += warpedRow[x] * blendForward;
                    }
                }
                totalWeight += blendForward;
            }

            // Blend original frame
            if (totalWeight < 1.0f) {
                float remaining = 1.0f - totalWeight;
                for (int y = 0; y < h; ++y) {
                    const auto* currRow = currCv.ptr<cv::Vec3b>(y);
                    auto* accumRow = accumCv.ptr<cv::Vec3f>(y);
                    for (int x = 0; x < w; ++x) {
                        const cv::Vec3b p = currRow[x];
                        accumRow[x] += cv::Vec3f(p[0], p[1], p[2]) * remaining;
                    }
                }
            }

            // Normalize and convert back
            cv::Mat resultCv(h, w, CV_8UC3);
            for (int y = 0; y < h; ++y) {
                const auto* accumRow = accumCv.ptr<cv::Vec3f>(y);
                auto* resultRow = resultCv.ptr<cv::Vec3b>(y);
                for (int x = 0; x < w; ++x) {
                    const cv::Vec3f v = accumRow[x];
                    resultRow[x] = cv::Vec3b(
                        std::clamp(static_cast<int>(v[0]), 0, 255),
                        std::clamp(static_cast<int>(v[1]), 0, 255),
                        std::clamp(static_cast<int>(v[2]), 0, 255));
                }
            }

            return cvToQImage(resultCv);
        } catch (...) {
            return currentFrame;
        }
    }
    
    return currentFrame;
}

QImage TimeRemapEffect::applyMotionBlur(
    const QImage& frame,
    const QPointF& velocity,
    float shutterAngle,
    int samples
) {
    if (frame.isNull() || velocity.isNull()) {
        return frame;
    }
    
    // Simple directional motion blur based on velocity
    QImage result = frame.copy();
    QPainter painter(&result);
    
    const int sampleCount = std::clamp(samples, 2, 32);
    const float blurLength = std::sqrt(velocity.x() * velocity.x() + velocity.y() * velocity.y()) * shutterAngle;
    const float angle = std::atan2(velocity.y(), velocity.x());
    const float cosA = std::cos(angle);
    const float sinA = std::sin(angle);
    
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    
    for (int i = 0; i < sampleCount; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sampleCount - 1);
        float offset = (t - 0.5f) * blurLength;
        
        int dx = static_cast<int>(std::round(cosA * offset));
        int dy = static_cast<int>(std::round(sinA * offset));
        
        painter.setOpacity(1.0f / sampleCount);
        painter.drawImage(dx, dy, frame);
    }
    
    return result;
}

double TimeRemapEffect::getTimeStretchRatio(double outputTime) const {
    double speed = remap_.getSpeedAtTime(outputTime);
    speed = std::max(std::abs(speed), 0.0001);
    return 1.0 / speed; // Speed > 1 means slow motion (stretch ratio < 1)
}

void TimeRemapEffect::reset() {
    remap_.reset();
    enabled_ = true;
    hasAudio_ = false;
}

// ==================== SuperSlowMotionEffect ====================

SuperSlowMotionEffect::SuperSlowMotionEffect() = default;
SuperSlowMotionEffect::~SuperSlowMotionEffect() = default;

void SuperSlowMotionEffect::setEnabled(bool enabled) {
    effect_.setEnabled(enabled);
}

void SuperSlowMotionEffect::setHasAudio(bool hasAudio) {
    effect_.setHasAudio(hasAudio);
    effect_.audio().setPreservePitch(profile_.preservePitch);
}

void SuperSlowMotionEffect::setPreviewMode(bool preview) {
    previewMode_ = preview;
}

void SuperSlowMotionEffect::setProfile(const SuperSlowMotionProfile& profile) {
    profile_ = profile;
    effect_.remap().setFrameBlendMode(profile_.blendMode);
    effect_.remap().setFrameBlendAmount(profile_.blendAmount);
    effect_.audio().setPreservePitch(profile_.preservePitch);
    rebuildConstantSpeedRemap();
}

void SuperSlowMotionEffect::setSpeedScale(double speedScale) {
    profile_.speedScale = std::max(speedScale, 0.01);
    rebuildConstantSpeedRemap();
}

void SuperSlowMotionEffect::setSourceDuration(double seconds) {
    effect_.remap().setSourceDuration(seconds);
    rebuildConstantSpeedRemap();
}

void SuperSlowMotionEffect::setSourceFrameCount(int frames) {
    effect_.remap().setSourceFrameCount(frames);
    rebuildConstantSpeedRemap();
}

void SuperSlowMotionEffect::setFrameRate(const FrameRate& rate) {
    effect_.remap().setFrameRate(rate);
    rebuildConstantSpeedRemap();
}

void SuperSlowMotionEffect::rebuildConstantSpeedRemap() {
    const double sourceDuration = effect_.remap().sourceDuration();
    const double safeSpeed = std::max(profile_.speedScale, 0.01);
    effect_.remap().clearKeyframes();
    effect_.remap().setFrameBlendMode(profile_.blendMode);
    effect_.remap().setFrameBlendAmount(profile_.blendAmount);

    TimeRemapKeyframe start;
    start.outputTime = 0.0;
    start.sourceTime = 0.0;
    start.interpolation = TimeRemapKeyframe::Interpolation::Linear;
    effect_.remap().addKeyframe(start);

    TimeRemapKeyframe end;
    end.outputTime = sourceDuration / safeSpeed;
    end.sourceTime = sourceDuration;
    end.interpolation = TimeRemapKeyframe::Interpolation::EaseInOut;
    effect_.remap().addKeyframe(end);

    effect_.audio().setPreservePitch(profile_.preservePitch);
}

int SuperSlowMotionEffect::processFrame(
    double outputTime,
    float& blendForward,
    float& blendBackward
) {
    return effect_.processFrame(outputTime, blendForward, blendBackward);
}

float SuperSlowMotionEffect::exposureWeight(float phase) const {
    const float normalized = clamp01((phase + 0.5f) / 1.0f);
    switch (profile_.exposureProfile) {
        case TemporalExposureProfile::Rectangular:
            return 1.0f;
        case TemporalExposureProfile::Triangle: {
            const float centered = 1.0f - std::abs(normalized * 2.0f - 1.0f);
            return std::max(0.0f, centered);
        }
        case TemporalExposureProfile::Trapezoid: {
            if (normalized >= 0.2f && normalized <= 0.8f) {
                return 1.0f;
            }
            const float edge = normalized < 0.2f ? normalized / 0.2f : (1.0f - normalized) / 0.2f;
            return std::max(0.0f, edge);
        }
        case TemporalExposureProfile::Cosine:
        default: {
            const float centered = std::cos((normalized - 0.5f) * kPi);
            return std::max(0.0f, centered);
        }
    }
}

QImage SuperSlowMotionEffect::blendFrames(
    const QImage& currentFrame,
    const QImage& nextFrame,
    const QImage& prevFrame,
    int sampleCount
) const {
    if (currentFrame.isNull()) {
        return currentFrame;
    }

    if (sampleCount <= 1) {
        return currentFrame;
    }

    const int samples = std::clamp(sampleCount, 2, 32);
    QVector<float> weights;
    weights.reserve(samples);

    float totalWeight = 0.0f;
    for (int i = 0; i < samples; ++i) {
        const float t = samples == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(samples - 1);
        const float phase = profile_.shutterPhase + (t - 0.5f) * profile_.shutterAngle;
        const float weight = exposureWeight(phase);
        weights.push_back(weight);
        totalWeight += weight;
    }

    if (totalWeight <= 0.0f) {
        totalWeight = 1.0f;
    }

    QImage result = currentFrame.copy();
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (int i = 0; i < samples; ++i) {
        const float t = samples == 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(samples - 1);
        const float phase = profile_.shutterPhase + (t - 0.5f) * profile_.shutterAngle;
        const float normalizedWeight = weights[i] / totalWeight;
        const float neighborWeight = normalizedWeight * profile_.blendAmount;

        if (phase < 0.0f) {
            const float local = clamp01((-phase) / std::max(profile_.shutterAngle, 0.001f));
            const float prevWeight = neighborWeight * local;
            const float currentWeight = normalizedWeight - prevWeight;

            painter.setOpacity(currentWeight);
            painter.drawImage(0, 0, currentFrame);

            if (prevWeight > 0.0f && !prevFrame.isNull()) {
                painter.setOpacity(prevWeight);
                painter.drawImage(0, 0, prevFrame);
            }
        } else {
            const float local = clamp01(phase / std::max(profile_.shutterAngle, 0.001f));
            const float nextWeight = neighborWeight * local;
            const float currentWeight = normalizedWeight - nextWeight;

            painter.setOpacity(currentWeight);
            painter.drawImage(0, 0, currentFrame);

            if (nextWeight > 0.0f && !nextFrame.isNull()) {
                painter.setOpacity(nextWeight);
                painter.drawImage(0, 0, nextFrame);
            }
        }
    }

    return result;
}

QImage SuperSlowMotionEffect::processFrameBlending(
    double outputTime,
    const QImage& currentFrame,
    const QImage& nextFrame,
    const QImage& prevFrame,
    int64_t frameNumber
) {
    if (currentFrame.isNull() || !isEnabled()) {
        return currentFrame;
    }

    if (profile_.blendMode == FrameBlendMode::None) {
        return currentFrame;
    }

    if (!effect_.needsFrameBlending(outputTime)) {
        return currentFrame;
    }

    const int sampleCount = recommendedSampleCount();
    if (profile_.blendMode == FrameBlendMode::MotionBlur) {
        return blendFrames(currentFrame, nextFrame, prevFrame, sampleCount);
    }

    if (profile_.blendMode == FrameBlendMode::FrameMix) {
        return blendFrames(currentFrame, nextFrame, prevFrame, sampleCount);
    }

    return effect_.processFrameBlending(outputTime, currentFrame, nextFrame, prevFrame, frameNumber);
}

double SuperSlowMotionEffect::getTimeStretchRatio(double outputTime) const {
    return effect_.getTimeStretchRatio(outputTime);
}

int SuperSlowMotionEffect::recommendedSampleCount() const {
    const int baseSamples = previewMode_ ? profile_.previewSamples : profile_.finalSamples;
    if (!profile_.adaptiveSampling) {
        return std::clamp(baseSamples, 2, 32);
    }

    const double slowFactor = std::clamp(1.0 / std::max(profile_.speedScale, 0.01), 1.0, 8.0);
    const double boosted = static_cast<double>(baseSamples) * (0.5 + 0.5 * slowFactor);
    return std::clamp(static_cast<int>(std::round(boosted)), 2, 32);
}

SuperSlowMotionEffect SuperSlowMotionEffect::createPreset(double speedScale, bool previewMode) {
    SuperSlowMotionEffect effect;
    SuperSlowMotionProfile profile;
    profile.speedScale = std::max(speedScale, 0.01);
    profile.blendMode = FrameBlendMode::FrameMix;
    profile.exposureProfile = TemporalExposureProfile::Triangle;
    profile.shutterAngle = 0.5f;
    profile.shutterPhase = -0.25f;
    profile.blendAmount = 0.85f;
    profile.previewSamples = 3;
    profile.finalSamples = 8;
    profile.adaptiveSampling = true;
    profile.preservePitch = true;

    effect.setProfile(profile);
    effect.setPreviewMode(previewMode);
    effect.rebuildConstantSpeedRemap();
    return effect;
}

void SuperSlowMotionEffect::reset() {
    effect_.reset();
    previewMode_ = true;
    profile_ = SuperSlowMotionProfile{};
    effect_.remap().setFrameBlendMode(profile_.blendMode);
    effect_.remap().setFrameBlendAmount(profile_.blendAmount);
    effect_.audio().setPreservePitch(profile_.preservePitch);
    rebuildConstantSpeedRemap();
}

} // namespace ArtifactCore
