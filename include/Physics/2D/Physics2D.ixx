module;
#include <box2d/box2d.h>
#include <memory>
#include <vector>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>
#include "../Define/DllExportMacro.hpp"

export module Physics2D;

import std;

export namespace ArtifactCore {

    // ─────────────────────────────────────────────────────────
    // RigidBody2D
    // Box2Dのb2Bodyをラップし、MoGraphデータと同期するためのクラス
    // ─────────────────────────────────────────────────────────
    class RigidBody2D {
    public:
        b2Body* body = nullptr;
        int cloneIndex = -1; // MoGraphの何番目のクローンとリンクしているか
        
        QVector2D position() const {
            if (!body) return {0,0};
            auto pos = body->GetPosition();
            return {pos.x, pos.y};
        }
        
        float angle() const {
            if (!body) return 0.0f;
            return body->GetAngle(); // radians
        }

        void applyForce(const QVector2D& force, const QVector2D& point) {
            if (body) {
                body->ApplyForce(b2Vec2(force.x(), force.y()), b2Vec2(point.x(), point.y()), true);
            }
        }

        void setLinearVelocity(const QVector2D& vel) {
            if (body) {
                body->SetLinearVelocity(b2Vec2(vel.x(), vel.y()));
            }
        }
    };

    // ─────────────────────────────────────────────────────────
    // Physics2DWorld
    // Box2Dのb2Worldを管理し、シミュレーションを進める
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
        void step(float deltaTime, int velocityIterations = 8, int positionIterations = 3);

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
