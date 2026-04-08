module;
#include <utility>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <BottomLevelAS.h>
#include <TopLevelAS.h>
#include <RefCntAutoPtr.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <map>
#include <string>

module Graphics.RayTracingManager;

import Graphics;
import Utils.String.UniString;

namespace ArtifactCore {

using namespace Diligent;

class RayTracingManager : public IRayTracingManager {
public:
    RayTracingManager() = default;
    ~RayTracingManager() { destroy(); }

    bool initialize(IRenderDevice* pDevice) override {
        if (!pDevice) return false;
        pDevice_ = pDevice;

        const auto& props = pDevice->GetDeviceInfo();
        rtSupported_ = (props.Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED);

        if (rtSupported_) {
            createUnitQuadBLAS();
            createTLAS();
        }

        return true;
    }

    void destroy() override {
        pUnitQuadBLAS_.Release();
        pTLAS_.Release();
        pDevice_.Release();
    }

    bool createOrUpdateBLAS(const UniString& /*id*/, const RTGeometryData& /*data*/) override {
        // Since we use a shared Unit Quad BLAS for all layers, 
        // we don't need custom BLAS per layer for now.
        return true;
    }

    bool buildTLAS(IDeviceContext* pContext) override {
        if (!rtSupported_ || !pDevice_ || !pContext || !pTLAS_) return false;

        // Future: Here we will collect all layer transforms and update TLAS instances.
        return true;
    }

    ITopLevelAS* getTLAS() const override { return pTLAS_; }
    bool isSupported() const override { return rtSupported_; }

private:
    void createUnitQuadBLAS() {
        BLASTriangleDesc triangleDesc;
        triangleDesc.GeometryName = "Unit Quad Geometry";
        triangleDesc.VertexValueType = VT_FLOAT32;
        triangleDesc.VertexComponentCount = 3;
        triangleDesc.MaxVertexCount = 4;
        triangleDesc.IndexType = VT_UINT32;
        triangleDesc.MaxPrimitiveCount = 2; 

        BottomLevelASDesc blasDesc;
        blasDesc.Name = "Unit Quad BLAS";
        blasDesc.TriangleCount = 1;
        blasDesc.pTriangles = &triangleDesc;
        
        pDevice_->CreateBLAS(blasDesc, &pUnitQuadBLAS_);
    }

    void createTLAS() {
        TopLevelASDesc tlasDesc;
        tlasDesc.Name = "Main Scene TLAS";
        tlasDesc.MaxInstanceCount = 1024;
        pDevice_->CreateTLAS(tlasDesc, &pTLAS_);
    }

    RefCntAutoPtr<IRenderDevice> pDevice_;
    RefCntAutoPtr<IBottomLevelAS> pUnitQuadBLAS_;
    RefCntAutoPtr<ITopLevelAS> pTLAS_;
    
    struct BLASNode {
        RefCntAutoPtr<IBottomLevelAS> pBLAS;
        Diligent::float4x4 transform;
    };
    std::map<std::wstring, BLASNode> blasMap_;
    bool rtSupported_ = false;
};

std::unique_ptr<IRayTracingManager> createRayTracingManager() {
    return std::make_unique<RayTracingManager>();
}

} // namespace ArtifactCore
