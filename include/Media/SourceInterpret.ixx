module;

#include <QString>
#include <QVector>
#include <memory>

export module Media.SourceInterpret;

export namespace ArtifactCore {

enum class InterpretChangeResult {
    NoChange,
    KeyframesShifted,
    TimeRemapAffected,
    DurationChanged,
    FrameBlendInvalidated
};

struct InterpretImpactReport {
    int affectedLayerCount = 0;
    int affectedKeyframeCount = 0;
    bool hasTimeRemap = false;
    bool hasFrameBlend = false;
    bool durationWillChange = false;
    double oldDurationSeconds = 0.0;
    double newDurationSeconds = 0.0;
    QVector<QString> affectedLayerNames;
    QVector<QString> warnings;

    bool hasAnyImpact() const {
        return affectedLayerCount > 0 || hasTimeRemap || durationWillChange;
    }
};

enum class FrameRatePreserveMode {
    KeepKeyframes,   // Remap keyframe times to match new frame rate
    KeepTime,        // Keep source timing, adjust keyframe values
    ReSample         // Discard keyframes, re-interpret source timing
};

struct SourceInterpretOverride {
    double frameRate = 0.0;          // 0.0 = use source default
    double pixelAspectRatio = 1.0;
    bool loopEnabled = false;
    bool isActive = false;           // true if any override is set
};

} // namespace ArtifactCore
