module;
#include "../Define/DllExportMacro.hpp"
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <random>
#include <chrono>
#include <string>

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
export module Particle.System;



import Particle;
import Mesh;
import Physics.Fluid;
import Physics2D;
import Audio.Analyze;


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
        Drag,
        Audio,
        Field,  // 汎用フィールド
        Fluid,  // 流体フィールド
        Physics // 物理衝突フィールド
    };

    enum class FalloffType {
        Constant,
        Linear,
        Quadratic,
        Smooth
    };

    enum class FieldShape {
        Infinite,
        Sphere,
        Box
    };

    ForceField(Type type) : type_(type) {}
    virtual ~ForceField() = default;

    virtual void apply(Particle& particle, double deltaTime) = 0;
    
    // 空間減衰の計算
    float computeFalloff(const float3& p) const;

    Type getType() const { return type_; }
    const std::string& getId() const { return id_; }
    void setId(const std::string& id) { id_ = id; }
    
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    // フィールド形状設定
    void setShape(FieldShape shape) { shape_ = shape; }
    void setFalloff(FalloffType type) { falloff_ = type; }
    void setSphere(float radius, float feather) { shape_ = FieldShape::Sphere; radius_ = radius; feather_ = feather; }
    void setBox(float3 size, float feather) { shape_ = FieldShape::Box; size_ = size; feather_ = feather; }

protected:
    Type type_;
    std::string id_;
    bool enabled_ = true;

    FieldShape shape_ = FieldShape::Infinite;
    FalloffType falloff_ = FalloffType::Constant;
    float3 center_{0, 0, 0};
    float3 size_{1, 1, 1};
    float radius_ = 1.0f;
    float feather_ = 0.1f;
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
// Audio Force - オーディオ解析に反応する力
// ============================================================================
class LIBRARY_DLL_API AudioForce : public ForceField {
public:
    enum class Target { RMS, Peak, Low, Mid, High };

    AudioForce() : ForceField(Type::Audio) {}

    void apply(Particle& p, double dt) override {
        float strength = audioValue_ * multiplier_ * static_cast<float>(dt);
        p.velocity.x += direction_.x * strength;
        p.velocity.y += direction_.y * strength;
        p.velocity.z += direction_.z * strength;
    }

    void setAudioValue(float val) { audioValue_ = val; }
    void setDirection(float x, float y, float z) { direction_ = {x, y, z}; }
    void setMultiplier(float m) { multiplier_ = m; }

private:
    float audioValue_ = 0.0f;
    float3 direction_{0.0f, 1.0f, 0.0f};
    float multiplier_ = 10.0f;
};

// ============================================================================
// Particle Constraint - パーティクル間の相互作用（流体・衝突）
// ============================================================================
class LIBRARY_DLL_API ParticleConstraint {
public:
    virtual ~ParticleConstraint() = default;
    virtual void resolve(std::vector<Particle>& particles, double dt) = 0;
};

class LIBRARY_DLL_API FluidConstraint : public ParticleConstraint {
public:
    FluidConstraint(float radius = 0.5f, float density = 1.0f) 
        : radius_(radius), targetDensity_(density) {}

    void resolve(std::vector<Particle>& particles, double dt) override;

private:
    float radius_;
    float targetDensity_;
};

// ============================================================================
// Fluid Field - 2D流体による力の適用
// ============================================================================
class LIBRARY_DLL_API FluidField : public ForceField {
public:
    FluidField(int width, int height) : ForceField(Type::Fluid), solver_(width, height) {}

    void apply(Particle& p, double dt) override {
        // パーティクルの位置をグリッド座標に変換
        float fx = (p.position.x - center_.x + size_.x * 0.5f) / size_.x * solver_.width();
        float fy = (p.position.y - center_.y + size_.y * 0.5f) / size_.y * solver_.height();
        
        int ix = static_cast<int>(fx);
        int iy = static_cast<int>(fy);
        
        float vx, vy;
        solver_.getVelocity(ix, iy, vx, vy);
        
        p.velocity.x += vx * strength_ * static_cast<float>(dt);
        p.velocity.y += vy * strength_ * static_cast<float>(dt);
    }

    void update(float dt) { solver_.update(dt); }
    FluidSolver2D& getSolver() { return solver_; }
    
    void setStrength(float s) { strength_ = s; }

