module;
class tst_QList;
#include <utility>
#include <QString>
#include <QVector2D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QList>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QPointF>
#include <QtAlgorithms>
#include <QVariant>
#include <QtGlobal>
#include <cmath>
#include <algorithm>
#include <utility>

module ArtifactCore.Rig2D;

import Utils.Id;
import Time.Rational;

namespace ArtifactCore {

// ─────────────────────────────────────────────────────────
// RigEvaluationContext2D 実装
// ─────────────────────────────────────────────────────────

RigEvaluationContext2D::RigEvaluationContext2D()
{
}

void RigEvaluationContext2D::setRig(Rig2D* rig)
{
    rig_ = rig;
}

void RigEvaluationContext2D::setTime(const RationalTime& time)
{
    time_ = time;
}

void RigEvaluationContext2D::indexBones(const QList<Bone2D*>& bones)
{
    bonesById_.clear();
    for (Bone2D* bone : bones) {
        if (bone) {
            bonesById_.insert(bone->id(), bone);
        }
    }
}

void RigEvaluationContext2D::indexControls(const QList<RigControl2D*>& controls)
{
    controlsById_.clear();
    for (RigControl2D* control : controls) {
        if (control) {
            controlsById_.insert(control->id(), control);
        }
    }
}

Bone2D* RigEvaluationContext2D::findBone(const Id& id) const
{
    const auto it = bonesById_.constFind(id);
    return it == bonesById_.constEnd() ? nullptr : it.value();
}

RigControl2D* RigEvaluationContext2D::findControl(const Id& id) const
{
    const auto it = controlsById_.constFind(id);
    return it == controlsById_.constEnd() ? nullptr : it.value();
}

namespace {

QJsonObject vector2DToJson(const QVector2D& value) {
    QJsonObject object;
    object["x"] = static_cast<double>(value.x());
    object["y"] = static_cast<double>(value.y());
    return object;
}

QVector2D vector2DFromJson(const QJsonValue& value, const QVector2D& fallback) {
    if (!value.isObject()) {
        return fallback;
    }
    const QJsonObject object = value.toObject();
    return QVector2D(static_cast<float>(object.value("x").toDouble(fallback.x())),
                     static_cast<float>(object.value("y").toDouble(fallback.y())));
}

QJsonObject transformToJson(const BoneTransform& transform) {
    QJsonObject object;
    object["position"] = vector2DToJson(transform.position);
    object["rotation"] = static_cast<double>(transform.rotation);
    object["scale"] = vector2DToJson(transform.scale);
    return object;
}

BoneTransform transformFromJson(const QJsonValue& value, const BoneTransform& fallback) {
    if (!value.isObject()) {
        return fallback;
    }
    const QJsonObject object = value.toObject();
    BoneTransform transform = fallback;
    transform.position = vector2DFromJson(object.value("position"), fallback.position);
    transform.rotation = static_cast<float>(object.value("rotation").toDouble(fallback.rotation));
    transform.scale = vector2DFromJson(object.value("scale"), fallback.scale);
    return transform;
}

QVector2D vector2DFromVariant(const QVariant& value, const QVector2D& fallback = QVector2D()) {
    if (value.canConvert<QVector2D>()) {
        return value.value<QVector2D>();
    }
    if (value.canConvert<QPointF>()) {
        const QPointF point = value.toPointF();
        return QVector2D(static_cast<float>(point.x()), static_cast<float>(point.y()));
    }
    if (value.isValid() && value.canConvert<double>()) {
        const double v = value.toDouble();
        return QVector2D(static_cast<float>(v), static_cast<float>(v));
    }
    return fallback;
}

double doubleFromVariant(const QVariant& value, double fallback = 0.0) {
    if (!value.isValid()) {
        return fallback;
    }
    bool ok = false;
    const double asDouble = value.toDouble(&ok);
    return ok ? asDouble : fallback;
}

double clampDouble(double value, double minValue, double maxValue) {
    if (minValue > maxValue) {
        std::swap(minValue, maxValue);
    }
    return std::clamp(value, minValue, maxValue);
}

double remapRange(double value, double inMin, double inMax, double outMin, double outMax) {
    const double span = inMax - inMin;
    if (std::abs(span) < 1e-8) {
        return outMin;
    }
    const double t = clampDouble((value - inMin) / span, 0.0, 1.0);
    return outMin + (outMax - outMin) * t;
}

float matrixRotationDegrees(const QMatrix4x4& matrix) {
    const QVector4D axisX = matrix.column(0);
    return static_cast<float>(std::atan2(axisX.y(), axisX.x()) * (180.0 / 3.14159265358979323846));
}

QJsonObject controlValueToJson(const QVariant& value) {
    QJsonObject object;
    if (value.canConvert<QVector2D>()) {
        const QVector2D vec = value.value<QVector2D>();
        object["x"] = static_cast<double>(vec.x());
        object["y"] = static_cast<double>(vec.y());
    } else {
        object["value"] = value.toDouble();
    }
    return object;
}

QVariant controlValueFromJson(const QJsonObject& object, RigControlKind kind, const QVariant& fallback) {
    switch (kind) {
    case RigControlKind::Point:
        return QVariant::fromValue(QVector2D(static_cast<float>(object.value("x").toDouble(0.0)),
                                             static_cast<float>(object.value("y").toDouble(0.0))));
    case RigControlKind::Angle:
    case RigControlKind::Slider:
    default:
        return QVariant(object.value("value").toDouble(fallback.toDouble()));
    }
}

QString channelNameForMap(const QString& channel) {
    const QString trimmed = channel.trimmed().toLower();
    return trimmed.isEmpty() ? QStringLiteral("rotation") : trimmed;
}

} // namespace

// ─────────────────────────────────────────────────────────
// Bone2D 実装
// ─────────────────────────────────────────────────────────

Bone2D::Bone2D() : id_(), name_("Bone") {
    globalMatrix_.setToIdentity();
    resolvedTransform_ = localTransform_;
}

Bone2D::Bone2D(const QString& name) : id_(), name_(name) {
    globalMatrix_.setToIdentity();
    resolvedTransform_ = localTransform_;
}

void Bone2D::addChild(Bone2D* child) {
    if (child && !children_.contains(child)) {
        children_.append(child);
        child->setParent(this);
    }
}

void Bone2D::removeChild(Bone2D* child) {
    if (child) {
        children_.removeOne(child);
        if (child->parent_ == this) {
            child->parent_ = nullptr;
        }
    }
}

BoneTransform Bone2D::evaluate(const RationalTime& time) const {
    Q_UNUSED(time);
    return localTransform_;
}

QJsonObject Bone2D::toJson() const {
    QJsonObject object;
    object["id"] = id_.toString();
    object["name"] = name_;
    object["length"] = static_cast<double>(length_);
    object["localTransform"] = transformToJson(localTransform_);
    if (parent_) {
        object["parentId"] = parent_->id().toString();
    }
    return object;
}

void Bone2D::fromJson(const QJsonObject& object) {
    const QString idString = object.value("id").toString();
    if (!idString.isEmpty()) {
        id_ = Id(idString);
    }
    name_ = object.value("name").toString(name_);
    length_ = static_cast<float>(object.value("length").toDouble(length_));
    localTransform_ = transformFromJson(object.value("localTransform"), localTransform_);
    resolvedTransform_ = localTransform_;
}

void Bone2D::updateHierarchy() {
    // ローカル変換からローカル行列を構築
    QMatrix4x4 localMatrix;
    localMatrix.setToIdentity();
    localMatrix.translate(resolvedTransform_.position.x(), resolvedTransform_.position.y(), 0.0f);
    localMatrix.rotate(resolvedTransform_.rotation, 0.0f, 0.0f, 1.0f);
    localMatrix.scale(resolvedTransform_.scale.x(), resolvedTransform_.scale.y(), 1.0f);

    // グローバル行列の計算
    if (parent_) {
        globalMatrix_ = parent_->globalMatrix_ * localMatrix;
    } else {
        globalMatrix_ = localMatrix;
    }

    // 子ボーンを再帰的に更新
    for (Bone2D* child : children_) {
        if (child) {
            child->updateHierarchy();
        }
    }
}

// ─────────────────────────────────────────────────────────
// RigControl2D 実装
// ─────────────────────────────────────────────────────────

RigControl2D::RigControl2D()
{
}

RigControl2D::RigControl2D(const QString& name, RigControlKind kind)
    : name_(name), kind_(kind)
{
}

void RigControl2D::setRange(const QVariant& minValue, const QVariant& maxValue)
{
    minValue_ = minValue;
    maxValue_ = maxValue;
}

QJsonObject RigControl2D::toJson() const
{
    QJsonObject object;
    object["id"] = id_.toString();
    object["name"] = name_;
    object["kind"] = static_cast<int>(kind_);
    object["enabled"] = enabled_;
    object["value"] = controlValueToJson(value_);
    object["minValue"] = controlValueToJson(minValue_);
    object["maxValue"] = controlValueToJson(maxValue_);
    return object;
}

RigControl2D RigControl2D::fromJson(const QJsonObject& object)
{
    RigControl2D control;
    const QString idString = object.value("id").toString();
    if (!idString.isEmpty()) {
        control.id_ = Id(idString);
    }
    control.name_ = object.value("name").toString(control.name_);
    control.kind_ = static_cast<RigControlKind>(object.value("kind").toInt(static_cast<int>(RigControlKind::Slider)));
    control.enabled_ = object.value("enabled").toBool(true);
    control.value_ = controlValueFromJson(object.value("value").toObject(), control.kind_, QVariant());
    control.minValue_ = controlValueFromJson(object.value("minValue").toObject(), control.kind_, QVariant());
    control.maxValue_ = controlValueFromJson(object.value("maxValue").toObject(), control.kind_, QVariant());
    return control;
}

// ─────────────────────────────────────────────────────────
// RigControlSet2D 実装
// ─────────────────────────────────────────────────────────

RigControlSet2D::RigControlSet2D()
{
}

RigControlSet2D::RigControlSet2D(RigControlSet2D&& other) noexcept
    : controls_(std::move(other.controls_))
{
    other.controls_.clear();
}

RigControlSet2D& RigControlSet2D::operator=(RigControlSet2D&& other) noexcept
{
    if (this != &other) {
        clear();
        controls_ = std::move(other.controls_);
        other.controls_.clear();
    }
    return *this;
}

RigControlSet2D::~RigControlSet2D()
{
    clear();
}

RigControl2D* RigControlSet2D::addControl(const QString& name, RigControlKind kind, const QVariant& defaultValue)
{
    auto* control = new RigControl2D(name, kind);
    control->setValue(defaultValue);
    switch (kind) {
    case RigControlKind::Point:
        control->setRange(QVector2D(-1.0f, -1.0f), QVector2D(1.0f, 1.0f));
        break;
    case RigControlKind::Angle:
        control->setRange(-180.0, 180.0);
        break;
    case RigControlKind::Slider:
    default:
        control->setRange(0.0, 1.0);
        break;
    }
    controls_.append(control);
    return control;
}

RigControl2D* RigControlSet2D::addSlider(const QString& name, double defaultValue, double minValue, double maxValue)
{
    auto* control = addControl(name, RigControlKind::Slider, QVariant(defaultValue));
    if (control) {
        control->setRange(minValue, maxValue);
    }
    return control;
}

RigControl2D* RigControlSet2D::addPoint(const QString& name, const QVector2D& defaultValue)
{
    return addControl(name, RigControlKind::Point, QVariant::fromValue(defaultValue));
}

RigControl2D* RigControlSet2D::addAngle(const QString& name, double defaultValue, double minValue, double maxValue)
{
    auto* control = addControl(name, RigControlKind::Angle, QVariant(defaultValue));
    if (control) {
        control->setRange(minValue, maxValue);
    }
    return control;
}

bool RigControlSet2D::removeControl(const Id& id)
{
    for (int i = 0; i < controls_.size(); ++i) {
        RigControl2D* control = controls_.at(i);
        if (control && control->id() == id) {
            delete control;
            controls_.removeAt(i);
            return true;
        }
    }
    return false;
}

RigControl2D* RigControlSet2D::findControl(const Id& id) const
{
    for (RigControl2D* control : controls_) {
        if (control && control->id() == id) {
            return control;
        }
    }
    return nullptr;
}

RigControl2D* RigControlSet2D::findControl(const QString& name) const
{
    for (RigControl2D* control : controls_) {
        if (control && control->name() == name) {
            return control;
        }
    }
    return nullptr;
}

int RigControlSet2D::controlCount() const
{
    return controls_.size();
}

bool RigControlSet2D::setControlValue(const Id& id, const QVariant& value)
{
    RigControl2D* control = findControl(id);
    if (!control) {
        return false;
    }
    control->setValue(value);
    return true;
}

QVariant RigControlSet2D::controlValue(const Id& id) const
{
    const RigControl2D* control = findControl(id);
    return control ? control->value() : QVariant();
}

void RigControlSet2D::clear()
{
    qDeleteAll(controls_);
    controls_.clear();
}

QJsonArray RigControlSet2D::toJson() const
{
    QJsonArray array;
    for (const RigControl2D* control : controls_) {
        if (control) {
            array.append(control->toJson());
        }
    }
    return array;
}

RigControlSet2D RigControlSet2D::fromJson(const QJsonArray& array)
{
    RigControlSet2D set;
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }
        auto* control = new RigControl2D(RigControl2D::fromJson(value.toObject()));
        set.controls_.append(control);
    }
    return set;
}

