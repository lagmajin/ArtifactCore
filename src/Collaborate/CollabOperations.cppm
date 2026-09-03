module;

#include <cmath>
#include <QString>
#include <QJsonObject>

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
inline constexpr const char* kOpLayerTransform = "layer.transform";
inline constexpr const char* kOpLayerAdd = "layer.add";
inline constexpr const char* kOpLayerRemove = "layer.remove";

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

// Validates `op.payload` against the schema implied by `op.type`.
// Returns an empty string when valid; otherwise a diagnostic.
[[nodiscard]] inline QString validateCollabOperation(
    const CollabOperationData& op) {
    const QJsonObject payload = op.payload;
    if (op.type == kOpPropertySet) {
        const QString path =
            payload.value(QStringLiteral("propertyPath")).toString();
        if (path.trimmed().isEmpty()) {
            return QStringLiteral("property.set requires a non-empty propertyPath");
        }
        if (!payload.contains(QStringLiteral("value"))) {
            return QStringLiteral("property.set requires a value");
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
    if (op.type == kOpLayerAdd) {
        const QString layerType =
            payload.value(QStringLiteral("layerType")).toString().trimmed();
        if (layerType.isEmpty()) {
            return QStringLiteral("layer.add requires a layerType");
        }
        if (!payload.value(QStringLiteral("layerJson")).isObject()) {
            return QStringLiteral("layer.add requires a layerJson object");
        }
        return {};
    }
    if (op.type == kOpLayerRemove) {
        return {};
    }
    // Unknown types pass through unchanged: forward compatibility for newer
    // clients must not break older session models.
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
