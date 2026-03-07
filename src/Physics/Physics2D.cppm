module;
#include <box2d/box2d.h>
#include <vector>
#include <memory>

module Physics2D;

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>




namespace ArtifactCore {

    class Physics2D::Impl {
    public:
        b2Vec2 gravity;
        b2WorldId worldId;
        std::vector<std::shared_ptr<RigidBody2D>> bodies;

        Impl() : gravity{0.0f, -9.8f} {
            b2WorldDef worldDef = b2DefaultWorldDef();
            worldDef.gravity = gravity;
            worldId = b2CreateWorld(&worldDef);
        }

        ~Impl() {
            if (b2World_IsValid(worldId)) {
                b2DestroyWorld(worldId);
            }
        }
    };

    Physics2D::Physics2D() : impl_(new Impl()) {}

    Physics2D::~Physics2D() {
        delete impl_;
    }

    void Physics2D::setGravity(float gx, float gy) {
        impl_->gravity = {gx, gy};
        if (b2World_IsValid(impl_->worldId)) {
            b2World_SetGravity(impl_->worldId, impl_->gravity);
        }
    }

    void Physics2D::step(float deltaTime, int subStepCount) {
        if (b2World_IsValid(impl_->worldId) && deltaTime > 0.0f) {
            b2World_Step(impl_->worldId, deltaTime, subStepCount);
        }
    }

    void Physics2D::addStaticBox(float x, float y, float width, float height, float friction) {
        if (!b2World_IsValid(impl_->worldId)) return;

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.position = {x, y};
        bodyDef.type = b2_staticBody;
        b2BodyId bodyId = b2CreateBody(impl_->worldId, &bodyDef);

        b2Polygon box = b2MakeBox(width / 2.0f, height / 2.0f);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.friction = friction;
        b2CreatePolygonShape(bodyId, &shapeDef, &box);
    }

    std::shared_ptr<RigidBody2D> Physics2D::addDynamicBox(float x, float y, float width, float height, float density, float friction, float restitution) {
        if (!b2World_IsValid(impl_->worldId)) return nullptr;

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = {x, y};
        b2BodyId bodyId = b2CreateBody(impl_->worldId, &bodyDef);

        b2Polygon box = b2MakeBox(width / 2.0f, height / 2.0f);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = density;
        shapeDef.friction = friction;
        shapeDef.restitution = restitution; // Bounciness

        b2CreatePolygonShape(bodyId, &shapeDef, &box);

        auto rb = std::make_shared<RigidBody2D>();
        rb->bodyId = bodyId;
        impl_->bodies.push_back(rb);
        
        return rb;
    }

    std::shared_ptr<RigidBody2D> Physics2D::addDynamicCircle(float x, float y, float radius, float density, float friction, float restitution) {
        if (!b2World_IsValid(impl_->worldId)) return nullptr;

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = {x, y};
        b2BodyId bodyId = b2CreateBody(impl_->worldId, &bodyDef);

        b2Circle circle = { {0.0f, 0.0f}, radius };
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = density;
        shapeDef.friction = friction;
        shapeDef.restitution = restitution;

        b2CreateCircleShape(bodyId, &shapeDef, &circle);

        auto rb = std::make_shared<RigidBody2D>();
        rb->bodyId = bodyId;
        impl_->bodies.push_back(rb);
        
        return rb;
    }

    std::shared_ptr<RigidBody2D> Physics2D::addPolygonBody(float x, float y, const std::vector<QVector2D>& vertices, bool isDynamic, float density) {
        if (!b2World_IsValid(impl_->worldId) || vertices.size() < 3) return nullptr;

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = isDynamic ? b2_dynamicBody : b2_staticBody;
        bodyDef.position = {x, y};
        b2BodyId bodyId = b2CreateBody(impl_->worldId, &bodyDef);

        std::vector<b2Vec2> b2verts;
        for (const auto& v : vertices) {
            b2verts.push_back({v.x(), v.y()});
        }

        b2Polygon poly = b2MakePolygon(b2verts.data(), (int)b2verts.size(), 0.1f);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = density;
        b2CreatePolygonShape(bodyId, &shapeDef, &poly);

        auto rb = std::make_shared<RigidBody2D>();
        rb->bodyId = bodyId;
        impl_->bodies.push_back(rb);
        return rb;
    }

    b2JointId Physics2D::addDistanceJoint(std::shared_ptr<RigidBody2D> bodyA, std::shared_ptr<RigidBody2D> bodyB, float length, float damping, float stiffness) {
        if (!b2Body_IsValid(bodyA->getId()) || !b2Body_IsValid(bodyB->getId())) return b2_nullJointId;

        b2DistanceJointDef jointDef = b2DefaultDistanceJointDef();
        jointDef.bodyIdA = bodyA->getId();
        jointDef.bodyIdB = bodyB->getId();
        jointDef.length = length;
        jointDef.dampingRatio = damping;
        jointDef.hertz = stiffness;

        return b2CreateDistanceJoint(impl_->worldId, &jointDef);
    }

    b2JointId Physics2D::addRevoluteJoint(std::shared_ptr<RigidBody2D> bodyA, std::shared_ptr<RigidBody2D> bodyB, QVector2D anchor) {
        if (!b2Body_IsValid(bodyA->getId()) || !b2Body_IsValid(bodyB->getId())) return b2_nullJointId;

        b2RevoluteJointDef jointDef = b2DefaultRevoluteJointDef();
        jointDef.bodyIdA = bodyA->getId();
        jointDef.bodyIdB = bodyB->getId();
        jointDef.localAnchorA = b2Body_GetLocalPoint(bodyA->getId(), b2Vec2{anchor.x(), anchor.y()});
        jointDef.localAnchorB = b2Body_GetLocalPoint(bodyB->getId(), b2Vec2{anchor.x(), anchor.y()});

        return b2CreateRevoluteJoint(impl_->worldId, &jointDef);
    }

    void Physics2D::clear() {
        impl_->bodies.clear();
        if (b2World_IsValid(impl_->worldId)) {
            b2DestroyWorld(impl_->worldId);
        }
        b2WorldDef worldDef = b2DefaultWorldDef();
        worldDef.gravity = impl_->gravity;
        impl_->worldId = b2CreateWorld(&worldDef);
    }

    std::vector<std::shared_ptr<RigidBody2D>> Physics2D::getBodies() const {
        return impl_->bodies;
    }

}
