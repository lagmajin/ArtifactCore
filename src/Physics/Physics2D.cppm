module;
#include <box2d/box2d.h>
#include <vector>
#include <memory>

module Physics2D;

import std;

namespace ArtifactCore {

    class Physics2D::Impl {
    public:
        b2Vec2 gravity;
        std::unique_ptr<b2World> world;
        std::vector<std::shared_ptr<RigidBody2D>> bodies;

        Impl() : gravity(0.0f, -9.8f) {
            world = std::make_unique<b2World>(gravity);
        }

        ~Impl() {
            world.reset();
        }
    };

    Physics2D::Physics2D() : impl_(new Impl()) {}

    Physics2D::~Physics2D() {
        delete impl_;
    }

    void Physics2D::setGravity(float gx, float gy) {
        impl_->gravity.Set(gx, gy);
        if (impl_->world) {
            impl_->world->SetGravity(impl_->gravity);
        }
    }

    void Physics2D::step(float deltaTime, int velocityIterations, int positionIterations) {
        if (impl_->world && deltaTime > 0.0f) {
            impl_->world->Step(deltaTime, velocityIterations, positionIterations);
        }
    }

    void Physics2D::addStaticBox(float x, float y, float width, float height, float friction) {
        if (!impl_->world) return;

        b2BodyDef groundBodyDef;
        groundBodyDef.position.Set(x, y);
        b2Body* groundBody = impl_->world->CreateBody(&groundBodyDef);

        b2PolygonShape groundBox;
        groundBox.SetAsBox(width / 2.0f, height / 2.0f);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &groundBox;
        fixtureDef.friction = friction;
        groundBody->CreateFixture(&fixtureDef);
    }

    std::shared_ptr<RigidBody2D> Physics2D::addDynamicBox(float x, float y, float width, float height, float density, float friction, float restitution) {
        if (!impl_->world) return nullptr;

        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position.Set(x, y);
        b2Body* body = impl_->world->CreateBody(&bodyDef);

        b2PolygonShape dynamicBox;
        dynamicBox.SetAsBox(width / 2.0f, height / 2.0f);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &dynamicBox;
        fixtureDef.density = density;
        fixtureDef.friction = friction;
        fixtureDef.restitution = restitution; // Bounciness

        body->CreateFixture(&fixtureDef);

        auto rb = std::make_shared<RigidBody2D>();
        rb->body = body;
        impl_->bodies.push_back(rb);
        
        return rb;
    }

    std::shared_ptr<RigidBody2D> Physics2D::addDynamicCircle(float x, float y, float radius, float density, float friction, float restitution) {
        if (!impl_->world) return nullptr;

        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position.Set(x, y);
        b2Body* body = impl_->world->CreateBody(&bodyDef);

        b2CircleShape dynamicCircle;
        dynamicCircle.m_radius = radius;

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &dynamicCircle;
        fixtureDef.density = density;
        fixtureDef.friction = friction;
        fixtureDef.restitution = restitution;

        body->CreateFixture(&fixtureDef);

        auto rb = std::make_shared<RigidBody2D>();
        rb->body = body;
        impl_->bodies.push_back(rb);
        
        return rb;
    }

    void Physics2D::clear() {
        impl_->bodies.clear();
        impl_->world = std::make_unique<b2World>(impl_->gravity);
    }

    std::vector<std::shared_ptr<RigidBody2D>> Physics2D::getBodies() const {
        return impl_->bodies;
    }

}
