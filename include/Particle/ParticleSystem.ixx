module;
#include "../Define/DllExportMacro.hpp"
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <random>
#include <chrono>
#include <string>

export module Particle.System;

import Particle;

export namespace ArtifactCore {

// ============================================================================
// Particle Pool - 高効率パーティクルメモリ管理
// ============================================================================
template<typename ParticleT = Particle>
class ParticlePool {
public:
    explicit ParticlePool(size_t initialCapacity = 10000) {
        particles_.reserve(initialCapacity);
        freeIndices_.reserve(initialCapacity);
    }

    size_t spawn() {
        if (!freeIndices_.empty()) {
            size_t idx = freeIndices_.back();
            freeIndices_.pop_back();
            particles_[idx] = ParticleT{};
            return idx;
        }
        size_t idx = particles_.size();
        particles_.push_back(ParticleT{});
        return idx;
    }

    void kill(size_t index) {
        if (index < particles_.size()) {
            freeIndices_.push_back(index);
        }
    }

    ParticleT& operator[](size_t index) { return particles_[index]; }
    const ParticleT& operator[](size_t index) const { return particles_[index]; }
    
    size_t size() const { return particles_.size(); }
    size_t activeCount() const { return particles_.size() - freeIndices_.size(); }

    void clear() {
        particles_.clear();
        freeIndices_.clear();
    }

    void reserve(size_t capacity) {
        particles_.reserve(capacity);
        freeIndices_.reserve(capacity);
    }

    const std::vector<size_t>& getFreeIndices() const { return freeIndices_; }

private:
    std::vector<ParticleT> particles_;
    std::vector<size_t> freeIndices_;
};

// ============================================================================
// Force Field - 力場基底クラス
// ============================================================================
class LIBRARY_DLL_API ForceField {
public:
    enum class Type {
        Gravity,
        Wind,
        Turbulence,
        Vortex,
        Attractor,
        Drag
    };

    ForceField(Type type) : type_(type) {}
    virtual ~ForceField() = default;

    virtual void apply(Particle& particle, double deltaTime) = 0;
    
    Type getType() const { return type_; }
    const std::string& getId() const { return id_; }
    void setId(const std::string& id) { id_ = id; }
    
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

protected:
    Type type_;
    std::string id_;
    bool enabled_ = true;
};

// ============================================================================
// Gravity Force
// ============================================================================
class LIBRARY_DLL_API GravityForce : public ForceField {
public:
    GravityForce() : ForceField(Type::Gravity) {}
    
    void apply(Particle& p, double dt) override {
        p.velocity.x += gravity_.x * static_cast<float>(dt);
        p.velocity.y += gravity_.y * static_cast<float>(dt);
        p.velocity.z += gravity_.z * static_cast<float>(dt);
    }

    void setGravity(float x, float y, float z) { gravity_ = {x, y, z}; }
    float3 getGravity() const { return gravity_; }

private:
    float3 gravity_{0.0f, -9.81f, 0.0f};
};

// ============================================================================
// Wind Force
// ============================================================================
class LIBRARY_DLL_API WindForce : public ForceField {
public:
    WindForce() : ForceField(Type::Wind) {}
    
    void apply(Particle& p, double dt) override {
        p.velocity.x += direction_.x * strength_ * static_cast<float>(dt);
        p.velocity.y += direction_.y * strength_ * static_cast<float>(dt);
        p.velocity.z += direction_.z * strength_ * static_cast<float>(dt);
    }

    void setDirection(float x, float y, float z) {
        float len = std::sqrt(x*x + y*y + z*z);
        if (len > 0.0f) {
            direction_ = {x/len, y/len, z/len};
        }
    }
    void setStrength(float strength) { strength_ = strength; }
    float getStrength() const { return strength_; }

private:
    float3 direction_{1.0f, 0.0f, 0.0f};
    float strength_ = 1.0f;
};

// ============================================================================
// Turbulence Force - ノイズベース乱流
// ============================================================================
class LIBRARY_DLL_API TurbulenceForce : public ForceField {
public:
    TurbulenceForce() : ForceField(Type::Turbulence), rng_(std::random_device{}()) {}
    
    void apply(Particle& p, double dt) override {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        float strength = strength_ * static_cast<float>(dt);
        p.velocity.x += dist(rng_) * strength;
        p.velocity.y += dist(rng_) * strength;
        p.velocity.z += dist(rng_) * strength;
    }

    void setStrength(float strength) { strength_ = strength; }
    float getStrength() const { return strength_; }

private:
    float strength_ = 1.0f;
    std::mt19937 rng_;
};

// ============================================================================
// Vortex Force - 渦巻き
// ============================================================================
class LIBRARY_DLL_API VortexForce : public ForceField {
public:
    VortexForce() : ForceField(Type::Vortex) {}
    
