module;
#include <utility>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>
#include <QList>
#include <QHash>
#include <QJsonObject>
#include <QVariant>
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

enum class RigControlKind {
    Slider,
    Point,
    Angle
};

enum class RigConstraintKind {
    Parent,
    MapRange,
    Aim,
    TwoBoneIK
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
    void setLocalTransform(const BoneTransform& transform) { localTransform_ = transform; resolvedTransform_ = transform; }
    const BoneTransform& resolvedTransform() const { return resolvedTransform_; }
    void setResolvedTransform(const BoneTransform& transform) { resolvedTransform_ = transform; }
    void syncResolvedToLocal() { resolvedTransform_ = localTransform_; }

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
    BoneTransform resolvedTransform_;
    QMatrix4x4 globalMatrix_;
    float length_ = 50.0f;
};

class RigControl2D {
public:
    RigControl2D();
    explicit RigControl2D(const QString& name, RigControlKind kind);

    Id id() const { return id_; }
    QString name() const { return name_; }
    void setName(const QString& name) { name_ = name; }

    RigControlKind kind() const { return kind_; }
    void setKind(RigControlKind kind) { kind_ = kind; }

    QVariant value() const { return value_; }
    void setValue(const QVariant& value) { value_ = value; }

    QVariant minValue() const { return minValue_; }
    QVariant maxValue() const { return maxValue_; }
    void setRange(const QVariant& minValue, const QVariant& maxValue);

    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    QJsonObject toJson() const;
    static RigControl2D fromJson(const QJsonObject& object);

private:
    Id id_;
    QString name_;
    RigControlKind kind_ = RigControlKind::Slider;
    QVariant value_;
    QVariant minValue_;
    QVariant maxValue_;
    bool enabled_ = true;
};

class Rig2D;

class RigEvaluationContext2D {
public:
    RigEvaluationContext2D();

    void setRig(Rig2D* rig);
    Rig2D* rig() const { return rig_; }
    const RationalTime& time() const { return time_; }
    void setTime(const RationalTime& time);

    void indexBones(const QList<Bone2D*>& bones);
    void indexControls(const QList<RigControl2D*>& controls);

    Bone2D* findBone(const Id& id) const;
    RigControl2D* findControl(const Id& id) const;

private:
    Rig2D* rig_ = nullptr;
    RationalTime time_;
    QHash<Id, Bone2D*> bonesById_;
    QHash<Id, RigControl2D*> controlsById_;
};

class RigConstraint2D {
public:
    RigConstraint2D();
    explicit RigConstraint2D(const QString& name);
    virtual ~RigConstraint2D() = default;

    Id id() const { return id_; }
    QString name() const { return name_; }
    void setName(const QString& name) { name_ = name; }

    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    virtual RigConstraintKind kind() const = 0;
    virtual void evaluate(RigEvaluationContext2D& context) = 0;
    virtual QJsonObject toJson() const = 0;

protected:
    Id id_;
    QString name_;
    bool enabled_ = true;
};

class ParentConstraint2D final : public RigConstraint2D {
public:
    ParentConstraint2D();
    ParentConstraint2D(const QString& name, const Id& targetBoneId, const Id& parentBoneId);

    RigConstraintKind kind() const override { return RigConstraintKind::Parent; }
    void evaluate(RigEvaluationContext2D& context) override;
    QJsonObject toJson() const override;
    static std::shared_ptr<ParentConstraint2D> fromJson(const QJsonObject& object);

    Id targetBoneId() const { return targetBoneId_; }
    void setTargetBoneId(const Id& id) { targetBoneId_ = id; }
    Id parentBoneId() const { return parentBoneId_; }
    void setParentBoneId(const Id& id) { parentBoneId_ = id; }
    BoneTransform offset() const { return offset_; }
    void setOffset(const BoneTransform& offset) { offset_ = offset; }

private:
    Id targetBoneId_;
    Id parentBoneId_;
    BoneTransform offset_;
};

class MapRangeConstraint2D final : public RigConstraint2D {
public:
    MapRangeConstraint2D();
    MapRangeConstraint2D(const QString& name, const Id& controlId, const Id& targetBoneId);

    RigConstraintKind kind() const override { return RigConstraintKind::MapRange; }
    void evaluate(RigEvaluationContext2D& context) override;
    QJsonObject toJson() const override;
    static std::shared_ptr<MapRangeConstraint2D> fromJson(const QJsonObject& object);

