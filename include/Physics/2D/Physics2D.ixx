module;
#include <box2d/box2d.h>
#include <memory>
#include <vector>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>
#include "../../Define/DllExportMacro.hpp"

export module Physics2D;

import std;

export namespace ArtifactCore {

    // ─────────────────────────────────────────────────────────
    // RigidBody2D
    // Box2D v3のb2BodyIdをラップし、MoGraphデータと同期するためのクラス
    // ─────────────────────────────────────────────────────────
    class RigidBody2D {
    public:
        b2BodyId bodyId;
        int cloneIndex = -1; // MoGraphの何番目のクローンとリンクしているか
        
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

        void applyForce(const QVector2D& force, const QVector2D& point) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_ApplyForce(bodyId, b2Vec2{force.x(), force.y()}, b2Vec2{point.x(), point.y()}, true);
            }
        }

        void setLinearVelocity(const QVector2D& vel) {
            if (b2Body_IsValid(bodyId)) {
                b2Body_SetLinearVelocity(bodyId, b2Vec2{vel.x(), vel.y()});
            }
        }
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

        // 動的な四角形(クローン等)の追加
        std::shared_ptr<RigidBody2D> addDynamicBox(float x, float y, float width, float height, float density = 1.0f, float friction = 0.3f, float restitution = 0.5f);
        
        // 動的な円の追加
        std::shared_ptr<RigidBody2D> addDynamicCircle(float x, float y, float radius, float density = 1.0f, float friction = 0.3f, float restitution = 0.5f);

        // 状態のリセット
        void clear();

        // 登録されている全ボディを取得
        std::vector<std::shared_ptr<RigidBody2D>> getBodies() const;
    };

}
