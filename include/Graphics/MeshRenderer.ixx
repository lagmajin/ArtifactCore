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
#include <QVector3D>
#include <QString>
#include "../Define/DllExportMacro.hpp"
#include <cstdint>
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
    // Runtime-only texture input for a spotlight GOBO. This deliberately has
    // no layer reference or project serialization: the caller owns source
    // selection and must rebind after device/resource recreation.
    struct GoboProjectionTextureInput {
        std::string sourceKey;
        Diligent::ITextureView* srv = nullptr;
        std::uint64_t revision = 0;

        bool isValid() const noexcept { return !sourceKey.empty() && srv != nullptr; }
    };

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
     * @param colors Pointer to float4 vertex colors (optional, can be nullptr; white = identity)
     * @param uvs1 Pointer to float2 second UV set (optional; falls back at the caller)
     */
    void updateMeshGeometry(const float* positions, const float* normals, const float* uvs,
                            const uint32_t* indices, const float* colors = nullptr,
                            const float* uvs1 = nullptr);

    // Upload the packed resources consumed by the future mesh-shader path.
    // The indexed renderer remains the fallback until a mesh-shader PSO is active.
    void updateMeshletGeometry(const MeshletGpu* meshlets, size_t meshletCount,
                               const uint32_t* indices, size_t indexCount,
                               const MeshletLodGpu* lods, size_t lodCount);

    IBuffer* positionBuffer() const noexcept;
    IBuffer* indexBuffer() const noexcept;
    size_t vertexCount() const noexcept;
    size_t indexCount() const noexcept;
    size_t maxInstances() const noexcept;

    /**
     * @brief Set a base-color texture to be sampled by the mesh shader.
     *        Empty path clears the texture and falls back to a white texture.
     */
    void setBaseColorTexture(const QString& path);
    void setBaseColorUvSet(int uvSet);
    void setPbrMapUvSets(int metallicRoughnessUvSet,
                         int occlusionUvSet,
                         int emissionUvSet,
                         int opacityUvSet,
                         int clearcoatNormalUvSet);
    void clearBaseColorTexture();
    void setEmissionTexture(const QString& path);
    void clearEmissionTexture();
    void setEmissionColor(const QColor& color, float strength);
    void setPbrFactors(float metallic, float roughness,
                       float normalStrength, float occlusionStrength);
    void setDiffuseRoughness(float roughness);
    void setPrincipledFactors(float specular, float ior, float transmission,
                               float clearcoat, float clearcoatRoughness,
                               float sheen = 0.0f, float clearcoatIor = 1.5f,
                               bool thinWall = false);
    void setSpecularTint(const QColor& color);
    void setThinFilm(float thicknessNanometers, float ior);
    void setClearcoatTint(const QColor& color, float normalStrength = 1.0f);
    void setClearcoatNormalTexture(const QString& path);
    void setSheenFactors(float roughness, const QColor& tint);
    void setSheenColorTexture(const QString& path, float scaleU,
                              float scaleV, float rotationDegrees,
                              float offsetU, float offsetV, int uvSet);
    void setSubsurfaceFactors(float weight, const QColor& color,
                              const QVector3D& radius = QVector3D(1.0f, 0.2f, 0.1f),
                              float ior = 1.4f,
                              float anisotropy = 0.0f);
    void setAnisotropy(float value, float rotationDegrees);
    // Shared UV texture transform plus independent base/normal-map scales.
    void setUvTransform(float offsetU, float offsetV, float scaleU, float scaleV,
                        float rotationDegrees,
                        float normalRotationDegrees = 0.0f,
                        float normalScale = 1.0f,
                        float baseColorScale = 1.0f,
                        float baseColorOffsetU = 0.0f,
                        float baseColorOffsetV = 0.0f,
                        float baseColorRotationDegrees = 0.0f);
    void setMetallicRoughnessUvScale(float scale,
                                     float offsetU = 0.0f,
                                     float offsetV = 0.0f);
    void setMetallicRoughnessUvRotation(float rotationDegrees);
    void setNormalUvOffset(float offsetU, float offsetV, int uvSet = 0);
    void setOcclusionTextureUvScale(float scale,
                                    float offsetU = 0.0f,
                                    float offsetV = 0.0f);
    void setOcclusionTextureUvRotation(float rotationDegrees);
    void setEmissionTextureUvScale(float scale,
                                   float offsetU = 0.0f,
                                   float offsetV = 0.0f);
    void setEmissionTextureUvRotation(float rotationDegrees);
    void setOpacityTextureUvScale(float scale,
                                  float offsetU = 0.0f,
                                  float offsetV = 0.0f);
    void setOpacityTextureUvRotation(float rotationDegrees);
    void setClearcoatNormalUvTransform(float rotationDegrees, float scale,
                                       float offsetU = 0.0f,
                                       float offsetV = 0.0f);
    void setClearcoatTexture(const QString& path, float scale,
                             float rotationDegrees, float offsetU,
                             float offsetV, int uvSet);
    void setClearcoatRoughnessTexture(const QString& path, float scale,
                                      float rotationDegrees, float offsetU,
                                      float offsetV, int uvSet);
    // Per-material IBL intensity scale for indirect lighting.
    void setEnvironmentIntensity(float intensity);
    // Blender-style material graph splices (Artifact.ShaderNode.Core
    // NodeGraph::compileMaterialGraph output: helpers for global scope,
    // block for the PSMain body). Rebuilds the PSOs; empty block restores
    // the fixed PBR path.
    void setMaterialGraph(const std::string& helperHlsl,
                          const std::string& blockHlsl);
    void setMaterialGraphBlock(const std::string& blockHlsl);
    void clearMaterialGraph();
    bool hasMaterialGraph() const;
    // Parses ShaderNode JSON and applies it when it differs from the
    // current payload. Returns false on invalid JSON (keeps old material).
    bool setMaterialGraphJson(const std::string& json);
    void setMetallicRoughnessTexture(const QString& path);
    void setMetallicTexture(const QString& path);
    void setRoughnessTexture(const QString& path);
    void setMetallicTextureUvTransform(float scaleU, float scaleV,
        float rotationDegrees, float offsetU, float offsetV, int uvSet);
    void setRoughnessTextureUvTransform(float scaleU, float scaleV,
        float rotationDegrees, float offsetU, float offsetV, int uvSet);
    void setNormalTexture(const QString& path);
    void setOcclusionTexture(const QString& path);
    // Optional split-sum environment lighting inputs. Passing nullptr for all
    // views disables IBL while preserving the existing studio fallback.
    void setEnvironmentMaps(ITextureView* irradianceMap,
                            ITextureView* prefilteredEnvironment,
                            ITextureView* brdfLut,
                            float intensity = 1.0f,
                            ITextureView* sheenPrefilteredEnvironment = nullptr);
    // Load an equirectangular HDR/EXR image and create a runtime cubemap.
    // The generated cubemap is used as the initial environment input; callers
    // may replace the derived irradiance/prefilter maps through setEnvironmentMaps.
    bool setEnvironmentMap(const QString& path, float intensity = 1.0f);
    /// Shares the current environment resources with another renderer owned
    /// by the same render device.
    void shareEnvironmentMapsFrom(const MeshRenderer& source);
    bool hasEnvironmentMap() const;
    void setEnvironmentRotation(float degrees);
    /// Returns the currently loaded specular environment cubemap SRV.
    /// The returned view is owned by MeshRenderer and remains valid until the
    /// next environment-map replacement or renderer destruction.
    Diligent::ITextureView* environmentMapView() const;
    /// Returns the Charlie-distribution prefiltered environment used by sheen.
    Diligent::ITextureView* sheenEnvironmentMapView() const;
    /// Returns the diffuse irradiance cubemap used by the split-sum IBL path.
    Diligent::ITextureView* irradianceMapView() const;
    /// Returns the split-sum BRDF integration lookup texture.
    Diligent::ITextureView* brdfLutView() const;
    void setSceneLights(const std::vector<Light>& lights);
    // Overrides the file-backed GOBO texture for a packed scene-light slot.
    // This is an internal render input only; it neither creates nor persists a
    // connection between a layer and a light.
    void setGoboProjectionTexture(std::size_t sceneLightIndex,
                                  const GoboProjectionTextureInput& input);
    void clearGoboProjectionTexture(std::size_t sceneLightIndex);
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
    /// Enables alpha-test masking while keeping the opaque depth-write path.
    void setAlphaMasked(bool masked);
    void setAlphaCutoff(float cutoff);

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

    // CPU/GPU ABI for the scalar material constants used by MeshPSSource.
    // Keep these float4 fields in the same order as cbuffer MaterialParams.
    struct MaterialState {
        std::array<float, 4> baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
        std::array<float, 4> emissionColorStrength{};
        std::array<float, 4> pbrFactors{0.0f, 0.5f, 1.0f, 1.0f};
        std::array<float, 4> pbrTextureFlags{};
        std::array<float, 4> principledFactors{0.5f, 1.5f, 0.0f, 0.0f};
        std::array<float, 4> specularTintFactors{1.0f, 1.0f, 1.0f, 0.0f};
        std::array<float, 4> thinFilmFactors{0.0f, 1.33f, 0.0f, 0.0f};
        std::array<float, 4> clearcoatFactors{0.0f, 0.2f, 1.5f, 0.0f};
        std::array<float, 4> alphaSettings{0.5f, 0.0f, 0.0f, 0.0f};
        std::array<float, 4> uvTransformA{0.0f, 0.0f, 1.0f, 1.0f};
        std::array<float, 4> uvTransformB{0.0f, 0.0f, 1.0f, 1.0f};
        std::array<float, 4> clearcoatNormalUvTransform{0.0f, 1.0f, 0.0f, 0.0f};
        std::array<float, 4> envFactors{1.0f, 0.0f, 0.0f, 0.0f};
        std::array<float, 4> sheenFactors{0.5f, 1.0f, 1.0f, 1.0f};
        std::array<float, 4> anisotropyFactors{};
        std::array<float, 4> subsurfaceFactors{0.0f, 1.0f, 1.0f, 1.0f};
        std::array<float, 4> subsurfaceRadius{1.0f, 0.2f, 0.1f, 1.4f};
        std::array<float, 4> subsurfaceScatteringFactors{};
        std::array<float, 4> clearcoatTintFactors{1.0f, 1.0f, 1.0f, 1.0f};
        std::array<float, 4> surfaceTextureUvTransform{1.0f, 0.0f, 0.0f, 0.0f};
        std::array<float, 4> normalTextureUvOffset{};
        std::array<float, 4> occlusionTextureUvTransform{1.0f, 0.0f, 0.0f, 0.0f};
        std::array<float, 4> emissionTextureUvTransform{1.0f, 0.0f, 0.0f, 0.0f};
        std::array<float, 4> opacityTextureUvTransform{1.0f, 0.0f, 0.0f, 0.0f};
        std::array<float, 4> baseColorTextureUvTransform{};
        std::array<float, 4> opacityTextureUvOffset{};
        std::array<float, 4> emissionTextureUvRotation{};
        // x/y/z/w select Metallic-Roughness/Occlusion/Emission/Opacity UV sets.
        std::array<float, 4> textureUvSetSelectors{};
        // x/y indicate independent maps, z/w select their UV set.
        std::array<float, 4> pbrTextureFlags2{};
        std::array<float, 4> metallicTextureUvTransform{1.0f, 1.0f, 0.0f, 0.0f};
        std::array<float, 4> roughnessTextureUvTransform{1.0f, 1.0f, 0.0f, 0.0f};
        std::array<float, 4> independentPbrTextureUvRotation{};
        std::array<float, 4> transmissionMode{};
        // x: enabled, y: UV1 selector, z: uniform scale, w: rotation radians.
        std::array<float, 4> clearcoatTextureFactors{};
        std::array<float, 4> clearcoatTextureUvOffset{};
        // x: has roughness map; y: UV1 selector; z: scale; w: rotation.
        std::array<float, 4> clearcoatRoughnessTextureFactors{};
        std::array<float, 4> clearcoatRoughnessTextureUvOffset{};
        // x: enabled, y: UV1 selector, z/w: independent UV scales.
        std::array<float, 4> sheenColorTextureFactors{};
        // x/y: UV offset, z: rotation radians.
        std::array<float, 4> sheenColorTextureUvOffset{};
    };
    static_assert(sizeof(MaterialState) == 39u * 4u * sizeof(float));

    // Multi-material draw. Each range names a contiguous index span and the
    // scalar material state and texture SRVs bound while that span is drawn.
    // With no ranges configured the call falls back to a single whole-mesh
    // draw using the material set through the existing setters.
    struct MaterialRange {
        std::uint32_t firstIndex = 0;
        std::uint32_t indexCount = 0;
        std::uint32_t materialIndex = 0;
        bool transparent = false;
        MaterialState material;
        QString baseColorTexturePath;
        QString metallicRoughnessTexturePath;
        QString metallicTexturePath;
        QString roughnessTexturePath;
        QString normalTexturePath;
        QString clearcoatNormalTexturePath;
        QString clearcoatTexturePath;
        QString clearcoatRoughnessTexturePath;
        QString occlusionTexturePath;
        QString emissionTexturePath;
        QString opacityTexturePath;
        QString sheenColorTexturePath;
    };
    void setMaterialRanges(const MaterialRange* ranges, std::size_t count);
    void clearMaterialRanges();
    void drawMaterialSlots(IDeviceContext* pContext, size_t instanceCount);
    // Shadow counterpart of drawMaterialSlots.  prepareShadow must run first.
    void drawShadowMaterialSlots(IDeviceContext* pContext, size_t instanceCount);

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
