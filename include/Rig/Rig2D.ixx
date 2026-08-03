module;
class tst_QList;
#include <utility>
#include <QString>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>
#include <QList>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>
#include <memory>
#include <vector>
#include <set>
#include <map>

export module ArtifactCore.Rig2D;

import Utils.Id;
import Memory.SharedPtr;
import Time.Rational;
import Animation.Value;
import Frame.Position;

export namespace ArtifactCore {

// ボーンのローカル変換を保持する構造体
struct BoneTransform {
    QVector2D position = {0.0f, 0.0f};
    float rotation = 0.0f;           // 度単位
    QVector2D scale = {1.0f, 1.0f};

    // AnimatableValueT<T> が必要とする演算子
    BoneTransform operator+(const BoneTransform& other) const;
    BoneTransform operator-(const BoneTransform& other) const;
    BoneTransform operator*(float scalar) const;
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

    // 評価。キーフレームがあれば時間補間、なければ静的ローカル変換を返す。
    BoneTransform evaluate(const RationalTime& time) const;

    // キーフレーム管理
    void addKeyFrame(const FramePosition& frame, const BoneTransform& transform);
    void removeKeyFrameAt(const FramePosition& frame);
    bool hasKeyFrameAt(const FramePosition& frame) const;
    size_t keyFrameCount() const;
    void clearKeyFrames();

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
    AnimatableValueT<BoneTransform> keyframes_;
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

class RigControlSet2D {
public:
    RigControlSet2D();
    RigControlSet2D(const RigControlSet2D&) = delete;
    RigControlSet2D& operator=(const RigControlSet2D&) = delete;
    RigControlSet2D(RigControlSet2D&& other) noexcept;
    RigControlSet2D& operator=(RigControlSet2D&& other) noexcept;
    ~RigControlSet2D();

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
    void clear();

    QJsonArray toJson() const;
    static RigControlSet2D fromJson(const QJsonArray& array);

private:
    QList<RigControl2D*> controls_;
};

class RigPropertyBinding2D {
public:
    RigPropertyBinding2D();
    RigPropertyBinding2D(const QString& name,
                        const Id& controlId,
                        const LayerID& targetLayerId,
                        const QString& targetPropertyPath);

    Id id() const { return id_; }
    QString name() const { return name_; }
    void setName(const QString& name) { name_ = name; }

    Id controlId() const { return controlId_; }
    void setControlId(const Id& id) { controlId_ = id; }

    LayerID targetLayerId() const { return targetLayerId_; }
    void setTargetLayerId(const LayerID& id) { targetLayerId_ = id; }

    QString targetPropertyPath() const { return targetPropertyPath_; }
    void setTargetPropertyPath(const QString& path) { targetPropertyPath_ = path; }

    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    QJsonObject toJson() const;
    static SharedPtr<RigPropertyBinding2D> fromJson(const QJsonObject& object);

private:
    Id id_;
    QString name_;
    Id controlId_;
    LayerID targetLayerId_;
    QString targetPropertyPath_;
    bool enabled_ = true;
};

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
    static SharedPtr<ParentConstraint2D> fromJson(const QJsonObject& object);

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
    static SharedPtr<MapRangeConstraint2D> fromJson(const QJsonObject& object);

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
    static SharedPtr<AimConstraint2D> fromJson(const QJsonObject& object);

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
    static SharedPtr<TwoBoneIKConstraint2D> fromJson(const QJsonObject& object);

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

// ─────────────────────────────────────────────────────────
// Smart Bone: 1つのボーン角度が複数ボーンを駆動 (Spine方式)
// ─────────────────────────────────────────────────────────

struct SmartBoneKey {
    float driverAngle = 0.0f;
    std::map<Id, BoneTransform> targetTransforms;
    std::map<Id, QVector2D> meshOffsets;
};

class SmartBoneController {
public:
    SmartBoneController() = default;

    void setDriverBone(const Id& boneId) { driverBoneId_ = boneId; }
    Id driverBone() const { return driverBoneId_; }

    void addKey(float driverAngle, const std::map<Id, BoneTransform>& targets) {
        SmartBoneKey key;
        key.driverAngle = driverAngle;
        key.targetTransforms = targets;
        keys_.push_back(key);
        std::sort(keys_.begin(), keys_.end(),
            [](const SmartBoneKey& a, const SmartBoneKey& b) { return a.driverAngle < b.driverAngle; });
    }

    void removeKey(float driverAngle) {
        keys_.erase(std::remove_if(keys_.begin(), keys_.end(),
            [driverAngle](const SmartBoneKey& k) { return std::abs(k.driverAngle - driverAngle) < 0.001f; }),
            keys_.end());
    }

    void clearKeys() { keys_.clear(); }
    size_t keyCount() const { return keys_.size(); }