// ─────────────────────────────────────────────────────────
// RigPropertyBinding2D 実装
// ─────────────────────────────────────────────────────────

RigPropertyBinding2D::RigPropertyBinding2D()
{
}

RigPropertyBinding2D::RigPropertyBinding2D(const QString& name,
                                           const Id& controlId,
                                           const LayerID& targetLayerId,
                                           const QString& targetPropertyPath)
    : name_(name),
      controlId_(controlId),
      targetLayerId_(targetLayerId),
      targetPropertyPath_(targetPropertyPath)
{
}

QJsonObject RigPropertyBinding2D::toJson() const
{
    QJsonObject object;
    object["id"] = id_.toString();
    object["name"] = name_;
    object["controlId"] = controlId_.toString();
    object["targetLayerId"] = targetLayerId_.toString();
    object["targetPropertyPath"] = targetPropertyPath_;
    object["enabled"] = enabled_;
    return object;
}

std::shared_ptr<RigPropertyBinding2D> RigPropertyBinding2D::fromJson(const QJsonObject& object)
{
    auto binding = std::make_shared<RigPropertyBinding2D>();
    const QString idString = object.value("id").toString();
    if (!idString.isEmpty()) {
        binding->id_ = Id(idString);
    }
    binding->name_ = object.value("name").toString(binding->name_);
    binding->controlId_ = Id(object.value("controlId").toString());
    binding->targetLayerId_ = LayerID(object.value("targetLayerId").toString());
    binding->targetPropertyPath_ = object.value("targetPropertyPath").toString(binding->targetPropertyPath_);
    binding->enabled_ = object.value("enabled").toBool(true);
    return binding;
}