    // Audio Injection
    void injectAudioFrequency(const AudioAnalyzer::AnalysisResult& audio, float dt) {
        // Sample: Use Low/Mid/High intensities to inject fluid bursts
        // Low: Center bottom burst
        if (audio.lowIntensity > 0.5f) {
            solver_.addDensity(solver_.width()/2, solver_.height()-2, audio.lowIntensity * 5.0f);
            solver_.addVelocity(solver_.width()/2, solver_.height()-2, 0, -audio.lowIntensity * 10.0f);
        }
        // High: Random bursts
        if (audio.highIntensity > 0.3f) {
            int rx = rand() % solver_.width();
            int ry = rand() % solver_.height();
            solver_.addDensity(rx, ry, audio.highIntensity * 2.0f);
        }
    }

private:
    FluidSolver2D solver_;
    float strength_ = 1.0f;
};
// ============================================================================
// Physics Collider Field - パーティクルが剛体に衝突する基盤
// ============================================================================
class LIBRARY_DLL_API PhysicsColliderField : public ForceField {
public:
    PhysicsColliderField(Physics2D* physics) : ForceField(Type::Physics), physics_(physics) {}

    void apply(Particle& p, double dt) override {
        if (!physics_) return;
        
        // 簡易的な衝突：各剛体との距離をチェック
        // ※ 本来は空間分割(Quadtree等)が必要
        for (auto& body : physics_->getBodies()) {
            QVector2D bPos = body->position();
            float3 diff = {p.position.x - bPos.x(), p.position.y - bPos.y(), 0.0f};
            float distSq = diff.x*diff.x + diff.y*diff.y;

            // 例：半径1.0の仮の当たり判定
            float radius = 1.0f; 
            if (distSq < radius * radius) {
                float dist = std::sqrt(distSq);
                float normal_len = dist + 0.0001f;
                float3 normal = {diff.x / normal_len, diff.y / normal_len, 0.0f};
                
                // 反射ベクトル
                float dot = p.velocity.x * normal.x + p.velocity.y * normal.y;
                p.velocity.x -= 2.0f * dot * normal.x * bounciness_;
                p.velocity.y -= 2.0f * dot * normal.y * bounciness_;
                
                // めり込み防止
                p.position.x += normal.x * (radius - dist);
                p.position.y += normal.y * (radius - dist);
            }
        }
    }

    void setBounciness(float b) { bounciness_ = b; }

private:
    Physics2D* physics_;
    float bounciness_ = 0.5f;
};

// ============================================================================
// Emission Config - パーティクル発生設定
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

    // Visual Settings
    int textureIndex = -1;
    int blendMode = 0;
    float startOpacity = 1.0f;
    float endOpacity = 0.0f;
    float startSizeScale = 1.0f;
    float endSizeScale = 1.0f;

    // Noise Distortion
    float3 noiseStrength{0.0f, 0.0f, 0.0f};
    float noiseFrequency = 1.0f;
    int noiseOctaves = 1;

    // Color Gradients
    struct ColorStop {
        float time; // 0.0 - 1.0
        float4 color;
    };
    std::vector<ColorStop> colorGradients;

    // Motion Blur
    struct MotionBlurConfig {
        bool enabled = false;
        float shutterAngle = 180.0f; // 0-360
        int samples = 8;
        float intensity = 1.0f;
        float velocityStretch = 1.0f; // 速度に応じた伸び
    } motionBlur;
};

struct SubEmitterConfig {
    EmissionConfig config;
    enum class Trigger {
        Birth,
        Death,
        Trails  // 定期的な発生（軌跡）
    } trigger = Trigger::Trails;
    float interval = 0.1f; // Trails時の発生間隔
    int count = 1;         // トリガー時の発生数
};

class LIBRARY_DLL_API ParticleEmitter {
public:
    enum class Shape {
        Point,
        Sphere,
        Box,
        Cone,
        Circle,
        Line,
        Mesh,   // 3Dモデルから発生
        Layer   // 画像レイヤーから発生
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

    void addSubEmitter(const SubEmitterConfig& sub) { subEmitters_.push_back(sub); }
    void clearSubEmitters() { subEmitters_.clear(); }
    const std::vector<SubEmitterConfig>& getSubEmitters() const { return subEmitters_; }

    // ジオメトリ・レイヤーソース
    void setMeshSource(std::shared_ptr<Mesh> mesh) { meshSource_ = mesh; }
    void setLayerSource(void* texture) { layerSource_ = texture; } // placeholder
    void setColorInheritance(float amount) { colorInheritance_ = amount; }

protected:
    virtual void initParticle(Particle& p);
    float3 randomDirection() const;

    Shape shape_ = Shape::Point;
    float3 position_{0.0f, 0.0f, 0.0f};
    EmissionConfig config_;
    std::string id_;
    bool enabled_ = true;
    
    std::vector<SubEmitterConfig> subEmitters_;
    
    // Geometry sources
    std::shared_ptr<Mesh> meshSource_;
    void* layerSource_ = nullptr;
    float colorInheritance_ = 0.0f;

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

    // 拘束管理
    void addConstraint(std::shared_ptr<ParticleConstraint> constraint);
    void clearConstraints();

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
    EmissionConfig config_;
};

} // namespace ArtifactCore