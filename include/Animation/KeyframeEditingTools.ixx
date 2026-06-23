module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include <QVector>

export module Animation.KeyframeEditingTools;

import Math.Interpolate;
import Audio.Segment;

export namespace ArtifactCore {

struct KeyframePoint {
    double frame = 0.0;
    double value = 0.0;
    InterpolationType interpolation = InterpolationType::Linear;
    float cp1_x = 0.42f;
    float cp1_y = 0.0f;
    float cp2_x = 0.58f;
    float cp2_y = 1.0f;
};

struct ThinKeyframesRequest {
    double tolerance = 0.01;
    bool preserveExtremes = true;
};

struct BatchEasingRequest {
    InterpolationType type = InterpolationType::EaseInOut;
};

struct ScaleOffsetRequest {
    double timeScale = 1.0;
    double timeOffset = 0.0;
    double valueScale = 1.0;
    double valueOffset = 0.0;
};

struct RandomizeRequest {
    double amplitude = 1.0;
    std::uint32_t seed = 0;
};

struct SmoothRequest {
    int windowSize = 3;
    int iterations = 1;
};

struct QuantizeToBeatRequest {
    double bpm = 120.0;
    double offset = 0.0;
    bool snapAll = true;
    InterpolationType interpolation = InterpolationType::Linear;
};

struct MirrorFlipRequest {
    bool mirrorTime = true;
    bool flipValue = false;
    double mirrorCenter = -1.0;
    double flipCenter = -1.0;
};

struct AudioToKeyframeRequest {
    double startFrame = 0.0;
    double endFrame = 30.0;
    double frameRate = 30.0;
    int channelIndex = 0;
    double amplitudeScale = 1.0;
    double offset = 0.0;
    bool useRMS = true;
};

class KeyframeEditingTools {
public:
    static bool thinKeyframes(std::vector<KeyframePoint>& keyframes,
                              const ThinKeyframesRequest& request);

    static bool applyEasing(std::vector<KeyframePoint>& keyframes,
                            const BatchEasingRequest& request);

    static bool scaleOffset(std::vector<KeyframePoint>& keyframes,
                            const ScaleOffsetRequest& request);

    static bool randomize(std::vector<KeyframePoint>& keyframes,
                          const RandomizeRequest& request);

    static bool smooth(std::vector<KeyframePoint>& keyframes,
                       const SmoothRequest& request);

    static bool quantizeToBeat(std::vector<KeyframePoint>& keyframes,
                                const QuantizeToBeatRequest& request = {});

    static bool mirrorFlip(std::vector<KeyframePoint>& keyframes,
                            const MirrorFlipRequest& request);

    static std::vector<KeyframePoint> audioToKeyframes(
        const AudioSegment& audio,
        const AudioToKeyframeRequest& request);

    static double interpolateKeyframes(const std::vector<KeyframePoint>& keyframes,
                                        double frame);

    static std::vector<KeyframePoint> expressionToKeyframes(
        const std::string& expression,
        double frameRate,
        double startFrame,
        double endFrame,
        std::uint32_t seed = 0);

    static std::vector<KeyframePoint> copyAnimationRelative(
        const std::vector<KeyframePoint>& source,
        double targetStartFrame,
        double targetEndFrame,
        double targetBaseValue = 0.0,
        double targetAmplitude = 1.0);
};

} // namespace ArtifactCore
