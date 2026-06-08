module;
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QDebug>
#include <cstdint>

export module Frame.SkipTracker;

export namespace ArtifactCore {

export enum class FrameSkipReason {
    None,
    TooHeavy,
    SeekConflict,
    CacheMiss,
    DecodePending,
    DroppedByPolicy,
    Unknown
};

export struct FrameDispatchEvent {
    int64_t requestedFrame = 0;
    int64_t committedFrame = 0;
    int64_t displayedFrame = 0;
    bool skipped = false;
    FrameSkipReason skipReason = FrameSkipReason::None;
    QString skipDetail;
    QDateTime timestamp;
    double elapsedMs = 0.0;
};

export class FrameSkipTracker {
public:
    static FrameSkipTracker* instance();

    void beginDispatch(int64_t requestedFrame);
    void commitFrame(int64_t frame);
    void displayFrame(int64_t frame);

    void recordSkip(int64_t requestedFrame, int64_t fallbackFrame,
                    FrameSkipReason reason, const QString& detail = "");

    FrameDispatchEvent lastEvent() const;
    QVector<FrameDispatchEvent> recentEvents(int count = 50) const;
    QVector<FrameDispatchEvent> skippedFrames() const;

    int totalSkips() const { return skipCount_; }
    int consecutiveSkips() const { return consecutiveSkips_; }
    void reset();

    QString summary() const;

    bool trackingEnabled() const { return enabled_; }
    void setTrackingEnabled(bool enabled) { enabled_ = enabled; }

private:
    FrameSkipTracker() = default;

    bool enabled_ = true;
    int skipCount_ = 0;
    int consecutiveSkips_ = 0;
    FrameDispatchEvent current_;
    QVector<FrameDispatchEvent> history_;
    static constexpr int kMaxHistory = 200;
};

} // namespace ArtifactCore