// ─────────────────────────────────────────────────────────
// RigConstraint2D / derived 実装
// ─────────────────────────────────────────────────────────

RigConstraint2D::RigConstraint2D()
{
}

RigConstraint2D::RigConstraint2D(const QString& name)
    : name_(name)
{
}

ParentConstraint2D::ParentConstraint2D()
    : RigConstraint2D(QStringLiteral("Parent Constraint"))
{
}

ParentConstraint2D::ParentConstraint2D(const QString& name, const Id& targetBoneId, const Id& parentBoneId)
    : RigConstraint2D(name), targetBoneId_(targetBoneId), parentBoneId_(parentBoneId)
{
}

void ParentConstraint2D::evaluate(RigEvaluationContext2D& context)
{
    if (!enabled_) {
        return;
    }

    Bone2D* target = context.findBone(targetBoneId_);
    Bone2D* parent = context.findBone(parentBoneId_);
    if (!target || !parent) {
        return;
    }

    BoneTransform resolved = target->resolvedTransform();
    resolved.position = offset_.position;
    resolved.rotation = offset_.rotation;
    resolved.scale = offset_.scale;
    target->setResolvedTransform(resolved);
}

QJsonObject ParentConstraint2D::toJson() const
{
    QJsonObject object;
    object["kind"] = QStringLiteral("Parent");
    object["id"] = id_.toString();
    object["name"] = name_;
    object["enabled"] = enabled_;
    object["targetBoneId"] = targetBoneId_.toString();
    object["parentBoneId"] = parentBoneId_.toString();
    object["offset"] = transformToJson(offset_);
    return object;
}

