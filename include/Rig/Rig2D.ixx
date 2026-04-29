module;
#include <utility>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>
#include <QList>
#include <QJsonObject>
#include <memory>
#include <vector>

export module ArtifactCore.Rig2D;

import Utils.Id;
import Time.Rational;

export namespace ArtifactCore {

// ボーンのローカル変換を保持する構造体
struct BoneTransform {
    QVector2D position = {0.0f, 0.0f};
    float rotation = 0.0f;           // 度単位
    QVector2D scale = {1.0f, 1.0f};
};

// 2Dボーン階層のノード
class Bone2D {
public:
    Bone2D();
    explicit Bone2D(const QString& name);
    ~Bone2D() = default;

    Id id() const { return id_; }
    void setId(const Id& id) { id_ = id; }

    QString name() const { return name_; }
    void setName(const QString& name) { name_ = name; }

    // 親子関係
    Bone2D* parent() const { return parent_; }
    void setParent(Bone2D* parent) { parent_ = parent; }

    const QList<Bone2D*>& children() const { return children_; }
    void addChild(Bone2D* child);
    void removeChild(Bone2D* child);

    // ローカル変換
    const BoneTransform& localTransform() const { return localTransform_; }
    void setLocalTransform(const BoneTransform& transform) { localTransform_ = transform; }

    void setLocalPosition(const QVector2D& pos) { localTransform_.position = pos; }
    void setLocalRotation(float rot) { localTransform_.rotation = rot; }
    void setLocalScale(const QVector2D& scale) { localTransform_.scale = scale; }

    // グローバル変換（更新後に有効）
    const QMatrix4x4& globalMatrix() const { return globalMatrix_; }

    // ボーンの長さ（視覚化用）
    float length() const { return length_; }
    void setLength(float length) { length_ = length; }

    // 評価境界。現時点では静的ローカル変換を返し、将来のキー/制約評価をここへ集約する。
    BoneTransform evaluate(const RationalTime& time) const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& object);

    // 階層を更新（グローバル変換の再計算）
    void updateHierarchy();

private:
    Id id_;
    QString name_;
    Bone2D* parent_ = nullptr;
    QList<Bone2D*> children_;
    BoneTransform localTransform_;
    QMatrix4x4 globalMatrix_;
    float length_ = 50.0f;
};

// 2Dリグシステム全体を管理
class Rig2D {
public:
    Rig2D();
    Rig2D(const Rig2D&) = delete;
    Rig2D& operator=(const Rig2D&) = delete;
    Rig2D(Rig2D&& other) noexcept;
    Rig2D& operator=(Rig2D&& other) noexcept;
    ~Rig2D();

    // ボーン管理
    Bone2D* addBone(const QString& name, Bone2D* parent = nullptr);
    Bone2D* addBone(const QString& name, const Id& parentId);
    void removeBone(Bone2D* bone);
    bool removeBone(const Id& id);
    void clearBones();

    const QList<Bone2D*>& bones() const { return bones_; }
    Bone2D* findBone(const Id& id) const;
    Bone2D* findBone(const QString& name) const;

    // ルートボーン
    Bone2D* rootBone() const { return rootBone_; }
    void setRootBone(Bone2D* bone) { rootBone_ = bone; }

    // 更新
    void update();
    void evaluate(const RationalTime& time);
    bool setBoneLocalTransform(const Id& id, const BoneTransform& transform);
    bool boneLocalTransform(const Id& id, BoneTransform* outTransform) const;

    QJsonObject toJson() const;
    static Rig2D fromJson(const QJsonObject& object);

    // IKソルバー
    void solveTwoBoneIK(Bone2D* bone1, Bone2D* bone2, Bone2D* effector, const QVector2D& target);
    void solveCCDIK(Bone2D* effector, const QVector2D& target, int iterations = 10, float tolerance = 0.1f);

private:
    QList<Bone2D*> bones_;
    Bone2D* rootBone_ = nullptr;
};

} // namespace ArtifactCore
