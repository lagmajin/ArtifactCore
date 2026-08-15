module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>

export module Animation.VirtualPointer;

export namespace ArtifactCore {

enum class PointerEventKind : std::uint8_t {
    None,
    Move,
    Press,
    Release,
    DoubleClick,
    DragStart,
    DragEnd,
    HoverEnter,
    HoverLeave,
};

struct VirtualPointerStyle {
    QString shape = QStringLiteral("arrow");
    float size = 32.0f;
    float opacity = 1.0f;
    bool showClickRipple = true;
    bool showTrail = false;

    QJsonObject toJson() const {
        QJsonObject object;
        object.insert(QStringLiteral("shape"), shape);
        object.insert(QStringLiteral("size"), static_cast<double>(size));
        object.insert(QStringLiteral("opacity"), static_cast<double>(opacity));
        object.insert(QStringLiteral("showClickRipple"), showClickRipple);
        object.insert(QStringLiteral("showTrail"), showTrail);
        return object;
    }

    static VirtualPointerStyle fromJson(const QJsonObject& object) {
        VirtualPointerStyle style;
        style.shape = object.value(QStringLiteral("shape")).toString(style.shape);
        style.size = std::clamp(static_cast<float>(object.value(QStringLiteral("size")).toDouble(style.size)), 1.0f, 512.0f);
        style.opacity = std::clamp(static_cast<float>(object.value(QStringLiteral("opacity")).toDouble(style.opacity)), 0.0f, 1.0f);
        style.showClickRipple = object.value(QStringLiteral("showClickRipple")).toBool(style.showClickRipple);
        style.showTrail = object.value(QStringLiteral("showTrail")).toBool(style.showTrail);
        return style;
    }
};

struct VirtualPointerFrame {
    qint64 frame = 0;
    double timestamp = 0.0;
    QPointF position;
    bool visible = true;
    std::uint32_t pressedButtons = 0;
    PointerEventKind eventKind = PointerEventKind::None;
    float pressure = 0.0f;
    float strength = 1.0f;

    QJsonObject toJson() const {
        QJsonObject object;
        object.insert(QStringLiteral("frame"), frame);
        object.insert(QStringLiteral("timestamp"), timestamp);
        object.insert(QStringLiteral("x"), position.x());
        object.insert(QStringLiteral("y"), position.y());
        object.insert(QStringLiteral("visible"), visible);
        object.insert(QStringLiteral("pressedButtons"), static_cast<qint64>(pressedButtons));
        object.insert(QStringLiteral("eventKind"), static_cast<int>(eventKind));
        object.insert(QStringLiteral("pressure"), static_cast<double>(pressure));
        object.insert(QStringLiteral("strength"), static_cast<double>(strength));
        return object;
    }

    static VirtualPointerFrame fromJson(const QJsonObject& object) {
        VirtualPointerFrame result;
        result.frame = object.value(QStringLiteral("frame")).toInteger(result.frame);
        result.timestamp = object.value(QStringLiteral("timestamp")).toDouble(result.timestamp);
        result.position = QPointF(object.value(QStringLiteral("x")).toDouble(),
                                  object.value(QStringLiteral("y")).toDouble());
        result.visible = object.value(QStringLiteral("visible")).toBool(result.visible);
        result.pressedButtons = static_cast<std::uint32_t>(
            std::max<qint64>(0, object.value(QStringLiteral("pressedButtons")).toInteger()));
        const int eventValue = object.value(QStringLiteral("eventKind")).toInt(0);
        result.eventKind = eventValue >= 0 && eventValue <= static_cast<int>(PointerEventKind::HoverLeave)
            ? static_cast<PointerEventKind>(eventValue)
            : PointerEventKind::None;
        result.pressure = std::clamp(static_cast<float>(object.value(QStringLiteral("pressure")).toDouble(result.pressure)), 0.0f, 1.0f);
        result.strength = std::clamp(static_cast<float>(object.value(QStringLiteral("strength")).toDouble(result.strength)), 0.0f, 1.0f);
        return result;
    }
};

struct VirtualPointerBinding {
    QString compositionId;
    QString layerId;
    QString propertyPath = QStringLiteral("transform.position");
    bool enabled = true;

    QJsonObject toJson() const {
        QJsonObject object;
        object.insert(QStringLiteral("compositionId"), compositionId);
        object.insert(QStringLiteral("layerId"), layerId);
        object.insert(QStringLiteral("propertyPath"), propertyPath);
        object.insert(QStringLiteral("enabled"), enabled);
        return object;
    }

    static VirtualPointerBinding fromJson(const QJsonObject& object) {
        VirtualPointerBinding binding;
        binding.compositionId = object.value(QStringLiteral("compositionId")).toString();
        binding.layerId = object.value(QStringLiteral("layerId")).toString();
        binding.propertyPath = object.value(QStringLiteral("propertyPath")).toString(binding.propertyPath);
        binding.enabled = object.value(QStringLiteral("enabled")).toBool(binding.enabled);
        return binding;
    }
};

class VirtualPointerTrack {
public:
    QString id;
    QString name = QStringLiteral("Pointer");
    double frameRate = 30.0;
    VirtualPointerStyle style;
    VirtualPointerBinding binding;
    QVector<VirtualPointerFrame> frames;

    void recordFrame(const VirtualPointerFrame& frame) {
        const auto existing = std::find_if(frames.begin(), frames.end(),
            [&frame](const auto& value) { return value.frame == frame.frame; });
        if (existing != frames.end()) {
            *existing = frame;
            return;
        }
        appendFrame(frame);
    }