std::shared_ptr<ParentConstraint2D> ParentConstraint2D::fromJson(const QJsonObject& object)
{
    auto constraint = std::make_shared<ParentConstraint2D>();
    const QString idString = object.value("id").toString();
    if (!idString.isEmpty()) {
        constraint->id_ = Id(idString);
    }
    constraint->name_ = object.value("name").toString(constraint->name_);
    constraint->enabled_ = object.value("enabled").toBool(true);
    constraint->targetBoneId_ = Id(object.value("targetBoneId").toString());
    constraint->parentBoneId_ = Id(object.value("parentBoneId").toString());
    constraint->offset_ = transformFromJson(object.value("offset"), constraint->offset_);
    return constraint;
}

MapRangeConstraint2D::MapRangeConstraint2D()
    : RigConstraint2D(QStringLiteral("Map Range"))
{
}

MapRangeConstraint2D::MapRangeConstraint2D(const QString& name, const Id& controlId, const Id& targetBoneId)
    : RigConstraint2D(name), controlId_(controlId), targetBoneId_(targetBoneId)
{
}

void MapRangeConstraint2D::setMapping(double inputMin, double inputMax, double outputMin, double outputMax)
{
    inputMin_ = inputMin;
    inputMax_ = inputMax;
    outputMin_ = outputMin;
    outputMax_ = outputMax;
}

void MapRangeConstraint2D::evaluate(RigEvaluationContext2D& context)
{
    if (!enabled_) {
        return;
    }

    Bone2D* target = context.findBone(targetBoneId_);
    if (!target) {
        return;
    }

    RigControl2D* control = context.findControl(controlId_);
    const QVariant rawControl = control ? control->value() : QVariant();
    const double mapped = remapRange(doubleFromVariant(rawControl, inputMin_), inputMin_, inputMax_, outputMin_, outputMax_);
    BoneTransform resolved = target->resolvedTransform();
    const QString channel = channelNameForMap(targetChannel_);
    if (channel == QStringLiteral("position.x")) {
        resolved.position.setX(static_cast<float>(mapped));
    } else if (channel == QStringLiteral("position.y")) {
        resolved.position.setY(static_cast<float>(mapped));
    } else if (channel == QStringLiteral("scale.x")) {
        resolved.scale.setX(static_cast<float>(mapped));
    } else if (channel == QStringLiteral("scale.y")) {
        resolved.scale.setY(static_cast<float>(mapped));
    } else {
        resolved.rotation = static_cast<float>(mapped);
    }
    target->setResolvedTransform(resolved);
}

QJsonObject MapRangeConstraint2D::toJson() const
{
    QJsonObject object;
    object["kind"] = QStringLiteral("MapRange");
    object["id"] = id_.toString();
    object["name"] = name_;
    object["enabled"] = enabled_;
    object["controlId"] = controlId_.toString();
    object["targetBoneId"] = targetBoneId_.toString();
    object["targetChannel"] = targetChannel_;
    object["inputMin"] = inputMin_;
    object["inputMax"] = inputMax_;
    object["outputMin"] = outputMin_;
    object["outputMax"] = outputMax_;
    return object;
}

std::shared_ptr<MapRangeConstraint2D> MapRangeConstraint2D::fromJson(const QJsonObject& object)
{
    auto constraint = std::make_shared<MapRangeConstraint2D>();
    const QString idString = object.value("id").toString();
    if (!idString.isEmpty()) {
        constraint->id_ = Id(idString);
    }
    constraint->name_ = object.value("name").toString(constraint->name_);
    constraint->enabled_ = object.value("enabled").toBool(true);
    constraint->controlId_ = Id(object.value("controlId").toString());
    constraint->targetBoneId_ = Id(object.value("targetBoneId").toString());
    constraint->targetChannel_ = channelNameForMap(object.value("targetChannel").toString(constraint->targetChannel_));
    constraint->inputMin_ = object.value("inputMin").toDouble(constraint->inputMin_);
    constraint->inputMax_ = object.value("inputMax").toDouble(constraint->inputMax_);
    constraint->outputMin_ = object.value("outputMin").toDouble(constraint->outputMin_);
    constraint->outputMax_ = object.value("outputMax").toDouble(constraint->outputMax_);
    return constraint;
}

AimConstraint2D::AimConstraint2D()
    : RigConstraint2D(QStringLiteral("Aim Constraint"))
{
}

AimConstraint2D::AimConstraint2D(const QString& name, const Id& sourceBoneId, const Id& targetBoneId)
    : RigConstraint2D(name), sourceBoneId_(sourceBoneId), targetBoneId_(targetBoneId)
{
}

void AimConstraint2D::evaluate(RigEvaluationContext2D& context)
{
    if (!enabled_) {
        return;
    }

    Bone2D* source = context.findBone(sourceBoneId_);
    Bone2D* target = context.findBone(targetBoneId_);
    if (!source || !target) {
        return;
    }

    const QVector2D sourcePos = source->resolvedTransform().position;
    const QVector2D targetPos = target->resolvedTransform().position;
    const QVector2D delta = targetPos - sourcePos;
    if (delta.lengthSquared() < 1e-8f) {
        return;
    }

    const float worldAngle = static_cast<float>(std::atan2(delta.y(), delta.x()) * (180.0 / 3.14159265358979323846) + angleOffset_);
    float parentWorldAngle = 0.0f;
    if (source->parent()) {
        parentWorldAngle = matrixRotationDegrees(source->parent()->globalMatrix());
    }

    BoneTransform resolved = source->resolvedTransform();
    resolved.rotation = worldAngle - parentWorldAngle;
    source->setResolvedTransform(resolved);
}

