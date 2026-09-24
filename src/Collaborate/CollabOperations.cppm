module;

#include <cmath>
#include <algorithm>
#include <QString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>

export module Collaborate.Operations;

// Re-exported: these types appear in this module's public API.
export import Collaborate.Session;
export import Collaborate.Review;
export import Core.ArtifactUtility;

export namespace ArtifactCore {

// ---- typed operation schema ----
//
// The wire format keeps `type` + `layerId` at the top level and a schema-
// specific `payload` object. Validators enforce the contract so producers
// cannot broadcast malformed operations.

inline constexpr const char* kOpPropertySet = "property.set";
inline constexpr const char* kOpPropertyBatch = "property.batch";
inline constexpr const char* kOpPropertyKeyframes = "property.keyframes";
inline constexpr const char* kOpPropertyExpression = "property.expression";
inline constexpr const char* kOpLayerComponents = "layer.components";
inline constexpr const char* kOpLayerAudioDeClickRanges = "layer.audioDeClickRanges";
inline constexpr const char* kOpLayerDeformation2D = "layer.deformation2D";
inline constexpr const char* kOpLayerSolidSize = "layer.solidSize";
inline constexpr const char* kOpLayerSourceCrop = "layer.sourceCrop";
inline constexpr const char* kOpLayerShapePolygon = "layer.shapePolygon";
inline constexpr const char* kOpLayerShapePath = "layer.shapePath";
inline constexpr const char* kOpLayerShapeOperator = "layer.shapeOperator";
inline constexpr const char* kOpLayerShapeContents = "layer.shapeContents";
inline constexpr const char* kOpLayerStack = "layer.stack";
inline constexpr const char* kOpLayerText = "layer.text";
inline constexpr const char* kOpLayerRename = "layer.rename";
inline constexpr const char* kOpLayerVariant = "layer.variant";
inline constexpr const char* kOpLayerBlendMode = "layer.blendMode";
inline constexpr const char* kOpLayerOpacity = "layer.opacity";
inline constexpr const char* kOpLayerParent = "layer.parent";
inline constexpr const char* kOpLayerAnimationStack = "layer.animationStack";
inline constexpr const char* kOpLayerVisibility = "layer.visibility";
inline constexpr const char* kOpLayerFlag = "layer.flag";
inline constexpr const char* kOpLayerEditLock = "layer.editLock";
inline constexpr const char* kOpLayerTransform = "layer.transform";
inline constexpr const char* kOpLayerMoveAtFrame = "layer.moveAtFrame";
inline constexpr const char* kOpLayerAdd = "layer.add";
inline constexpr const char* kOpLayerRemove = "layer.remove";
inline constexpr const char* kOpLayerReorder = "layer.reorder";
inline constexpr const char* kOpReviewCommentAdd = "review.comment.add";
inline constexpr const char* kOpReviewCommentResolve = "review.comment.resolve";
inline constexpr const char* kOpReviewCommentEdit = "review.comment.edit";
inline constexpr const char* kOpReviewCommentRemove = "review.comment.remove";

namespace detail {
[[nodiscard]] inline CollabOperationData makeBaseOperation(
    const QString& clientId, const QString& type, const QString& layerId,
    const qint64 atMs) {
    CollabOperationData op;
    op.type = type;
    op.layerId = layerId;
    op.clientId = clientId;
    op.timestampMs = atMs;
    return op;
}

[[nodiscard]] inline bool hasNumber(const QJsonObject& obj, const QString& key) {
    if (!obj.contains(key)) return false;
    const double value = obj.value(key).toDouble();
    return std::isfinite(value);
}

[[nodiscard]] inline bool isExternalPathFieldKey(const QString& fieldKey) {
    const QString key = fieldKey.toLower();
    for (const QString& token : {QStringLiteral("sourcepath"),
                                 QStringLiteral("filepath"),
                                 QStringLiteral("sequencepaths"),
                                 QStringLiteral("depthmappath"),
                                 QStringLiteral("texturepath"),
                                 QStringLiteral("proxypath"),
                                 QStringLiteral("assetpath"),
                                 QStringLiteral("mediapath"),
                                 QStringLiteral("sourceuri")}) {
        if (key.contains(token)) return true;
    }
    return false;
}

[[nodiscard]] inline bool hasExternalPathString(
    const QJsonValue& value, const QString& parentKey = {}) {
    if (value.isString()) {
        return isExternalPathFieldKey(parentKey) &&
               !value.toString().trimmed().isEmpty();
    }
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            const QString key = it.key().toLower();
            const QJsonValue child = it.value();
            if (hasExternalPathString(child, key)) return true;
        }
    } else if (value.isArray()) {
        for (const QJsonValue& child : value.toArray()) {
            if (hasExternalPathString(child, parentKey)) return true;
        }
    }
    return false;
}

[[nodiscard]] inline bool isAssetPathPropertyPath(const QString& propertyPath) {
    return isExternalPathFieldKey(propertyPath);
}
} // namespace detail

[[nodiscard]] inline CollabOperationData makePropertySetOperation(
    const QString& clientId, const QString& layerId,
    const QString& propertyPath, const QJsonValue& value, const qint64 atMs) {
    auto op = detail::makeBaseOperation(clientId, kOpPropertySet, layerId, atMs);
    op.payload = QJsonObject{
        {QStringLiteral("propertyPath"), propertyPath},
        {QStringLiteral("value"), value}};
    return op;
}

[[nodiscard]] inline CollabOperationData makePropertyCompareSetOperation(
    const QString& clientId, const QString& layerId,
    const QString& propertyPath, const QJsonValue& expectedValue,
    const QJsonValue& value, const qint64 atMs) {
    auto op = makePropertySetOperation(clientId, layerId, propertyPath,
                                       value, atMs);
    op.payload.insert(QStringLiteral("expectedValue"), expectedValue);
    return op;
}

[[nodiscard]] inline CollabOperationData makeLayerTransformOperation(
    const QString& clientId, const QString& layerId, const double positionX,
    const double positionY, const double rotationDegrees, const double scaleX,
    const double scaleY, const qint64 atMs) {
    auto op = detail::makeBaseOperation(clientId, kOpLayerTransform, layerId, atMs);
    op.payload = QJsonObject{
        {QStringLiteral("positionX"), positionX},
        {QStringLiteral("positionY"), positionY},
        {QStringLiteral("rotationDegrees"), rotationDegrees},
        {QStringLiteral("scaleX"), scaleX},
        {QStringLiteral("scaleY"), scaleY}};
    return op;
}