    void evaluate(float driverAngle, std::map<Id, BoneTransform>& outTargets) const {
        if (keys_.empty()) return;
        if (keys_.size() == 1) {
            outTargets = keys_[0].targetTransforms;
            return;
        }
        auto it = std::lower_bound(keys_.begin(), keys_.end(), driverAngle,
            [](const SmartBoneKey& k, float v) { return k.driverAngle < v; });
        if (it == keys_.begin()) {
            outTargets = keys_.front().targetTransforms;
        } else if (it == keys_.end()) {
            outTargets = keys_.back().targetTransforms;
        } else {
            const auto& a = *(it - 1);
            const auto& b = *it;
            float span = b.driverAngle - a.driverAngle;
            float t = std::abs(span) < 0.001f ? 0.0f : std::clamp((driverAngle - a.driverAngle) / span, 0.0f, 1.0f);
            for (const auto& [id, btA] : a.targetTransforms) {
                auto itB = b.targetTransforms.find(id);
                if (itB == b.targetTransforms.end()) {
                    outTargets[id] = btA;
                } else {
                    BoneTransform bt;
                    bt.position = btA.position + (itB->second.position - btA.position) * t;
                    bt.rotation = btA.rotation + (itB->second.rotation - btA.rotation) * t;
                    bt.scale    = btA.scale    + (itB->second.scale    - btA.scale)    * t;
                    outTargets[id] = bt;
                }
            }
        }
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["driverBoneId"] = driverBoneId_.toString();
        QJsonArray arr;
        for (const auto& k : keys_) {
            QJsonObject ko;
            ko["angle"] = static_cast<double>(k.driverAngle);
            QJsonObject to;
            for (const auto& [id, bt] : k.targetTransforms) {
                to[id.toString()] = transformToJson(bt);
            }
            ko["targets"] = to;
            arr.append(ko);
        }
        obj["keys"] = arr;
        return obj;
    }

    static SmartBoneController fromJson(const QJsonObject& obj) {
        SmartBoneController ctrl;
        ctrl.setDriverBone(Id(obj.value("driverBoneId").toString()));
        for (const auto& v : obj.value("keys").toArray()) {
            QJsonObject ko = v.toObject();
            SmartBoneKey key;
            key.driverAngle = static_cast<float>(ko.value("angle").toDouble());
            QJsonObject to = ko.value("targets").toObject();
            for (auto it = to.begin(); it != to.end(); ++it) {
                key.targetTransforms[Id(it.key())] = transformFromJson(it.value(), BoneTransform{});
            }
            ctrl.keys_.push_back(key);
        }
        std::sort(ctrl.keys_.begin(), ctrl.keys_.end(),
            [](const SmartBoneKey& a, const SmartBoneKey& b) { return a.driverAngle < b.driverAngle; });
        return ctrl;
    }

private:
    Id driverBoneId_;
    std::vector<SmartBoneKey> keys_;
};

// ─────────────────────────────────────────────────────────
// Skin2D: メッシュスキニング
// ─────────────────────────────────────────────────────────

struct SkinVertex {
    QVector2D position;
    QVector2D uv;
    float weights[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    int16_t boneIndices[4] = {0, -1, -1, -1};
};

class SkinMesh {
public:
    void setVertices(const std::vector<SkinVertex>& verts) { vertices_ = verts; restPositions_.clear(); for (const auto& v : vertices_) restPositions_.push_back(v.position); }
    void setTriangles(const std::vector<uint32_t>& tris) { triangles_ = tris; }
    const std::vector<SkinVertex>& vertices() const { return vertices_; }
    const std::vector<uint32_t>& triangles() const { return triangles_; }
    const std::vector<QVector2D>& restPositions() const { return restPositions_; }
    int vertexCount() const { return static_cast<int>(vertices_.size()); }
    int triangleCount() const { return static_cast<int>(triangles_.size()) / 3; }

    void autoBind(Rig2D* rig, int maxBones = 4) {
        if (!rig || vertices_.empty()) return;
        std::vector<Bone2D*> bones;
        for (auto* b : rig->bones()) { if (b && b->length() > 0.01f) bones.push_back(b); }
        if (bones.empty()) return;
        for (size_t vi = 0; vi < vertices_.size(); ++vi) {
            std::vector<std::pair<float,int>> dists;
            for (size_t bi = 0; bi < bones.size(); ++bi) {
                QVector2D bp(bones[bi]->globalMatrix().column(3).x(), bones[bi]->globalMatrix().column(3).y());
                float d = (vertices_[vi].position - bp).lengthSquared();
                dists.push_back({d, static_cast<int>(bi)});
            }
            std::sort(dists.begin(), dists.end());
            int count = std::min(maxBones, static_cast<int>(dists.size()));
            float totalW = 0.0f;
            for (int i = 0; i < count; ++i) {
                float w = dists[i].first < 1e-8f ? 1.0f : 1.0f / std::max(dists[i].first, 1e-8f);
                vertices_[vi].weights[i] = w;
                vertices_[vi].boneIndices[i] = static_cast<int16_t>(dists[i].second);
                totalW += w;
            }
            if (totalW > 1e-8f) {
                for (int i = 0; i < count; ++i) vertices_[vi].weights[i] /= totalW;
            }
            for (int i = count; i < 4; ++i) { vertices_[vi].weights[i] = 0.0f; vertices_[vi].boneIndices[i] = -1; }
        }
    }