QJsonObject AimConstraint2D::toJson() const
{
    QJsonObject object;
    object["kind"] = QStringLiteral("Aim");
    object["id"] = id_.toString();
    object["name"] = name_;
    object["enabled"] = enabled_;
    object["sourceBoneId"] = sourceBoneId_.toString();
    object["targetBoneId"] = targetBoneId_.toString();
    object["angleOffset"] = angleOffset_;
    return object;
}

std::shared_ptr<AimConstraint2D> AimConstraint2D::fromJson(const QJsonObject& object)
{
    auto constraint = std::make_shared<AimConstraint2D>();
    const QString idString = object.value("id").toString();
    if (!idString.isEmpty()) {
        constraint->id_ = Id(idString);
    }
    constraint->name_ = object.value("name").toString(constraint->name_);
    constraint->enabled_ = object.value("enabled").toBool(true);
    constraint->sourceBoneId_ = Id(object.value("sourceBoneId").toString());
    constraint->targetBoneId_ = Id(object.value("targetBoneId").toString());
    constraint->angleOffset_ = static_cast<float>(object.value("angleOffset").toDouble(constraint->angleOffset_));
    return constraint;
}

TwoBoneIKConstraint2D::TwoBoneIKConstraint2D()
    : RigConstraint2D(QStringLiteral("Two Bone IK"))
{
}

TwoBoneIKConstraint2D::TwoBoneIKConstraint2D(const QString& name,
                                             const Id& upperBoneId,
                                             const Id& lowerBoneId,
                                             const Id& effectorBoneId,
                                             const Id& targetBoneId)
    : RigConstraint2D(name),
      upperBoneId_(upperBoneId),
      lowerBoneId_(lowerBoneId),
      effectorBoneId_(effectorBoneId),
      targetBoneId_(targetBoneId)
{
}

void TwoBoneIKConstraint2D::evaluate(RigEvaluationContext2D& context)
{
    if (!enabled_) {
        return;
    }

    Bone2D* upper = context.findBone(upperBoneId_);
    Bone2D* lower = context.findBone(lowerBoneId_);
    Bone2D* effector = context.findBone(effectorBoneId_);
    Bone2D* target = context.findBone(targetBoneId_);
    if (!upper || !lower || !effector || !target) {
        return;
    }

    const QVector2D p1 = upper->resolvedTransform().position;
    const QVector2D p2 = lower->resolvedTransform().position;
    const QVector2D p3 = effector->resolvedTransform().position;
    const QVector2D targetPos = target->resolvedTransform().position;

    const float len1 = std::max(0.001f, (p2 - p1).length());
    const float len2 = std::max(0.001f, (p3 - p2).length());
    const float dist = (targetPos - p1).length();
    const float maxReach = len1 + len2;

    QVector2D jointPos = p2;
    QVector2D effectorPos = p3;

    if (dist > maxReach) {
        const QVector2D dir = (targetPos - p1).normalized();
        jointPos = p1 + dir * len1;
        effectorPos = p1 + dir * maxReach;
    } else {
        const float cosAngle1 = clampDouble((len1 * len1 + dist * dist - len2 * len2) / (2.0f * len1 * std::max(dist, 0.001f)), -1.0, 1.0);
        const float angle1 = std::acos(cosAngle1);
        const float baseAngle = std::atan2(targetPos.y() - p1.y(), targetPos.x() - p1.x());
        jointPos = p1 + QVector2D(std::cos(baseAngle + angle1), std::sin(baseAngle + angle1)) * len1;
        effectorPos = targetPos;
    }

    BoneTransform upperResolved = upper->resolvedTransform();
    BoneTransform lowerResolved = lower->resolvedTransform();
    const float upperWorldAngle = static_cast<float>(std::atan2(jointPos.y() - p1.y(), jointPos.x() - p1.x()) * (180.0 / 3.14159265358979323846));
    const float lowerWorldAngle = static_cast<float>(std::atan2(effectorPos.y() - jointPos.y(), effectorPos.x() - jointPos.x()) * (180.0 / 3.14159265358979323846));
    float upperParentWorldAngle = 0.0f;
    if (upper->parent()) {
        upperParentWorldAngle = matrixRotationDegrees(upper->parent()->globalMatrix());
    }
    upperResolved.rotation = upperWorldAngle - upperParentWorldAngle;
    lowerResolved.rotation = lowerWorldAngle - upperWorldAngle;
    upper->setResolvedTransform(upperResolved);
    lower->setResolvedTransform(lowerResolved);
}

QJsonObject TwoBoneIKConstraint2D::toJson() const
{
    QJsonObject object;
    object["kind"] = QStringLiteral("TwoBoneIK");
    object["id"] = id_.toString();
    object["name"] = name_;
    object["enabled"] = enabled_;
    object["upperBoneId"] = upperBoneId_.toString();
    object["lowerBoneId"] = lowerBoneId_.toString();
    object["effectorBoneId"] = effectorBoneId_.toString();
    object["targetBoneId"] = targetBoneId_.toString();
    object["poleAngle"] = poleAngle_;
    return object;
}

std::shared_ptr<TwoBoneIKConstraint2D> TwoBoneIKConstraint2D::fromJson(const QJsonObject& object)
{
    auto constraint = std::make_shared<TwoBoneIKConstraint2D>();
    const QString idString = object.value("id").toString();
    if (!idString.isEmpty()) {
        constraint->id_ = Id(idString);
    }
    constraint->name_ = object.value("name").toString(constraint->name_);
    constraint->enabled_ = object.value("enabled").toBool(true);
    constraint->upperBoneId_ = Id(object.value("upperBoneId").toString());
    constraint->lowerBoneId_ = Id(object.value("lowerBoneId").toString());
    constraint->effectorBoneId_ = Id(object.value("effectorBoneId").toString());
    constraint->targetBoneId_ = Id(object.value("targetBoneId").toString());
    constraint->poleAngle_ = static_cast<float>(object.value("poleAngle").toDouble(constraint->poleAngle_));
    return constraint;
}

