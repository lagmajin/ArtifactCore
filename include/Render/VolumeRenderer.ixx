module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

export module Render.VolumeRenderer;

import Render.Vector3D;
import Render.Ray;
import Render.Camera;
import Render.ImageBuffer;

export namespace ArtifactCore::RayTrace {

struct VolumeResolution {
    int width = 0;
    int height = 0;
    int depth = 0;

    [[nodiscard]] constexpr bool valid() const noexcept {
        return width > 0 && height > 0 && depth > 0;
    }

    [[nodiscard]] constexpr std::size_t cellCount() const noexcept {
        if (!valid()) return 0;
        const auto w = static_cast<std::size_t>(width);
        const auto h = static_cast<std::size_t>(height);
        const auto d = static_cast<std::size_t>(depth);
        if (h > std::numeric_limits<std::size_t>::max() / w) return 0;
        const auto wh = w * h;
        if (d > std::numeric_limits<std::size_t>::max() / wh) return 0;
        return wh * d;
    }

    [[nodiscard]] constexpr std::size_t indexOf(int x, int y, int z) const noexcept {
        return static_cast<std::size_t>(x)
            + static_cast<std::size_t>(width) * (
                static_cast<std::size_t>(y)
                + static_cast<std::size_t>(height) * static_cast<std::size_t>(z));
    }
};

struct VolumeAABB {
    Vec3 min{};
    Vec3 max{1.0f, 1.0f, 1.0f};

    [[nodiscard]] Vec3 center() const noexcept {
        return { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
    }

    [[nodiscard]] Vec3 extent() const noexcept {
        return { max.x - min.x, max.y - min.y, max.z - min.z };
    }

    [[nodiscard]] Vec3 voxelSize(const VolumeResolution& resolution) const noexcept {
        const auto e = extent();
        return {
            resolution.width > 1 ? e.x / static_cast<float>(resolution.width) : e.x,
            resolution.height > 1 ? e.y / static_cast<float>(resolution.height) : e.y,
            resolution.depth > 1 ? e.z / static_cast<float>(resolution.depth) : e.z,
        };
    }
};

struct VolumeScalarField {
    float* data = nullptr;
    VolumeResolution resolution{};

    [[nodiscard]] bool empty() const noexcept {
        return data == nullptr || resolution.cellCount() == 0;
    }

    [[nodiscard]] float& at(int x, int y, int z) noexcept {
        return data[resolution.indexOf(x, y, z)];
    }

    [[nodiscard]] float at(int x, int y, int z) const noexcept {
        return data[resolution.indexOf(x, y, z)];
    }

    [[nodiscard]] float sampleTrilinear(const Vec3& voxelCoord) const noexcept;
};

struct VolumeVectorField {
    Vec3* data = nullptr;
    VolumeResolution resolution{};

    [[nodiscard]] bool empty() const noexcept {
        return data == nullptr || resolution.cellCount() == 0;
    }

    [[nodiscard]] Vec3& at(int x, int y, int z) noexcept {
        return data[resolution.indexOf(x, y, z)];
    }

    [[nodiscard]] const Vec3& at(int x, int y, int z) const noexcept {
        return data[resolution.indexOf(x, y, z)];
    }
};

struct VolumeFieldSet {
    VolumeScalarField density;
    VolumeScalarField temperature;
    VolumeScalarField fuel;
    VolumeScalarField reaction;
    VolumeVectorField velocity;
};

enum class VolumeLightType : std::uint8_t {
    Point = 0,
    Spot = 1,
};

struct VolumetricLight {
    VolumeLightType type = VolumeLightType::Spot;
    Vec3 position{0.0f, 5.0f, 0.0f};
    Vec3 direction{0.0f, -1.0f, 0.0f};
    Color color{1.0f, 0.95f, 0.8f};
    float intensity = 1.0f;
    float range = 10.0f;
    float coneInnerAngle = 15.0f;
    float coneOuterAngle = 35.0f;
    float coneSoftness = 3.0f;
    float falloffExponent = 2.0f;
    bool enabled = true;
};

enum class TransferFunctionKind : std::uint8_t {
    Fire = 0,
    Smoke = 1,
    Steam = 2,
    Explosion = 3,
    Custom = 4,
    Fog = 5,
    FireAndSmoke = 6
};

struct TransferFunctionSample {
    float densityValue = 0.0f;
    Color emission{};
    float absorption = 1.0f;
    Color fireEmission{};
};

using TransferFunctionCallback = std::function<TransferFunctionSample(float density, float temperature)>;

struct TransferFunction {
    TransferFunctionKind kind = TransferFunctionKind::Smoke;
    float densityScale = 1.0f;
    float temperatureScale = 1.0f;
    TransferFunctionCallback customCallback;