    void deform(Rig2D* rig, std::vector<QVector2D>& outPositions) const {
        outPositions.resize(vertices_.size());
        if (!rig) return;
        const auto& bones = rig->bones();
        for (size_t i = 0; i < vertices_.size(); ++i) {
            QVector2D p(0, 0);
            for (int w = 0; w < 4; ++w) {
                int bi = vertices_[i].boneIndices[w];
                if (bi < 0 || bi >= static_cast<int>(bones.size()) || !bones[bi]) continue;
                QMatrix4x4 m = bones[bi]->globalMatrix();
                QVector2D bp(restPositions_[i].x() * m(0,0) + restPositions_[i].y() * m(0,1) + m(0,3),
                             restPositions_[i].x() * m(1,0) + restPositions_[i].y() * m(1,1) + m(1,3));
                p += bp * vertices_[i].weights[w];
            }
            outPositions[i] = p;
        }
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        QJsonArray va;
        for (const auto& v : vertices_) {
            QJsonObject vo;
            vo["px"] = static_cast<double>(v.position.x());
            vo["py"] = static_cast<double>(v.position.y());
            vo["u"] = static_cast<double>(v.uv.x());
            vo["v"] = static_cast<double>(v.uv.y());
            QJsonArray wa, ba;
            for (int i = 0; i < 4; ++i) { wa.append(static_cast<double>(v.weights[i])); ba.append(v.boneIndices[i]); }
            vo["w"] = wa; vo["bi"] = ba;
            va.append(vo);
        }
        obj["vertices"] = va;
        QJsonArray ta;
        for (auto t : triangles_) ta.append(static_cast<qint64>(t));
        obj["triangles"] = ta;
        return obj;
    }

    static SkinMesh fromJson(const QJsonObject& obj) {
        SkinMesh m;
        QJsonArray va = obj.value("vertices").toArray();
        for (const auto& vv : va) {
            QJsonObject vo = vv.toObject();
            SkinVertex sv;
            sv.position = QVector2D(static_cast<float>(vo.value("px").toDouble()), static_cast<float>(vo.value("py").toDouble()));
            sv.uv = QVector2D(static_cast<float>(vo.value("u").toDouble()), static_cast<float>(vo.value("v").toDouble()));
            QJsonArray wa = vo.value("w").toArray();
            QJsonArray ba = vo.value("bi").toArray();
            for (int i = 0; i < 4; ++i) { sv.weights[i] = static_cast<float>(wa[i].toDouble()); sv.boneIndices[i] = static_cast<int16_t>(ba[i].toInt()); }
            m.vertices_.push_back(sv);
        }
        for (auto tv : obj.value("triangles").toArray()) m.triangles_.push_back(static_cast<uint32_t>(tv.toInteger()));
        m.restPositions_.clear();
        for (const auto& v : m.vertices_) m.restPositions_.push_back(v.position);
        return m;
    }

private:
    std::vector<SkinVertex> vertices_;
    std::vector<uint32_t> triangles_;
    std::vector<QVector2D> restPositions_;
};

// ─────────────────────────────────────────────────────────
// 2Dリグシステム全体を管理
// ─────────────────────────────────────────────────────────
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
    const QList<RigControl2D*>& controls() const { return controlSet_.controls(); }
    bool setControlValue(const Id& id, const QVariant& value);
    QVariant controlValue(const Id& id) const;

    // Constraint management
    SharedPtr<RigConstraint2D> addConstraint(SharedPtr<RigConstraint2D> constraint);
    bool removeConstraint(const Id& id);
    SharedPtr<RigConstraint2D> findConstraint(const Id& id) const;
    SharedPtr<RigConstraint2D> findConstraint(const QString& name) const;
    int constraintCount() const;
    const QList<SharedPtr<RigConstraint2D>>& constraints() const { return constraints_; }

    SharedPtr<RigPropertyBinding2D> addPropertyBinding(SharedPtr<RigPropertyBinding2D> binding);
    bool removePropertyBinding(const Id& id);
    SharedPtr<RigPropertyBinding2D> findPropertyBinding(const Id& id) const;
    SharedPtr<RigPropertyBinding2D> findPropertyBinding(const QString& name) const;
    int propertyBindingCount() const;
    const QList<SharedPtr<RigPropertyBinding2D>>& propertyBindings() const { return propertyBindings_; }