// ─────────────────────────────────────────────────────────
// Rig2D 実装
// ─────────────────────────────────────────────────────────

Rig2D::Rig2D() {
}

Rig2D::Rig2D(Rig2D&& other) noexcept
    : bones_(std::move(other.bones_)),
      controlSet_(std::move(other.controlSet_)),
      constraints_(std::move(other.constraints_)),
      propertyBindings_(std::move(other.propertyBindings_)),
      rootBone_(other.rootBone_) {
    other.rootBone_ = nullptr;
    other.bones_.clear();
    other.constraints_.clear();
    other.propertyBindings_.clear();
}

Rig2D& Rig2D::operator=(Rig2D&& other) noexcept {
    if (this != &other) {
        clearBones();
        bones_ = std::move(other.bones_);
        controlSet_ = std::move(other.controlSet_);
        constraints_ = std::move(other.constraints_);
        propertyBindings_ = std::move(other.propertyBindings_);
        rootBone_ = other.rootBone_;
        other.rootBone_ = nullptr;
        other.bones_.clear();
        other.constraints_.clear();
        other.propertyBindings_.clear();
    }
    return *this;
}

Rig2D::~Rig2D() {
    clearBones();
}

Bone2D* Rig2D::addBone(const QString& name, Bone2D* parent) {
    auto* bone = new Bone2D(name);
    bones_.append(bone);
    if (parent) {
        parent->addChild(bone);
    }
    if (!rootBone_) {
        rootBone_ = bone;
    }
    return bone;
}

Bone2D* Rig2D::addBone(const QString& name, const Id& parentId) {
    return addBone(name, findBone(parentId));
}

void Rig2D::removeBone(Bone2D* bone) {
    if (!bone) return;

    // 親から切り離し
    if (bone->parent()) {
        bone->parent()->removeChild(bone);
    }

    // 子を切り離し（削除しない）
    for (Bone2D* child : bone->children()) {
        if (child) {
            child->setParent(nullptr);
        }
    }

    bones_.removeOne(bone);
    if (rootBone_ == bone) {
        rootBone_ = bones_.isEmpty() ? nullptr : bones_.first();
    }
    delete bone;
}

bool Rig2D::removeBone(const Id& id) {
    Bone2D* bone = findBone(id);
    if (!bone) {
        return false;
    }
    removeBone(bone);
    return true;
}

void Rig2D::clearBones() {
    qDeleteAll(bones_);
    bones_.clear();
    controlSet_.clear();
    rootBone_ = nullptr;
    constraints_.clear();
    propertyBindings_.clear();
}

Bone2D* Rig2D::findBone(const Id& id) const {
    for (Bone2D* bone : bones_) {
        if (bone && bone->id() == id) {
            return bone;
        }
    }
    return nullptr;
}

Bone2D* Rig2D::findBone(const QString& name) const {
    for (Bone2D* bone : bones_) {
        if (bone && bone->name() == name) {
            return bone;
        }
    }
    return nullptr;
}

void Rig2D::update() {
    if (rootBone_) {
        rootBone_->updateHierarchy();
    }
}

void Rig2D::evaluate(const RationalTime& time) {
    for (Bone2D* bone : bones_) {
        if (bone) {
            bone->setResolvedTransform(bone->evaluate(time));
        }
    }
    update();
    RigEvaluationContext2D context;
    context.setRig(this);
    context.setTime(time);
    context.indexBones(bones_);
    context.indexControls(controlSet_.controls());
    for (RigControl2D* control : controlSet_.controls()) {
        if (!control) {
            continue;
        }
        if (control->kind() == RigControlKind::Slider || control->kind() == RigControlKind::Angle) {
            const double minValue = doubleFromVariant(control->minValue(), 0.0);
            const double maxValue = doubleFromVariant(control->maxValue(), 1.0);
            control->setValue(clampDouble(doubleFromVariant(control->value(), minValue), minValue, maxValue));
        }
    }
    for (const auto& constraint : constraints_) {
        if (constraint && constraint->enabled()) {
            constraint->evaluate(context);
        }
    }
    update();
}

bool Rig2D::setBoneLocalTransform(const Id& id, const BoneTransform& transform) {
    Bone2D* bone = findBone(id);
    if (!bone) {
        return false;
    }
    bone->setLocalTransform(transform);
    return true;
}

bool Rig2D::boneLocalTransform(const Id& id, BoneTransform* outTransform) const {
    if (!outTransform) {
        return false;
    }
    const Bone2D* bone = findBone(id);
    if (!bone) {
        return false;
    }
    *outTransform = bone->localTransform();
    return true;
}

RigControl2D* Rig2D::addControl(const QString& name, RigControlKind kind, const QVariant& defaultValue)
{
    return controlSet_.addControl(name, kind, defaultValue);
}

RigControl2D* Rig2D::addSlider(const QString& name, double defaultValue, double minValue, double maxValue)
{
    auto* control = addControl(name, RigControlKind::Slider, QVariant(defaultValue));
    if (control) {
        control->setRange(minValue, maxValue);
    }
    return control;
}

RigControl2D* Rig2D::addPoint(const QString& name, const QVector2D& defaultValue)
{
    return addControl(name, RigControlKind::Point, QVariant::fromValue(defaultValue));
}

RigControl2D* Rig2D::addAngle(const QString& name, double defaultValue, double minValue, double maxValue)
{
    auto* control = addControl(name, RigControlKind::Angle, QVariant(defaultValue));
    if (control) {
        control->setRange(minValue, maxValue);
    }
    return control;
}

bool Rig2D::removeControl(const Id& id)
{
    return controlSet_.removeControl(id);
}

