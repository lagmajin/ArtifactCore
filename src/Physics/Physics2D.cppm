module;
#include <box2d/box2d.h>
#include <vector>
#include <memory>

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
module Physics2D;

import Container.NamedVector;

namespace ArtifactCore {

    class Physics2D::Impl {
    public:
        b2Vec2 gravity;
        b2WorldId worldId;
        NamedVector<SharedPtr<RigidBody2D>> bodies;
        std::vector<b2JointId> joints;
        SharedPtr<RigidBody2D> floorBody;

        // Screen-space convention (+Y down), matching MPM/SoftBody gravity.
        Impl() : gravity{0.0f, 9.8f} {
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
        shapeDef.material.friction = friction;
        b2CreatePolygonShape(bodyId, &shapeDef, &box);
        auto rb = makeShared<RigidBody2D>();
        rb->bodyId = bodyId;
        impl_->bodies.add(rb);
    }

    void Physics2D::setStaticFloor(float topY, float width, float thickness, float friction) {
        if (!b2World_IsValid(impl_->worldId)) return;
        if (impl_->floorBody) {
            removeBody(impl_->floorBody);
            impl_->floorBody.reset();
        }

        const float safeWidth = std::max(1.0f, width);
        const float safeThickness = std::max(0.1f, thickness);
        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_staticBody;
        bodyDef.position = {0.0f, topY + safeThickness * 0.5f};
        const b2BodyId bodyId = b2CreateBody(impl_->worldId, &bodyDef);
        const b2Polygon box = b2MakeBox(safeWidth * 0.5f, safeThickness * 0.5f);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.material.friction = friction;
        b2CreatePolygonShape(bodyId, &shapeDef, &box);

        auto rb = makeShared<RigidBody2D>();
        rb->bodyId = bodyId;
        rb->cloneIndex = -3;
        impl_->bodies.add(rb);
        impl_->floorBody = rb;
    }

    void Physics2D::addStaticCircle(float x, float y, float radius, float friction) {
        if (!b2World_IsValid(impl_->worldId)) return;

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.position = {x, y};
        bodyDef.type = b2_staticBody;
        b2BodyId bodyId = b2CreateBody(impl_->worldId, &bodyDef);

        b2Circle circle = { {0.0f, 0.0f}, radius };
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.material.friction = friction;
        b2CreateCircleShape(bodyId, &shapeDef, &circle);
        auto rb = makeShared<RigidBody2D>();
        rb->bodyId = bodyId;
        impl_->bodies.add(rb);
    }

    SharedPtr<RigidBody2D> Physics2D::addStaticAnchor(float x, float y, float radius) {
        if (!b2World_IsValid(impl_->worldId)) return nullptr;

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.position = {x, y};
        bodyDef.type = b2_staticBody;
        b2BodyId bodyId = b2CreateBody(impl_->worldId, &bodyDef);

        b2Circle circle = { {0.0f, 0.0f}, std::max(0.5f, radius) };
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.material.friction = 0.6f;
        b2CreateCircleShape(bodyId, &shapeDef, &circle);
        auto rb = makeShared<RigidBody2D>();
        rb->bodyId = bodyId;
        impl_->bodies.add(rb);
        return rb;
    }

    SharedPtr<RigidBody2D> Physics2D::addDynamicBox(float x, float y, float width, float height, float density, float friction, float restitution) {
        if (!b2World_IsValid(impl_->worldId)) return nullptr;

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = {x, y};
        b2BodyId bodyId = b2CreateBody(impl_->worldId, &bodyDef);

        b2Polygon box = b2MakeBox(width / 2.0f, height / 2.0f);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = density;
        shapeDef.material.friction = friction;
        shapeDef.material.restitution = restitution;

        b2CreatePolygonShape(bodyId, &shapeDef, &box);

        auto rb = makeShared<RigidBody2D>();
        rb->bodyId = bodyId;
        impl_->bodies.add(rb);
        
        return rb;
    }

    SharedPtr<RigidBody2D> Physics2D::addDynamicCircle(float x, float y, float radius, float density, float friction, float restitution) {
        if (!b2World_IsValid(impl_->worldId)) return nullptr;

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = {x, y};
        b2BodyId bodyId = b2CreateBody(impl_->worldId, &bodyDef);

        b2Circle circle = { {0.0f, 0.0f}, radius };
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = density;
        shapeDef.material.friction = friction;
        shapeDef.material.restitution = restitution;

        b2CreateCircleShape(bodyId, &shapeDef, &circle);

        auto rb = makeShared<RigidBody2D>();
        rb->bodyId = bodyId;
        impl_->bodies.add(rb);
        
        return rb;
    }

    void Physics2D::removeBody(const SharedPtr<RigidBody2D>& body) {
        if (!body || !b2Body_IsValid(body->getId()) || !b2World_IsValid(impl_->worldId)) return;

        const b2BodyId bodyId = body->getId();
        b2DestroyBody(bodyId);
        impl_->bodies.removeIf(
            [&](const SharedPtr<RigidBody2D>& candidate) {
                return !candidate || candidate.get() == body.get();
            });
    }

