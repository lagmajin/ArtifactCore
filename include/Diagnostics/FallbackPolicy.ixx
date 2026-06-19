module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <QString>
#include <QDebug>
#include <QDateTime>

export module Core.Diagnostics.FallbackPolicy;

import Container.Debug;
import Container.NamedVector;

export namespace ArtifactCore {

export enum class FallbackCategory {
    Font,
    Image,
    Color,
    Effect,
    Asset,
    Other
};

export enum class FallbackAction {
    Fallback,
    Bypass,
    Warning,
    Strict,
    Ignore
};

export struct FallbackEvent {
    QDateTime timestamp;
    FallbackCategory category;
    FallbackAction action;
    QString originalId;
    QString resolvedId;
    QString message;
    bool isWarning = true;
};

export struct FallbackPolicy {
    FallbackAction action = FallbackAction::Fallback;
    QString fallbackValue;
    QString warningMessage;
    bool enabled = true;
    bool logWarning = true;

    static FallbackPolicy defaultFont() {
        return {FallbackAction::Fallback, "Arial", "Font missing, using fallback", true, true};
    }

    static FallbackPolicy defaultImage() {
        return {FallbackAction::Fallback, "placeholder", "Image missing, using placeholder", true, true};
    }

    static FallbackPolicy defaultColor() {
        return {FallbackAction::Warning, "#ff00ffff", "Color token missing, using magenta warning", true, true};
    }

    static FallbackPolicy defaultEffect() {
        return {FallbackAction::Bypass, "", "Effect unsupported, bypassing", true, true};
    }
};

export class FallbackTracker {
public:
    static FallbackTracker* instance();

    void record(const FallbackEvent& event);
    void record(FallbackCategory category, FallbackAction action,
                const QString& originalId, const QString& resolvedId,
                const QString& message = "");

    std::vector<FallbackEvent> getEvents() const;
    std::vector<FallbackEvent> getEventsByCategory(FallbackCategory category) const;
    std::vector<FallbackEvent> getEventsSince(const QDateTime& since) const;
    void clear();

    int totalCount() const;
    int countByCategory(FallbackCategory category) const;
    bool hasWarnings() const;

    void setPolicy(FallbackCategory category, const FallbackPolicy& policy);
    FallbackPolicy policy(FallbackCategory category) const;

    bool warningsEnabled() const { return warningsEnabled_; }
    void setWarningsEnabled(bool enabled) { warningsEnabled_ = enabled; }

private:
    FallbackTracker() = default;

    NamedVector<FallbackEvent> events_{makeNamedVector<FallbackEvent>(ContainerName{"FallbackEvents"}, ARTIFACT_CONTAINER_HERE)};
    bool warningsEnabled_ = true;
    FallbackPolicy fontPolicy_{FallbackPolicy::defaultFont()};
    FallbackPolicy imagePolicy_{FallbackPolicy::defaultImage()};
    FallbackPolicy colorPolicy_{FallbackPolicy::defaultColor()};
    FallbackPolicy effectPolicy_{FallbackPolicy::defaultEffect()};
};

// --- inline helpers ---

template<typename T>
inline std::optional<T> tryFallbackPolicy(FallbackCategory category,
    std::function<std::optional<T>()> resolver,
    std::function<T()> fallback,
    const QString& originalId)
{
    auto* tracker = FallbackTracker::instance();
    FallbackPolicy policy = tracker->policy(category);

    if (!policy.enabled) {
        auto result = resolver();
        if (!result.has_value()) {
            tracker->record({QDateTime::currentDateTime(), category, FallbackAction::Strict,
                           originalId, "", "Resource missing and fallback disabled", true});
        }
        return result;
    }

    auto result = resolver();
    if (result.has_value()) {
        return result;
    }

    QString fb = policy.fallbackValue;

    if (policy.action == FallbackAction::Bypass) {
        tracker->record({QDateTime::currentDateTime(), category, FallbackAction::Bypass,
                       originalId, "[bypass]", policy.warningMessage, policy.logWarning});
        return std::nullopt;
    }

    if (policy.action == FallbackAction::Strict) {
        tracker->record({QDateTime::currentDateTime(), category, FallbackAction::Strict,
                       originalId, "", "Resource missing and policy is strict", true});
        return std::nullopt;
    }

    T fallbackResult = fallback();
    tracker->record({QDateTime::currentDateTime(), category, FallbackAction::Fallback,
                   originalId, fb, policy.warningMessage, policy.logWarning});
    return fallbackResult;
}

} // namespace ArtifactCore
