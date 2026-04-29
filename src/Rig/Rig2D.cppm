module;
#include <utility>
#include <QString>
#include <QVector2D>
#include <QMatrix4x4>
#include <QList>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QtGlobal>
#include <cmath>
#include <algorithm>
#include <utility>

module ArtifactCore.Rig2D;

import Utils.Id;
import Time.Rational;

namespace ArtifactCore {

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

} // namespace

// ─────────────────────────────────────────────────────────
// Bone2D 実装
// ─────────────────────────────────────────────────────────

Bone2D::Bone2D() : id_(), name_("Bone") {
    globalMatrix_.setToIdentity();
}

Bone2D::Bone2D(const QString& name) : id_(), name_(name) {
    globalMatrix_.setToIdentity();
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
}

void Bone2D::updateHierarchy() {
    // ローカル変換からローカル行列を構築
    QMatrix4x4 localMatrix;
    localMatrix.setToIdentity();
    localMatrix.translate(localTransform_.position.x(), localTransform_.position.y(), 0.0f);
    localMatrix.rotate(localTransform_.rotation, 0.0f, 0.0f, 1.0f);
    localMatrix.scale(localTransform_.scale.x(), localTransform_.scale.y(), 1.0f);

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
// Rig2D 実装
// ─────────────────────────────────────────────────────────

Rig2D::Rig2D() {
}

Rig2D::Rig2D(Rig2D&& other) noexcept
    : rootBone_(other.rootBone_) {
    bones_.swap(other.bones_);
    other.rootBone_ = nullptr;
}

Rig2D& Rig2D::operator=(Rig2D&& other) noexcept {
    if (this != &other) {
        clearBones();
        bones_.swap(other.bones_);
        rootBone_ = other.rootBone_;
        other.rootBone_ = nullptr;
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
    rootBone_ = nullptr;
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
            bone->setLocalTransform(bone->evaluate(time));
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
