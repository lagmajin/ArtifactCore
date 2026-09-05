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
        NamedVector<SharedPtr<RigidBody2D>> bodies{
            makeNamedVector<SharedPtr<RigidBody2D>>(ContainerName{"Physics2DBodies"})};
        std::vector<b2JointId> joints;
        struct LayerJoint { LayerID owner; b2JointId id; };
        NamedVector<LayerJoint> layerJoints{
            makeNamedVector<LayerJoint>(ContainerName{"Physics2DLayerJoints"})};
        NamedVector<LayerJoint> mouseJoints{
            makeNamedVector<LayerJoint>(ContainerName{"Physics2DMouseJoints"})};
        b2BodyId mouseAnchorBody = b2_nullBodyId;
        SharedPtr<RigidBody2D> floorBody;
        std::vector<PhysicsContactEvent> contactEvents;

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
        impl_->contactEvents.clear();
        if (!b2World_IsValid(impl_->worldId) || deltaTime <= 0.0f) return;
        b2World_Step(impl_->worldId, deltaTime, subStepCount);
        const auto ownerForShape = [this](b2ShapeId shapeId) {
            if (!b2Shape_IsValid(shapeId)) return LayerID{};
            const b2BodyId bodyId = b2Shape_GetBody(shapeId);
            for (const auto& body : impl_->bodies) {
                if (body && b2StoreBodyId(body->getId()) == b2StoreBodyId(bodyId) &&
                    body->cloneIndex == -1) return body->ownerLayerId;
            }
            return LayerID{};
        };
        const auto append = [&](PhysicsContactPhase phase, b2ShapeId shapeA,
                                b2ShapeId shapeB, b2Vec2 point = {},
                                b2Vec2 normal = {}, float speed = 0.0f) {
            PhysicsContactEvent event;
            event.phase = phase;
            event.firstLayerId = ownerForShape(shapeA);
            event.secondLayerId = ownerForShape(shapeB);
            if (event.firstLayerId.isNil() && event.secondLayerId.isNil()) return;
            event.point = {point.x, point.y};
            event.normal = {normal.x, normal.y};
            event.approachSpeed = std::max(0.0f, speed);
            impl_->contactEvents.push_back(std::move(event));
        };
        const b2ContactEvents events = b2World_GetContactEvents(impl_->worldId);
        for (int index = 0; index < events.beginCount; ++index) {
            const auto& event = events.beginEvents[index];
            append(PhysicsContactPhase::Begin, event.shapeIdA, event.shapeIdB);
        }
        for (int index = 0; index < events.endCount; ++index) {
            const auto& event = events.endEvents[index];
            append(PhysicsContactPhase::End, event.shapeIdA, event.shapeIdB);
        }
        for (int index = 0; index < events.hitCount; ++index) {
            const auto& event = events.hitEvents[index];
            append(PhysicsContactPhase::Hit, event.shapeIdA, event.shapeIdB,
                   event.point, event.normal, event.approachSpeed);
        }
    }

    std::vector<PhysicsContactEvent> Physics2D::takeContactEvents() {
        auto events = std::move(impl_->contactEvents);
        impl_->contactEvents.clear();
        return events;
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
        shapeDef.enableContactEvents = true;
        shapeDef.enableHitEvents = true;
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
        shapeDef.enableContactEvents = true;
        shapeDef.enableHitEvents = true;
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
        shapeDef.enableContactEvents = true;
        shapeDef.enableHitEvents = true;

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
        shapeDef.enableContactEvents = true;
        shapeDef.enableHitEvents = true;

        b2CreateCircleShape(bodyId, &shapeDef, &circle);

        auto rb = makeShared<RigidBody2D>();
        rb->bodyId = bodyId;
        impl_->bodies.add(rb);
        
        return rb;
    }

    void Physics2D::removeBody(const SharedPtr<RigidBody2D>& body) {
        if (!body || !b2Body_IsValid(body->getId()) || !b2World_IsValid(impl_->worldId)) return;
        if (b2StoreWorldId(b2Body_GetWorld(body->getId())) != b2StoreWorldId(impl_->worldId)) return;

        const b2BodyId bodyId = body->getId();
        b2DestroyBody(bodyId);
        body->bodyId = b2_nullBodyId;
        // Box2D destroys attached joints with the body. Drop their stale ids.
        std::erase_if(impl_->joints, [](b2JointId id) { return !b2Joint_IsValid(id); });
        impl_->layerJoints.removeIf(
            [](const Impl::LayerJoint& joint) { return !b2Joint_IsValid(joint.id); });
        impl_->mouseJoints.removeIf(
            [](const Impl::LayerJoint& joint) { return !b2Joint_IsValid(joint.id); });
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
        shapeDef.enableContactEvents = true;
        shapeDef.enableHitEvents = true;
        b2CreatePolygonShape(bodyId, &shapeDef, &poly);

        auto rb = makeShared<RigidBody2D>();
        rb->bodyId = bodyId;
        impl_->bodies.add(rb);
        return rb;
    }

    b2JointId Physics2D::addDistanceJoint(SharedPtr<RigidBody2D> bodyA, SharedPtr<RigidBody2D> bodyB, float length, float damping, float stiffness,
        bool spring, bool rope, QVector2D localAnchorA, QVector2D localAnchorB) {
        if (!bodyA || !bodyB || !b2Body_IsValid(bodyA->getId()) || !b2Body_IsValid(bodyB->getId())) return b2_nullJointId;
        if (bodyA == bodyB || b2StoreWorldId(b2Body_GetWorld(bodyA->getId())) != b2StoreWorldId(impl_->worldId) ||
            b2StoreWorldId(b2Body_GetWorld(bodyB->getId())) != b2StoreWorldId(impl_->worldId)) return b2_nullJointId;
        if (!std::isfinite(length) || !std::isfinite(damping) || !std::isfinite(stiffness) ||
            !std::isfinite(localAnchorA.x()) || !std::isfinite(localAnchorA.y()) ||
            !std::isfinite(localAnchorB.x()) || !std::isfinite(localAnchorB.y())) return b2_nullJointId;

        b2DistanceJointDef jointDef = b2DefaultDistanceJointDef();
        jointDef.bodyIdA = bodyA->getId();
        jointDef.bodyIdB = bodyB->getId();
        jointDef.localAnchorA = {localAnchorA.x(), localAnchorA.y()};
        jointDef.localAnchorB = {localAnchorB.x(), localAnchorB.y()};
        if (length <= 0.0f) {
            const auto a = b2Body_GetWorldPoint(bodyA->getId(), jointDef.localAnchorA);
            const auto b = b2Body_GetWorldPoint(bodyB->getId(), jointDef.localAnchorB);
            length = std::hypot(b.x - a.x, b.y - a.y);
        }
        jointDef.length = std::max(0.005f, length);
        jointDef.dampingRatio = std::clamp(damping, 0.0f, 10.0f);
        // Box2D requires enableSpring for limits. Zero hertz removes the
        // bilateral spring force, leaving only the rope's maximum length.
        jointDef.enableSpring = rope || (spring && stiffness > 0.0f);
        jointDef.hertz = rope ? 0.0f : std::clamp(stiffness, 0.0f, 120.0f);
        jointDef.enableLimit = rope;
        jointDef.minLength = 0.005f;
        jointDef.maxLength = jointDef.length;

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
        if (b2StoreWorldId(b2Joint_GetWorld(jointId)) != b2StoreWorldId(impl_->worldId)) return;
        b2DestroyJoint(jointId);
        impl_->joints.erase(
            std::remove_if(impl_->joints.begin(), impl_->joints.end(),
                           [jointId](const b2JointId candidate) {
                               return b2StoreJointId(candidate) == b2StoreJointId(jointId);
                           }),
            impl_->joints.end());
        impl_->layerJoints.removeIf(
            [jointId](const Impl::LayerJoint& candidate) {
                return b2StoreJointId(candidate.id) == b2StoreJointId(jointId);
            });
        impl_->mouseJoints.removeIf(
            [jointId](const Impl::LayerJoint& candidate) {
                return b2StoreJointId(candidate.id) == b2StoreJointId(jointId);
            });
    }

    void Physics2D::clearJoints() {
        impl_->layerJoints.clear();
        impl_->mouseJoints.clear();
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

    void Physics2D::setLayerJoint(LayerID owner, b2JointId jointId) {
        removeLayerJoint(owner);
        if (b2Joint_IsValid(jointId)) impl_->layerJoints.add(Impl::LayerJoint{owner, jointId});
    }

    bool Physics2D::hasLayerJoint(LayerID owner) const {
        for (const auto& joint : impl_->layerJoints) {
            if (joint.owner == owner && b2Joint_IsValid(joint.id)) return true;
        }
        return false;
    }

    void Physics2D::removeLayerJoint(LayerID owner) {
        for (const auto& joint : impl_->layerJoints) {
            if (joint.owner == owner) removeJoint(joint.id);
        }
        impl_->layerJoints.removeIf(
            [owner](const Impl::LayerJoint& joint) { return joint.owner == owner; });
    }

    float Physics2D::layerJointForce(LayerID owner) const {
        for (const auto& joint : impl_->layerJoints) {
            if (joint.owner != owner || !b2Joint_IsValid(joint.id)) continue;
            const b2Vec2 force = b2Joint_GetConstraintForce(joint.id);
            return std::hypot(force.x, force.y);
        }
        return 0.0f;
    }

    bool Physics2D::startMouseDrag(
        LayerID owner, SharedPtr<RigidBody2D> body, QVector2D target,
        float hertz, float dampingRatio, float maxForce) {
        if (!body || body->type() != RigidBody2D::Type::Dynamic ||
            !b2Body_IsValid(body->getId()) || !b2World_IsValid(impl_->worldId) ||
            !std::isfinite(target.x()) || !std::isfinite(target.y()) ||
            !std::isfinite(hertz) || !std::isfinite(dampingRatio) ||
            !std::isfinite(maxForce)) return false;
        if (b2StoreWorldId(b2Body_GetWorld(body->getId())) !=
            b2StoreWorldId(impl_->worldId)) return false;

        endMouseDrag(owner);
        if (!b2Body_IsValid(impl_->mouseAnchorBody)) {
            b2BodyDef anchorDef = b2DefaultBodyDef();
            anchorDef.type = b2_staticBody;
            impl_->mouseAnchorBody = b2CreateBody(impl_->worldId, &anchorDef);
        }
        if (!b2Body_IsValid(impl_->mouseAnchorBody)) return false;

        b2MouseJointDef jointDef = b2DefaultMouseJointDef();
        jointDef.bodyIdA = impl_->mouseAnchorBody;
        jointDef.bodyIdB = body->getId();
        jointDef.target = {target.x(), target.y()};
        jointDef.hertz = std::clamp(hertz, 0.1f, 60.0f);
        jointDef.dampingRatio = std::clamp(dampingRatio, 0.0f, 10.0f);
        jointDef.maxForce = std::max(1.0f, maxForce);
        const b2JointId jointId = b2CreateMouseJoint(impl_->worldId, &jointDef);
        if (B2_IS_NULL(jointId)) return false;
        impl_->joints.push_back(jointId);
        impl_->mouseJoints.add(Impl::LayerJoint{owner, jointId});
        body->setAwake(true);
        return true;
    }

    bool Physics2D::updateMouseDrag(LayerID owner, QVector2D target) {
        if (!std::isfinite(target.x()) || !std::isfinite(target.y())) return false;
        for (const auto& joint : impl_->mouseJoints) {
            if (joint.owner != owner || !b2Joint_IsValid(joint.id)) continue;
            b2MouseJoint_SetTarget(joint.id, {target.x(), target.y()});
            return true;
        }
        return false;
    }

    void Physics2D::endMouseDrag(LayerID owner) {
        std::vector<b2JointId> joints;
        for (const auto& joint : impl_->mouseJoints) {
            if (joint.owner == owner) joints.push_back(joint.id);
        }
        for (const b2JointId joint : joints) removeJoint(joint);
        impl_->mouseJoints.removeIf(
            [owner](const Impl::LayerJoint& joint) { return joint.owner == owner; });
    }

    bool Physics2D::hasMouseDrag(LayerID owner) const {
        for (const auto& joint : impl_->mouseJoints) {
            if (joint.owner == owner && b2Joint_IsValid(joint.id)) return true;
        }
        return false;
    }

    b2JointId Physics2D::addRevoluteJoint(SharedPtr<RigidBody2D> bodyA, SharedPtr<RigidBody2D> bodyB,
                                          QVector2D anchor, bool enableAngleLimit,
                                          float lowerAngleDegrees, float upperAngleDegrees) {
        if (!bodyA || !bodyB || bodyA == bodyB ||
            !b2Body_IsValid(bodyA->getId()) || !b2Body_IsValid(bodyB->getId())) return b2_nullJointId;
        if (b2StoreWorldId(b2Body_GetWorld(bodyA->getId())) != b2StoreWorldId(impl_->worldId) ||
            b2StoreWorldId(b2Body_GetWorld(bodyB->getId())) != b2StoreWorldId(impl_->worldId)) return b2_nullJointId;

        b2RevoluteJointDef jointDef = b2DefaultRevoluteJointDef();
        jointDef.bodyIdA = bodyA->getId();
        jointDef.bodyIdB = bodyB->getId();
        jointDef.localAnchorA = b2Body_GetLocalPoint(bodyA->getId(), b2Vec2{anchor.x(), anchor.y()});
        jointDef.localAnchorB = b2Body_GetLocalPoint(bodyB->getId(), b2Vec2{anchor.x(), anchor.y()});
        jointDef.enableLimit = enableAngleLimit;
        if (enableAngleLimit) {
            constexpr float kDegreesToRadians = 0.01745329251994329577f;
            jointDef.lowerAngle = std::min(lowerAngleDegrees, upperAngleDegrees) *
                                  kDegreesToRadians;
            jointDef.upperAngle = std::max(lowerAngleDegrees, upperAngleDegrees) *
                                  kDegreesToRadians;
        }

        const b2JointId jointId = b2CreateRevoluteJoint(impl_->worldId, &jointDef);
        if (!B2_IS_NULL(jointId)) {
            impl_->joints.push_back(jointId);
        }
        return jointId;
    }

    b2JointId Physics2D::addPrismaticJoint(
        SharedPtr<RigidBody2D> bodyA, SharedPtr<RigidBody2D> bodyB,
        QVector2D anchor, QVector2D axis, bool enableLimit, float lowerLimit,
        float upperLimit, bool enableMotor, float motorSpeed, float maxMotorForce) {
        if (!bodyA || !bodyB || bodyA == bodyB ||
            !b2Body_IsValid(bodyA->getId()) || !b2Body_IsValid(bodyB->getId())) return b2_nullJointId;
        if (b2StoreWorldId(b2Body_GetWorld(bodyA->getId())) != b2StoreWorldId(impl_->worldId) ||
            b2StoreWorldId(b2Body_GetWorld(bodyB->getId())) != b2StoreWorldId(impl_->worldId)) return b2_nullJointId;
        if (!std::isfinite(anchor.x()) || !std::isfinite(anchor.y()) ||
            !std::isfinite(axis.x()) || !std::isfinite(axis.y()) ||
            !std::isfinite(lowerLimit) || !std::isfinite(upperLimit) ||
            !std::isfinite(motorSpeed) || !std::isfinite(maxMotorForce)) return b2_nullJointId;
        if (axis.lengthSquared() < 0.000001f) axis = QVector2D(1.0f, 0.0f);
        else axis.normalize();
        b2PrismaticJointDef jointDef = b2DefaultPrismaticJointDef();
        jointDef.bodyIdA = bodyA->getId();
        jointDef.bodyIdB = bodyB->getId();
        jointDef.localAnchorA = b2Body_GetLocalPoint(bodyA->getId(), {anchor.x(), anchor.y()});
        jointDef.localAnchorB = b2Body_GetLocalPoint(bodyB->getId(), {anchor.x(), anchor.y()});
        jointDef.localAxisA = b2Body_GetLocalVector(bodyA->getId(), {axis.x(), axis.y()});
        jointDef.enableLimit = enableLimit;
        jointDef.lowerTranslation = std::min(lowerLimit, upperLimit);
        jointDef.upperTranslation = std::max(lowerLimit, upperLimit);
        jointDef.enableMotor = enableMotor && maxMotorForce > 0.0f;
        jointDef.motorSpeed = motorSpeed;
        jointDef.maxMotorForce = std::max(0.0f, maxMotorForce);
        const b2JointId jointId = b2CreatePrismaticJoint(impl_->worldId, &jointDef);
        if (!B2_IS_NULL(jointId)) impl_->joints.push_back(jointId);
        return jointId;
    }

    void Physics2D::clear() {
        clearJoints();
        impl_->contactEvents.clear();
        for (const auto& body : impl_->bodies) {
            if (body) body->bodyId = b2_nullBodyId;
        }
        impl_->floorBody.reset();
        impl_->mouseAnchorBody = b2_nullBodyId;
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
