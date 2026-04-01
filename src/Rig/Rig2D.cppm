module;
#include <QString>
#include <QVector2D>
#include <QMatrix4x4>
#include <QList>
#include <cmath>
#include <algorithm>

module ArtifactCore.Rig2D;

import Utils.Id;

namespace ArtifactCore {

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
