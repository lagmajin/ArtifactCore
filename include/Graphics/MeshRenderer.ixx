module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineStateCache.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <QColor>
#include "../Define/DllExportMacro.hpp"
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include "InstanceData.h"
export module Graphics.MeshRenderer;

import Graphics.GPUcomputeContext;
import Frame.Debug;
import Core.Light;

export namespace ArtifactCore {

using namespace Diligent;

/**
 * @brief Mesh instancing renderer using DiligentEngine
 * Provides GPU instancing similar to ParticleRenderer but for arbitrary meshes.
 */
class LIBRARY_DLL_API MeshRenderer {
public:
    // GPU ABI for the mesh-shader path. Keep these types free of Qt/Diligent
    // containers so the same layout can be mirrored in HLSL.
    struct MeshletGpu {
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        uint32_t vertexOffset = 0;
        uint32_t vertexCount = 0;
        float boundsCenter[3] = {};
        float boundsRadius = 0.0f;
    };

    struct MeshletLodGpu {
        uint32_t meshletOffset = 0;
        uint32_t meshletCount = 0;
        float switchPixels = 0.0f;
        uint32_t reserved = 0;
    };

    static_assert(sizeof(MeshletGpu) == 32);
    static_assert(sizeof(MeshletLodGpu) == 16);

    MeshRenderer(GpuContext& context);
    ~MeshRenderer();

    /**
     * @brief Initialize renderer with maximum instance count and mesh reference
     * @param maxInstances Maximum number of instances to support
     * @param vertexCount Number of vertices in the mesh
     * @param indexCount Number of indices in the mesh (0 for non-indexed)
     */
    void initialize(size_t maxInstances, size_t vertexCount, size_t indexCount);

    /**
     * @brief Select the color attachment format used when creating mesh PSOs.
     *        Call this before drawing to a different render target format.
     */
    void setRenderTargetFormat(Diligent::TEXTURE_FORMAT format);
    
    void setFrameCostStats(ArtifactCore::RenderCostStats* stats);
    void setPipelineStateCache(IPipelineStateCache* cache);

    /**
     * @brief Upload mesh geometry to GPU buffers
     * @param positions Pointer to float3 positions (x,y,z per vertex)
     * @param normals Pointer to float3 normals (optional, can be nullptr)
     * @param uvs Pointer to float2 uvs (optional, can be nullptr)
     * @param indices Pointer to uint32 indices (optional for non-indexed)
     */
    void updateMeshGeometry(const float* positions, const float* normals, const float* uvs,
                            const uint32_t* indices);

    // Upload the packed resources consumed by the future mesh-shader path.
    // The indexed renderer remains the fallback until a mesh-shader PSO is active.
    void updateMeshletGeometry(const MeshletGpu* meshlets, size_t meshletCount,
                               const uint32_t* indices, size_t indexCount,
                               const MeshletLodGpu* lods, size_t lodCount);

    IBuffer* positionBuffer() const noexcept;
    IBuffer* indexBuffer() const noexcept;
    size_t vertexCount() const noexcept;
    size_t indexCount() const noexcept;