RigControl2D* Rig2D::findControl(const Id& id) const
{
    return controlSet_.findControl(id);
}

RigControl2D* Rig2D::findControl(const QString& name) const
{
    return controlSet_.findControl(name);
}

int Rig2D::controlCount() const
{
    return controlSet_.controlCount();
}

bool Rig2D::setControlValue(const Id& id, const QVariant& value)
{
    return controlSet_.setControlValue(id, value);
}

QVariant Rig2D::controlValue(const Id& id) const
{
    return controlSet_.controlValue(id);
}

std::shared_ptr<RigConstraint2D> Rig2D::addConstraint(std::shared_ptr<RigConstraint2D> constraint)
{
    if (!constraint) {
        return {};
    }
    constraints_.append(constraint);
    return constraint;
}

bool Rig2D::removeConstraint(const Id& id)
{
    for (int i = 0; i < constraints_.size(); ++i) {
        const auto& constraint = constraints_.at(i);
        if (constraint && constraint->id() == id) {
            constraints_.removeAt(i);
            return true;
        }
    }
    return false;
}

std::shared_ptr<RigConstraint2D> Rig2D::findConstraint(const Id& id) const
{
    for (const auto& constraint : constraints_) {
        if (constraint && constraint->id() == id) {
            return constraint;
        }
    }
    return {};
}

std::shared_ptr<RigConstraint2D> Rig2D::findConstraint(const QString& name) const
{
    for (const auto& constraint : constraints_) {
        if (constraint && constraint->name() == name) {
            return constraint;
        }
    }
    return {};
}

int Rig2D::constraintCount() const
{
    return constraints_.size();
}

std::shared_ptr<RigPropertyBinding2D> Rig2D::addPropertyBinding(std::shared_ptr<RigPropertyBinding2D> binding)
{
    if (!binding) {
        return {};
    }
    propertyBindings_.append(binding);
    return binding;
}

bool Rig2D::removePropertyBinding(const Id& id)
{
    for (int i = 0; i < propertyBindings_.size(); ++i) {
        const auto& binding = propertyBindings_.at(i);
        if (binding && binding->id() == id) {
            propertyBindings_.removeAt(i);
            return true;
        }
    }
    return false;
}

std::shared_ptr<RigPropertyBinding2D> Rig2D::findPropertyBinding(const Id& id) const
{
    for (const auto& binding : propertyBindings_) {
        if (binding && binding->id() == id) {
            return binding;
        }
    }
    return {};
}

std::shared_ptr<RigPropertyBinding2D> Rig2D::findPropertyBinding(const QString& name) const
{
    for (const auto& binding : propertyBindings_) {
        if (binding && binding->name() == name) {
            return binding;
        }
    }
    return {};
}

int Rig2D::propertyBindingCount() const
{
    return propertyBindings_.size();
}

QJsonObject Rig2D::toJson() const {
    QJsonObject object;
    object["version"] = 1;
    if (rootBone_) {
        object["rootBoneId"] = rootBone_->id().toString();
    }

    QJsonArray bonesArray;
    for (const Bone2D* bone : bones_) {
        if (bone) {
            bonesArray.append(bone->toJson());
        }
    }
    object["bones"] = bonesArray;

    QJsonArray controlsArray;
    for (const RigControl2D* control : controlSet_.controls()) {
        if (control) {
            controlsArray.append(control->toJson());
        }
    }
    object["controls"] = controlsArray;

    QJsonArray constraintsArray;
    for (const auto& constraint : constraints_) {
        if (constraint) {
            constraintsArray.append(constraint->toJson());
        }
    }
    object["constraints"] = constraintsArray;

    QJsonArray propertyBindingsArray;
    for (const auto& binding : propertyBindings_) {
        if (binding) {
            propertyBindingsArray.append(binding->toJson());
        }
    }
    object["propertyBindings"] = propertyBindingsArray;
    return object;
}

Rig2D Rig2D::fromJson(const QJsonObject& object) {
    Rig2D rig;
    const QJsonArray bonesArray = object.value("bones").toArray();
    QList<QPair<Id, Id>> parentLinks;

    for (const QJsonValue& value : bonesArray) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject boneObject = value.toObject();
        auto* bone = new Bone2D();
        bone->fromJson(boneObject);
        rig.bones_.append(bone);

        const QString parentIdString = boneObject.value("parentId").toString();
        if (!parentIdString.isEmpty()) {
            parentLinks.append(QPair<Id, Id>(bone->id(), Id(parentIdString)));
        }
    }

    for (const auto& link : parentLinks) {
        Bone2D* child = rig.findBone(link.first);
        Bone2D* parent = rig.findBone(link.second);
        if (child && parent) {
            parent->addChild(child);
        }
    }

    rig.controlSet_ = RigControlSet2D::fromJson(object.value("controls").toArray());

    const QJsonArray constraintsArray = object.value("constraints").toArray();
    for (const QJsonValue& value : constraintsArray) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject constraintObject = value.toObject();
        const QString kind = constraintObject.value("kind").toString();
        if (kind == QStringLiteral("Parent")) {
            rig.constraints_.append(ParentConstraint2D::fromJson(constraintObject));
        } else if (kind == QStringLiteral("MapRange")) {
            rig.constraints_.append(MapRangeConstraint2D::fromJson(constraintObject));
        } else if (kind == QStringLiteral("Aim")) {
            rig.constraints_.append(AimConstraint2D::fromJson(constraintObject));
        } else if (kind == QStringLiteral("TwoBoneIK")) {
            rig.constraints_.append(TwoBoneIKConstraint2D::fromJson(constraintObject));
        }
    }

    const QJsonArray bindingsArray = object.value("propertyBindings").toArray();
    for (const QJsonValue& value : bindingsArray) {
        if (!value.isObject()) {
            continue;
        }
        rig.propertyBindings_.append(RigPropertyBinding2D::fromJson(value.toObject()));
    }

    const QString rootBoneIdString = object.value("rootBoneId").toString();
    if (!rootBoneIdString.isEmpty()) {
        rig.rootBone_ = rig.findBone(Id(rootBoneIdString));
    }
    if (!rig.rootBone_ && !rig.bones_.isEmpty()) {
        rig.rootBone_ = rig.bones_.first();
    }
    rig.update();
    return rig;
}

