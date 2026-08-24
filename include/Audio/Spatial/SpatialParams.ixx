module;
#include <cstdint>
#include <cmath>

export module Audio.Spatial.Params;

export namespace ArtifactCore {
namespace Audio {
namespace Spatial {

enum class DistanceModel : std::uint8_t {
    Linear = 0,
    Inverse = 1,
    Exponential = 2
};

struct alignas(64) SpatialParams {
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    float rolloff = 1.0f;
    DistanceModel model = DistanceModel::Inverse;
    float coneInnerAngle = 360.0f;
    float coneOuterAngle = 360.0f;
    float coneOuterGain = 0.0f;
    bool doppler = false;
    float dopplerFactor = 1.0f;
    float airAbsorption = 0.0f;

    bool operator==(const SpatialParams& o) const = default;
};

inline SpatialParams sanitizedSpatialParams(SpatialParams p) {
    if (!std::isfinite(p.minDistance) || p.minDistance < 0.001f) p.minDistance = 0.001f;
    if (!std::isfinite(p.maxDistance) || p.maxDistance < p.minDistance) p.maxDistance = p.minDistance + 0.001f;
    if (!std::isfinite(p.rolloff) || p.rolloff < 0.0f) p.rolloff = 0.0f;
    if (p.rolloff > 10.0f) p.rolloff = 10.0f;
    if (!std::isfinite(p.coneInnerAngle)) p.coneInnerAngle = 360.0f;
    if (!std::isfinite(p.coneOuterAngle)) p.coneOuterAngle = 360.0f;
    p.coneInnerAngle = std::clamp(p.coneInnerAngle, 0.0f, 360.0f);
    p.coneOuterAngle = std::clamp(p.coneOuterAngle, p.coneInnerAngle, 360.0f);
    if (!std::isfinite(p.coneOuterGain)) p.coneOuterGain = 0.0f;
    p.coneOuterGain = std::clamp(p.coneOuterGain, 0.0f, 1.0f);
    if (!std::isfinite(p.dopplerFactor) || p.dopplerFactor < 0.0f) p.dopplerFactor = 0.0f;
    if (!std::isfinite(p.airAbsorption) || p.airAbsorption < 0.0f) p.airAbsorption = 0.0f;
    return p;
}

} // namespace Spatial
} // namespace Audio
} // namespace ArtifactCore
