module;
#include <cstdint>
#include <cmath>
#include <algorithm>

export module Audio.Spatial.Params;

import Audio.Segment;

export namespace ArtifactCore {
namespace Audio {
namespace Spatial {

enum class DistanceModel : std::uint8_t {
    Linear = 0,
    Inverse = 1,
    Exponential = 2
};

enum class SpatialRenderMode : std::uint8_t {
    Speaker = 0,
    Headphone = 1
};

struct alignas(64) SpatialParams {
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    float rolloff = 1.0f;
    float spread = 0.0f;
    // Stereo sources use two virtual directions around the object in speaker
    // mode. Zero keeps the source collapsed to a point.
    float stereoWidthDegrees = 30.0f;
    DistanceModel model = DistanceModel::Inverse;
    float coneInnerAngle = 360.0f;
    float coneOuterAngle = 360.0f;
    float coneOuterGain = 0.0f;
    bool doppler = false;
    float dopplerFactor = 1.0f;
    float airAbsorption = 0.0f;
    // LFE is an explicit send, never a side-effect of object positioning.
    float lfeSend = 0.0f;
    float lfeCutoffHz = 120.0f;
    // Render layout for this object. The composition mixer promotes its bus to
    // the widest active object layout and handles any final device downmix.
    AudioChannelLayout outputLayout = AudioChannelLayout::Stereo;
    SpatialRenderMode renderMode = SpatialRenderMode::Speaker;

    bool operator==(const SpatialParams& o) const = default;
};

inline SpatialParams sanitizedSpatialParams(SpatialParams p) {
    if (!std::isfinite(p.minDistance) || p.minDistance < 0.001f) p.minDistance = 0.001f;
    if (!std::isfinite(p.maxDistance) || p.maxDistance < p.minDistance) p.maxDistance = p.minDistance + 0.001f;
    if (!std::isfinite(p.rolloff) || p.rolloff < 0.0f) p.rolloff = 0.0f;
    if (p.rolloff > 10.0f) p.rolloff = 10.0f;
    if (!std::isfinite(p.spread) || p.spread < 0.0f) p.spread = 0.0f;
    p.spread = std::clamp(p.spread, 0.0f, 1.0f);
    if (!std::isfinite(p.stereoWidthDegrees)) p.stereoWidthDegrees = 30.0f;
    p.stereoWidthDegrees = std::clamp(p.stereoWidthDegrees, 0.0f, 120.0f);
    if (!std::isfinite(p.coneInnerAngle)) p.coneInnerAngle = 360.0f;
    if (!std::isfinite(p.coneOuterAngle)) p.coneOuterAngle = 360.0f;
    p.coneInnerAngle = std::clamp(p.coneInnerAngle, 0.0f, 360.0f);
    p.coneOuterAngle = std::clamp(p.coneOuterAngle, p.coneInnerAngle, 360.0f);
    if (!std::isfinite(p.coneOuterGain)) p.coneOuterGain = 0.0f;
    p.coneOuterGain = std::clamp(p.coneOuterGain, 0.0f, 1.0f);
    if (!std::isfinite(p.dopplerFactor) || p.dopplerFactor < 0.0f) p.dopplerFactor = 0.0f;
    if (!std::isfinite(p.airAbsorption) || p.airAbsorption < 0.0f) p.airAbsorption = 0.0f;
    if (!std::isfinite(p.lfeSend)) p.lfeSend = 0.0f;
    p.lfeSend = std::clamp(p.lfeSend, 0.0f, 1.0f);
    if (!std::isfinite(p.lfeCutoffHz)) p.lfeCutoffHz = 120.0f;
    p.lfeCutoffHz = std::clamp(p.lfeCutoffHz, 40.0f, 250.0f);
    switch (p.outputLayout) {
    case AudioChannelLayout::Stereo:
    case AudioChannelLayout::Surround51:
    case AudioChannelLayout::Surround71:
    case AudioChannelLayout::Surround714:
        break;
    default:
        p.outputLayout = AudioChannelLayout::Stereo;
        break;
    }
    if (p.renderMode != SpatialRenderMode::Speaker &&
        p.renderMode != SpatialRenderMode::Headphone) {
        p.renderMode = SpatialRenderMode::Speaker;
    }
    return p;
}

} // namespace Spatial
} // namespace Audio
} // namespace ArtifactCore
