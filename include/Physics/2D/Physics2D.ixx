module;
#include <box2d/box2d.h>
#include <memory>
#include <vector>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>
#include "../../Define/DllExportMacro.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Physics2D;

import Memory.SharedPtr;

export namespace ArtifactCore {

    // ─────────────────────────────────────────────────────────
    // RigidBody2D
    // Box2D v3のb2BodyIdをラップし、状態同期と制御をまとめる薄い本体
    // ─────────────────────────────────────────────────────────
    class RigidBody2D {
    public:
        enum class Type {
            Static,
            Kinematic,
            Dynamic
        };

        b2BodyId bodyId{};
        int cloneIndex = -1; // Clonerの何番目のクローンとリンクしているか
        
        QVector2D position() const {
            if (!b2Body_IsValid(bodyId)) return {0,0};
            b2Vec2 pos = b2Body_GetPosition(bodyId);
            return {pos.x, pos.y};
        }
        
        float angle() const {
            if (!b2Body_IsValid(bodyId)) return 0.0f;
            // Box2D v3 uses b2Rot for rotation, we can get angle by getting rotation
            // Actually b2Body_GetRotation exists, let's use it
            b2Rot rot = b2Body_GetRotation(bodyId);
            return std::atan2(rot.s, rot.c); 
        }

        QVector2D linearVelocity() const {
            if (!b2Body_IsValid(bodyId)) return {0, 0};
            b2Vec2 vel = b2Body_GetLinearVelocity(bodyId);
            return {vel.x, vel.y};
        }

        float angularVelocity() const {
            if (!b2Body_IsValid(bodyId)) return 0.0f;
            return b2Body_GetAngularVelocity(bodyId);
        }

        Type type() const {
            if (!b2Body_IsValid(bodyId)) return Type::Static;
            switch (b2Body_GetType(bodyId)) {
            case b2_staticBody: return Type::Static;
            case b2_kinematicBody: return Type::Kinematic;
            case b2_dynamicBody: return Type::Dynamic;
            default: return Type::Static;
            }
        }