    void appendFrame(const VirtualPointerFrame& frame) {
        frames.push_back(frame);
        std::sort(frames.begin(), frames.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.frame < rhs.frame;
        });
    }

    void clear() { frames.clear(); }

    bool isBound() const {
        return binding.enabled && !binding.compositionId.isEmpty() &&
               !binding.layerId.isEmpty() && !binding.propertyPath.isEmpty();
    }

    std::optional<VirtualPointerFrame> stateAtFrame(qint64 targetFrame) const {
        if (frames.isEmpty()) return std::nullopt;
        if (targetFrame <= frames.front().frame) return frames.front();
        if (targetFrame >= frames.back().frame) return frames.back();

        const auto upper = std::lower_bound(frames.cbegin(), frames.cend(), targetFrame,
            [](const auto& value, qint64 frame) { return value.frame < frame; });
        if (upper == frames.cend()) return frames.back();
        if (upper->frame == targetFrame) return *upper;

        const auto lower = upper - 1;
        const double span = static_cast<double>(upper->frame - lower->frame);
        const double amount = span > 0.0
            ? static_cast<double>(targetFrame - lower->frame) / span
            : 0.0;
        VirtualPointerFrame result = *lower;
        result.frame = targetFrame;
        result.timestamp = lower->timestamp + (upper->timestamp - lower->timestamp) * amount;
        result.position = QPointF(
            lower->position.x() + (upper->position.x() - lower->position.x()) * amount,
            lower->position.y() + (upper->position.y() - lower->position.y()) * amount);
        result.pressure = static_cast<float>(lower->pressure + (upper->pressure - lower->pressure) * amount);
        result.strength = static_cast<float>(lower->strength + (upper->strength - lower->strength) * amount);
        result.visible = amount < 0.5 ? lower->visible : upper->visible;
        result.pressedButtons = amount < 0.5 ? lower->pressedButtons : upper->pressedButtons;
        result.eventKind = amount < 0.5 ? lower->eventKind : upper->eventKind;
        return result;
    }

    std::optional<QPointF> positionAtFrame(qint64 targetFrame) const {
        const auto state = stateAtFrame(targetFrame);
        return state ? std::optional<QPointF>(state->position) : std::nullopt;
    }

    std::optional<VirtualPointerFrame> stateAtTime(double targetTime) const {
        if (frames.isEmpty() || !std::isfinite(targetTime)) return std::nullopt;
        if (targetTime <= frames.front().timestamp) return frames.front();
        if (targetTime >= frames.back().timestamp) return frames.back();

        const auto upper = std::lower_bound(frames.cbegin(), frames.cend(), targetTime,
            [](const auto& value, double time) { return value.timestamp < time; });
        if (upper == frames.cend()) return frames.back();
        if (upper->timestamp == targetTime) return *upper;

        const auto lower = upper - 1;
        const double span = upper->timestamp - lower->timestamp;
        const double amount = span > 0.0
            ? (targetTime - lower->timestamp) / span
            : 0.0;
        VirtualPointerFrame result = *lower;
        result.timestamp = targetTime;
        result.position = QPointF(
            lower->position.x() + (upper->position.x() - lower->position.x()) * amount,
            lower->position.y() + (upper->position.y() - lower->position.y()) * amount);
        result.pressure = static_cast<float>(lower->pressure + (upper->pressure - lower->pressure) * amount);
        result.strength = static_cast<float>(lower->strength + (upper->strength - lower->strength) * amount);
        result.visible = amount < 0.5 ? lower->visible : upper->visible;
        result.pressedButtons = amount < 0.5 ? lower->pressedButtons : upper->pressedButtons;
        result.eventKind = amount < 0.5 ? lower->eventKind : upper->eventKind;
        return result;
    }

    QVector<VirtualPointerFrame> eventFrames() const {
        QVector<VirtualPointerFrame> result;
        for (const auto& frame : frames) {
            if (frame.eventKind != PointerEventKind::None) result.push_back(frame);
        }
        return result;
    }

    QJsonObject toJson() const {
        QJsonObject object;
        object.insert(QStringLiteral("id"), id);
        object.insert(QStringLiteral("name"), name);
        object.insert(QStringLiteral("frameRate"), frameRate);
        object.insert(QStringLiteral("style"), style.toJson());
        object.insert(QStringLiteral("binding"), binding.toJson());
        QJsonArray frameArray;
        for (const auto& frame : frames) frameArray.append(frame.toJson());
        object.insert(QStringLiteral("frames"), frameArray);
        return object;
    }

    static VirtualPointerTrack fromJson(const QJsonObject& object) {
        VirtualPointerTrack track;
        track.id = object.value(QStringLiteral("id")).toString();
        track.name = object.value(QStringLiteral("name")).toString(track.name);
        track.frameRate = std::max(1.0, object.value(QStringLiteral("frameRate")).toDouble(track.frameRate));
        track.style = VirtualPointerStyle::fromJson(object.value(QStringLiteral("style")).toObject());
        track.binding = VirtualPointerBinding::fromJson(object.value(QStringLiteral("binding")).toObject());
        for (const auto& value : object.value(QStringLiteral("frames")).toArray()) {
            if (value.isObject()) track.appendFrame(VirtualPointerFrame::fromJson(value.toObject()));
        }
        return track;
    }
};

}