[[nodiscard]] inline CollabOperationData makeLayerAddOperation(
    const QString& clientId, const QString& layerId, const QString& layerType,
    const QString& name, const QJsonObject& layerJson, const qint64 atMs) {
    auto op = detail::makeBaseOperation(clientId, kOpLayerAdd, layerId, atMs);
    op.payload = QJsonObject{
        {QStringLiteral("layerType"), layerType},
        {QStringLiteral("name"), name},
        {QStringLiteral("layerJson"), layerJson}};
    return op;
}

[[nodiscard]] inline CollabOperationData makeLayerRemoveOperation(
    const QString& clientId, const QString& layerId, const qint64 atMs) {
    return detail::makeBaseOperation(clientId, kOpLayerRemove, layerId, atMs);
}

[[nodiscard]] inline CollabOperationData makeReviewCommentAddOperation(
    const QString& clientId, const CollabComment& comment, const qint64 atMs) {
    auto op = detail::makeBaseOperation(clientId, kOpReviewCommentAdd,
                                        comment.layerId, atMs);
    op.payload = QJsonObject{
        {QStringLiteral("commentId"), comment.commentId},
        {QStringLiteral("parentCommentId"), comment.parentCommentId},
        {QStringLiteral("authorClientId"), comment.authorClientId},
        {QStringLiteral("authorUserId"), comment.authorUserId},
        {QStringLiteral("authorName"), comment.authorName},
        {QStringLiteral("compositionId"), comment.compositionId},
        {QStringLiteral("layerId"), comment.layerId},
        {QStringLiteral("frame"), comment.frame},
        {QStringLiteral("text"), comment.text},
        {QStringLiteral("createdAtMs"), comment.createdAtMs}};
    return op;
}

[[nodiscard]] inline CollabOperationData makeReviewCommentResolveOperation(
    const QString& clientId, const QString& commentId, const bool resolved,
    const qint64 atMs) {
    auto op = detail::makeBaseOperation(clientId, kOpReviewCommentResolve,
                                        QString(), atMs);
    op.payload = QJsonObject{
        {QStringLiteral("commentId"), commentId},
        {QStringLiteral("actorClientId"), clientId},
        {QStringLiteral("resolved"), resolved},
        {QStringLiteral("createdAtMs"), atMs}};
    return op;
}

[[nodiscard]] inline CollabOperationData makeReviewCommentEditOperation(
    const QString& clientId, const QString& userName, const QString& commentId,
    const QString& text, const qint64 atMs) {
    auto op = detail::makeBaseOperation(clientId, kOpReviewCommentEdit,
                                        QString(), atMs);
    op.payload = QJsonObject{
        {QStringLiteral("commentId"), commentId},
        {QStringLiteral("actorClientId"), clientId},
        {QStringLiteral("actorName"), userName.trimmed()},
        {QStringLiteral("text"), text.trimmed()},
        {QStringLiteral("createdAtMs"), atMs}};
    return op;
}

[[nodiscard]] inline CollabOperationData makeReviewCommentRemoveOperation(
    const QString& clientId, const QString& commentId, const qint64 atMs) {
    auto op = detail::makeBaseOperation(clientId, kOpReviewCommentRemove,
                                        QString(), atMs);
    op.payload = QJsonObject{
        {QStringLiteral("commentId"), commentId},
        {QStringLiteral("actorClientId"), clientId},
        {QStringLiteral("createdAtMs"), atMs}};
    return op;
}

[[nodiscard]] inline bool applyReviewOperation(
    CollaborationReview& review, const CollabOperationData& op) {
    if (op.type == kOpReviewCommentEdit) {
        return review.editComment(
            op.payload.value(QStringLiteral("commentId")).toString(),
            op.payload.value(QStringLiteral("text")).toString(),
            op.payload.value(QStringLiteral("actorClientId")).toString(),
            op.payload.value(QStringLiteral("actorName")).toString(),
            op.payload.value(QStringLiteral("createdAtMs")).toVariant().toLongLong());
    }
    if (op.type == kOpReviewCommentRemove) {
        return review.deleteComment(
            op.payload.value(QStringLiteral("commentId")).toString(),
            op.payload.value(QStringLiteral("actorClientId")).toString());
    }
    if (op.type == kOpReviewCommentResolve) {
        const QString commentId =
            op.payload.value(QStringLiteral("commentId")).toString();
        const bool resolved =
            op.payload.value(QStringLiteral("resolved")).toBool();
        return resolved ? review.resolveComment(commentId)
                        : review.reopenComment(commentId);
    }
    if (op.type != kOpReviewCommentAdd) return false;
    CollabComment comment;
    comment.commentId = op.payload.value(QStringLiteral("commentId")).toString();
    comment.parentCommentId =
        op.payload.value(QStringLiteral("parentCommentId")).toString();
    comment.authorClientId =
        op.payload.value(QStringLiteral("authorClientId")).toString();
    comment.authorUserId =
        op.payload.value(QStringLiteral("authorUserId")).toString();
    comment.authorName =
        op.payload.value(QStringLiteral("authorName")).toString();
    comment.compositionId =
        op.payload.value(QStringLiteral("compositionId")).toString();
    comment.layerId = op.payload.value(QStringLiteral("layerId")).toString();
    comment.frame = op.payload.value(QStringLiteral("frame")).toVariant().toLongLong();
    comment.text = op.payload.value(QStringLiteral("text")).toString();
    comment.createdAtMs =
        op.payload.value(QStringLiteral("createdAtMs")).toVariant().toLongLong();
    return review.importComment(comment);
}

