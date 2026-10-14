module;
#include <utility>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <Texture.h>
// RefCntAutoPtr.hpp not needed in this interface (raw pointers used throughout)
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <vector>
#include <memory>

export module Graphics.RayTracingManager;

import Utils.String.UniString;

export namespace ArtifactCore {

using namespace Diligent;

struct RayTracingCapabilities {
    RENDER_DEVICE_TYPE deviceType = RENDER_DEVICE_TYPE_UNDEFINED;
    DEVICE_FEATURE_STATE featureState = DEVICE_FEATURE_STATE_DISABLED;
    bool supported = false;
    bool tlasCreated = false;
    bool tlasBuilt = false;
    Uint32 blasBuildCount = 0;
    Uint32 blasUpdateCount = 0;
    Uint32 blasRebuildCount = 0;
    Uint32 registeredBLASCount = 0;
    Uint32 activeInstanceCount = 0;
    Uint32 tlasBuildCount = 0;
    Uint32 tlasUpdateCount = 0;
    Uint32 tlasRebuildCount = 0;
    bool lastBuildSucceeded = false;
    bool pipelineCreated = false;
    bool sbtCreated = false;
    bool sbtBound = false;
    bool outputTextureCreated = false;
    bool outputResourcesBound = false;
    Uint32 traceDispatchCount = 0;
    Uint32 lastTraceWidth = 0;
    Uint32 lastTraceHeight = 0;
    Uint32 maxRecursionDepth = 0;
    Uint32 maxRayGenThreads = 0;
    Uint32 maxInstancesPerTLAS = 0;
    Uint32 maxPrimitivesPerBLAS = 0;
    Uint32 maxGeometriesPerBLAS = 0;
    Uint32 scratchBufferAlignment = 0;
    Uint32 instanceBufferAlignment = 0;
};

// Minimal geometry data for BLAS construction
struct RTGeometryData {
    IBuffer* pVertexBuffer = nullptr;
    Uint32 vertexCount = 0;
    IBuffer* pIndexBuffer = nullptr;
    Uint32 indexCount = 0;
    Diligent::float4x4 transform = Diligent::float4x4::Identity();
    // Increment or replace when vertex/index contents change in-place.
    Uint64 geometryRevision = 0;
    // Change when indices/topology change; position-only edits can refit the BLAS.
    Uint64 topologyRevision = 0;
};

struct RTPbrMaterial {
    float baseColor[3] = {1.0f, 1.0f, 1.0f};
    float baseColorAlpha = 1.0f;
    float baseColorUvScale = 1.0f;
    float baseColorUvRotationDegrees = 0.0f;
    float baseColorUvOffsetU = 0.0f;
    float baseColorUvOffsetV = 0.0f;
    int baseColorUvSet = 0;
    float metallicRoughnessUvScale = 1.0f;
    float metallicRoughnessUvOffsetU = 0.0f;
    float metallicRoughnessUvOffsetV = 0.0f;
    float metallicRoughnessUvRotationDegrees = 0.0f;
    int metallicRoughnessUvSet = 0;
    float metallic = 0.0f;
    float roughness = 0.5f;
    float diffuseRoughness = 0.0f;
    float specular = 0.5f;
    float specularTint[3] = {1.0f, 1.0f, 1.0f};
    float thinFilmThickness = 0.0f;
    float thinFilmIOR = 1.33f;
    float ior = 1.5f;
    float transmission = 0.0f;
    float clearcoat = 0.0f;
    float clearcoatRoughness = 0.03f;
    float clearcoatIOR = 1.5f;
    UniString clearcoatTexture;
    float clearcoatTextureUvScale = 1.0f;
    float clearcoatTextureUvOffsetU = 0.0f;
    float clearcoatTextureUvOffsetV = 0.0f;
    float clearcoatTextureUvRotationDegrees = 0.0f;
    int clearcoatTextureUvSet = 0;
    UniString clearcoatRoughnessTexture;
    float clearcoatRoughnessTextureUvScale = 1.0f;
    float clearcoatRoughnessTextureUvOffsetU = 0.0f;
    float clearcoatRoughnessTextureUvOffsetV = 0.0f;
    float clearcoatRoughnessTextureUvRotationDegrees = 0.0f;
    int clearcoatRoughnessTextureUvSet = 0;
    float clearcoatTint[3] = {1.0f, 1.0f, 1.0f};
    float clearcoatNormalStrength = 1.0f;
    UniString clearcoatNormalTexture;
    float clearcoatNormalUvRotationDegrees = 0.0f;
    float clearcoatNormalUvScale = 1.0f;
    float clearcoatNormalUvOffsetU = 0.0f;
    float clearcoatNormalUvOffsetV = 0.0f;
    int clearcoatNormalUvSet = 0;
    float sheen = 0.0f;
    float sheenRoughness = 0.5f;
    float sheenTint[3] = {1.0f, 1.0f, 1.0f};
    UniString sheenColorTexture;
    float sheenColorTextureUvScaleU = 1.0f;
    float sheenColorTextureUvScaleV = 1.0f;
    float sheenColorTextureUvRotationDegrees = 0.0f;
    float sheenColorTextureUvOffsetU = 0.0f;
    float sheenColorTextureUvOffsetV = 0.0f;
    int sheenColorTextureUvSet = 0;
    UniString sheenRoughnessTexture;
    float sheenRoughnessTextureUvScaleU = 1.0f;
    float sheenRoughnessTextureUvScaleV = 1.0f;
    float sheenRoughnessTextureUvRotationDegrees = 0.0f;
    float sheenRoughnessTextureUvOffsetU = 0.0f;
    float sheenRoughnessTextureUvOffsetV = 0.0f;
    int sheenRoughnessTextureUvSet = 0;
    float subsurface = 0.0f;
    float subsurfaceIOR = 1.4f;
    float subsurfaceAnisotropy = 0.0f;
    float subsurfaceRadius[3] = {1.0f, 0.2f, 0.1f};
    float subsurfaceColor[3] = {1.0f, 1.0f, 1.0f};
    float anisotropy = 0.0f;
    float anisotropyRotationDegrees = 0.0f;
    UniString anisotropyTexture;
    float anisotropyTextureUvScaleU = 1.0f;
    float anisotropyTextureUvScaleV = 1.0f;
    float anisotropyTextureUvRotationDegrees = 0.0f;
    float anisotropyTextureUvOffsetU = 0.0f;
    float anisotropyTextureUvOffsetV = 0.0f;
    int anisotropyTextureUvSet = 0;
    float emissionColor[3] = {0.0f, 0.0f, 0.0f};
    float emissionStrength = 0.0f;
    float opacity = 1.0f;
    float alphaCutoff = 0.5f;
    Uint32 alphaMode = 0; // MaterialAlphaMode: Opaque=0, Masked=1, Blended=2.
    float thinWall = 0.0f;
    UniString baseColorTexture;
    UniString metallicRoughnessTexture;
    UniString metallicTexture;
    float metallicTextureUvScaleU = 1.0f;
    float metallicTextureUvScaleV = 1.0f;
    float metallicTextureUvRotationDegrees = 0.0f;
    float metallicTextureUvOffsetU = 0.0f;
    float metallicTextureUvOffsetV = 0.0f;
    int metallicTextureUvSet = 0;
    UniString roughnessTexture;
    float roughnessTextureUvScaleU = 1.0f;
    float roughnessTextureUvScaleV = 1.0f;
    float roughnessTextureUvRotationDegrees = 0.0f;
    float roughnessTextureUvOffsetU = 0.0f;
    float roughnessTextureUvOffsetV = 0.0f;
    int roughnessTextureUvSet = 0;
    UniString normalTexture;
    float normalStrength = 1.0f;
    float normalUvRotationDegrees = 0.0f;
    float normalUvScale = 1.0f;
    float normalUvOffsetU = 0.0f;
    float normalUvOffsetV = 0.0f;
    int normalUvSet = 0;
    UniString occlusionTexture;
    float occlusionStrength = 1.0f;
    float occlusionTextureUvScale = 1.0f;
    float occlusionTextureUvOffsetU = 0.0f;
    float occlusionTextureUvOffsetV = 0.0f;
    float occlusionTextureUvRotationDegrees = 0.0f;
    int occlusionTextureUvSet = 0;
    float uvOffsetU = 0.0f;
    float uvOffsetV = 0.0f;
    float uvScaleU = 1.0f;
    float uvScaleV = 1.0f;
    float uvRotationDegrees = 0.0f;
    UniString emissionTexture;
    float emissionTextureUvScale = 1.0f;
    float emissionTextureUvOffsetU = 0.0f;
    float emissionTextureUvOffsetV = 0.0f;
    float emissionTextureUvRotationDegrees = 0.0f;
    int emissionTextureUvSet = 0;
    UniString opacityTexture;
    float opacityTextureUvScale = 1.0f;
    float opacityTextureUvOffsetU = 0.0f;
    float opacityTextureUvOffsetV = 0.0f;
    float opacityTextureUvRotationDegrees = 0.0f;
    int opacityTextureUvSet = 0;
};

struct RTRayCamera {
    float origin[3] = {0.0f, 0.0f, -2.0f};
    float target[3] = {0.0f, 0.0f, 0.0f};
    // The image-space vertical direction is derived from this vector.
    float up[3] = {0.0f, 1.0f, 0.0f};
    float verticalFovDegrees = 57.523f;
};

struct RTSceneLight {
    // LightType ordinal: Directional=0, Point=1, Spot=2, Ambient=3, Area=4.
    float positionType[4] = {};
    float directionIntensity[4] = {};
    float colorAttenuationConstant[4] = {};
    float attenuationSpot[4] = {};
    float shadowParams[4] = {};
};

class IRayTracingManager {
public:
    virtual ~IRayTracingManager() = default;