    // Smart Bones
    SmartBoneController* addSmartBone() { smartBones_.push_back(SmartBoneController()); return &smartBones_.back(); }
    void removeSmartBone(int index) { if (index >= 0 && static_cast<size_t>(index) < smartBones_.size()) smartBones_.erase(smartBones_.begin() + index); }
    SmartBoneController* smartBone(int index) { return (index >= 0 && static_cast<size_t>(index) < smartBones_.size()) ? &smartBones_[index] : nullptr; }
    int smartBoneCount() const { return static_cast<int>(smartBones_.size()); }
    void evaluateSmartBones();

    // Skin Mesh
    SkinMesh* skinMesh() { return skinMesh_.get(); }
    void setSkinMesh(std::unique_ptr<SkinMesh> mesh) { skinMesh_ = std::move(mesh); }
    SkinMesh* createSkinMesh() { skinMesh_ = std::make_unique<SkinMesh>(); return skinMesh_.get(); }

    // Pose
    void setPoseName(const QString& name) { poseName_ = name; }
    QString poseName() const { return poseName_; }

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
    RigControlSet2D controlSet_;
    QList<SharedPtr<RigConstraint2D>> constraints_;
    QList<SharedPtr<RigPropertyBinding2D>> propertyBindings_;
    std::vector<SmartBoneController> smartBones_;
    std::unique_ptr<SkinMesh> skinMesh_;
    QString poseName_;
    Bone2D* rootBone_ = nullptr;
};

// ─────────────────────────────────────────────────────────
// Pose: リグ状態のスナップショット
// ─────────────────────────────────────────────────────────

struct PoseSnapshot {
    QString name;
    QString category;
    std::map<Id, BoneTransform> boneTransforms;
    std::map<Id, QVariant> controlValues;
};

inline PoseSnapshot capturePose(const Rig2D& rig) {
    PoseSnapshot pose;
    pose.name = rig.poseName();
    for (const auto* bone : rig.bones()) {
        if (!bone) continue;
        pose.boneTransforms[bone->id()] = bone->localTransform();
    }
    for (const auto* ctrl : rig.controls()) {
        if (!ctrl) continue;
        pose.controlValues[ctrl->id()] = ctrl->value();
    }
    return pose;
}

inline void applyPose(Rig2D& rig, const PoseSnapshot& pose, float blendWeight = 1.0f) {
    for (const auto& [id, bt] : pose.boneTransforms) {
        Bone2D* bone = rig.findBone(id);
        if (!bone) continue;
        if (blendWeight >= 1.0f) {
            bone->setLocalTransform(bt);
        } else {
            BoneTransform current = bone->localTransform();
            current.position = current.position + (bt.position - current.position) * blendWeight;
            current.rotation = current.rotation + (bt.rotation - current.rotation) * blendWeight;
            current.scale    = current.scale    + (bt.scale    - current.scale)    * blendWeight;
            bone->setLocalTransform(current);
        }
    }
    for (const auto& [id, val] : pose.controlValues) {
        RigControl2D* ctrl = rig.findControl(id);
        if (!ctrl) continue;
        ctrl->setValue(val);
    }
    rig.setPoseName(pose.name);
}

inline PoseSnapshot blendPoses(const PoseSnapshot& a, const PoseSnapshot& b, float t) {
    PoseSnapshot result;
    result.name = a.name + QStringLiteral(" -> ") + b.name;
    std::set<Id> allIds;
    for (const auto& [id, _] : a.boneTransforms) allIds.insert(id);
    for (const auto& [id, _] : b.boneTransforms) allIds.insert(id);
    for (const auto& id : allIds) {
        auto itA = a.boneTransforms.find(id);
        auto itB = b.boneTransforms.find(id);
        if (itA == a.boneTransforms.end()) { result.boneTransforms[id] = itB->second; }
        else if (itB == b.boneTransforms.end()) { result.boneTransforms[id] = itA->second; }
        else {
            BoneTransform bt;
            bt.position = itA->second.position + (itB->second.position - itA->second.position) * t;
            bt.rotation = itA->second.rotation + (itB->second.rotation - itA->second.rotation) * t;
            bt.scale    = itA->second.scale    + (itB->second.scale    - itA->second.scale)    * t;
            result.boneTransforms[id] = bt;
        }
    }
    for (const auto& [id, val] : a.controlValues) {
        auto itB = b.controlValues.find(id);
        if (itB != b.controlValues.end() && val.canConvert<double>() && itB->second.canConvert<double>()) {
            result.controlValues[id] = val.toDouble() + (itB->second.toDouble() - val.toDouble()) * t;
        }
    }
    return result;
}

} // namespace ArtifactCore