// Validates `op.payload` against the schema implied by `op.type`.
// Returns an empty string when valid; otherwise a diagnostic.
[[nodiscard]] inline QString validateCollabOperation(
    const CollabOperationData& op) {
    const QJsonObject payload = op.payload;
    if (op.type.trimmed().isEmpty()) {
        return QStringLiteral("operation requires a non-empty type");
    }
    const bool requiresLayerId =
        op.type == kOpPropertySet || op.type == kOpLayerTransform ||
        op.type == kOpLayerMoveAtFrame ||
        op.type == kOpPropertyKeyframes || op.type == kOpPropertyExpression ||
        op.type == kOpLayerComponents || op.type == kOpLayerAudioDeClickRanges ||
        op.type == kOpLayerDeformation2D || op.type == kOpLayerSolidSize ||
        op.type == kOpLayerSourceCrop ||
        op.type == kOpLayerShapePolygon ||
        op.type == kOpLayerShapePath ||
        op.type == kOpLayerShapeOperator ||
        op.type == kOpLayerShapeContents ||
        op.type == kOpLayerStack ||
        op.type == kOpLayerText || op.type == kOpLayerRename ||
        op.type == kOpLayerVariant ||
        op.type == kOpLayerBlendMode ||
        op.type == kOpLayerOpacity ||
        op.type == kOpLayerParent ||
        op.type == kOpLayerAnimationStack ||
        op.type == kOpLayerVisibility ||
        op.type == kOpLayerFlag ||
        op.type == kOpLayerEditLock ||
        op.type == kOpLayerAdd ||
        op.type == kOpLayerRemove ||
        op.type == kOpLayerReorder;
    if (requiresLayerId && op.layerId.trimmed().isEmpty()) {
        return QStringLiteral("%1 requires a non-empty layerId").arg(op.type);
    }
    if (op.type == kOpPropertySet) {
        const QString path =
            payload.value(QStringLiteral("propertyPath")).toString();
        const QJsonValue value = payload.value(QStringLiteral("value"));
        const QJsonValue expected = payload.value(QStringLiteral("expectedValue"));
        if (path.trimmed().isEmpty()) {
            return QStringLiteral("property.set requires a non-empty propertyPath");
        }
        if (!payload.contains(QStringLiteral("value"))) {
            return QStringLiteral("property.set requires a value");
        }
        if (detail::isAssetPathPropertyPath(path) ||
            detail::hasExternalPathString(value) ||
            detail::hasExternalPathString(expected)) {
            return QStringLiteral("property.set cannot synchronize local asset paths");
        }
        return {};
    }
    if (op.type == kOpPropertyBatch) {
        const QJsonValue changesValue = payload.value(QStringLiteral("changes"));
        if (!changesValue.isArray()) {
            return QStringLiteral("property.batch requires a changes array");
        }
        const QJsonArray changes = changesValue.toArray();
        if (changes.isEmpty() || changes.size() > 128) {
            return QStringLiteral("property.batch requires 1 to 128 changes");
        }
        QSet<QString> targets;
        for (const QJsonValue& changeValue : changes) {
            if (!changeValue.isObject()) {
                return QStringLiteral("property.batch entries must be objects");
            }
            const QJsonObject change = changeValue.toObject();
            const QString layerId = change.value(QStringLiteral("layerId")).toString();
            const QString path = change.value(QStringLiteral("propertyPath")).toString();
            const QString kind = change.value(QStringLiteral("kind")).toString(
                QStringLiteral("value"));
            const QJsonValue expectedValue = change.value(QStringLiteral("expectedValue"));
            const QJsonValue nextValue = change.value(QStringLiteral("value"));
            bool validChange = false;
            if (kind == QStringLiteral("value")) {
                validChange = change.contains(QStringLiteral("expectedValue")) &&
                              change.contains(QStringLiteral("value"));
            } else if (kind == QStringLiteral("keyframes")) {
                const QJsonValue expected = change.value(QStringLiteral("expectedKeyframes"));
                const QJsonValue value = change.value(QStringLiteral("keyframes"));
                const bool hasExpectedAnimatable = change.contains(QStringLiteral("expectedAnimatable"));
                const bool hasAnimatable = change.contains(QStringLiteral("animatable"));
                validChange = expected.isArray() && value.isArray() &&
                    expected.toArray().size() <= 100000 && value.toArray().size() <= 100000 &&
                    QJsonDocument(expected.toArray()).toJson(QJsonDocument::Compact).size() <= 262144 &&
                    QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact).size() <= 262144 &&
                    hasExpectedAnimatable == hasAnimatable &&
                    (!hasExpectedAnimatable ||
                     (change.value(QStringLiteral("expectedAnimatable")).isBool() &&
                      change.value(QStringLiteral("animatable")).isBool()));
            } else if (kind == QStringLiteral("expression")) {
                const QJsonValue expected = change.value(QStringLiteral("expectedExpression"));
                const QJsonValue value = change.value(QStringLiteral("expression"));
                validChange = expected.isString() && value.isString() &&
                    expected.toString().toUtf8().size() <= 262144 &&
                    value.toString().toUtf8().size() <= 262144;
            }
            if (layerId.trimmed().isEmpty() || path.trimmed().isEmpty() || !validChange) {
                return QStringLiteral("property.batch entries require a valid kind-specific payload, layerId, and propertyPath");
            }
            if (detail::isAssetPathPropertyPath(path) ||
                detail::hasExternalPathString(expectedValue) ||
                detail::hasExternalPathString(nextValue) ||
                detail::hasExternalPathString(change.value(QStringLiteral("expectedKeyframes"))) ||
                detail::hasExternalPathString(change.value(QStringLiteral("keyframes"))) ||
                detail::hasExternalPathString(change.value(QStringLiteral("expectedExpression"))) ||
                detail::hasExternalPathString(change.value(QStringLiteral("expression")))) {
                return QStringLiteral("property.batch cannot synchronize local asset paths");
            }
            const QString target = layerId + QChar(0x1f) + path;
            if (targets.contains(target)) {
                return QStringLiteral("property.batch cannot change the same property twice");
            }
            targets.insert(target);
        }
        if (QJsonDocument(payload).toJson(QJsonDocument::Compact).size() > 1048576) {
            return QStringLiteral("property.batch payload exceeds the 1 MiB limit");
        }
        return {};
    }
    if (op.type == kOpPropertyKeyframes) {
        const QString path = payload.value(QStringLiteral("propertyPath")).toString();
        if (path.trimmed().isEmpty() || detail::isAssetPathPropertyPath(path) ||
            !payload.value(QStringLiteral("expectedKeyframes")).isArray() ||
            !payload.value(QStringLiteral("keyframes")).isArray() ||
            payload.value(QStringLiteral("expectedKeyframes")).toArray().size() > 100000 ||
            payload.value(QStringLiteral("keyframes")).toArray().size() > 100000 ||
            detail::hasExternalPathString(payload.value(QStringLiteral("expectedKeyframes"))) ||
            detail::hasExternalPathString(payload.value(QStringLiteral("keyframes")))) {
            return QStringLiteral("property.keyframes requires a propertyPath and keyframe arrays of at most 100000 entries");
        }
        const bool hasExpectedAnimatable =
            payload.contains(QStringLiteral("expectedAnimatable"));
        const bool hasAnimatable = payload.contains(QStringLiteral("animatable"));
        if (hasExpectedAnimatable != hasAnimatable ||
            (hasExpectedAnimatable &&
             (!payload.value(QStringLiteral("expectedAnimatable")).isBool() ||
              !payload.value(QStringLiteral("animatable")).isBool()))) {
            return QStringLiteral("property.keyframes animatable flags must be paired booleans");
        }
        return {};
    }
    if (op.type == kOpPropertyExpression) {
        const QString path = payload.value(QStringLiteral("propertyPath")).toString();
        if (path.trimmed().isEmpty() || detail::isAssetPathPropertyPath(path) ||
            !payload.value(QStringLiteral("expectedExpression")).isString() ||
            !payload.value(QStringLiteral("expression")).isString()) {
            return QStringLiteral("property.expression requires a propertyPath and expected/next expression strings");
        }
        return {};
    }
    if (op.type == kOpLayerComponents) {
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        if (!expected.isObject() || !value.isObject() ||
            detail::hasExternalPathString(expected) ||
            detail::hasExternalPathString(value) ||
            QJsonDocument(expected.toObject()).toJson(QJsonDocument::Compact).size() > 262144 ||
            QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact).size() > 262144) {
            return QStringLiteral("layer.components requires expected and next component snapshots");
        }
        return {};
    }
    if (op.type == kOpLayerAudioDeClickRanges) {
        const auto validRanges = [](const QJsonValue& input) {
            if (!input.isArray()) return false;
            const auto ranges = input.toArray();
            if (ranges.size() > 8192 || QJsonDocument(ranges).toJson(QJsonDocument::Compact).size() > 262144) return false;
            qint64 previousEnd = -1;
            for (const auto& entry : ranges) {
                if (!entry.isArray() || entry.toArray().size() != 2) return false;
                const auto pair = entry.toArray();
                qint64 start = 0, end = 0;
                for (int i = 0; i < 2; ++i) {
                    if (!pair.at(i).isString()) return false;
                    bool ok = false;
                    const qint64 parsed = pair.at(i).toString().toLongLong(&ok, 10);
                    if (!ok || parsed < 0 || QString::number(parsed) != pair.at(i).toString()) return false;
                    (i == 0 ? start : end) = parsed;
                }
                if (start >= end || (previousEnd >= 0 && start <= previousEnd)) return false;
                previousEnd = end;
            }
            return true;
        };
        if (!validRanges(payload.value(QStringLiteral("expected"))) ||
            !validRanges(payload.value(QStringLiteral("value"))))
            return QStringLiteral("layer.audioDeClickRanges requires bounded normalized decimal-string ranges");
        return {};
    }
    if (op.type == kOpLayerDeformation2D) {
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        if (!expected.isObject() || !value.isObject() ||
            detail::hasExternalPathString(expected) || detail::hasExternalPathString(value) ||
            QJsonDocument(expected.toObject()).toJson(QJsonDocument::Compact).size() > 262144 ||
            QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact).size() > 262144)
            return QStringLiteral("layer.deformation2D requires bounded path-free state snapshots");
        return {};
    }
    if (op.type == kOpLayerSolidSize) {
        const auto validSize = [](const QJsonValue& value) {
            if (!value.isObject()) return false;
            const QJsonObject size = value.toObject();
            const auto validDimension = [](const QJsonValue& dimension) {
                if (!dimension.isDouble()) return false;
                const double number = dimension.toDouble();
                return std::isfinite(number) && std::floor(number) == number &&
                       number >= 1.0 && number <= 16384.0;
            };
            return validDimension(size.value(QStringLiteral("width"))) &&
                   validDimension(size.value(QStringLiteral("height")));
        };
        if (!validSize(payload.value(QStringLiteral("expected"))) ||
            !validSize(payload.value(QStringLiteral("value"))))
            return QStringLiteral("layer.solidSize requires expected/value integer dimensions in [1, 16384]");
        return {};
    }
    if (op.type == kOpLayerSourceCrop) {
        const auto validSnapshot = [](const QJsonValue& input) {
            if (!input.isObject()) return false;
            const QJsonObject object = input.toObject();
            if (object.size() != 7 ||
                !object.value(QStringLiteral("enabled")).isBool() ||
                !object.value(QStringLiteral("preserveAspect")).isBool()) return false;
            const auto finiteInRange = [](const QJsonValue& value,
                                          const double minimum,
                                          const double maximum) {
                if (!value.isDouble()) return false;
                const double number = value.toDouble();
                return std::isfinite(number) && number >= minimum && number <= maximum;
            };
            const auto validPair = [&](const QString& key, const double minimum,
                                       const double maximum) {
                const QJsonValue value = object.value(key);
                if (!value.isArray() || value.toArray().size() != 2) return false;
                const auto pair = value.toArray();
                return finiteInRange(pair[0], minimum, maximum) &&
                       finiteInRange(pair[1], minimum, maximum);
            };
            const QJsonValue rectValue = object.value(QStringLiteral("cropRect"));
            if (!rectValue.isArray() || rectValue.toArray().size() != 4) return false;
            const auto rect = rectValue.toArray();
            return finiteInRange(rect[0], -1000000.0, 1000000.0) &&
                   finiteInRange(rect[1], -1000000.0, 1000000.0) &&
                   finiteInRange(rect[2], 0.0, 1000000.0) &&
                   finiteInRange(rect[3], 0.0, 1000000.0) &&
                   validPair(QStringLiteral("pan"), -1000000.0, 1000000.0) &&
                   finiteInRange(object.value(QStringLiteral("zoom")), 0.001, 1000.0) &&
                   finiteInRange(object.value(QStringLiteral("rotation")), -360000.0, 360000.0) &&
                   validPair(QStringLiteral("anchor"), 0.0, 1.0) &&
                   object.contains(QStringLiteral("preserveAspect"));
        };
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        if (!validSnapshot(expected) || !validSnapshot(value) ||
            QJsonDocument(expected.toObject()).toJson(QJsonDocument::Compact).size() > 32768 ||
            QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact).size() > 32768) {
            return QStringLiteral("layer.sourceCrop requires bounded canonical expected/value snapshots");
        }
        return {};
    }
    if (op.type == kOpLayerShapePolygon) {
        const auto validSnapshot = [](const QJsonValue& input) {
            if (!input.isObject()) return false;
            const QJsonObject object = input.toObject();
            if (object.size() != 2 ||
                !object.value(QStringLiteral("closed")).isBool() ||
                !object.value(QStringLiteral("points")).isArray()) return false;
            const QJsonArray points = object.value(QStringLiteral("points")).toArray();
            if (points.size() > 100000) return false;
            for (const QJsonValue& pointValue : points) {
                if (!pointValue.isArray() || pointValue.toArray().size() != 2)
                    return false;
                for (const QJsonValue& coordinate : pointValue.toArray()) {
                    if (!coordinate.isDouble() || !std::isfinite(coordinate.toDouble()) ||
                        std::abs(coordinate.toDouble()) > 1000000.0) return false;
                }
            }
            return true;
        };
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        if (!validSnapshot(expected) || !validSnapshot(value) ||
            QJsonDocument(expected.toObject()).toJson(QJsonDocument::Compact).size() > 262144 ||
            QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact).size() > 262144)
            return QStringLiteral("layer.shapePolygon requires bounded point snapshots");
        return {};
    }
    if (op.type == kOpLayerShapePath) {
        const auto validSnapshot = [](const QJsonValue& input) {
            if (!input.isObject()) return false;
            const QJsonObject object = input.toObject();
            if (object.size() != 2 ||
                !object.value(QStringLiteral("polygon")).isObject() ||
                !object.value(QStringLiteral("path")).isObject()) return false;
            const QJsonObject polygon = object.value(QStringLiteral("polygon")).toObject();
            const QJsonObject path = object.value(QStringLiteral("path")).toObject();
            if (polygon.size() != 2 || path.size() != 2 ||
                !polygon.value(QStringLiteral("closed")).isBool() ||
                !path.value(QStringLiteral("closed")).isBool() ||
                !polygon.value(QStringLiteral("points")).isArray() ||
                !path.value(QStringLiteral("vertices")).isArray()) return false;
            const QJsonArray points = polygon.value(QStringLiteral("points")).toArray();
            const QJsonArray vertices = path.value(QStringLiteral("vertices")).toArray();
            if (points.size() > 100000) return false;
            for (const QJsonValue& point : points) {
                if (!point.isArray() || point.toArray().size() != 2) return false;
                for (const QJsonValue& coordinate : point.toArray()) {
                    if (!coordinate.isDouble() || !std::isfinite(coordinate.toDouble()) ||
                        std::abs(coordinate.toDouble()) > 1000000.0) return false;
                }
            }
            if (vertices.size() > 100000) return false;
            for (const QJsonValue& vertex : vertices) {
                if (!vertex.isArray() || vertex.toArray().size() != 7 ||
                    !vertex.toArray()[6].isBool()) return false;
                for (int i = 0; i < 6; ++i) {
                    const QJsonValue coordinate = vertex.toArray()[i];
                    if (!coordinate.isDouble() ||
                        !std::isfinite(coordinate.toDouble()) ||
                        std::abs(coordinate.toDouble()) > 1000000.0) return false;
                }
            }
            return points.isEmpty() || vertices.isEmpty();
        };
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        if (!validSnapshot(expected) || !validSnapshot(value) ||
            QJsonDocument(expected.toObject()).toJson(QJsonDocument::Compact).size() > 262144 ||
            QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact).size() > 262144)
            return QStringLiteral("layer.shapePath requires bounded vertex snapshots");
        return {};
    }
    if (op.type == kOpLayerShapeOperator) {
        const QJsonValue index = payload.value(QStringLiteral("operatorIndex"));
        const QString field = payload.value(QStringLiteral("field")).toString();
        const QJsonValue expected = payload.value(QStringLiteral("expectedValue"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        const bool safeField = !field.isEmpty() &&
            field.size() <= 64 &&
            ((field.front() >= QLatin1Char('A') && field.front() <= QLatin1Char('Z')) ||
             (field.front() >= QLatin1Char('a') && field.front() <= QLatin1Char('z'))) &&
            std::all_of(field.cbegin(), field.cend(), [](const QChar character) {
                return (character >= QLatin1Char('A') && character <= QLatin1Char('Z')) ||
                       (character >= QLatin1Char('a') && character <= QLatin1Char('z')) ||
                       (character >= QLatin1Char('0') && character <= QLatin1Char('9'));
            });
        if (!index.isDouble() || index.toDouble() < 0 ||
            index.toDouble() > 100000 || std::floor(index.toDouble()) != index.toDouble() ||
            !safeField || field.toUtf8().size() > 64 ||
            !expected.isDouble() || !std::isfinite(expected.toDouble()) ||
            !value.isDouble() || !std::isfinite(value.toDouble())) {
            return QStringLiteral("layer.shapeOperator requires a bounded index, field, and finite expected/value numbers");
        }
        return {};
    }
    if (op.type == kOpLayerShapeContents) {
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        const auto validSnapshot = [](const QJsonValue& snapshot) {
            if (!snapshot.isObject()) return false;
            const QJsonObject object = snapshot.toObject();
            const QJsonValue contents = object.value(QStringLiteral("contents"));
            const QJsonValue nodes = object.value(QStringLiteral("stackNodes"));
            if (!contents.isArray() || contents.toArray().size() > 256 ||
                !nodes.isArray() || nodes.toArray().size() > 1024 ||
                object.size() != 3 ||
                !object.value(QStringLiteral("activeContentIndex")).isDouble() ||
                std::floor(object.value(QStringLiteral("activeContentIndex")).toDouble()) !=
                    object.value(QStringLiteral("activeContentIndex")).toDouble() ||
                object.value(QStringLiteral("activeContentIndex")).toInt(-2) < -1 ||
                object.value(QStringLiteral("activeContentIndex")).toInt(-2) >= contents.toArray().size())
                return false;
            for (const QJsonValue& content : contents.toArray())
                if (!content.isObject()) return false;
            for (const QJsonValue& node : nodes.toArray())
                if (!node.isObject()) return false;
            return QJsonDocument(object).toJson(QJsonDocument::Compact).size() <= 262144;
        };
        if (!validSnapshot(expected) || !validSnapshot(value) ||
            detail::hasExternalPathString(expected) ||
            detail::hasExternalPathString(value))
            return QStringLiteral("layer.shapeContents requires bounded content and stack snapshots");
        return {};
    }
    if (op.type == kOpLayerStack) {
        const QString stackKind = payload.value(QStringLiteral("stackKind")).toString();
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        if ((stackKind != QStringLiteral("clonerTransforms") &&
             stackKind != QStringLiteral("cloneEffectors") &&
             stackKind != QStringLiteral("textAnimators")) ||
            !expected.isArray() || !value.isArray() ||
            detail::hasExternalPathString(expected) ||
            detail::hasExternalPathString(value) ||
            QJsonDocument(expected.toArray()).toJson(QJsonDocument::Compact).size() > 262144 ||
            QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact).size() > 262144) {
            return QStringLiteral("layer.stack requires a supported stack kind and bounded array snapshots");
        }
        return {};
    }
    if (op.type == kOpLayerText) {
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        if (!expected.isString() || !value.isString() ||
            expected.toString().toUtf8().size() > 131072 ||
            value.toString().toUtf8().size() > 131072) {
            return QStringLiteral("layer.text requires bounded expected and next text values");
        }
        return {};
    }
    if (op.type == kOpLayerRename) {
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        if (!expected.isString() || !value.isString() ||
            expected.toString().toUtf8().size() > 4096 ||
            value.toString().toUtf8().size() > 4096) {
            return QStringLiteral("layer.rename requires bounded expected and next names");
        }
        return {};
    }
    if (op.type == kOpLayerVariant) {
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        const auto validIndex = [](const QJsonValue& item) {
            if (!item.isDouble()) return false;
            const double number = item.toDouble();
            return std::isfinite(number) && number >= 0.0 && number <= 100000.0 &&
                   std::floor(number) == number;
        };
        if (!validIndex(expected) || !validIndex(value)) {
            return QStringLiteral("layer.variant requires non-negative integer expected and next indices");
        }
        return {};
    }
    if (op.type == kOpLayerBlendMode) {
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        const auto validMode = [](const QJsonValue& item) {
            if (!item.isDouble()) return false;
            const double number = item.toDouble();
            return std::isfinite(number) && number >= 0.0 && number <= 33.0 &&
                   std::floor(number) == number;
        };
        if (!validMode(expected) || !validMode(value)) {
            return QStringLiteral("layer.blendMode requires valid expected and next mode indices");
        }
        return {};
    }
    if (op.type == kOpLayerOpacity) {
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue value = payload.value(QStringLiteral("value"));
        if (!expected.isDouble() || !value.isDouble() ||
            !std::isfinite(expected.toDouble()) || !std::isfinite(value.toDouble()) ||
            expected.toDouble() < 0.0 || expected.toDouble() > 1.0 ||
            value.toDouble() < 0.0 || value.toDouble() > 1.0) {
            return QStringLiteral("layer.opacity requires finite expected and next values in [0, 1]");
        }
        return {};
    }
    if (op.type == kOpLayerParent) {
        const QJsonValue expected = payload.value(QStringLiteral("expectedParentId"));
        const QJsonValue next = payload.value(QStringLiteral("parentId"));
        if (!expected.isString() || !next.isString() ||
            expected.toString().size() > 64 || next.toString().size() > 64 ||
            expected.toString().trimmed().isEmpty() || next.toString().trimmed().isEmpty()) {
            return QStringLiteral("layer.parent requires bounded expected and next parent IDs");
        }
        if (next.toString().compare(op.layerId, Qt::CaseInsensitive) == 0) {
            return QStringLiteral("layer.parent cannot assign a layer as its own parent");
        }
        return {};
    }
    if (op.type == kOpLayerAnimationStack) {
        const QJsonValue expected = payload.value(QStringLiteral("expected"));
        const QJsonValue next = payload.value(QStringLiteral("value"));
        if (!expected.isObject() || !next.isObject() ||
            detail::hasExternalPathString(expected) ||
            detail::hasExternalPathString(next) ||
            QJsonDocument(expected.toObject()).toJson(QJsonDocument::Compact).size() > 262144 ||
            QJsonDocument(next.toObject()).toJson(QJsonDocument::Compact).size() > 262144) {
            return QStringLiteral("layer.animationStack requires bounded path-free expected and next snapshots");
        }
        return {};
    }
    if (op.type == kOpLayerVisibility) {
        if (!payload.value(QStringLiteral("expected")).isBool() ||
            !payload.value(QStringLiteral("value")).isBool()) {
            return QStringLiteral("layer.visibility requires boolean expected and next values");
        }
        return {};
    }
    if (op.type == kOpLayerFlag) {
        const QString flag = payload.value(QStringLiteral("flag")).toString();
        if ((flag != QStringLiteral("solo") && flag != QStringLiteral("shy")) ||
            !payload.value(QStringLiteral("expected")).isBool() ||
            !payload.value(QStringLiteral("value")).isBool()) {
            return QStringLiteral("layer.flag requires a supported flag and boolean expected/next values");
        }
        return {};
    }
    if (op.type == kOpLayerEditLock) {
        if (!payload.value(QStringLiteral("expected")).isBool() ||
            !payload.value(QStringLiteral("value")).isBool()) {
            return QStringLiteral("layer.editLock requires boolean expected and next values");
        }
        return {};
    }
    if (op.type == kOpLayerTransform) {
        for (const auto key : {QStringLiteral("positionX"),
                               QStringLiteral("positionY"),
                               QStringLiteral("rotationDegrees"),
                               QStringLiteral("scaleX"),
                               QStringLiteral("scaleY")}) {
            if (!detail::hasNumber(payload, key)) {
                return QStringLiteral("layer.transform requires finite %1")
                    .arg(key);
            }
        }
        return {};
    }
    if (op.type == kOpLayerMoveAtFrame) {
        const QJsonValue frame = payload.value(QStringLiteral("frame"));
        const QJsonValue timeScale = payload.value(QStringLiteral("timeScale"));
        if (!frame.isDouble() || !std::isfinite(frame.toDouble()) ||
            std::floor(frame.toDouble()) != frame.toDouble() ||
            std::abs(frame.toDouble()) > 1000000000.0 ||
            !timeScale.isDouble() || std::floor(timeScale.toDouble()) != timeScale.toDouble() ||
            timeScale.toDouble() < 1.0 || timeScale.toDouble() > 10000.0) {
            return QStringLiteral("layer.moveAtFrame requires a bounded integer frame");
        }
        for (const auto key : {QStringLiteral("expectedX"),
                               QStringLiteral("expectedY"),
                               QStringLiteral("x"), QStringLiteral("y")}) {
            if (!detail::hasNumber(payload, key) ||
                std::abs(payload.value(key).toDouble()) > 1000000000.0) {
                return QStringLiteral("layer.moveAtFrame requires bounded finite %1")
                    .arg(key);
            }
        }
        return {};
    }
    if (op.type == kOpLayerAdd) {
        const QString layerType =
            payload.value(QStringLiteral("layerType")).toString().trimmed();
        const QJsonValue index = payload.value(QStringLiteral("index"));
        const QJsonObject layerJson = payload.value(QStringLiteral("layerJson")).toObject();
        if (layerType.isEmpty() ||
            payload.value(QStringLiteral("compositionId")).toString().trimmed().isEmpty() ||
            !index.isDouble() || index.toDouble() < 0 || index.toDouble() > 100000 ||
            std::floor(index.toDouble()) != index.toDouble() || layerJson.isEmpty() ||
            !payload.value(QStringLiteral("leftNeighborId")).isString() ||
            !payload.value(QStringLiteral("rightNeighborId")).isString() ||
            !payload.value(QStringLiteral("anchorLayerId")).isString() ||
            payload.value(QStringLiteral("anchorLayerId")).toString() !=
                (!payload.value(QStringLiteral("leftNeighborId")).toString().isEmpty()
                     ? payload.value(QStringLiteral("leftNeighborId")).toString()
                     : payload.value(QStringLiteral("rightNeighborId")).toString()) ||
            layerJson.value(QStringLiteral("id")).toString() != op.layerId ||
            layerJson.value(QStringLiteral("layerType")).toString() != layerType ||
            detail::hasExternalPathString(layerJson) ||
            QJsonDocument(payload).toJson(QJsonDocument::Compact).size() > 786432) {
            return QStringLiteral("layer.add requires a bounded path-free layer snapshot, matching identity, composition, and index");
        }
        return {};
    }
    if (op.type == kOpLayerRemove) {
        const QJsonValue index = payload.value(QStringLiteral("index"));
        const QJsonObject expected = payload.value(QStringLiteral("expectedLayerJson")).toObject();
        if (payload.value(QStringLiteral("compositionId")).toString().trimmed().isEmpty() ||
            !index.isDouble() || index.toDouble() < 0 || index.toDouble() > 100000 ||
            std::floor(index.toDouble()) != index.toDouble() || expected.isEmpty() ||
            expected.value(QStringLiteral("id")).toString() != op.layerId ||
            detail::hasExternalPathString(expected) ||
            QJsonDocument(payload).toJson(QJsonDocument::Compact).size() > 786432) {
            return QStringLiteral("layer.remove requires a bounded path-free expected snapshot, composition, and index");
        }
        return {};
    }
    if (op.type == kOpLayerReorder) {
        const QJsonValue expectedIndex = op.payload.value(QStringLiteral("expectedIndex"));
        const QJsonValue index = op.payload.value(QStringLiteral("index"));
        const QString compositionId =
            op.payload.value(QStringLiteral("compositionId")).toString();
        if (compositionId.trimmed().isEmpty() || compositionId.size() > 256 ||
            !expectedIndex.isDouble() || !index.isDouble() ||
            expectedIndex.toDouble() < 0 || expectedIndex.toDouble() > 100000 ||
            index.toDouble() < 0 || index.toDouble() > 100000 ||
            std::floor(expectedIndex.toDouble()) != expectedIndex.toDouble() ||
            std::floor(index.toDouble()) != index.toDouble()) {
            return QStringLiteral("layer.reorder requires a composition and bounded integer indices");
        }
    }
    if (op.type == kOpReviewCommentAdd) {
        const auto& payload = op.payload;
        if (payload.value(QStringLiteral("commentId")).toString().trimmed().isEmpty() ||
            payload.value(QStringLiteral("authorClientId")).toString().trimmed().isEmpty() ||
            payload.value(QStringLiteral("compositionId")).toString().trimmed().isEmpty() ||
            payload.value(QStringLiteral("text")).toString().trimmed().isEmpty() ||
            payload.value(QStringLiteral("text")).toString().size() > 4096) {
            return QStringLiteral("review.comment.add requires comment, author, composition, and text of at most 4096 characters");
        }
        if (payload.value(QStringLiteral("authorClientId")).toString() !=
            op.clientId) {
            return QStringLiteral("review.comment.add author must match the operation client");
        }
        if (payload.value(QStringLiteral("layerId")).toString() != op.layerId) {
            return QStringLiteral("review.comment.add layerId must match the operation layerId");
        }
        if (!detail::hasNumber(payload, QStringLiteral("createdAtMs"))) {
            return QStringLiteral("review.comment.add requires a finite createdAtMs");
        }
        if (payload.contains(QStringLiteral("frame")) &&
            !detail::hasNumber(payload, QStringLiteral("frame"))) {
            return QStringLiteral("review.comment.add frame must be finite");
        }
        return {};
    }
    if (op.type == kOpReviewCommentResolve) {
        const QString commentId =
            op.payload.value(QStringLiteral("commentId")).toString();
        const QString actorClientId =
            op.payload.value(QStringLiteral("actorClientId")).toString();
        if (commentId.trimmed().isEmpty() || actorClientId.isEmpty() ||
            actorClientId != op.clientId ||
            !op.payload.value(QStringLiteral("resolved")).isBool() ||
            !detail::hasNumber(op.payload, QStringLiteral("createdAtMs"))) {
            return QStringLiteral("review.comment.resolve has an invalid identity or decision");
        }
        return {};
    }
    if (op.type == kOpReviewCommentEdit) {
        const QString commentId =
            op.payload.value(QStringLiteral("commentId")).toString();
        const QString actorClientId =
            op.payload.value(QStringLiteral("actorClientId")).toString();
        const QString text = op.payload.value(QStringLiteral("text")).toString();
        if (commentId.trimmed().isEmpty() || actorClientId.isEmpty() ||
            actorClientId != op.clientId || text.trimmed().isEmpty() ||
            text.size() > 4096 ||
            (op.payload.contains(QStringLiteral("actorName")) &&
             !op.payload.value(QStringLiteral("actorName")).isString()) ||
            op.payload.value(QStringLiteral("actorName")).toString().size() > 128 ||
            !detail::hasNumber(op.payload, QStringLiteral("createdAtMs"))) {
            return QStringLiteral("review.comment.edit has an invalid identity or text");
        }
        return {};
    }
    if (op.type == kOpReviewCommentRemove) {
        const QString commentId =
            op.payload.value(QStringLiteral("commentId")).toString();
        const QString actorClientId =
            op.payload.value(QStringLiteral("actorClientId")).toString();
        if (commentId.trimmed().isEmpty() || actorClientId.isEmpty() ||
            actorClientId != op.clientId ||
            !detail::hasNumber(op.payload, QStringLiteral("createdAtMs"))) {
            return QStringLiteral("review.comment.remove has an invalid identity");
        }
        return {};
    }
    // Unknown types pass through unchanged: forward compatibility for newer
    // clients must not break older session models.
    return {};
}

