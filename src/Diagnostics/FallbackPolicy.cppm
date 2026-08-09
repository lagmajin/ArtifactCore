module;
#include <utility>
#include <algorithm>
#include <mutex>
#include <QDateTime>
#include <QDebug>

module Core.Diagnostics.FallbackPolicy;

namespace ArtifactCore {

FallbackTracker* FallbackTracker::instance() {
    static FallbackTracker tracker;
    return &tracker;
}

void FallbackTracker::record(const FallbackEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(event);
    if (event.isWarning && warningsEnabled_) {
        qWarning() << "[FallbackTracker]" << event.message
                   << "original=" << event.originalId
                   << "resolved=" << event.resolvedId;
    }
}

void FallbackTracker::record(FallbackCategory category, FallbackAction action,
                              const QString& originalId, const QString& resolvedId,
                              const QString& message) {
    FallbackEvent event;
    event.timestamp = QDateTime::currentDateTime();
    event.category = category;
    event.action = action;
    event.originalId = originalId;
    event.resolvedId = resolvedId;
    event.message = message.isEmpty() ? "Fallback applied" : message;
    event.isWarning = (action != FallbackAction::Ignore);
    record(event);
}

std::vector<FallbackEvent> FallbackTracker::getEvents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
}

std::vector<FallbackEvent> FallbackTracker::getEventsByCategory(FallbackCategory category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<FallbackEvent> result;
    for (const auto& e : events_) {
        if (e.category == category) {
            result.push_back(e);
        }
    }
    return result;
}

std::vector<FallbackEvent> FallbackTracker::getEventsSince(const QDateTime& since) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<FallbackEvent> result;
    for (const auto& e : events_) {
        if (e.timestamp >= since) {
            result.push_back(e);
        }
    }
    return result;
}

void FallbackTracker::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
}

int FallbackTracker::totalCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(events_.size());
}

int FallbackTracker::countByCategory(FallbackCategory category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (const auto& e : events_) {
        if (e.category == category) ++count;
    }
    return count;
}

bool FallbackTracker::hasWarnings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& e : events_) {
        if (e.isWarning) return true;
    }
    return false;
}

void FallbackTracker::setPolicy(FallbackCategory category, const FallbackPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    switch (category) {
    case FallbackCategory::Font: fontPolicy_ = policy; break;
    case FallbackCategory::Image: imagePolicy_ = policy; break;
    case FallbackCategory::Color: colorPolicy_ = policy; break;
    case FallbackCategory::Effect: effectPolicy_ = policy; break;
    default: break;
    }
}

FallbackPolicy FallbackTracker::policy(FallbackCategory category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    switch (category) {
    case FallbackCategory::Font: return fontPolicy_;
    case FallbackCategory::Image: return imagePolicy_;
    case FallbackCategory::Color: return colorPolicy_;
    case FallbackCategory::Effect: return effectPolicy_;
    default: return FallbackPolicy();
    }
}

} // namespace ArtifactCore