    void apply(Particle& p, double dt) override {
        float3 toParticle{
            p.position.x - center_.x,
            p.position.y - center_.y,
            p.position.z - center_.z
        };
        
        // 接線方向の力
        float dist = std::sqrt(toParticle.x*toParticle.x + toParticle.y*toParticle.y + toParticle.z*toParticle.z);
        if (dist > 0.001f) {
            float factor = strength_ * static_cast<float>(dt) / dist;
            p.velocity.x += -toParticle.z * factor;
            p.velocity.z += toParticle.x * factor;
        }
    }

    void setCenter(float x, float y, float z) { center_ = {x, y, z}; }
    void setStrength(float strength) { strength_ = strength; }

private:
    float3 center_{0.0f, 0.0f, 0.0f};
    float strength_ = 10.0f;
};

// ============================================================================
// Attractor Force - 引力点
// ============================================================================
class LIBRARY_DLL_API AttractorForce : public ForceField {
public:
    AttractorForce() : ForceField(Type::Attractor) {}
    
    void apply(Particle& p, double dt) override {
        float3 dir{
            center_.x - p.position.x,
            center_.y - p.position.y,
            center_.z - p.position.z
        };
        float dist = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
        if (dist > 0.01f) {
            float force = strength_ * static_cast<float>(dt) / (dist * dist);
            p.velocity.x += dir.x * force;
            p.velocity.y += dir.y * force;
            p.velocity.z += dir.z * force;
        }
    }

    void setCenter(float x, float y, float z) { center_ = {x, y, z}; }
    void setStrength(float strength) { strength_ = strength; }

private:
    float3 center_{0.0f, 0.0f, 0.0f};
    float strength_ = 10.0f;
};

// ============================================================================
// Drag Force - 空気抵抗
// ============================================================================
class LIBRARY_DLL_API DragForce : public ForceField {
public:
    DragForce() : ForceField(Type::Drag) {}
    
    void apply(Particle& p, double dt) override {
        float factor = 1.0f - drag_ * static_cast<float>(dt);
        factor = std::max(0.0f, factor);
        p.velocity.x *= factor;
        p.velocity.y *= factor;
        p.velocity.z *= factor;
    }

    void setDrag(float drag) { drag_ = drag; }
    float getDrag() const { return drag_; }

private:
    float drag_ = 0.1f;
};

// ============================================================================
// Particle Emitter - エミッター基底
// ============================================================================
struct EmissionConfig {
    int rate = 10;              // 1秒あたりの生成数
    int burstCount = 0;         // 一度に生成する数
    float burstInterval = 0.0f; // バースト間隔
    
    float lifetimeMin = 1.0f;
    float lifetimeMax = 2.0f;
    
    float speedMin = 1.0f;
    float speedMax = 2.0f;
    
    float sizeMin = 1.0f;
    float sizeMax = 1.0f;
    
    float4 colorStart{1.0f, 1.0f, 1.0f, 1.0f};
    float4 colorEnd{1.0f, 1.0f, 1.0f, 0.0f};
    
    float3 direction{0.0f, 1.0f, 0.0f};
    float spreadAngle = 0.0f;   // ラジアン
};

class LIBRARY_DLL_API ParticleEmitter {
public:
    enum class Shape {
        Point,
        Sphere,
        Box,
        Cone,
        Circle,
        Line
    };

    ParticleEmitter();
    virtual ~ParticleEmitter() = default;

    void emit(ParticlePool<>& pool, size_t count);
    void update(double dt, ParticlePool<>& pool);

    void setConfig(const EmissionConfig& config) { config_ = config; }
    const EmissionConfig& getConfig() const { return config_; }

    void setShape(Shape shape) { shape_ = shape; }
    Shape getShape() const { return shape_; }

    void setPosition(float x, float y, float z) { position_ = {x, y, z}; }
    float3 getPosition() const { return position_; }

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }

    const std::string& getId() const { return id_; }
    void setId(const std::string& id) { id_ = id; }

protected:
    virtual void initParticle(Particle& p);
    float3 randomDirection() const;

    Shape shape_ = Shape::Point;
    float3 position_{0.0f, 0.0f, 0.0f};
    EmissionConfig config_;
    std::string id_;
    bool enabled_ = true;
    
    double accumulator_ = 0.0;
    mutable std::mt19937 rng_{std::random_device{}()};
};

// ============================================================================
// Particle Collider - 衝突検出
// ============================================================================
class LIBRARY_DLL_API ParticleCollider {
public:
    enum class Type {
        Plane,
        Sphere,
        Box,
        Heightfield
    };

    ParticleCollider(Type type) : type_(type) {}
    virtual ~ParticleCollider() = default;

    virtual bool collide(Particle& p, double dt) = 0;

    Type getType() const { return type_; }
    const std::string& getId() const { return id_; }
    void setId(const std::string& id) { id_ = id; }

    void setRestitution(float r) { restitution_ = r; }
    float getRestitution() const { return restitution_; }

    void setFriction(float f) { friction_ = f; }
    float getFriction() const { return friction_; }

protected:
    Type type_;
    std::string id_;
    float restitution_ = 0.5f;  // 反発係数
    float friction_ = 0.1f;     // 摩擦係数
};