    [[nodiscard]] TransferFunctionSample evaluate(float density, float temperature) const noexcept;
    static TransferFunction makeFire();
    static TransferFunction makeSmoke();
    static TransferFunction makeSteam();
    static TransferFunction makeExplosion();
    static TransferFunction makeFog();
    static TransferFunction makeFireAndSmoke();
};

struct VolumeRenderSettings {
    float stepSize = 0.05f;
    float densityScale = 1.0f;
    float fireReactionScale = 1.5f;
    float fireIntensityScale = 1.0f;
    int maxSteps = 2048;
    Color backgroundColor{0.0f, 0.0f, 0.0f};
    bool useTemperatureEmission = true;
    bool showVelocity = false;
    float shadowStepSize = 0.1f;
    int shadowMaxSteps = 512;
    float shadowDensityScale = 1.0f;
    float shadowIntensity = 1.0f;
    Color lightDirection{0.5f, -1.0f, 0.3f};
    float lightIntensity = 1.0f;
    float ambientLightIntensity = 0.0f;
    float phaseAnisotropy = 0.0f;
    float multipleScatteringStrength = 0.3f;
    float multipleScatteringSpread = 0.5f;
    int multipleScatteringOctaves = 3;
};

struct GpuVolumeRenderSettings {
    int outputWidth = 256;
    int outputHeight = 256;
    int raySteps = 96;
    float densityScale = 1.0f;
    float temperatureScale = 1.0f;
    float opacity = 1.0f;
    float fireReactionScale = 1.5f;
    float detailStrength = 0.0f;
    float detailScale = 12.0f;
    float detailTimeSeconds = 0.0f;
    std::uint32_t detailSeed = 0;
    // Experimental directional smoke lighting; zero preserves the unlit look.
    float smokeLightingStrength = 0.0f;
    float smokeShadowDensity = 20.0f;
    // Normalized domain rectangle, fixed for the render rather than auto-tracked.
    float viewCenterX = 0.5f;
    float viewCenterY = 0.5f;
    float viewScaleX = 1.0f;
    float viewScaleY = 1.0f;
    // Normalized domain-space offset for a caller-supplied uniform detail translation.
    float detailOffsetX = 0.0f;
    float detailOffsetY = 0.0f;
    float detailOffsetZ = 0.0f;
};

// Backend-neutral Diligent compute raymarcher for application volume fields.
// Resource allocation happens during initialize/resize; render reuses textures.
class GpuVolumeRenderer {
public:
    GpuVolumeRenderer() noexcept;
    ~GpuVolumeRenderer();
    GpuVolumeRenderer(const GpuVolumeRenderer&) = delete;
    GpuVolumeRenderer& operator=(const GpuVolumeRenderer&) = delete;

    bool initialize(Diligent::IRenderDevice* device,
                    Diligent::IDeviceContext* context);
    bool render(Diligent::IDeviceContext* context,
                const VolumeFieldSet& fields,
                const TransferFunction& transfer,
                const GpuVolumeRenderSettings& settings);
    // Borrowed view of the reusable output. A subsequent render can overwrite
    // its contents; resizing or destroying this renderer invalidates the view.
    // Submit queued consumers before reuse, or give concurrently queued
    // volumes separate renderer/output resources on the same device.
    [[nodiscard]] Diligent::ITextureView* outputSRV() const noexcept;
    [[nodiscard]] bool ready() const noexcept;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

class CPUVolumeRenderer {
public:
    VolumeAABB bounds{};
    VolumeFieldSet fields;
    VolumeRenderSettings settings;
    Camera camera;
    std::vector<VolumetricLight> lights;

    CPUVolumeRenderer() = default;

    void setVolumeData(const VolumeAABB& aabb, const VolumeFieldSet& fieldSet);
    void setTransferFunction(const TransferFunction& tf);
    void addLight(const VolumetricLight& light);
    void clearLights();

    [[nodiscard]] ImageBuffer render(int width, int height) const;

private:
    TransferFunction transferFunction_ = TransferFunction::makeSmoke();

    [[nodiscard]] bool intersectAABB(const Ray& ray, float& tNear, float& tFar) const noexcept;
    [[nodiscard]] Vec3 worldToVoxel(const Vec3& worldPos) const noexcept;
    [[nodiscard]] Color evaluateVolumetricLights(const Vec3& worldPos, const Vec3& sampleDir) const noexcept;
    [[nodiscard]] Color sampleLight(const Vec3& voxelPos) const noexcept;
    [[nodiscard]] float sampleDensity(const Vec3& voxelPos) const noexcept;
    [[nodiscard]] Color raymarch(const Ray& ray) const noexcept;
    [[nodiscard]] float phaseHenyeyGreenstein(float cosTheta, float g) const noexcept;
    [[nodiscard]] float multipleScatteringBoost(float density, float depth) const noexcept;
};

[[nodiscard]] float normalizeStepToVoxel(float stepSize, const VolumeAABB& bounds, const VolumeResolution& resolution) noexcept;

} // namespace ArtifactCore::RayTrace
