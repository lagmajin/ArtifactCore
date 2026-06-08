module;
#include <QString>
#include <QVector>
#include <QDateTime>
#include <algorithm>
#include <cstdint>

module Frame.SkipTracker;

namespace ArtifactCore {

FrameSkipTracker* FrameSkipTracker::instance() {
    static FrameSkipTracker tracker;
    return &tracker;
}

void FrameSkipTracker::beginDispatch(int64_t requestedFrame) {
    if (!enabled_) return;
    current_ = {};
    current_.requestedFrame = requestedFrame;
    current_.timestamp = QDateTime::currentDateTime();
}

void FrameSkipTracker::commitFrame(int64_t frame) {
    if (!enabled_) return;
    current_.committedFrame = frame;
}

void FrameSkipTracker::displayFrame(int64_t frame) {
    if (!enabled_) return;
    current_.displayedFrame = frame;
    current_.skipped = (current_.requestedFrame != frame);

    if (current_.skipped) {
        current_.skipReason = FrameSkipReason::Unknown;
        ++skipCount_;
        ++consecutiveSkips_;
    } else {
        consecutiveSkips_ = 0;
    }

    history_.push_back(current_);
    if (history_.size() > kMaxHistory) {
        history_.erase(history_.begin());
    }

    if (current_.skipped) {
        qWarning() << "[FrameSkipTracker] frame skip: requested="
                   << current_.requestedFrame << "displayed=" << frame
                   << "reason=" << static_cast<int>(current_.skipReason);
    }
}

void FrameSkipTracker::recordSkip(int64_t requestedFrame, int64_t fallbackFrame,
                                   FrameSkipReason reason, const QString& detail) {
    if (!enabled_) return;
    FrameDispatchEvent event;
    event.requestedFrame = requestedFrame;
    event.committedFrame = fallbackFrame;
    event.displayedFrame = fallbackFrame;
    event.skipped = true;
    event.skipReason = reason;
    event.skipDetail = detail;
    event.timestamp = QDateTime::currentDateTime();

    ++skipCount_;
    ++consecutiveSkips_;
    history_.push_back(event);
    if (history_.size() > kMaxHistory) {
        history_.erase(history_.begin());
    }

    qWarning() << "[FrameSkipTracker] recorded skip: requested="
               << requestedFrame << "fallback=" << fallbackFrame
               << "reason=" << detail;
}

FrameDispatchEvent FrameSkipTracker::lastEvent() const {
    if (history_.isEmpty()) return {};
    return history_.back();
}

QVector<FrameDispatchEvent> FrameSkipTracker::recentEvents(int count) const {
    if (history_.isEmpty()) return {};
    const qsizetype n = history_.size();
    int start = n > count ? static_cast<int>(n - count) : 0;
    QVector<FrameDispatchEvent> result;
    for (int i = start; i < history_.size(); ++i) {
        result.push_back(history_[i]);
    }
    return result;
}

QVector<FrameDispatchEvent> FrameSkipTracker::skippedFrames() const {
    QVector<FrameDispatchEvent> result;
    for (const auto& e : history_) {
        if (e.skipped) {
            result.push_back(e);
        }
    }
    return result;
}

void FrameSkipTracker::reset() {
    skipCount_ = 0;
    consecutiveSkips_ = 0;
    history_.clear();
    current_ = {};
}

QString FrameSkipTracker::summary() const {
    return QStringLiteral("FrameSkipTracker: %1 skips (%2 consecutive)")
        .arg(skipCount_).arg(consecutiveSkips_);
}

} // namespace ArtifactCore