[[nodiscard]] inline QString validateCollabReviewOperation(
    const CollaborationReview& review, const CollabOperationData& op) {
    const QString schemaError = validateCollabOperation(op);
    if (!schemaError.isEmpty()) return schemaError;

    const QJsonObject& payload = op.payload;
    if (op.type == kOpReviewCommentAdd) {
        const QString commentId =
            payload.value(QStringLiteral("commentId")).toString();
        if (!review.commentForId(commentId).commentId.isEmpty()) {
            return QStringLiteral("review.comment.add commentId already exists");
        }
        const QString parentId =
            payload.value(QStringLiteral("parentCommentId")).toString();
        if (!parentId.isEmpty()) {
            const CollabComment parent = review.commentForId(parentId);
            const qint64 frame = payload.contains(QStringLiteral("frame"))
                                     ? payload.value(QStringLiteral("frame"))
                                           .toVariant().toLongLong()
                                     : -1;
            if (parent.commentId.isEmpty() || parent.isReply() ||
                parent.deleted || parent.resolved ||
                parent.compositionId !=
                    payload.value(QStringLiteral("compositionId")).toString() ||
                parent.layerId != payload.value(QStringLiteral("layerId")).toString() ||
                parent.frame != frame) {
                return QStringLiteral("review.comment.add has an invalid thread anchor");
            }
        }
        return {};
    }
    if (op.type == kOpReviewCommentResolve) {
        const CollabComment comment = review.commentForId(
            payload.value(QStringLiteral("commentId")).toString());
        const bool resolved = payload.value(QStringLiteral("resolved")).toBool();
        if (comment.commentId.isEmpty() || comment.isReply() || comment.deleted ||
            comment.resolved == resolved) {
            return QStringLiteral("review.comment.resolve target is unavailable or unchanged");
        }
        return {};
    }
    if (op.type == kOpReviewCommentEdit) {
        const CollabComment comment = review.commentForId(
            payload.value(QStringLiteral("commentId")).toString());
        if (comment.commentId.isEmpty() || comment.deleted ||
            comment.authorClientId !=
                payload.value(QStringLiteral("actorClientId")).toString()) {
            return QStringLiteral("review.comment.edit is limited to the comment author");
        }
        return {};
    }
    if (op.type == kOpReviewCommentRemove) {
        const QString commentId =
            payload.value(QStringLiteral("commentId")).toString();
        const QString actorClientId =
            payload.value(QStringLiteral("actorClientId")).toString();
        if (!review.canDeleteComment(commentId, actorClientId)) {
            return QStringLiteral("review.comment.remove is limited to the comment author");
        }
        return {};
    }
    return {};
}

