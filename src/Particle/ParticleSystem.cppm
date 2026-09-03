module;
#include <utility>
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <random>

#include <QList>

module Particle.System;

import Container.NamedVector;

import Particle;
import Mesh;
import Math.Noise;


namespace ArtifactCore {

// ============================================================================
// ForceField Implementation
// ============================================================================
float ForceField::computeFalloff(const float3& p) const {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return 0.0f;
    if (shape_ == FieldShape::Infinite) return 1.0f;

    float dist = 0.0f;
    if (shape_ == FieldShape::Sphere) {
        float3 diff{p.x - center_.x, p.y - center_.y, p.z - center_.z};
        dist = std::sqrt(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
        if (dist > radius_) return 0.0f;
        if (feather_ <= 0.0f) return dist < radius_ ? 1.0f : 0.0f;
        if (dist < radius_ - feather_) return 1.0f;
    } else if (shape_ == FieldShape::Box) {
        float3 diff{std::abs(p.x - center_.x), std::abs(p.y - center_.y), std::abs(p.z - center_.z)};
        if (diff.x > size_.x || diff.y > size_.y || diff.z > size_.z) return 0.0f;
        // Simplified box feather
        float distToEdge = std::min({size_.x - diff.x, size_.y - diff.y, size_.z - diff.z});
        if (distToEdge > feather_) return 1.0f;
        if (feather_ <= 0.0f) return distToEdge <= 0.0f ? 1.0f : 0.0f;
        dist = radius_ - distToEdge; // Remap for falloff calculation
    }

    // Apply falloff curve
    float t = 0.0f;
    if (shape_ == FieldShape::Sphere) {
        t = (radius_ - dist) / feather_;
    } else {
        // Box falloff logic
        float distToEdge = std::min({size_.x - std::abs(p.x - center_.x), size_.y - std::abs(p.y - center_.y), size_.z - std::abs(p.z - center_.z)});
        t = std::clamp(distToEdge / feather_, 0.0f, 1.0f);
    }
    
    t = std::clamp(t, 0.0f, 1.0f);

    switch (falloff_) {
        case FalloffType::Linear: return t;
        case FalloffType::Quadratic: return t * t;
        case FalloffType::Smooth: return t * t * (3.0f - 2.0f * t);
        case FalloffType::Constant:
        default: return 1.0f;
    }
}

// ============================================================================
// ParticleEmitter Implementation
// ============================================================================
ParticleEmitter::ParticleEmitter() = default;

void ParticleEmitter::initParticle(Particle& p) {
    const auto normalizedRange = [](float minimum, float maximum,
                                    float fallbackMinimum, float fallbackMaximum) {
        if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
            return std::pair<float, float>{fallbackMinimum, fallbackMaximum};
        }
        minimum = std::max(0.0f, minimum);
        maximum = std::max(0.0f, maximum);
        if (minimum > maximum) std::swap(minimum, maximum);
        return std::pair<float, float>{minimum, maximum};
    };
    const auto lifetime = normalizedRange(config_.lifetimeMin, config_.lifetimeMax, 1.0f, 2.0f);
    const auto speed = normalizedRange(config_.speedMin, config_.speedMax, 1.0f, 2.0f);
    const auto size = normalizedRange(config_.sizeMin, config_.sizeMax, 1.0f, 1.0f);
    std::uniform_real_distribution<float> lifetimeDist(lifetime.first, lifetime.second);
    std::uniform_real_distribution<float> speedDist(speed.first, speed.second);
    std::uniform_real_distribution<float> sizeDist(size.first, size.second);
    std::uniform_int_distribution<uint32_t> seedDist;
    
    p.lifetime = lifetimeDist(rng_);
    p.age = 0.0f;
    p.size = sizeDist(rng_);
    p.seed = seedDist(rng_);
    
    // Position based on shape
    switch (shape_) {
        case Shape::Point:
            p.position = position_;
            p.prevPosition = position_;
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
        case Shape::Mesh: {
            if (meshSource_ && meshSource_->vertexCount() > 0) {
                std::uniform_int_distribution<int> vDist(0, meshSource_->vertexCount() - 1);
                int vIdx = vDist(rng_);
                auto posAttr = meshSource_->vertexAttributes().get<QVector3D>("position");
                if (posAttr) {
                    QVector3D vPos = (*posAttr)[vIdx];
                    p.position = {vPos.x(), vPos.y(), vPos.z()};
                }
            }
            break;
        }
        case Shape::Layer: {
            if (layerPositionSampler_) {
                std::uniform_real_distribution<float> uvDist(0.0f, 1.0f);
                const float3 sampled = layerPositionSampler_(uvDist(rng_), uvDist(rng_));
                if (std::isfinite(sampled.x) && std::isfinite(sampled.y) &&
                    std::isfinite(sampled.z)) {
                    p.position = sampled;
                } else {
                    p.position = position_;
                }
            } else {
                p.position = position_;
            }
            break;
        }
        default:
            p.position = position_;
            p.prevPosition = position_;
    }
    
    // 他の形状でも初期化
    p.prevPosition = p.position;
    
    // Velocity based on direction and spread
    float3 dir = randomDirection();
    std::uniform_real_distribution<float> velocitySpeedDist(speed.first, speed.second);
    const float velocitySpeed = velocitySpeedDist(rng_);
    p.velocity.x = dir.x * velocitySpeed;
    p.velocity.y = dir.y * velocitySpeed;
    p.velocity.z = dir.z * velocitySpeed;
    
    // Color
    p.color = config_.colorStart;
    
    // Reset other properties
    p.acceleration = {0.0f, 0.0f, 0.0f};
    p.rotation = {0.0f, 0.0f, 0.0f};
    p.angularVelocity = {0.0f, 0.0f, 0.0f};
    p.scale = {1.0f, 1.0f};
    p.mass = 1.0f;
    p.drag = 0.0f;
    p.opacity = config_.startOpacity;
    p.textureIndex = config_.textureIndex;
    p.blendMode = config_.blendMode;
    p.lastSubEmitAge = 0.0f;

    // Sub-emitters: Birth trigger
    for (const auto& sub : subEmitters_) {
        if (sub.trigger == SubEmitterConfig::Trigger::Birth) {
            // ここで即座に親パーティクルの位置から子を発生させる
            // Note: 実際の実装では pool.spawn() を呼ぶ必要があるため、
            // emit() 内での呼び出しか、遅延フラグを立てる設計が一般的
        }
    }
}

float3 ParticleEmitter::randomDirection() const {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    // Base direction
    float3 dir{
        std::isfinite(config_.direction.x) ? config_.direction.x : 0.0f,
        std::isfinite(config_.direction.y) ? config_.direction.y : 1.0f,
        std::isfinite(config_.direction.z) ? config_.direction.z : 0.0f};
    
    // Apply spread
    const float spreadAngle = std::isfinite(config_.spreadAngle)
        ? std::clamp(config_.spreadAngle, 0.0f, 3.14159265f)
        : 0.0f;
    if (spreadAngle > 0.0f) {
        float spreadX = dist(rng_) * spreadAngle;
        float spreadY = dist(rng_) * spreadAngle;
        
        // Simple spread (could be improved with proper rotation)
        dir.x += spreadX;
        dir.y += spreadY;
        
    }

    const float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if (std::isfinite(len) && len > 0.000001f) {
        dir.x /= len;
        dir.y /= len;
        dir.z /= len;
    } else {
        dir = {0.0f, 1.0f, 0.0f};
    }
    
    return dir;
}

void ParticleEmitter::_emit(ParticlePool<>& pool, size_t count, size_t maxParticles) {
    for (size_t i = 0; i < count; ++i) {
        if (maxParticles != 0 && pool.activeCount() >= maxParticles) break;
        size_t idx = pool.spawn();
        initParticle(pool[idx]);
        pool[idx].emitterToken = emitterToken();

        const Particle parent = pool[idx];
        for (const auto& sub : subEmitters_) {
            if (sub.trigger != SubEmitterConfig::Trigger::Birth || sub.count <= 0) {
                continue;
            }
            ParticleEmitter childEmitter;
            childEmitter.setConfig(sub.config);
            childEmitter.setPosition(parent.position.x, parent.position.y,
                                     parent.position.z);
            childEmitter._emit(pool, static_cast<size_t>(sub.count), maxParticles);
        }
    }
}

void ParticleEmitter::update(double dt, ParticlePool<>& pool, size_t maxParticles) {
    if (!enabled_ || !std::isfinite(dt) || dt <= 0.0) return;

    const auto emitUpToLimit = [&](size_t requested) {
        const size_t available = maxParticles == 0
            ? requested
            : (pool.activeCount() < maxParticles
                ? maxParticles - pool.activeCount() : 0);
        _emit(pool, std::min(requested, available), maxParticles);
    };

    if (!burstInitialized_) {
        burstInitialized_ = true;
        if (config_.burstCount > 0) {
            emitUpToLimit(static_cast<size_t>(config_.burstCount));
        }
    }

    if (config_.burstCount > 0 && std::isfinite(config_.burstInterval) &&
        config_.burstInterval > 0.0f) {
        burstAccumulator_ += dt;
        while (burstAccumulator_ >= config_.burstInterval) {
            emitUpToLimit(static_cast<size_t>(config_.burstCount));
            burstAccumulator_ -= config_.burstInterval;
        }
    }
    
    if (config_.rate > 0) {
        accumulator_ += dt;
        const double interval = 1.0 / static_cast<double>(config_.rate);

        while (accumulator_ >= interval &&
               (maxParticles == 0 || pool.activeCount() < maxParticles)) {
            _emit(pool, 1, maxParticles);
            accumulator_ -= interval;
        }
    }
}

// ============================================================================
// ParticleSystem::Impl
// ============================================================================
class ParticleSystem::Impl {
public:
    ParticlePool<> pool_;
    std::vector<SharedPtr<ParticleEmitter>> emitters_;
    std::vector<SharedPtr<ForceField>> forceFields_;
    std::vector<SharedPtr<ParticleCollider>> colliders_;
    std::vector<SharedPtr<ParticleConstraint>> constraints_;
    
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

void ParticleSystem::addEmitter(SharedPtr<ParticleEmitter> emitter) {
    if (emitter) impl_->emitters_.push_back(std::move(emitter));
}

void ParticleSystem::removeEmitter(const std::string& id) {
    impl_->emitters_.erase(
        std::remove_if(impl_->emitters_.begin(), impl_->emitters_.end(),
            [&id](const auto& e) { return !e || e->getId() == id; }),
        impl_->emitters_.end());
}

void ParticleSystem::clearEmitters() { impl_->emitters_.clear(); }
size_t ParticleSystem::emitterCount() const { return impl_->emitters_.size(); }

void ParticleSystem::addForceField(SharedPtr<ForceField> field) {
    if (field) impl_->forceFields_.push_back(std::move(field));
}

void ParticleSystem::removeForceField(const std::string& id) {
    impl_->forceFields_.erase(
        std::remove_if(impl_->forceFields_.begin(), impl_->forceFields_.end(),
            [&id](const auto& f) { return !f || f->getId() == id; }),
        impl_->forceFields_.end());
}

void ParticleSystem::clearForceFields() { impl_->forceFields_.clear(); }
size_t ParticleSystem::forceFieldCount() const { return impl_->forceFields_.size(); }

void ParticleSystem::addCollider(SharedPtr<ParticleCollider> collider) {
    if (collider) impl_->colliders_.push_back(std::move(collider));
}

void ParticleSystem::removeCollider(const std::string& id) {
    impl_->colliders_.erase(
        std::remove_if(impl_->colliders_.begin(), impl_->colliders_.end(),
            [&id](const auto& c) { return !c || c->getId() == id; }),
        impl_->colliders_.end());
}

void ParticleSystem::clearColliders() { impl_->colliders_.clear(); }
size_t ParticleSystem::colliderCount() const { return impl_->colliders_.size(); }

void ParticleSystem::addConstraint(SharedPtr<ParticleConstraint> constraint) {
    if (constraint) impl_->constraints_.push_back(std::move(constraint));
}

void ParticleSystem::clearConstraints() {
    impl_->constraints_.clear();
}

void ParticleSystem::update(double deltaTime) {
    if (!impl_ || impl_->paused_ || !std::isfinite(deltaTime) || deltaTime <= 0.0) {
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    const double dt = deltaTime * impl_->simulationSpeed_;
    if (!std::isfinite(dt) || dt <= 0.0) return;
    
    impl_->stats_.spawnedThisFrame = 0;
    impl_->stats_.killedThisFrame = 0;
    const size_t activeParticlesAtFrameStart = impl_->pool_.activeCount();
    
    // Emit new particles
    for (auto& emitter : impl_->emitters_) {
        if (emitter->isEnabled()) {
            emitter->update(dt, impl_->pool_, impl_->maxParticles_);
        }
    }

    // Update Fluid Fields
    for (auto& field : impl_->forceFields_) {
        if (field->isEnabled() && field->getType() == ForceField::Type::Fluid) {
            auto fluidField = static_cast<FluidField*>(field.get());
            fluidField->update(static_cast<float>(dt));
        }
    }
    
    // Get free indices for active particle iteration
    std::vector<bool> isFree;
    isFree.resize(impl_->pool_.size());
    for (size_t idx : impl_->pool_.getFreeIndices()) {
        isFree[idx] = true;
    }
    
    // Update each particle
    const size_t particleCountAtFrameStart = impl_->pool_.size();
    for (size_t i = 0; i < particleCountAtFrameStart; ++i) {
        if (isFree[i]) continue;
        
        Particle& p = impl_->pool_[i];
        
        // Update previous position for motion blur
        p.prevPosition = p.position;

        // Update age
        p.age += static_cast<float>(dt);
        
        // --- サブエミッター処理 (Trails) ---
        for (auto& emitter : impl_->emitters_) {
            if (emitter->emitterToken() != p.emitterToken) continue;
            for (const auto& sub : emitter->getSubEmitters()) {
                if (sub.trigger == SubEmitterConfig::Trigger::Trails) {
                    const float interval = std::isfinite(sub.interval)
                        ? std::max(0.0f, sub.interval) : 0.0f;
                    if (sub.count > 0 && p.age - p.lastSubEmitAge >= interval) {
                        ParticleEmitter childEmitter;
                        childEmitter.setConfig(sub.config);
                        childEmitter.setPosition(p.position.x, p.position.y, p.position.z);
                        childEmitter._emit(impl_->pool_, static_cast<size_t>(sub.count),
                                           impl_->maxParticles_);
                        p.lastSubEmitAge = p.age;
                    }
                }
            }
        }

        // Kill if expired
        if (p.age >= p.lifetime) {
            // --- サブエミッター処理 (Death) ---
            for (auto& emitter : impl_->emitters_) {
                if (emitter->emitterToken() != p.emitterToken) continue;
                for (const auto& sub : emitter->getSubEmitters()) {
                    if (sub.trigger == SubEmitterConfig::Trigger::Death) {
                         if (sub.count > 0) {
                            ParticleEmitter childEmitter;
                            childEmitter.setConfig(sub.config);
                            childEmitter.setPosition(p.position.x, p.position.y, p.position.z);
                            childEmitter._emit(impl_->pool_, static_cast<size_t>(sub.count),
                                               impl_->maxParticles_);
                         }
                    }
                }
            }

            impl_->pool_.kill(i);
            impl_->stats_.killedThisFrame++;
            continue;
        }
        
        // Apply forces
        for (auto& field : impl_->forceFields_) {
            if (field->isEnabled()) {
                float falloff = field->computeFalloff(p.position);
                if (falloff > 0.0f) {
                    // Temporarily modify p.velocity inside apply or wrap it
                    // To keep it simple, we use a temporary velocity or pass falloff to apply
                    // But Particle needs to be modified or we just apply force multiplied by falloff
                    // Since apply() modifies velocity directly, we'd need to change the API
                    // For now, let's assume apply() takes falloff or we do it here if possible
                    field->apply(p, dt * falloff);
                }
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

        // Apply Noise Distortion
        if (config_.noiseStrength.x > 0.0f || config_.noiseStrength.y > 0.0f || config_.noiseStrength.z > 0.0f) {
            float freq = config_.noiseFrequency;
            float time = p.age;
            float nx = NoiseGenerator::perlin(p.position.x * freq, p.position.y * freq, time);
            float ny = NoiseGenerator::perlin(p.position.y * freq, p.position.z * freq, time + 1.0f);
            float nz = NoiseGenerator::perlin(p.position.z * freq, p.position.x * freq, time + 2.0f);
            
            p.position.x += nx * config_.noiseStrength.x * static_cast<float>(dt);
            p.position.y += ny * config_.noiseStrength.y * static_cast<float>(dt);
            p.position.z += nz * config_.noiseStrength.z * static_cast<float>(dt);
        }
        
        // Integrate rotation
        p.rotation.x += p.angularVelocity.x * static_cast<float>(dt);
        p.rotation.y += p.angularVelocity.y * static_cast<float>(dt);
        p.rotation.z += p.angularVelocity.z * static_cast<float>(dt);
        
        // Collision detection
        for (auto& collider : impl_->colliders_) {
            collider->collide(p, dt);
        }
    }
    
    // 拘束の解決（流体・パーティクル間衝突など）
    if (!impl_->constraints_.empty()) {
        // 現在のアクティブなパーティクルを取得して解決
        std::vector<Particle> activeParticles;
        std::vector<size_t> indices;
        for (size_t i = 0; i < impl_->pool_.size(); ++i) {
            if (!isFree[i]) {
                activeParticles.push_back(impl_->pool_[i]);
                indices.push_back(i);
            }
        }
        
        for (auto& constraint : impl_->constraints_) {
            constraint->resolve(activeParticles, dt);
        }
        
        // 解決した値をプールに戻す
        for (size_t k = 0; k < activeParticles.size(); ++k) {
            impl_->pool_[indices[k]] = activeParticles[k];
        }
    }
    
    // 更新ループの最後で色などを適用
    for (size_t i = 0; i < impl_->pool_.size(); ++i) {
        if (isFree[i]) continue;
        Particle& p = impl_->pool_[i];
        
        float lifeRatio = std::clamp(p.age / p.lifetime, 0.0f, 1.0f);
        
        // Opacity interpolation
        p.opacity = config_.startOpacity + (config_.endOpacity - config_.startOpacity) * lifeRatio;
        
        // Size scale interpolation
        float currentScale = config_.startSizeScale + (config_.endSizeScale - config_.startSizeScale) * lifeRatio;
        // p.size 自体にスケールをかけるか、Rendering時に反映するか
        // ここでは p.size をベース値とし、スケールで動的に変える
        
        // Interpolate color (Configuration based)
        if (config_.colorGradients.empty()) {
            p.color.x = config_.colorStart.x + (config_.colorEnd.x - config_.colorStart.x) * lifeRatio;
            p.color.y = config_.colorStart.y + (config_.colorEnd.y - config_.colorStart.y) * lifeRatio;
            p.color.z = config_.colorStart.z + (config_.colorEnd.z - config_.colorStart.z) * lifeRatio;
            p.color.w = config_.colorStart.w + (config_.colorEnd.w - config_.colorStart.w) * lifeRatio;
        } else {
            // Find the two stops to interpolate between
            const auto& stops = config_.colorGradients;
            if (lifeRatio <= stops.front().time) {
                p.color = stops.front().color;
            } else if (lifeRatio >= stops.back().time) {
                p.color = stops.back().color;
            } else {
                for (size_t k = 0; k < stops.size() - 1; ++k) {
                    if (lifeRatio >= stops[k].time && lifeRatio <= stops[k+1].time) {
                        float t = (lifeRatio - stops[k].time) / (stops[k+1].time - stops[k].time);
                        p.color.x = stops[k].color.x + (stops[k+1].color.x - stops[k].color.x) * t;
                        p.color.y = stops[k].color.y + (stops[k+1].color.y - stops[k].color.y) * t;
                        p.color.z = stops[k].color.z + (stops[k+1].color.z - stops[k].color.z) * t;
                        p.color.w = stops[k].color.w + (stops[k+1].color.w - stops[k].color.w) * t;
                        break;
                    }
                }
            }
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    const size_t activeParticlesAtFrameEnd = impl_->pool_.activeCount();
    if (activeParticlesAtFrameEnd + impl_->stats_.killedThisFrame >=
        activeParticlesAtFrameStart) {
        impl_->stats_.spawnedThisFrame =
            activeParticlesAtFrameEnd + impl_->stats_.killedThisFrame -
            activeParticlesAtFrameStart;
    }
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
    impl_->maxParticles_ = std::max<size_t>(1, maxParticles);
    impl_->pool_.reserve(impl_->maxParticles_);
}

size_t ParticleSystem::maxParticles() const { return impl_->maxParticles_; }

void ParticleSystem::setSimulationSpeed(double speed) {
    impl_->simulationSpeed_ = std::isfinite(speed) ? std::clamp(speed, 0.0, 16.0) : 1.0;
}
double ParticleSystem::simulationSpeed() const { return impl_->simulationSpeed_; }

void ParticleSystem::setPaused(bool paused) { impl_->paused_ = paused; }
bool ParticleSystem::isPaused() const { return impl_->paused_; }

ParticleSystem::Statistics ParticleSystem::getStatistics() const { return impl_->stats_; }

// ============================================================================
// FluidConstraint Implementation (Simple PBD Fluid)
// ============================================================================
void FluidConstraint::resolve(std::vector<Particle>& particles, double dt) {
    if (particles.size() < 2) return;
    
    float rSq = radius_ * radius_;
    
    // 非常にシンプルな近接排斥（擬似流体）
    // 本来は密度計算が必要だが、ここではパフォーマンス重視で排斥のみ
    for (size_t i = 0; i < particles.size(); ++i) {
        for (size_t j = i + 1; j < particles.size(); ++j) {
            float3 diff{
                particles[i].position.x - particles[j].position.x,
                particles[i].position.y - particles[j].position.y,
                particles[i].position.z - particles[j].position.z
            };
            
            float distSq = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
            if (distSq > 0.0001f && distSq < rSq) {
                float dist = std::sqrt(distSq);
                float overlap = radius_ - dist;
                
                // 押し出しベクトル
                float factor = (overlap / dist) * 0.5f;
                float3 push{ diff.x * factor, diff.y * factor, diff.z * factor };
                
                particles[i].position.x += push.x;
                particles[i].position.y += push.y;
                particles[i].position.z += push.z;
                
                particles[j].position.x -= push.x;
                particles[j].position.y -= push.y;
                particles[j].position.z -= push.z;
                
                // 速度の減衰（粘性）
                particles[i].velocity.x *= 0.99f;
                particles[j].velocity.x *= 0.99f;
            }
        }
    }
}

} // namespace ArtifactCore
