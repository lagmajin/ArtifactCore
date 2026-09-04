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
        gainPrev_ = 1.0f;
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

void SpatialRenderer::processBlock(const AudioSegment& in, AudioSegment& out, int frames,
                                   Vec3 sourcePos, Vec3 listenerPos, Quat listenerRot) {
    if (frames <= 0 || in.channelData.isEmpty()) {
        out = in;
        return;
    }

    int availableFrames = in.channelData[0].size();
    for (const auto& channel : in.channelData) {
        availableFrames = std::min(availableFrames, channel.size());
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
        Vec3 fwd = quatRotate(listenerRot, {0.0f, 0.0f, -1.0f});
        float fwdLen2 = fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z;
        float dLen2 = dist2;
        if (fwdLen2 > 1e-12f && dLen2 > 1e-12f) {
            float dot = (delta.x*fwd.x + delta.y*fwd.y + delta.z*fwd.z) / std::sqrt(fwdLen2 * dLen2);
            dot = std::clamp(dot, -1.0f, 1.0f);
            float ang = std::acos(dot) * 57.29577951308232f;
            cone = coneGain(ang, params);
        }
    }

    // Phase 1 models air absorption as a deterministic distance-dependent
    // energy loss. Frequency-selective filtering remains a later phase.
    const float airDistance = std::clamp(dist / std::max(params.maxDistance, 0.001f), 0.0f, 1.0f);
    const float airGain = std::exp(-std::max(params.airAbsorption, 0.0f) * airDistance);
    float gainCurr = atten * cone * airGain;
    if (!std::isfinite(gainCurr)) gainCurr = 0.0f;

    Vec3 local = quatRotate(quatConjugate(listenerRot), delta);
    float azimuth, elevation;
    toSpherical(local, azimuth, elevation);

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

    float* dstL = out.channelData[0].data();
    const float* srcL = in.channelData[0].constData();
    float* dstR = outChannels > 1 ? out.channelData[1].data() : nullptr;
    const float* srcR = (in.channelData.size() > 1) ? in.channelData[1].constData() : srcL;

    bool monoIn = in.channelData.size() == 1;

    for (int i = 0; i < frames; ++i) {
        float ramp = gainPrev_ + step * static_cast<float>(i);
        if (monoIn) {
            float s = std::isfinite(srcL[i]) ? srcL[i] : 0.0f;
            dstL[i] = s * ramp * gL;
            if (dstR) dstR[i] = s * ramp * gR;
        } else {
            const float left = std::isfinite(srcL[i]) ? srcL[i] : 0.0f;
            const float right = std::isfinite(srcR[i]) ? srcR[i] : 0.0f;
            dstL[i] = left * ramp * gL;
            if (dstR) dstR[i] = right * ramp * gR;
        }
        for (int c = 2; c < outChannels; ++c) {
            float* dst = out.channelData[c].data();
            const float* src = (c < (int)in.channelData.size()) ? in.channelData[c].constData() : srcL;
            dst[i] = (std::isfinite(src[i]) ? src[i] : 0.0f) * ramp * gains[c];
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