// Validates every operation bundled in a pending proposal. Accepting a
// proposal should be gated on this returning an empty string.
[[nodiscard]] inline QString validateCollabProposal(
    const CollaborationReview& review, const QString& proposalId) {
    const auto proposal = review.proposal(proposalId);
    if (proposal.proposalId.isEmpty()) {
        return QStringLiteral("proposal not found");
    }
    if (proposal.status != CollabProposalStatus::Pending) {
        return QStringLiteral("proposal is not pending");
    }
    for (int i = 0; i < proposal.operations.size(); ++i) {
        const QString error =
            validateCollabOperation(proposal.operations[i]);
        if (!error.isEmpty()) {
            return QStringLiteral("operation %1: %2").arg(i).arg(error);
        }
    }
    return {};
}

// Validates and accepts in one step. On any invalid bundled operation the
// proposal stays pending and the diagnostic is returned with an empty array.
[[nodiscard]] inline Pair<QString, Array<CollabOperationData>>
acceptValidatedProposal(CollaborationReview& review,
                        const QString& proposalId,
                        const QString& decidedByClientId, const qint64 atMs,
                        const QString& reason = {}) {
    const QString error = validateCollabProposal(review, proposalId);
    if (!error.isEmpty()) {
        return {error, {}};
    }
    auto operations = review.acceptProposal(proposalId, decidedByClientId,
                                            atMs, reason);
    if (operations.isEmpty()) {
        return {QStringLiteral("proposal could not be accepted"), {}};
    }
    return {{}, artifactMove(operations)};
}

} // namespace ArtifactCore
