module;
#include <utility>
#include <RenderDevice.h>
#include <DeviceContext.h>
// RefCntAutoPtr.hpp not needed in this interface (raw pointers used throughout)
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <vector>
#include <memory>

export module Graphics.RayTracingManager;

import Graphics;
import Utils.String.UniString;

export namespace ArtifactCore {

using namespace Diligent;

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

    virtual ITopLevelAS* getTLAS() const = 0;
    virtual bool isSupported() const = 0;
};

// Factory/Singleton accessor for the core
std::unique_ptr<IRayTracingManager> createRayTracingManager();

} // namespace ArtifactCore
