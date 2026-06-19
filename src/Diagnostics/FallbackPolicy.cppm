module;
#include <utility>
#include <algorithm>
#include <QDateTime>
#include <QDebug>

module Core.Diagnostics.FallbackPolicy;

import Container.NamedVector;

namespace ArtifactCore {

FallbackTracker* FallbackTracker::instance() {
    static FallbackTracker tracker;
    return &tracker;
}

void FallbackTracker::record(const FallbackEvent& event) {
    events_.add(event);
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
    return events_.toStdVector();
}

std::vector<FallbackEvent> FallbackTracker::getEventsByCategory(FallbackCategory category) const {
    auto result = makeNamedVector<FallbackEvent>(ContainerName{"FallbackEventsByCategory"}, ARTIFACT_CONTAINER_HERE);
    for (const auto& e : events_) {
        if (e.category == category) {
            result.add(e);
        }
    }
    return result.toStdVector();
}

std::vector<FallbackEvent> FallbackTracker::getEventsSince(const QDateTime& since) const {
    auto result = makeNamedVector<FallbackEvent>(ContainerName{"FallbackEventsSince"}, ARTIFACT_CONTAINER_HERE);
    for (const auto& e : events_) {
        if (e.timestamp >= since) {
            result.add(e);
        }
    }
    return result.toStdVector();
}

void FallbackTracker::clear() {
    events_.clear();
}

int FallbackTracker::totalCount() const {
    return static_cast<int>(events_.size());
}

int FallbackTracker::countByCategory(FallbackCategory category) const {
    int count = 0;
    for (const auto& e : events_) {
        if (e.category == category) ++count;
    }
    return count;
}

bool FallbackTracker::hasWarnings() const {
    for (const auto& e : events_) {
        if (e.isWarning) return true;
    }
    return false;
}

void FallbackTracker::setPolicy(FallbackCategory category, const FallbackPolicy& policy) {
    switch (category) {
    case FallbackCategory::Font: fontPolicy_ = policy; break;
    case FallbackCategory::Image: imagePolicy_ = policy; break;
    case FallbackCategory::Color: colorPolicy_ = policy; break;
    case FallbackCategory::Effect: effectPolicy_ = policy; break;
    default: break;
    }
}

FallbackPolicy FallbackTracker::policy(FallbackCategory category) const {
    switch (category) {
    case FallbackCategory::Font: return fontPolicy_;
    case FallbackCategory::Image: return imagePolicy_;
    case FallbackCategory::Color: return colorPolicy_;
    case FallbackCategory::Effect: return effectPolicy_;
    default: return FallbackPolicy();
    }
}

} // namespace ArtifactCore
