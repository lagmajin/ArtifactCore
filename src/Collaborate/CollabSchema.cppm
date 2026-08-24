module;

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointF>

export module Collaborate.Schema;

import Collaborate.Session;

export namespace ArtifactCore {

// ---- Operation type constants (wire format contract) ----
// These strings are the canonical operation identifiers used in
// CollabOperationData::type. Both the sender and receiver must agree on
// the payload schema for each type.

inline constexpr const char* kOpTransformMove = "transform.move";
inline constexpr const char* kOpTransformRotate = "transform.rotate";
inline constexpr const char* kOpTransformScale = "transform.scale";
inline constexpr const char* kOpPropertySet = "property.set";
inline constexpr const char* kOpLayerAdd = "layer.add";
inline constexpr const char* kOpLayerRemove = "layer.remove";
inline constexpr const char* kOpLayerReorder = "layer.reorder";
inline constexpr const char* kOpLayerRename = "layer.rename";
inline constexpr const char* kOpEffectAdd = "effect.add";
inline constexpr const char* kOpEffectRemove = "effect.remove";
inline constexpr const char* kOpEffectParam = "effect.param";
inline constexpr const char* kOpMaskVertex = "mask.vertex";
inline constexpr const char* kOpCommentAdd = "comment.add";

// ---- Payload builders (typed, validated at compile time by signature) ----

[[nodiscard]] inline QJsonObject payloadTransformMove(
    const double x, const double y, const double z = 0.0) {
    return QJsonObject{
        {QStringLiteral("x"), x},
        {QStringLiteral("y"), y},
        {QStringLiteral("z"), z}};
}

[[nodiscard]] inline QJsonObject payloadTransformRotate(
    const double degrees, const QString& axis = QStringLiteral("z")) {
    return QJsonObject{
        {QStringLiteral("degrees"), degrees},
        {QStringLiteral("axis"), axis}};
}

[[nodiscard]] inline QJsonObject payloadTransformScale(
    const double sx, const double sy, const double sz = 1.0) {
    return QJsonObject{
        {QStringLiteral("sx"), sx},
        {QStringLiteral("sy"), sy},
        {QStringLiteral("sz"), sz}};
}

[[nodiscard]] inline QJsonObject payloadPropertySet(
    const QString& propertyPath, const QJsonValue& value) {
    return QJsonObject{
        {QStringLiteral("propertyPath"), propertyPath},
        {QStringLiteral("value"), value}};
}

[[nodiscard]] inline QJsonObject payloadLayerAdd(
    const QString& layerType, const QString& layerName,
    const QJsonObject& layerJson) {
    return QJsonObject{
        {QStringLiteral("layerType"), layerType},
        {QStringLiteral("name"), layerName},
        {QStringLiteral("layerJson"), layerJson}};
}

[[nodiscard]] inline QJsonObject payloadLayerReorder(
    const int fromIndex, const int toIndex) {
    return QJsonObject{
        {QStringLiteral("from"), fromIndex},
        {QStringLiteral("to"), toIndex}};
}

[[nodiscard]] inline QJsonObject payloadEffectParam(
    const QString& effectId, const QString& paramName,
    const QJsonValue& value) {
    return QJsonObject{
        {QStringLiteral("effectId"), effectId},
        {QStringLiteral("paramName"), paramName},
        {QStringLiteral("value"), value}};
}

[[nodiscard]] inline QJsonObject payloadMaskVertex(
    const QString& maskId, const int pathIndex, const int vertexIndex,
    const double posX, const double posY,
    const double inTanX, const double inTanY,
    const double outTanX, const double outTanY) {
    return QJsonObject{
        {QStringLiteral("maskId"), maskId},
        {QStringLiteral("pathIndex"), pathIndex},
        {QStringLiteral("vertexIndex"), vertexIndex},
        {QStringLiteral("position"),
         QJsonObject{{QStringLiteral("x"), posX},
                     {QStringLiteral("y"), posY}}},
        {QStringLiteral("inTangent"),
         QJsonObject{{QStringLiteral("x"), inTanX},
                     {QStringLiteral("y"), inTanY}}},
        {QStringLiteral("outTangent"),
         QJsonObject{{QStringLiteral("x"), outTanX},
                     {QStringLiteral("y"), outTanY}}}};
}

} // namespace ArtifactCore