// ============================================================================
// Plane Collider
// ============================================================================
class LIBRARY_DLL_API PlaneCollider : public ParticleCollider {
public:
    PlaneCollider() : ParticleCollider(Type::Plane) {}

    bool collide(Particle& p, double dt) override {
        // y=0の平面との衝突
        if (p.position.y < 0.0f && p.velocity.y < 0.0f) {
            p.position.y = 0.0f;
            p.velocity.y = -p.velocity.y * restitution_;
            p.velocity.x *= (1.0f - friction_);
            p.velocity.z *= (1.0f - friction_);
            return true;
        }
        return false;
    }

    void setHeight(float h) { height_ = h; }

private:
    float height_ = 0.0f;
};

// ============================================================================
// Sphere Collider
// ============================================================================
class LIBRARY_DLL_API SphereCollider : public ParticleCollider {
public:
    SphereCollider() : ParticleCollider(Type::Sphere) {}

    bool collide(Particle& p, double dt) override {
        float3 diff{
            p.position.x - center_.x,
            p.position.y - center_.y,
            p.position.z - center_.z
        };
        float dist = std::sqrt(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z);
        
        if (dist < radius_) {
            // 外側へ押し出す
            float norm = dist > 0.0f ? dist : 1.0f;
            float3 normal{diff.x/norm, diff.y/norm, diff.z/norm};
            
            p.position.x = center_.x + normal.x * radius_;
            p.position.y = center_.y + normal.y * radius_;
            p.position.z = center_.z + normal.z * radius_;
            
            // 反射
            float dot = p.velocity.x*normal.x + p.velocity.y*normal.y + p.velocity.z*normal.z;
            p.velocity.x = (p.velocity.x - 2.0f*dot*normal.x) * restitution_;
            p.velocity.y = (p.velocity.y - 2.0f*dot*normal.y) * restitution_;
            p.velocity.z = (p.velocity.z - 2.0f*dot*normal.z) * restitution_;
            
            return true;
        }
        return false;
    }

    void setCenter(float x, float y, float z) { center_ = {x, y, z}; }
    void setRadius(float r) { radius_ = r; }

private:
    float3 center_{0.0f, 0.0f, 0.0f};
    float radius_ = 1.0f;
};

// ============================================================================
// Particle Curve - 属性アニメーション
// ============================================================================
class LIBRARY_DLL_API ParticleCurve {
public:
    void addPoint(float time, float value) {
        points_.push_back({time, value});
        std::sort(points_.begin(), points_.end(), 
            [](const auto& a, const auto& b) { return a.time < b.time; });
    }

    float evaluate(float t) const {
        if (points_.empty()) return 0.0f;
        if (points_.size() == 1) return points_[0].value;
        
        t = std::clamp(t, 0.0f, 1.0f);
        
        for (size_t i = 1; i < points_.size(); ++i) {
            if (points_[i].time >= t) {
                float diff = points_[i].time - points_[i-1].time;
                if (diff < 0.0001f) return points_[i].value;
                float ratio = (t - points_[i-1].time) / diff;
                return points_[i-1].value + (points_[i].value - points_[i-1].value) * ratio;
            }
        }
        return points_.back().value;
    }

    void clear() { points_.clear(); }
    size_t pointCount() const { return points_.size(); }

private:
    struct Point { float time; float value; };
    std::vector<Point> points_;
};

// ============================================================================
// Particle System
// ============================================================================
class LIBRARY_DLL_API ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) noexcept;
    ParticleSystem& operator=(ParticleSystem&&) noexcept;

    // エミッター管理
    void addEmitter(std::shared_ptr<ParticleEmitter> emitter);
    void removeEmitter(const std::string& id);
    void clearEmitters();
    size_t emitterCount() const;

    // 力場管理
    void addForceField(std::shared_ptr<ForceField> field);
    void removeForceField(const std::string& id);
    void clearForceFields();
    size_t forceFieldCount() const;

    // コリジョン管理
    void addCollider(std::shared_ptr<ParticleCollider> collider);
    void removeCollider(const std::string& id);
    void clearColliders();
    size_t colliderCount() const;

    // シミュレーション
    void update(double deltaTime);
    void reset();
    
    // パーティクルアクセス
    size_t activeParticleCount() const;
    size_t totalParticleCount() const;
    ParticlePool<>& getPool();

    // 設定
    void setMaxParticles(size_t maxParticles);
    size_t maxParticles() const;
    
    void setSimulationSpeed(double speed);
    double simulationSpeed() const;
    
    void setPaused(bool paused);
    bool isPaused() const;

    // 統計
    struct Statistics {
        size_t totalParticles = 0;
        size_t activeParticles = 0;
        size_t spawnedThisFrame = 0;
        size_t killedThisFrame = 0;
        double simulationTimeMs = 0.0;
    };
    Statistics getStatistics() const;

private:
    class Impl;
    Impl* impl_;
};

} // namespace ArtifactCore