// ─────────────────────────────────────────────────────────
// Two-Bone IK ソルバー
// ─────────────────────────────────────────────────────────

void Rig2D::solveTwoBoneIK(Bone2D* bone1, Bone2D* bone2, Bone2D* effector, const QVector2D& target) {
    if (!bone1 || !bone2 || !effector) return;

    // ボーンのグローバル位置を取得
    QVector2D p1(bone1->globalMatrix().column(3).x(), bone1->globalMatrix().column(3).y());
    QVector2D p2(bone2->globalMatrix().column(3).x(), bone2->globalMatrix().column(3).y());
    QVector2D p3(effector->globalMatrix().column(3).x(), effector->globalMatrix().column(3).y());

    // ボーンの長さ
    float len1 = (p2 - p1).length();
    float len2 = (p3 - p2).length();

    // 目標までの距離
    float dist = (target - p1).length();

    // リーチ制限
    float maxReach = len1 + len2;
    if (dist > maxReach) {
        // 完全に伸ばす
        QVector2D dir = (target - p1).normalized();
        p2 = p1 + dir * len1;
        p3 = p1 + dir * maxReach;
    } else {
        // 三角形の余弦定理で角度を計算
        float cosAngle1 = (len1 * len1 + dist * dist - len2 * len2) / (2.0f * len1 * dist);
        float cosAngle2 = (len1 * len1 + len2 * len2 - dist * dist) / (2.0f * len1 * len2);

        cosAngle1 = std::clamp(cosAngle1, -1.0f, 1.0f);
        cosAngle2 = std::clamp(cosAngle2, -1.0f, 1.0f);

        float angle1 = std::acos(cosAngle1);
        float angle2 = std::acos(cosAngle2);

        // 現在の角度から目標角度へ
        float baseAngle = std::atan2(target.y() - p1.y(), target.x() - p1.x());

        // 関節位置を計算
        p2 = p1 + QVector2D(std::cos(baseAngle + angle1), std::sin(baseAngle + angle1)) * len1;
        p3 = target; // エフェクタは目標位置に到達
    }

    // ボーン1の回転を更新
    float newAngle1 = std::atan2(p2.y() - p1.y(), p2.x() - p1.x()) * (180.0f / 3.14159265f);
    if (bone1->parent()) {
        float parentAngle = std::atan2(
            bone1->parent()->globalMatrix().column(1).y(),
            bone1->parent()->globalMatrix().column(1).x()
        ) * (180.0f / 3.14159265f);
        bone1->setLocalRotation(newAngle1 - parentAngle);
    } else {
        bone1->setLocalRotation(newAngle1);
    }

    // ボーン2の回転を更新
    float newAngle2 = std::atan2(p3.y() - p2.y(), p3.x() - p2.x()) * (180.0f / 3.14159265f);
    float currentAngle1 = std::atan2(p2.y() - p1.y(), p2.x() - p1.x()) * (180.0f / 3.14159265f);
    bone2->setLocalRotation(newAngle2 - currentAngle1);

    // 階層を更新
    update();
}

// ─────────────────────────────────────────────────────────
// CCD-IK (Cyclic Coordinate Descent) ソルバー
// ─────────────────────────────────────────────────────────

void Rig2D::solveCCDIK(Bone2D* effector, const QVector2D& target, int iterations, float tolerance) {
    if (!effector) return;

    QVector2D effectorPos(effector->globalMatrix().column(3).x(), effector->globalMatrix().column(3).y());
    if ((target - effectorPos).length() < tolerance) return;

    // エフェクタからルートまでのチェーンを構築
    QList<Bone2D*> chain;
    Bone2D* current = effector;
    while (current) {
        chain.prepend(current);
        current = current->parent();
    }

    for (int iter = 0; iter < iterations; ++iter) {
        // 末端から順に各ボーンを調整
        for (int i = chain.size() - 1; i >= 0; --i) {
            Bone2D* bone = chain[i];
            if (!bone) continue;

            // 現在のボーンのグローバル位置
            QVector2D bonePos(bone->globalMatrix().column(3).x(), bone->globalMatrix().column(3).y());

            // エフェクタの現在位置
            QVector2D effPos(effector->globalMatrix().column(3).x(), effector->globalMatrix().column(3).y());

            // 目標へのベクトルとエフェクタへのベクトル
            QVector2D toTarget = target - bonePos;
            QVector2D toEffector = effPos - bonePos;

            float angleToTarget = std::atan2(toTarget.y(), toTarget.x());
            float angleToEffector = std::atan2(toEffector.y(), toEffector.x());

            // 回転差分
            float deltaAngle = (angleToTarget - angleToEffector) * (180.0f / 3.14159265f);

            // 回転を適用（ローカル回転に加算）
            bone->setLocalRotation(bone->localTransform().rotation + deltaAngle);

            // 階層を更新
            update();

            // 収束チェック
            effPos = QVector2D(effector->globalMatrix().column(3).x(), effector->globalMatrix().column(3).y());
            if ((target - effPos).length() < tolerance) {
                return;
            }
        }
    }
}

} // namespace ArtifactCore
