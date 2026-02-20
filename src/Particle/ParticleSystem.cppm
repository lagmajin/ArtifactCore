module;
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

module Particle.System;

import Particle;

namespace ArtifactCore {

// ============================================================================
// ParticleEmitter Implementation
// ============================================================================
ParticleEmitter::ParticleEmitter() = default;

void ParticleEmitter::initParticle(Particle& p) {
    std::uniform_real_distribution<float> lifetimeDist(config_.lifetimeMin, config_.lifetimeMax);
    std::uniform_real_distribution<float> speedDist(config_.speedMin, config_.speedMax);
    std::uniform_real_distribution<float> sizeDist(config_.sizeMin, config_.sizeMax);
    std::uniform_int_distribution<uint32_t> seedDist;
    
    p.lifetime = lifetimeDist(rng_);
    p.age = 0.0f;
    p.size = sizeDist(rng_);
    p.seed = seedDist(rng_);
    
    // Position based on shape
    switch (shape_) {
        case Shape::Point:
            p.position = position_;
            break;
        case Shape::Sphere: {
            std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);
            std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);
            float theta = angleDist(rng_);
            float phi = angleDist(rng_);
            float r = radiusDist(rng_);
            p.position.x = position_.x + r * std::sin(phi) * std::cos(theta);
            p.position.y = position_.y + r * std::sin(phi) * std::sin(theta);
            p.position.z = position_.z + r * std::cos(phi);
            break;
        }
        case Shape::Box: {
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            p.position.x = position_.x + dist(rng_);
            p.position.y = position_.y + dist(rng_);
            p.position.z = position_.z + dist(rng_);
            break;
        }
        case Shape::Circle: {
            std::uniform_real_distribution<float> angleDist(0.0f, 6.28318f);
            std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);
            float angle = angleDist(rng_);
            float r = radiusDist(rng_);
            p.position.x = position_.x + r * std::cos(angle);
            p.position.y = position_.y;
            p.position.z = position_.z + r * std::sin(angle);
            break;
        }
        default:
            p.position = position_;
    }
    
    // Velocity based on direction and spread
    float3 dir = randomDirection();
    float speed = speedDist(rng_);
    p.velocity.x = dir.x * speed;
    p.velocity.y = dir.y * speed;
    p.velocity.z = dir.z * speed;
    
    // Color
    p.color = config_.colorStart;
    
    // Reset other properties
    p.acceleration = {0.0f, 0.0f, 0.0f};
    p.rotation = {0.0f, 0.0f, 0.0f};
    p.angularVelocity = {0.0f, 0.0f, 0.0f};
    p.scale = {1.0f, 1.0f};
    p.mass = 1.0f;
    p.drag = 0.0f;
    p.opacity = 1.0f;
}

float3 ParticleEmitter::randomDirection() const {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    // Base direction
    float3 dir = config_.direction;
    
    // Apply spread
    if (config_.spreadAngle > 0.0f) {
        float spreadX = dist(rng_) * config_.spreadAngle;
        float spreadY = dist(rng_) * config_.spreadAngle;
        
        // Simple spread (could be improved with proper rotation)
        dir.x += spreadX;
        dir.y += spreadY;
        
        // Normalize
        float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
        if (len > 0.0f) {
            dir.x /= len;
            dir.y /= len;
            dir.z /= len;
        }
    }
    
    return dir;
}

void ParticleEmitter::emit(ParticlePool<>& pool, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        size_t idx = pool.spawn();
        initParticle(pool[idx]);
    }
}

void ParticleEmitter::update(double dt, ParticlePool<>& pool) {
    if (!enabled_) return;
    
    accumulator_ += dt;
    double interval = 1.0 / config_.rate;
    
    while (accumulator_ >= interval) {
        emit(pool, 1);
        accumulator_ -= interval;
    }
}

// ============================================================================
// ParticleSystem::Impl
// ============================================================================
class ParticleSystem::Impl {
public:
    ParticlePool<> pool_;
    std::vector<std::shared_ptr<ParticleEmitter>> emitters_;
    std::vector<std::shared_ptr<ForceField>> forceFields_;
    std::vector<std::shared_ptr<ParticleCollider>> colliders_;
    
    size_t maxParticles_ = 100000;
    double simulationSpeed_ = 1.0;
    bool paused_ = false;
    
    Statistics stats_;
    
    Impl() {
        pool_.reserve(maxParticles_);
    }
};

// ============================================================================
// ParticleSystem Implementation
// ============================================================================
ParticleSystem::ParticleSystem() : impl_(new Impl()) {}

ParticleSystem::~ParticleSystem() { delete impl_; }

