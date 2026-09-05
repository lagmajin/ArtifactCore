module;
#include <atomic>
#include <array>
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
                      Vec3 sourcePos, Quat sourceRot,
                      Vec3 listenerPos, Quat listenerRot);

    void reset();

private:
    struct Snapshot {
        SpatialParams params;
    };
    alignas(64) Snapshot snapshots_[2];
    std::atomic<std::uint64_t> seq_{0};
    std::atomic<int> activeIndex_{0};
    float sampleRate_ = 48000.0f;
    float gainPrev_ = 1.0f;
    alignas(16) float gainBuf_[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    // The analytic headphone path uses a fixed history to keep the callback
    // allocation-free. 128 samples covers the maximum physically plausible
    // interaural delay at all supported sample rates up to 96 kHz.
    std::array<float, 128> binauralHistory_{};
    int binauralWriteIndex_ = 0;
    float binauralLeftFilter_ = 0.0f;
    float binauralRightFilter_ = 0.0f;
    float binauralLeftDelayPrev_ = 0.0f;
    float binauralRightDelayPrev_ = 0.0f;
    float airFilterLeft_ = 0.0f;
    float airFilterRight_ = 0.0f;
    float lfeFilter_ = 0.0f;

    static float calcAzimuthGain(float azimuth, float* gains, int channels);
    void processAnalyticBinaural(const AudioSegment& in, AudioSegment& out, int frames,
                                 Vec3 localDirection, float gainCurr,
                                 float airLowPassAlpha);
};

} // namespace Spatial
} // namespace Audio
} // namespace ArtifactCore