    SharedPtr<RigidBody2D> Physics2D::addPolygonBody(float x, float y, const std::vector<QVector2D>& vertices, bool isDynamic, float density, float friction, float restitution) {
        if (!b2World_IsValid(impl_->worldId) || vertices.size() < 3) return nullptr;

        b2BodyDef bodyDef = b2DefaultBodyDef();
        bodyDef.type = isDynamic ? b2_dynamicBody : b2_staticBody;
        bodyDef.position = {x, y};
        b2BodyId bodyId = b2CreateBody(impl_->worldId, &bodyDef);

        std::vector<b2Vec2> b2verts;
        for (const auto& v : vertices) {
            b2verts.push_back({v.x(), v.y()});
        }

        b2Hull hull = b2ComputeHull(b2verts.data(), (int)b2verts.size());
        if (hull.count < 3) {
            b2DestroyBody(bodyId);
            return nullptr;
        }
        b2Polygon poly = b2MakePolygon(&hull, 0.01f); // Note: API changed - requires Hull
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = density;
        shapeDef.material.friction = friction;
        shapeDef.material.restitution = restitution;
        b2CreatePolygonShape(bodyId, &shapeDef, &poly);

        auto rb = makeShared<RigidBody2D>();
        rb->bodyId = bodyId;
        impl_->bodies.add(rb);
        return rb;
    }

    b2JointId Physics2D::addDistanceJoint(SharedPtr<RigidBody2D> bodyA, SharedPtr<RigidBody2D> bodyB, float length, float damping, float stiffness) {
        if (!b2Body_IsValid(bodyA->getId()) || !b2Body_IsValid(bodyB->getId())) return b2_nullJointId;

        b2DistanceJointDef jointDef = b2DefaultDistanceJointDef();
        jointDef.bodyIdA = bodyA->getId();
        jointDef.bodyIdB = bodyB->getId();
        jointDef.length = length;
        jointDef.dampingRatio = damping;
        jointDef.hertz = stiffness;

        const b2JointId jointId = b2CreateDistanceJoint(impl_->worldId, &jointDef);
        if (!B2_IS_NULL(jointId)) {
            impl_->joints.push_back(jointId);
        }
        return jointId;
    }

    void Physics2D::removeJoint(b2JointId jointId) {
        if (!b2World_IsValid(impl_->worldId) || B2_IS_NULL(jointId) ||
            !b2Joint_IsValid(jointId)) {
            return;
        }
        b2DestroyJoint(jointId);
        impl_->joints.erase(
            std::remove_if(impl_->joints.begin(), impl_->joints.end(),
                           [jointId](const b2JointId candidate) {
                               return b2StoreJointId(candidate) == b2StoreJointId(jointId);
                           }),
            impl_->joints.end());
    }

    void Physics2D::clearJoints() {
        if (!b2World_IsValid(impl_->worldId)) {
            impl_->joints.clear();
            return;
        }
        for (const b2JointId jointId : impl_->joints) {
            if (b2Joint_IsValid(jointId)) {
                b2DestroyJoint(jointId);
            }
        }
        impl_->joints.clear();
    }

    const std::vector<b2JointId>& Physics2D::getJoints() const {
        return impl_->joints;
    }

    b2JointId Physics2D::addRevoluteJoint(SharedPtr<RigidBody2D> bodyA, SharedPtr<RigidBody2D> bodyB, QVector2D anchor) {
        if (!b2Body_IsValid(bodyA->getId()) || !b2Body_IsValid(bodyB->getId())) return b2_nullJointId;

        b2RevoluteJointDef jointDef = b2DefaultRevoluteJointDef();
        jointDef.bodyIdA = bodyA->getId();
        jointDef.bodyIdB = bodyB->getId();
        jointDef.localAnchorA = b2Body_GetLocalPoint(bodyA->getId(), b2Vec2{anchor.x(), anchor.y()});
        jointDef.localAnchorB = b2Body_GetLocalPoint(bodyB->getId(), b2Vec2{anchor.x(), anchor.y()});

        const b2JointId jointId = b2CreateRevoluteJoint(impl_->worldId, &jointDef);
        if (!B2_IS_NULL(jointId)) {
            impl_->joints.push_back(jointId);
        }
        return jointId;
    }

    void Physics2D::clear() {
        clearJoints();
        impl_->floorBody.reset();
        impl_->bodies.clear();
        if (b2World_IsValid(impl_->worldId)) {
            b2DestroyWorld(impl_->worldId);
        }
        b2WorldDef worldDef = b2DefaultWorldDef();
        worldDef.gravity = impl_->gravity;
        impl_->worldId = b2CreateWorld(&worldDef);
    }

    std::vector<SharedPtr<RigidBody2D>> Physics2D::getBodies() const {
        return impl_->bodies.toStdVector();
    }

}