ParticleSystem::ParticleSystem(ParticleSystem&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

ParticleSystem& ParticleSystem::operator=(ParticleSystem&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

void ParticleSystem::addEmitter(std::shared_ptr<ParticleEmitter> emitter) {
    impl_->emitters_.push_back(emitter);
}

void ParticleSystem::removeEmitter(const std::string& id) {
    impl_->emitters_.erase(
        std::remove_if(impl_->emitters_.begin(), impl_->emitters_.end(),
            [&id](const auto& e) { return e->getId() == id; }),
        impl_->emitters_.end());
}

void ParticleSystem::clearEmitters() { impl_->emitters_.clear(); }
size_t ParticleSystem::emitterCount() const { return impl_->emitters_.size(); }

void ParticleSystem::addForceField(std::shared_ptr<ForceField> field) {
    impl_->forceFields_.push_back(field);
}

void ParticleSystem::removeForceField(const std::string& id) {
    impl_->forceFields_.erase(
        std::remove_if(impl_->forceFields_.begin(), impl_->forceFields_.end(),
            [&id](const auto& f) { return f->getId() == id; }),
        impl_->forceFields_.end());
}

void ParticleSystem::clearForceFields() { impl_->forceFields_.clear(); }
size_t ParticleSystem::forceFieldCount() const { return impl_->forceFields_.size(); }

void ParticleSystem::addCollider(std::shared_ptr<ParticleCollider> collider) {
    impl_->colliders_.push_back(collider);
}

void ParticleSystem::removeCollider(const std::string& id) {
    impl_->colliders_.erase(
        std::remove_if(impl_->colliders_.begin(), impl_->colliders_.end(),
            [&id](const auto& c) { return c->getId() == id; }),
        impl_->colliders_.end());
}

void ParticleSystem::clearColliders() { impl_->colliders_.clear(); }
size_t ParticleSystem::colliderCount() const { return impl_->colliders_.size(); }

void ParticleSystem::update(double deltaTime) {
    if (paused_) return;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    double dt = deltaTime * simulationSpeed_;
    
    impl_->stats_.spawnedThisFrame = 0;
    impl_->stats_.killedThisFrame = 0;
    
    // Emit new particles
    for (auto& emitter : impl_->emitters_) {
        if (emitter->isEnabled()) {
            emitter->update(dt, impl_->pool_);
        }
    }
    
    // Get free indices for active particle iteration
    std::vector<bool> isFree(impl_->pool_.size(), false);
    for (size_t idx : impl_->pool_.getFreeIndices()) {
        isFree[idx] = true;
    }
    
    // Update each particle
    for (size_t i = 0; i < impl_->pool_.size(); ++i) {
        if (isFree[i]) continue;
        
        Particle& p = impl_->pool_[i];
        
        // Update age
        p.age += static_cast<float>(dt);
        
        // Kill if expired
        if (p.age >= p.lifetime) {
            impl_->pool_.kill(i);
            impl_->stats_.killedThisFrame++;
            continue;
        }
        
        // Apply forces
        for (auto& field : impl_->forceFields_) {
            if (field->isEnabled()) {
                field->apply(p, dt);
            }
        }
        
        // Integrate velocity
        p.velocity.x += p.acceleration.x * static_cast<float>(dt);
        p.velocity.y += p.acceleration.y * static_cast<float>(dt);
        p.velocity.z += p.acceleration.z * static_cast<float>(dt);
        
        // Integrate position
        p.position.x += p.velocity.x * static_cast<float>(dt);
        p.position.y += p.velocity.y * static_cast<float>(dt);
        p.position.z += p.velocity.z * static_cast<float>(dt);
        
        // Integrate rotation
        p.rotation.x += p.angularVelocity.x * static_cast<float>(dt);
        p.rotation.y += p.angularVelocity.y * static_cast<float>(dt);
        p.rotation.z += p.angularVelocity.z * static_cast<float>(dt);
        
        // Collision detection
        for (auto& collider : impl_->colliders_) {
            collider->collide(p, dt);
        }
        
        // Update opacity based on lifetime
        float lifeRatio = p.age / p.lifetime;
        p.opacity = 1.0f - lifeRatio;
        
        // Interpolate color
        float t = lifeRatio;
        p.color.x = config_.colorStart.x + (config_.colorEnd.x - config_.colorStart.x) * t;
        p.color.y = config_.colorStart.y + (config_.colorEnd.y - config_.colorStart.y) * t;
        p.color.z = config_.colorStart.z + (config_.colorEnd.z - config_.colorStart.z) * t;
        p.color.w = config_.colorStart.w + (config_.colorEnd.w - config_.colorStart.w) * t;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    impl_->stats_.simulationTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    impl_->stats_.activeParticles = activeParticleCount();
    impl_->stats_.totalParticles = totalParticleCount();
}

void ParticleSystem::reset() {
    impl_->pool_.clear();
    impl_->stats_ = Statistics{};
}

size_t ParticleSystem::activeParticleCount() const {
    return impl_->pool_.activeCount();
}

size_t ParticleSystem::totalParticleCount() const {
    return impl_->pool_.size();
}

ParticlePool<>& ParticleSystem::getPool() {
    return impl_->pool_;
}

void ParticleSystem::setMaxParticles(size_t maxParticles) {
    impl_->maxParticles_ = maxParticles;
    impl_->pool_.reserve(maxParticles);
}

size_t ParticleSystem::maxParticles() const { return impl_->maxParticles_; }

void ParticleSystem::setSimulationSpeed(double speed) { impl_->simulationSpeed_ = speed; }
double ParticleSystem::simulationSpeed() const { return impl_->simulationSpeed_; }

void ParticleSystem::setPaused(bool paused) { impl_->paused_ = paused; }
bool ParticleSystem::isPaused() const { return impl_->paused_; }

ParticleSystem::Statistics ParticleSystem::getStatistics() const { return impl_->stats_; }

} // namespace ArtifactCore
