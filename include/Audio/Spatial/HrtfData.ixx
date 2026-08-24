module;
#include <cstdint>

export module Audio.Spatial.HrtfData;

export namespace ArtifactCore {
namespace Audio {
namespace Spatial {

constexpr int kHrtfAzimuthSteps = 0;
constexpr int kHrtfTapCount = 128;

struct HrtfData {
    static constexpr bool available = false;
};

} // namespace Spatial
} // namespace Audio
} // namespace ArtifactCore