    /**
     * @brief Set a base-color texture to be sampled by the mesh shader.
     *        Empty path clears the texture and falls back to a white texture.
     */
    void setBaseColorTexture(const QString& path);
    void setBaseColorTextureView(ITextureView* view);
    void clearBaseColorTexture();
    void setEmissionTexture(const QString& path);
    void clearEmissionTexture();
    void setEmissionColor(const QColor& color, float strength);
    void setPbrFactors(float metallic, float roughness,
                       float normalStrength, float occlusionStrength);
    void setPrincipledFactors(float specular, float ior, float transmission,
                              float clearcoat, float clearcoatRoughness);
    void setMetallicRoughnessTexture(const QString& path);
    void setNormalTexture(const QString& path);
    void setOcclusionTexture(const QString& path);
    // Optional split-sum environment lighting inputs. Passing nullptr for all
    // views disables IBL while preserving the existing studio fallback.
    void setEnvironmentMaps(ITextureView* irradianceMap,
                            ITextureView* prefilteredEnvironment,
                            ITextureView* brdfLut,
                            float intensity = 1.0f);
    // Load an equirectangular HDR/EXR image and create a runtime cubemap.
    // The generated cubemap is used as the initial environment input; callers
    // may replace the derived irradiance/prefilter maps through setEnvironmentMaps.
    bool setEnvironmentMap(const QString& path, float intensity = 1.0f);
    void setEnvironmentRotation(float degrees);
    /// Returns the currently loaded specular environment cubemap SRV.
    /// The returned view is owned by MeshRenderer and remains valid until the
    /// next environment-map replacement or renderer destruction.
    Diligent::ITextureView* environmentMapView() const;
    void setSceneLights(const std::vector<Light>& lights);
    // Set the depth texture and light transform consumed by the material pass.
    // Passing nullptr disables shadow comparison without changing the lights.
    void setShadowMap(ITextureView* shadowMap, const float* lightViewProjection,
                      bool enabled, int sceneLightIndex,
                      float depthBias = 0.0015f,
                      float softness = 0.0f);
    // Shadow-map depth prepass. The caller owns the DSV/viewport binding and
    // supplies a light view-projection matrix in the same Qt column-major
    // layout accepted by the regular matrix setters.
    void prepareShadow(IDeviceContext* pContext, const float* lightViewProjection);
    void drawShadow(IDeviceContext* pContext, size_t instanceCount);
    void setTransparentPass(bool transparent);

    /**
     * @brief Set an opacity texture to modulate mesh alpha.
     *        Empty path clears the texture and falls back to a white texture.
     */
    void setOpacityTexture(const QString& path);
    void clearOpacityTexture();

    /**
     * @brief Upload instance data to GPU structured buffer
     * @param instances Pointer to instance data array
     * @param count Number of instances to upload
     */
    void updateInstanceData(const InstanceData* instances, size_t count);

    /**
     * @brief Prepare for rendering (set PSO, bind resources)
     */
    void prepare(IDeviceContext* pContext);

    // Mesh-shader path. Returns to the caller without drawing when the
    // device does not expose mesh shaders or meshlet resources are absent.
    void prepareMeshShader(IDeviceContext* pContext, size_t lodIndex = 0);
    void drawMeshlets(IDeviceContext* pContext, size_t lodIndex = 0);
    bool meshShaderReady() const noexcept;
    size_t chooseMeshletLOD(float projectedRadiusPixels) const noexcept;
    void setMeshletMatrices(const float* viewMatrix,
                            const float* projectionMatrix,
                            const float* modelMatrix);

    /**
     * @brief Issue draw commands
     * @param pContext Device context
     * @param instanceCount Number of instances to draw
     */
    void draw(IDeviceContext* pContext, size_t instanceCount);

    // Matrix setters
    void setViewMatrix(const float* matrix);   // float[16]
    void setProjectionMatrix(const float* matrix); // float[16]
    void setPreviousViewMatrix(const float* matrix); // float[16]
    void setPreviousProjectionMatrix(const float* matrix); // float[16]

private:
    GpuContext& context_;
    class Impl;
    Impl* pImpl_ = nullptr;
    size_t maxInstances_ = 0;
    size_t vertexCount_ = 0;
    size_t indexCount_ = 0;
    Diligent::TEXTURE_FORMAT renderTargetFormat_ =
        Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;

    struct ShaderConstants {
        float viewMatrix[16];
        float projMatrix[16];
        float prevViewMatrix[16];
        float prevProjMatrix[16];
    };
    ShaderConstants constants_;
    ArtifactCore::RenderCostStats* frameCostStats_ = nullptr;
    bool prepared_ = false;

    void createPSO();
    void createBuffers();
};

} // namespace ArtifactCore