    virtual bool initialize(IRenderDevice* pDevice) = 0;
    virtual void destroy() = 0;

    // Acceleration Structure Management
    virtual bool createOrUpdateBLAS(const UniString& id, const RTGeometryData& data) = 0;
    virtual bool updateShadingGeometry(const UniString& id, const float* vertices,
                                       Uint32 vertexCount, const Uint32* indices,
                                       Uint32 indexCount,
                                       const float* vertexNormals = nullptr,
                                       // One material index per triangle; null selects material 0.
                                       const Uint32* primitiveMaterialIndices = nullptr,
                                       const float* vertexUvs = nullptr,
                                       const float* vertexColors = nullptr,
                                       // Optional second UV set, two floats per vertex.
                                       const float* vertexUvs1 = nullptr) = 0;
    virtual bool setInstancePbrMaterial(const UniString& id,
                                        const RTPbrMaterial& material,
                                        // Material index referenced by RT triangle data.
                                        Uint32 materialIndex = 0) = 0;
    virtual void setEnvironmentLighting(IDeviceContext* pContext,
                                        ITextureView* irradianceEnvironment,
                                        ITextureView* prefilteredEnvironment,
                                        ITextureView* brdfLut,
                                        bool enabled, float intensity,
                                        float rotationDegrees,
                                        ITextureView* sheenPrefilteredEnvironment = nullptr) = 0;
    virtual void setSceneLights(IDeviceContext* pContext,
                                const RTSceneLight* lights,
                                Uint32 lightCount) = 0;
    virtual bool updateInstanceTransform(const UniString& id,
                                         const Diligent::float4x4& transform) = 0;
    virtual bool hasBLAS(const UniString& id) const = 0;
    virtual bool setInstanceActive(const UniString& id, bool active) = 0;
    virtual bool buildBLAS(IDeviceContext* pContext) = 0;
    virtual bool buildTLAS(IDeviceContext* pContext) = 0;
    virtual bool ensurePipelineAndSBT(IDeviceContext* pContext) = 0;
    virtual bool traceUnitQuad(IDeviceContext* pContext, Uint32 width, Uint32 height) = 0;
    virtual bool traceCamera(IDeviceContext* pContext, Uint32 width, Uint32 height,
                             const RTRayCamera& camera) = 0;

    virtual ITopLevelAS* getTLAS() const = 0;
    virtual ITextureView* traceOutputSRV() const = 0;
    virtual const RayTracingCapabilities& capabilities() const = 0;
    virtual bool isSupported() const = 0;
};

// Factory/Singleton accessor for the core
std::unique_ptr<IRayTracingManager> createRayTracingManager();

} // namespace ArtifactCore