    Id controlId() const { return controlId_; }
    void setControlId(const Id& id) { controlId_ = id; }
    Id targetBoneId() const { return targetBoneId_; }
    void setTargetBoneId(const Id& id) { targetBoneId_ = id; }
    QString targetChannel() const { return targetChannel_; }
    void setTargetChannel(const QString& channel) { targetChannel_ = channel; }
    double inputMin() const { return inputMin_; }
    double inputMax() const { return inputMax_; }
    double outputMin() const { return outputMin_; }
    double outputMax() const { return outputMax_; }
    void setMapping(double inputMin, double inputMax, double outputMin, double outputMax);

private:
    Id controlId_;
    Id targetBoneId_;
    QString targetChannel_ = QStringLiteral("rotation");
    double inputMin_ = 0.0;
    double inputMax_ = 1.0;
    double outputMin_ = 0.0;
    double outputMax_ = 1.0;
};

class AimConstraint2D final : public RigConstraint2D {
public:
    AimConstraint2D();
    AimConstraint2D(const QString& name, const Id& sourceBoneId, const Id& targetBoneId);

    RigConstraintKind kind() const override { return RigConstraintKind::Aim; }
    void evaluate(RigEvaluationContext2D& context) override;
    QJsonObject toJson() const override;
    static std::shared_ptr<AimConstraint2D> fromJson(const QJsonObject& object);

    Id sourceBoneId() const { return sourceBoneId_; }
    void setSourceBoneId(const Id& id) { sourceBoneId_ = id; }
    Id targetBoneId() const { return targetBoneId_; }
    void setTargetBoneId(const Id& id) { targetBoneId_ = id; }
    float angleOffset() const { return angleOffset_; }
    void setAngleOffset(float angleOffset) { angleOffset_ = angleOffset; }

private:
    Id sourceBoneId_;
    Id targetBoneId_;
    float angleOffset_ = 0.0f;
};

class TwoBoneIKConstraint2D final : public RigConstraint2D {
public:
    TwoBoneIKConstraint2D();
    TwoBoneIKConstraint2D(const QString& name,
                          const Id& upperBoneId,
                          const Id& lowerBoneId,
                          const Id& effectorBoneId,
                          const Id& targetBoneId);

    RigConstraintKind kind() const override { return RigConstraintKind::TwoBoneIK; }
    void evaluate(RigEvaluationContext2D& context) override;
    QJsonObject toJson() const override;
    static std::shared_ptr<TwoBoneIKConstraint2D> fromJson(const QJsonObject& object);

    Id upperBoneId() const { return upperBoneId_; }
    void setUpperBoneId(const Id& id) { upperBoneId_ = id; }
    Id lowerBoneId() const { return lowerBoneId_; }
    void setLowerBoneId(const Id& id) { lowerBoneId_ = id; }
    Id effectorBoneId() const { return effectorBoneId_; }
    void setEffectorBoneId(const Id& id) { effectorBoneId_ = id; }
    Id targetBoneId() const { return targetBoneId_; }
    void setTargetBoneId(const Id& id) { targetBoneId_ = id; }
    float poleAngle() const { return poleAngle_; }
    void setPoleAngle(float poleAngle) { poleAngle_ = poleAngle; }

private:
    Id upperBoneId_;
    Id lowerBoneId_;
    Id effectorBoneId_;
    Id targetBoneId_;
    float poleAngle_ = 0.0f;
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

    // Control management
    RigControl2D* addControl(const QString& name, RigControlKind kind, const QVariant& defaultValue = QVariant());
    RigControl2D* addSlider(const QString& name, double defaultValue = 0.0, double minValue = 0.0, double maxValue = 1.0);
    RigControl2D* addPoint(const QString& name, const QVector2D& defaultValue = QVector2D());
    RigControl2D* addAngle(const QString& name, double defaultValue = 0.0, double minValue = -180.0, double maxValue = 180.0);
    bool removeControl(const Id& id);
    RigControl2D* findControl(const Id& id) const;
    RigControl2D* findControl(const QString& name) const;
    int controlCount() const;
    const QList<RigControl2D*>& controls() const { return controls_; }
    bool setControlValue(const Id& id, const QVariant& value);
    QVariant controlValue(const Id& id) const;

    // Constraint management
    std::shared_ptr<RigConstraint2D> addConstraint(std::shared_ptr<RigConstraint2D> constraint);
    bool removeConstraint(const Id& id);
    std::shared_ptr<RigConstraint2D> findConstraint(const Id& id) const;
    std::shared_ptr<RigConstraint2D> findConstraint(const QString& name) const;
    int constraintCount() const;
    const QList<std::shared_ptr<RigConstraint2D>>& constraints() const { return constraints_; }

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
    QList<RigControl2D*> controls_;
    QList<std::shared_ptr<RigConstraint2D>> constraints_;
    Bone2D* rootBone_ = nullptr;
};

} // namespace ArtifactCore
