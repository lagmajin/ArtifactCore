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

import Graphics;
import Utils.String.UniString;

export namespace ArtifactCore {

using namespace Diligent;

struct RayTracingCapabilities {
    RENDER_DEVICE_TYPE deviceType = RENDER_DEVICE_TYPE_UNDEFINED;
    DEVICE_FEATURE_STATE featureState = DEVICE_FEATURE_STATE_DISABLED;
    bool supported = false;
    bool unitQuadBLASCreated = false;
    bool unitQuadBLASBuilt = false;
    bool tlasCreated = false;
    bool tlasBuilt = false;
    Uint32 blasBuildCount = 0;
    Uint32 tlasBuildCount = 0;
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
};

class IRayTracingManager {
public:
    virtual ~IRayTracingManager() = default;

    virtual bool initialize(IRenderDevice* pDevice) = 0;
    virtual void destroy() = 0;

    // Acceleration Structure Management
    virtual bool createOrUpdateBLAS(const UniString& id, const RTGeometryData& data) = 0;
    virtual bool buildTLAS(IDeviceContext* pContext) = 0;
    virtual bool ensurePipelineAndSBT(IDeviceContext* pContext) = 0;
    virtual bool traceUnitQuad(IDeviceContext* pContext, Uint32 width, Uint32 height) = 0;

    virtual ITopLevelAS* getTLAS() const = 0;
    virtual ITextureView* traceOutputSRV() const = 0;
    virtual const RayTracingCapabilities& capabilities() const = 0;
    virtual bool isSupported() const = 0;
};

// Factory/Singleton accessor for the core
std::unique_ptr<IRayTracingManager> createRayTracingManager();

} // namespace ArtifactCore
