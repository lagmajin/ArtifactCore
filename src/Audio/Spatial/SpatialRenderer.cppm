module;
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstring>

#include <QVector>

module Audio.Spatial.Renderer;

import Audio.Segment;
import Audio.Panner;
import Audio.Spatial.Params;
import Audio.Spatial.Math;
import Audio.Spatial.SpeakerLayout;

namespace ArtifactCore {
namespace Audio {
namespace Spatial {

SpatialRenderer::SpatialRenderer() {
    snapshots_[0].params = SpatialParams{};
    snapshots_[1].params = SpatialParams{};
}

SpatialRenderer::~SpatialRenderer() = default;

void SpatialRenderer::setSampleRate(float sampleRate) {
    const float nextRate = (std::isfinite(sampleRate) && sampleRate > 0.0f) ? sampleRate : 48000.0f;
    if (std::abs(nextRate - sampleRate_) > 0.001f) {
        reset();
    }
    sampleRate_ = nextRate;
}

void SpatialRenderer::publishParams(const SpatialParams& params) {
    auto sanitized = sanitizedSpatialParams(params);
    int active = activeIndex_.load(std::memory_order_acquire);
    int inactive = active ^ 1;
    snapshots_[inactive].params = sanitized;
    seq_.store(seq_.load(std::memory_order_relaxed) + 1, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_release);
    snapshots_[inactive].params = sanitized;
    activeIndex_.store(inactive, std::memory_order_release);
    seq_.store(seq_.load(std::memory_order_relaxed) + 1, std::memory_order_release);
}

SpatialParams SpatialRenderer::snapshotParams() const {
    std::uint64_t s1, s2;
    SpatialParams out;
    do {
        s1 = seq_.load(std::memory_order_acquire);
        if (s1 & 1) continue;
        int active = activeIndex_.load(std::memory_order_acquire);
        out = snapshots_[active].params;
        std::atomic_thread_fence(std::memory_order_acquire);
        s2 = seq_.load(std::memory_order_acquire);
    } while (s1 != s2 || (s1 & 1));
    return out;
}

void SpatialRenderer::reset() {
    gainPrev_ = 1.0f;
    binauralHistory_.fill(0.0f);
    binauralWriteIndex_ = 0;
    binauralLeftFilter_ = 0.0f;
    binauralRightFilter_ = 0.0f;
    binauralLeftDelayPrev_ = 0.0f;
    binauralRightDelayPrev_ = 0.0f;
    airFilterLeft_ = 0.0f;
    airFilterRight_ = 0.0f;
    lfeFilter_ = 0.0f;
}

float SpatialRenderer::calcAzimuthGain(float azimuth, float* gains, int channels) {
    if (channels <= 2) {
        float pan = std::clamp(azimuth / 90.0f, -1.0f, 1.0f);
        auto g = AudioPanner::calculateConstantPowerGains(pan);
        if ((int)g.channelGains.size() >= 2) {
            gains[0] = g.channelGains[0];
            gains[1] = g.channelGains[1];
        } else {
            gains[0] = 0.707f; gains[1] = 0.707f;
        }
        return pan;
    }
    AudioPanner p;
    auto g = p.calculateGain(azimuth, 0.0f);
    int n = std::min(channels, (int)g.channelGains.size());
    for (int i = 0; i < n; ++i) gains[i] = g.channelGains[i];
    for (int i = n; i < channels; ++i) gains[i] = 0.0f;
    return 0.0f;
}

void SpatialRenderer::processAnalyticBinaural(const AudioSegment& in,
                                              AudioSegment& out, int frames,
                                              Vec3 localDirection, float gainCurr,
                                              float airLowPassAlpha) {
    constexpr float kHeadRadiusMeters = 0.0875f;
    constexpr float kSpeedOfSound = 343.0f;
    constexpr float kPi = 3.14159265358979323846f;
    const float lengthSquared = localDirection.x * localDirection.x +
                                localDirection.y * localDirection.y +
                                localDirection.z * localDirection.z;
    const float length = std::sqrt(std::max(0.0f, lengthSquared));
    const float azimuth = length > 1e-6f
        ? std::atan2(localDirection.x, -localDirection.z) : 0.0f;
    const float side = std::sin(azimuth);
    const float sideAmount = std::abs(side);
    // Source on the right delays and filters the left ear, and vice versa.
    const float itdSamples = side * kHeadRadiusMeters / kSpeedOfSound * sampleRate_;
    const float leftDelay = std::clamp(std::max(0.0f, itdSamples), 0.0f, 126.0f);
    const float rightDelay = std::clamp(std::max(0.0f, -itdSamples), 0.0f, 126.0f);
    const float backAmount = length > 1e-6f
        ? std::clamp(localDirection.z / length, 0.0f, 1.0f) : 0.0f;
    const float farCutoff = 18000.0f - sideAmount * 13000.0f - backAmount * 2500.0f;
    const float cutoff = std::clamp(farCutoff, 1200.0f, 18000.0f);
    const float lowPassAlpha = 1.0f - std::exp(-2.0f * kPi * cutoff / sampleRate_);
    const float nearGain = 1.0f - backAmount * 0.12f;
    const float farGain = (1.0f - sideAmount * 0.30f) * (1.0f - backAmount * 0.18f);
    const float delayStepLeft = (leftDelay - binauralLeftDelayPrev_) /
        static_cast<float>(std::max(frames, 1));
    const float delayStepRight = (rightDelay - binauralRightDelayPrev_) /
        static_cast<float>(std::max(frames, 1));

    out = AudioSegment{};
    out.sampleRate = in.sampleRate;
    out.startFrame = in.startFrame;
    out.layout = AudioChannelLayout::Stereo;
    out.channelData.resize(2);
    out.setFrameCount(frames);
    const float* sourceLeft = in.channelData[0].constData();
    const float* sourceRight = in.channelData.size() > 1 ? in.channelData[1].constData() : sourceLeft;
    const bool monoInput = in.channelData.size() == 1;
    const float gainStep = (gainCurr - gainPrev_) / static_cast<float>(std::max(frames, 1));
    const int historySize = static_cast<int>(binauralHistory_.size());

    for (int sample = 0; sample < frames; ++sample) {
        const float leftInput = std::isfinite(sourceLeft[sample]) ? sourceLeft[sample] : 0.0f;
        const float rightInput = std::isfinite(sourceRight[sample]) ? sourceRight[sample] : 0.0f;
        const float rawPointInput = monoInput ? leftInput : (leftInput + rightInput) * 0.70710678f;
        airFilterLeft_ += airLowPassAlpha * (rawPointInput - airFilterLeft_);
        const float pointInput = airFilterLeft_;
        binauralHistory_[binauralWriteIndex_] = pointInput;
        const auto delayed = [&](float delay) {
            const float position = static_cast<float>(binauralWriteIndex_) - delay;
            int first = static_cast<int>(std::floor(position));
            const float fraction = position - static_cast<float>(first);
            first %= historySize;
            if (first < 0) first += historySize;
            int second = first - 1;
            if (second < 0) second += historySize;
            return binauralHistory_[first] * (1.0f - fraction) + binauralHistory_[second] * fraction;
        };
        const float delayL = binauralLeftDelayPrev_ + delayStepLeft * static_cast<float>(sample);
        const float delayR = binauralRightDelayPrev_ + delayStepRight * static_cast<float>(sample);
        float renderedLeft = delayed(delayL);
        float renderedRight = delayed(delayR);
        binauralLeftFilter_ += lowPassAlpha * (renderedLeft - binauralLeftFilter_);
        binauralRightFilter_ += lowPassAlpha * (renderedRight - binauralRightFilter_);
        const bool leftEarIsFar = side > 0.001f;
        const bool rightEarIsFar = side < -0.001f;
        renderedLeft = leftEarIsFar ? binauralLeftFilter_ * farGain : renderedLeft * nearGain;
        renderedRight = rightEarIsFar ? binauralRightFilter_ * farGain : renderedRight * nearGain;
        const float ramp = gainPrev_ + gainStep * static_cast<float>(sample);
        out.channelData[0][sample] = std::isfinite(renderedLeft * ramp) ? renderedLeft * ramp : 0.0f;
        out.channelData[1][sample] = std::isfinite(renderedRight * ramp) ? renderedRight * ramp : 0.0f;
        ++binauralWriteIndex_;
        if (binauralWriteIndex_ == historySize) binauralWriteIndex_ = 0;
    }
    binauralLeftDelayPrev_ = leftDelay;
    binauralRightDelayPrev_ = rightDelay;
    gainPrev_ = gainCurr;
}

void SpatialRenderer::processBlock(const AudioSegment& in, AudioSegment& out, int frames,
                                   Vec3 sourcePos, Quat sourceRot,
                                   Vec3 listenerPos, Quat listenerRot) {
    if (frames <= 0 || in.channelData.isEmpty()) {
        out = in;
        return;
    }

    int availableFrames = in.channelData[0].size();
    for (const auto& channel : in.channelData) {
        availableFrames = std::min(availableFrames, static_cast<int>(channel.size()));
    }
    frames = std::min(frames, availableFrames);
    if (frames <= 0) {
        out = in;
        return;
    }

    SpatialParams params = snapshotParams();

    auto finiteVec = [](Vec3 v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    if (!finiteVec(sourcePos)) sourcePos = {};
    if (!finiteVec(listenerPos)) listenerPos = {};
    if (!std::isfinite(sourceRot.x) || !std::isfinite(sourceRot.y) ||
        !std::isfinite(sourceRot.z) || !std::isfinite(sourceRot.w)) {
        sourceRot = {};
    }
    if (!std::isfinite(listenerRot.x) || !std::isfinite(listenerRot.y) ||
        !std::isfinite(listenerRot.z) || !std::isfinite(listenerRot.w)) {
        listenerRot = {};
    }

    Vec3 delta = vecSub(sourcePos, listenerPos);
    float dist2 = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    float dist = std::sqrt(std::max(dist2, 0.0f));

    float atten = distanceAttenuation(dist, params);

    float cone = 1.0f;
    if (params.coneInnerAngle < 360.0f) {
        // Cone directivity belongs to the emitting object. Listener rotation
        // only determines the local panning frame below.
        Vec3 fwd = quatRotate(sourceRot, {0.0f, 0.0f, -1.0f});
        float fwdLen2 = fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z;
        float dLen2 = dist2;
        if (fwdLen2 > 1e-12f && dLen2 > 1e-12f) {
            float dot = (delta.x*fwd.x + delta.y*fwd.y + delta.z*fwd.z) / std::sqrt(fwdLen2 * dLen2);
            dot = std::clamp(dot, -1.0f, 1.0f);
            float ang = std::acos(dot) * 57.29577951308232f;
            cone = coneGain(ang, params);
        }
    }

    // Air absorption combines a deterministic energy loss with a one-pole
    // high-frequency roll-off. Its state is reset on seek/source/rate changes.
    const float airDistance = std::clamp(dist / std::max(params.maxDistance, 0.001f), 0.0f, 1.0f);
    const float airGain = std::exp(-std::max(params.airAbsorption, 0.0f) * airDistance);
    const float airCutoff = std::clamp(20000.0f * std::exp(
        -std::max(params.airAbsorption, 0.0f) * airDistance * 1.2f), 1000.0f, 20000.0f);
    const float airLowPassAlpha = params.airAbsorption > 0.0f && airDistance > 0.0f
        ? 1.0f - std::exp(-6.2831853071795864769f * airCutoff / sampleRate_)
        : 1.0f;
    float gainCurr = atten * cone * airGain;
    if (!std::isfinite(gainCurr)) gainCurr = 0.0f;

    Vec3 local = quatRotate(quatConjugate(listenerRot), delta);
    float azimuth, elevation;
    toSpherical(local, azimuth, elevation);

    if (params.renderMode == SpatialRenderMode::Headphone) {
        processAnalyticBinaural(in, out, frames, local, gainCurr, airLowPassAlpha);
        return;
    }

    // Multi-speaker layouts use VBAP gains computed from the declared speaker
    // directions. Stereo assets retain a controllable pair of virtual
    // directions around the object instead of being summed into one point.
    if (params.outputLayout != AudioChannelLayout::Stereo) {
        SpeakerGains speakerGains = calculateSpeakerGains(params.outputLayout, local);
        const int outChannels = speakerGains.channelCount;
        if (outChannels <= 0) {
            out = in;
            return;
        }
        const auto applySpread = [&](SpeakerGains& gains) {
            const float spread = std::clamp(params.spread, 0.0f, 1.0f);
            if (spread <= 0.0f) return;
            int activeSpeakers = 0;
            const auto descriptor = speakerLayout(params.outputLayout);
            for (int channel = 0; channel < outChannels; ++channel) {
                if (descriptor.speakers[channel].receivesObjectPanning) ++activeSpeakers;
            }
            const float uniform = activeSpeakers > 0
                ? 1.0f / std::sqrt(static_cast<float>(activeSpeakers)) : 0.0f;
            for (int channel = 0; channel < outChannels; ++channel) {
                if (descriptor.speakers[channel].receivesObjectPanning) {
                    gains.values[channel] = gains.values[channel] * (1.0f - spread) +
                                            uniform * spread;
                }
            }
            float energy = 0.0f;
            for (int channel = 0; channel < outChannels; ++channel) {
                energy += gains.values[channel] * gains.values[channel];
            }
            if (energy > 1e-12f && std::isfinite(energy)) {
                const float inverseEnergy = 1.0f / std::sqrt(energy);
                for (int channel = 0; channel < outChannels; ++channel) {
                    gains.values[channel] *= inverseEnergy;
                }
            }
        };
        applySpread(speakerGains);

        out = AudioSegment{};
        out.sampleRate = in.sampleRate;
        out.startFrame = in.startFrame;
        out.layout = params.outputLayout;
        out.channelData.resize(outChannels);
        out.setFrameCount(frames);
        const float* srcL = in.channelData[0].constData();
        const float* srcR = in.channelData.size() > 1 ? in.channelData[1].constData() : srcL;
        const bool monoIn = in.channelData.size() == 1;
        SpeakerGains leftSpeakerGains = speakerGains;
        SpeakerGains rightSpeakerGains = speakerGains;
        if (!monoIn && params.stereoWidthDegrees > 0.0f) {
            const float halfWidthRadians = params.stereoWidthDegrees * 0.00872664625997164788f;
            const float c = std::cos(halfWidthRadians);
            const float s = std::sin(halfWidthRadians);
            const Vec3 leftDirection{local.x * c - local.z * s, local.y, local.x * s + local.z * c};
            const Vec3 rightDirection{local.x * c + local.z * s, local.y, -local.x * s + local.z * c};
            leftSpeakerGains = calculateSpeakerGains(params.outputLayout, leftDirection);
            rightSpeakerGains = calculateSpeakerGains(params.outputLayout, rightDirection);
            applySpread(leftSpeakerGains);
            applySpread(rightSpeakerGains);
        }
        const float step = (gainCurr - gainPrev_) / static_cast<float>(std::max(frames, 1));
        const bool hasLfe = outChannels > 3 && params.lfeSend > 0.0f;
        const float lfeAlpha = hasLfe
            ? 1.0f - std::exp(-6.2831853071795864769f * params.lfeCutoffHz / sampleRate_)
            : 1.0f;
        for (int sample = 0; sample < frames; ++sample) {
            const float left = std::isfinite(srcL[sample]) ? srcL[sample] : 0.0f;
            const float right = std::isfinite(srcR[sample]) ? srcR[sample] : 0.0f;
            airFilterLeft_ += airLowPassAlpha * (left - airFilterLeft_);
            const float filteredLeft = airFilterLeft_;
            const float filteredRight = monoIn
                ? filteredLeft
                : (airFilterRight_ += airLowPassAlpha * (right - airFilterRight_));
            const float pointSample = monoIn ? filteredLeft : (filteredLeft + filteredRight) * 0.70710678f;
            const float ramp = gainPrev_ + step * static_cast<float>(sample);
            for (int channel = 0; channel < outChannels; ++channel) {
                const float rendered = monoIn
                    ? pointSample * ramp * speakerGains.values[channel]
                    : (filteredLeft * leftSpeakerGains.values[channel] +
                       filteredRight * rightSpeakerGains.values[channel]) *
                        (0.70710678f * ramp);
                out.channelData[channel][sample] = std::isfinite(rendered) ? rendered : 0.0f;
            }
            if (hasLfe) {
                lfeFilter_ += lfeAlpha * (pointSample - lfeFilter_);
                const float lfe = lfeFilter_ * ramp * params.lfeSend;
                out.channelData[3][sample] = std::isfinite(lfe) ? lfe : 0.0f;
            }
        }
        gainPrev_ = gainCurr;
        return;
    }

    // Phase 1 always provides a stereo preview for mono sources.  Preserve
    // wider layouts when the caller has already allocated them.
    int outChannels = std::max(2, static_cast<int>(out.channelData.size()));
    float gains[8] = {1,1,0,0,0,0,0,0};
    calcAzimuthGain(azimuth, gains, outChannels);

    out = in;
    if ((int)out.channelData.size() != outChannels) {
        out.channelData.resize(outChannels);
    }
    if (outChannels == 2 && in.channelData.size() < 2) {
        out.layout = AudioChannelLayout::Stereo;
    }
    for (int c = 0; c < outChannels; ++c) {
        if (out.channelData[c].size() < frames) out.channelData[c].resize(frames);
    }

    float step = (gainCurr - gainPrev_) / static_cast<float>(std::max(frames, 1));
    float gL = gains[0];
    float gR = outChannels > 1 ? gains[1] : gains[0];
    const float spread = params.spread;
    if (outChannels > 1 && spread > 0.0f) {
        const float mid = 0.70710678f;
        gL = gL * (1.0f - spread) + mid * spread;
        gR = gR * (1.0f - spread) + mid * spread;
    }
    if (outChannels > 1) {
        const float verticalBlend = std::clamp(std::abs(elevation) / 90.0f * 0.35f, 0.0f, 0.35f);
        const float mid = (gL + gR) * 0.5f;
        gL = gL * (1.0f - verticalBlend) + mid * verticalBlend;
        gR = gR * (1.0f - verticalBlend) + mid * verticalBlend;
    }

    float* dstL = out.channelData[0].data();
    const float* srcL = in.channelData[0].constData();
    float* dstR = outChannels > 1 ? out.channelData[1].data() : nullptr;
    const float* srcR = (in.channelData.size() > 1) ? in.channelData[1].constData() : srcL;

    bool monoIn = in.channelData.size() == 1;

    for (int i = 0; i < frames; ++i) {
        float ramp = gainPrev_ + step * static_cast<float>(i);
        if (monoIn) {
            const float raw = std::isfinite(srcL[i]) ? srcL[i] : 0.0f;
            airFilterLeft_ += airLowPassAlpha * (raw - airFilterLeft_);
            float s = airFilterLeft_;
            dstL[i] = s * ramp * gL;
            if (!std::isfinite(dstL[i])) dstL[i] = 0.0f;
            if (dstR) { dstR[i] = s * ramp * gR; if (!std::isfinite(dstR[i])) dstR[i] = 0.0f; }
        } else {
            const float rawLeft = std::isfinite(srcL[i]) ? srcL[i] : 0.0f;
            const float rawRight = std::isfinite(srcR[i]) ? srcR[i] : 0.0f;
            airFilterLeft_ += airLowPassAlpha * (rawLeft - airFilterLeft_);
            airFilterRight_ += airLowPassAlpha * (rawRight - airFilterRight_);
            const float left = airFilterLeft_;
            const float right = airFilterRight_;
            dstL[i] = left * ramp * gL;
            if (dstR) dstR[i] = right * ramp * gR;
            if (!std::isfinite(dstL[i])) dstL[i] = 0.0f;
            if (dstR && !std::isfinite(dstR[i])) dstR[i] = 0.0f;
        }
        for (int c = 2; c < outChannels; ++c) {
            float* dst = out.channelData[c].data();
            const float* src = (c < (int)in.channelData.size()) ? in.channelData[c].constData() : srcL;
            dst[i] = (std::isfinite(src[i]) ? src[i] : 0.0f) * ramp * gains[c];
            if (!std::isfinite(dst[i])) dst[i] = 0.0f;
        }
    }

    gainPrev_ = gainCurr;

    if (outChannels > 2) {
        for (int c = 2; c < outChannels; ++c) {
            Q_UNUSED(c);
        }
    }
}

} // namespace Spatial
} // namespace Audio
} // namespace ArtifactCore
