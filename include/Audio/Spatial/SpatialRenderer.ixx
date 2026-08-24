module;
#include <atomic>
#include <cstdint>
#include <algorithm>
#include <cmath>

export module Audio.Spatial.Renderer;

import Audio.Segment;
import Audio.Spatial.Params;
import Audio.Spatial.Math;

export namespace ArtifactCore {
namespace Audio {
namespace Spatial {

class SpatialRenderer {
public:
    SpatialRenderer();
    ~SpatialRenderer();

    void setSampleRate(float sampleRate);
    void publishParams(const SpatialParams& params);
    SpatialParams snapshotParams() const;

    void processBlock(const AudioSegment& in, AudioSegment& out, int frames,
                      Vec3 sourcePos, Vec3 listenerPos, Quat listenerRot);

    void reset();

private:
    struct Snapshot {
        SpatialParams params;
    };
    alignas(64) Snapshot snapshots_[2];
    std::atomic<std::uint64_t> seq_{0};
    int activeIndex_ = 0;
    float sampleRate_ = 48000.0f;
    float gainPrev_ = 1.0f;
    alignas(16) float gainBuf_[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    static float calcAzimuthGain(float azimuth, float* gains, int channels);
};

} // namespace Spatial
} // namespace Audio
} // namespace ArtifactCore