        void applyForce(const QVector2D& force, const QVector2D& point) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_ApplyForce(bodyId, b2Vec2{force.x(), force.y()}, b2Vec2{point.x(), point.y()}, true);
            }
        }

        void applyImpulse(const QVector2D& impulse, const QVector2D& point) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_ApplyLinearImpulse(bodyId, b2Vec2{impulse.x(), impulse.y()}, b2Vec2{point.x(), point.y()}, true);
            }
        }

        void applyTorque(float torque) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_ApplyTorque(bodyId, torque, true);
            }
        }

        void setTransform(const QVector2D& pos, float angle) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_SetTransform(bodyId, b2Vec2{pos.x(), pos.y()}, b2MakeRot(angle));
            }
        }

        void setLinearDamping(float damping) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_SetLinearDamping(bodyId, damping);
            }
        }

        void setAngularDamping(float damping) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_SetAngularDamping(bodyId, damping);
            }
        }

        void setLinearVelocity(const QVector2D& vel) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_SetLinearVelocity(bodyId, b2Vec2{vel.x(), vel.y()});
            }
        }

        void setAngularVelocity(float vel) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_SetAngularVelocity(bodyId, vel);
            }
        }

        void setAwake(bool awake) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_SetAwake(bodyId, awake);
            }
        }

        void enableSleep(bool enabled) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_EnableSleep(bodyId, enabled);
            }
        }

        void setSleepThreshold(float threshold) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_SetSleepThreshold(bodyId, std::max(0.0f, threshold));
            }
        }

        void setContinuousCollision(bool enabled) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_SetBullet(bodyId, enabled);
            }
        }

        void simplifyCollisionMesh() {
            if (!b2Body_IsValid(bodyId) || collisionMeshSimplified_) return;
            const int shapeCount = b2Body_GetShapeCount(bodyId);
            if (shapeCount <= 0) return;
            std::vector<b2ShapeId> shapes(static_cast<std::size_t>(shapeCount));
            const int count = b2Body_GetShapes(bodyId, shapes.data(), shapeCount);
            for (int i = 0; i < count; ++i) {
                if (!b2Shape_IsValid(shapes[static_cast<std::size_t>(i)]) ||
                    b2Shape_GetType(shapes[static_cast<std::size_t>(i)]) != b2_polygonShape) {
                    continue;
                }
                const b2Polygon polygon = b2Shape_GetPolygon(shapes[static_cast<std::size_t>(i)]);
                if (polygon.count < 3) continue;
                float minX = polygon.vertices[0].x;
                float maxX = minX;
                float minY = polygon.vertices[0].y;
                float maxY = minY;
                for (int v = 1; v < polygon.count; ++v) {
                    minX = std::min(minX, polygon.vertices[v].x);
                    maxX = std::max(maxX, polygon.vertices[v].x);
                    minY = std::min(minY, polygon.vertices[v].y);
                    maxY = std::max(maxY, polygon.vertices[v].y);
                }
                const b2SurfaceMaterial material = b2Shape_GetSurfaceMaterial(shapes[static_cast<std::size_t>(i)]);
                const float density = b2Shape_GetDensity(shapes[static_cast<std::size_t>(i)]);
                collisionPolygonBackups_.push_back({polygon, material, density});
                b2ShapeDef shapeDef = b2DefaultShapeDef();
                shapeDef.density = density;
                shapeDef.material = material;
                b2DestroyShape(shapes[static_cast<std::size_t>(i)], false);
                const b2Polygon box = b2MakeOffsetBox(
                    std::max(0.001f, (maxX - minX) * 0.5f),
                    std::max(0.001f, (maxY - minY) * 0.5f),
                    b2Vec2{(minX + maxX) * 0.5f, (minY + maxY) * 0.5f},
                    b2MakeRot(0.0f));
                b2CreatePolygonShape(bodyId, &shapeDef, &box);
            }
            collisionMeshSimplified_ = true;
            b2Body_ApplyMassFromShapes(bodyId);
        }

        void restoreCollisionMesh() {
            if (!b2Body_IsValid(bodyId) || !collisionMeshSimplified_) return;
            const int shapeCount = b2Body_GetShapeCount(bodyId);
            std::vector<b2ShapeId> shapes(static_cast<std::size_t>(std::max(0, shapeCount)));
            const int count = shapeCount > 0 ? b2Body_GetShapes(bodyId, shapes.data(), shapeCount) : 0;
            for (int i = 0; i < count; ++i) {
                const auto shape = shapes[static_cast<std::size_t>(i)];
                if (b2Shape_IsValid(shape) && b2Shape_GetType(shape) == b2_polygonShape) {
                    b2DestroyShape(shape, false);
                }
            }
            for (const auto& backup : collisionPolygonBackups_) {
                b2ShapeDef shapeDef = b2DefaultShapeDef();
                shapeDef.density = backup.density;
                shapeDef.material = backup.material;
                b2CreatePolygonShape(bodyId, &shapeDef, &backup.polygon);
            }
            collisionPolygonBackups_.clear();
            collisionMeshSimplified_ = false;
            b2Body_ApplyMassFromShapes(bodyId);
        }

        void setType(Type newType) {
            if (!b2Body_IsValid(bodyId)) return;
            b2BodyType box2dType = b2_staticBody;
            switch (newType) {
            case Type::Static: box2dType = b2_staticBody; break;
            case Type::Kinematic: box2dType = b2_kinematicBody; break;
            case Type::Dynamic: box2dType = b2_dynamicBody; break;
            }
            b2Body_SetType(bodyId, box2dType);
        }

        void setFixedRotation(bool fixed) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_SetFixedRotation(bodyId, fixed);
            }
        }

        void addBoxCollider(float width, float height, float density = 1.0f) {
            if (!b2Body_IsValid(bodyId)) return;
            b2Polygon box = b2MakeBox(width * 0.5f, height * 0.5f);
            b2ShapeDef shapeDef = b2DefaultShapeDef();
            shapeDef.density = density;
            b2CreatePolygonShape(bodyId, &shapeDef, &box);
        }

        void addCircleCollider(float radius, float density = 1.0f) {
            if (!b2Body_IsValid(bodyId)) return;
            b2Circle circle = { {0.0f, 0.0f}, radius };
            b2ShapeDef shapeDef = b2DefaultShapeDef();
            shapeDef.density = density;
            b2CreateCircleShape(bodyId, &shapeDef, &circle);
        }

        b2BodyId getId() const { return bodyId; }

    private:
        struct CollisionPolygonBackup {
            b2Polygon polygon;
            b2SurfaceMaterial material;
            float density = 0.0f;
        };
        bool collisionMeshSimplified_ = false;
        std::vector<CollisionPolygonBackup> collisionPolygonBackups_;
    };

    // ─────────────────────────────────────────────────────────
    // Physics2DWorld
    // Box2D v3のb2WorldIdを管理し、シミュレーションを進める
    // ─────────────────────────────────────────────────────────
    class LIBRARY_DLL_API Physics2D {
    private:
        class Impl;
        Impl* impl_;

    public:
        Physics2D();
        ~Physics2D();

        // 重力の設定
        void setGravity(float gx, float gy);
        
        // シミュレーションを1ステップ進める (deltaTime = 1.0f/60.0f など)
        void step(float deltaTime, int subStepCount = 4);

        // 床(スタティックな壁)の追加
        void addStaticBox(float x, float y, float width, float height, float friction = 0.3f);

        // スタティックな円の追加
        void addStaticCircle(float x, float y, float radius, float friction = 0.3f);

        // ジョイント用の静的アンカー(プロキシ)追加
        SharedPtr<RigidBody2D> addStaticAnchor(float x, float y, float radius = 8.0f);

        // 動的な四角形(クローン等)の追加
        SharedPtr<RigidBody2D> addDynamicBox(float x, float y, float width, float height, float density = 1.0f, float friction = 0.3f, float restitution = 0.5f);
        
        // 動的な円の追加
        SharedPtr<RigidBody2D> addDynamicCircle(float x, float y, float radius, float density = 1.0f, float friction = 0.3f, float restitution = 0.5f);

        // 複雑な形状(ポリゴン)の追加
        SharedPtr<RigidBody2D> addPolygonBody(float x, float y, const std::vector<QVector2D>& vertices, bool isDynamic = true, float density = 1.0f, float friction = 0.3f, float restitution = 0.5f);

        // 生成済みのボディを取り除く
        void removeBody(const SharedPtr<RigidBody2D>& body);

        // ジョイントの追加
        b2JointId addDistanceJoint(SharedPtr<RigidBody2D> bodyA, SharedPtr<RigidBody2D> bodyB, float length, float damping = 0.5f, float stiffness = 1.0f);
        b2JointId addRevoluteJoint(SharedPtr<RigidBody2D> bodyA, SharedPtr<RigidBody2D> bodyB, QVector2D anchor);

        // 生成済みのジョイントを取り除く / 全破棄
        void removeJoint(b2JointId jointId);
        void clearJoints();

        // 登録されている全ジョイントを取得
        const std::vector<b2JointId>& getJoints() const;

        // 状態のリセット
        void clear();

        // 登録されている全ボディを取得
        std::vector<SharedPtr<RigidBody2D>> getBodies() const;
    };

}
