module;
#include <utility>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <BottomLevelAS.h>
#include <TopLevelAS.h>
#include <PipelineState.h>
#include <Shader.h>
#include <ShaderBindingTable.h>
#include <GraphicsTypesX.hpp>
#include <Texture.h>
#include <RefCntAutoPtr.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <cstring>
#include <map>
#include <string>

module Graphics.RayTracingManager;

import Graphics;
import Utils.String.UniString;

namespace ArtifactCore {

using namespace Diligent;

namespace {
constexpr const char* kWarmupRayTracingShaderSource = R"(
RWTexture2D<float4> g_OutputTex : register(u0);

[shader("raygeneration")]
void RTWarmup_RayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    g_OutputTex[pixel] = float4(0.2f, 0.45f, 0.9f, 1.0f);
}

[shader("miss")]
void RTWarmup_Miss()
{
}
)";

constexpr const char* kWarmupRayGenName = "RTWarmup_RayGen";
constexpr const char* kWarmupMissName = "RTWarmup_Miss";

void bindWarmupOutput(IPipelineState* pPSO, ITextureView* pUAV)
{
    if (!pPSO || !pUAV) {
        return;
    }
    if (auto* pVar = pPSO->GetStaticVariableByName(SHADER_TYPE_RAY_GEN, "g_OutputTex")) {
        pVar->Set(pUAV);
    }
}
} // namespace

class RayTracingManager : public IRayTracingManager {
public:
    RayTracingManager() = default;
    ~RayTracingManager() { destroy(); }

    bool initialize(IRenderDevice* pDevice) override {
        if (!pDevice) return false;

        destroy();
        pDevice_ = pDevice;

        const auto& props = pDevice->GetDeviceInfo();
        const auto& adapterInfo = pDevice->GetAdapterInfo();
        caps_.deviceType = props.Type;
        caps_.featureState = props.Features.RayTracing;
        caps_.supported = (props.Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED) &&
                          ((adapterInfo.RayTracing.CapFlags & RAY_TRACING_CAP_FLAG_STANDALONE_SHADERS) != 0);
        caps_.maxRecursionDepth = adapterInfo.RayTracing.MaxRecursionDepth;
        caps_.maxRayGenThreads = adapterInfo.RayTracing.MaxRayGenThreads;
        caps_.maxInstancesPerTLAS = adapterInfo.RayTracing.MaxInstancesPerTLAS;
        caps_.maxPrimitivesPerBLAS = adapterInfo.RayTracing.MaxPrimitivesPerBLAS;
        caps_.maxGeometriesPerBLAS = adapterInfo.RayTracing.MaxGeometriesPerBLAS;
        caps_.scratchBufferAlignment = adapterInfo.RayTracing.ScratchBufferAlignment;
        caps_.instanceBufferAlignment = adapterInfo.RayTracing.InstanceBufferAlignment;
        rtSupported_ = caps_.supported;

        if (rtSupported_) {
            createUnitQuadBLAS();
            createTLAS();
        }

        return true;
    }

    void destroy() override {
        pTraceOutputSRV_.Release();
        pTraceOutputUAV_.Release();
        pTraceOutputTexture_.Release();
        pRayTracingPSO_.Release();
        pSBT_.Release();
        pWarmupRayGen_.Release();
        pWarmupMiss_.Release();
        pUnitQuadBLAS_.Release();
        pTLAS_.Release();
        pDevice_.Release();
        caps_ = {};
    }

    bool createOrUpdateBLAS(const UniString& /*id*/, const RTGeometryData& /*data*/) override {
        // Since we use a shared Unit Quad BLAS for all layers, 
        // we don't need custom BLAS per layer for now.
        ++caps_.blasBuildCount;
        return true;
    }

    bool buildTLAS(IDeviceContext* pContext) override {
        if (!rtSupported_ || !pDevice_ || !pContext || !pTLAS_) return false;

        // Future: Here we will collect all layer transforms and update TLAS instances.
        ++caps_.tlasBuildCount;
        caps_.tlasBuilt = true;
        return true;
    }

    bool ensurePipelineAndSBT(IDeviceContext* pContext) override {
        if (!rtSupported_ || !pDevice_) {
            return false;
        }

        if (!pTraceOutputTexture_) {
            TextureDesc texDesc;
            texDesc.Name = "RayTracingOutput";
            texDesc.Type = RESOURCE_DIM_TEX_2D;
            texDesc.Width = 1;
            texDesc.Height = 1;
            texDesc.MipLevels = 1;
            texDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
            texDesc.Usage = USAGE_DEFAULT;
            texDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;

            pDevice_->CreateTexture(texDesc, nullptr, &pTraceOutputTexture_);
            if (pTraceOutputTexture_) {
                pTraceOutputSRV_ = pTraceOutputTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                pTraceOutputUAV_ = pTraceOutputTexture_->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
                caps_.outputTextureCreated = true;
                caps_.outputResourcesBound = pTraceOutputUAV_ != nullptr;
            }
        }

        if (!pRayTracingPSO_) {
            ShaderCreateInfo shaderCI;
            shaderCI.Source = kWarmupRayTracingShaderSource;
            shaderCI.SourceLength = std::strlen(kWarmupRayTracingShaderSource);
            shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
            shaderCI.ShaderCompiler = SHADER_COMPILER_DXC;
            shaderCI.HLSLVersion = {6, 3};
            shaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

            shaderCI.Desc.ShaderType = SHADER_TYPE_RAY_GEN;
            shaderCI.Desc.Name = "RT Warmup RayGen";
            shaderCI.EntryPoint = kWarmupRayGenName;
            pDevice_->CreateShader(shaderCI, &pWarmupRayGen_);

            shaderCI.Desc.ShaderType = SHADER_TYPE_RAY_MISS;
            shaderCI.Desc.Name = "RT Warmup Miss";
            shaderCI.EntryPoint = kWarmupMissName;
            pDevice_->CreateShader(shaderCI, &pWarmupMiss_);

            if (!pWarmupRayGen_ || !pWarmupMiss_) {
                return false;
            }

            RayTracingPipelineStateCreateInfoX psoCI("RT Warmup PSO");
            psoCI.AddGeneralShader(kWarmupRayGenName, pWarmupRayGen_);
            psoCI.AddGeneralShader(kWarmupMissName, pWarmupMiss_);
            psoCI.RayTracingPipeline.MaxRecursionDepth = 1;
            psoCI.RayTracingPipeline.ShaderRecordSize = 0;
            psoCI.MaxAttributeSize = 8;
            psoCI.MaxPayloadSize = 16;
            psoCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            ShaderResourceVariableDesc Vars[] = 
            {
                {SHADER_TYPE_RAY_GEN, "g_OutputTex", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
            };
            psoCI.PSODesc.ResourceLayout.Variables    = Vars;
            psoCI.PSODesc.ResourceLayout.NumVariables = 1;

            pDevice_->CreateRayTracingPipelineState(psoCI, &pRayTracingPSO_);
            if (pRayTracingPSO_) {
                bindWarmupOutput(pRayTracingPSO_, pTraceOutputUAV_);
                caps_.pipelineCreated = true;
            }
        }

        if (pRayTracingPSO_ && !pSBT_) {
            ShaderBindingTableDesc sbtDesc;
            sbtDesc.Name = "RT Warmup SBT";
            sbtDesc.pPSO = pRayTracingPSO_;
            pDevice_->CreateSBT(sbtDesc, &pSBT_);
            if (pSBT_) {
                pSBT_->BindRayGenShader(kWarmupRayGenName);
                pSBT_->BindMissShader(kWarmupMissName, 0);
                if (pContext) {
                    pContext->UpdateSBT(pSBT_);
                }
                caps_.sbtCreated = true;
                caps_.sbtBound = true;
            }
        }

        return caps_.pipelineCreated && caps_.sbtCreated && caps_.outputResourcesBound;
    }

    bool traceUnitQuad(IDeviceContext* pContext, Uint32 width, Uint32 height) override {
        if (!rtSupported_ || !pDevice_) {
            return false;
        }

        const Uint32 safeWidth = width > 0 ? width : 1;
        const Uint32 safeHeight = height > 0 ? height : 1;
        caps_.lastTraceWidth = safeWidth;
        caps_.lastTraceHeight = safeHeight;

        if (!pTraceOutputTexture_ || pTraceOutputTexture_->GetDesc().Width != safeWidth ||
            pTraceOutputTexture_->GetDesc().Height != safeHeight) {
            TextureDesc texDesc;
            texDesc.Name = "RayTracingOutput";
            texDesc.Type = RESOURCE_DIM_TEX_2D;
            texDesc.Width = safeWidth;
            texDesc.Height = safeHeight;
            texDesc.MipLevels = 1;
            texDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
            texDesc.Usage = USAGE_DEFAULT;
            texDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;

            pTraceOutputSRV_.Release();
            pTraceOutputUAV_.Release();
            pTraceOutputTexture_.Release();
            pDevice_->CreateTexture(texDesc, nullptr, &pTraceOutputTexture_);
            if (!pTraceOutputTexture_) {
                caps_.outputTextureCreated = false;
                caps_.outputResourcesBound = false;
                return false;
            }
            pTraceOutputSRV_ = pTraceOutputTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
            pTraceOutputUAV_ = pTraceOutputTexture_->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS);
            caps_.outputTextureCreated = true;
            caps_.outputResourcesBound = pTraceOutputUAV_ != nullptr;
            bindWarmupOutput(pRayTracingPSO_, pTraceOutputUAV_);
        }

        if (!pRayTracingPSO_ || !pSBT_ || !pContext) {
            return false;
        }

        TraceRaysAttribs traceAttribs{pSBT_, safeWidth, safeHeight, 1};
        pContext->TraceRays(traceAttribs);
        ++caps_.traceDispatchCount;
        return true;
    }

    ITopLevelAS* getTLAS() const override { return pTLAS_; }
    ITextureView* traceOutputSRV() const override { return pTraceOutputSRV_; }
    const RayTracingCapabilities& capabilities() const override { return caps_; }
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
        caps_.unitQuadBLASCreated = pUnitQuadBLAS_ != nullptr;
        caps_.unitQuadBLASBuilt = false;
    }

    void createTLAS() {
        TopLevelASDesc tlasDesc;
        tlasDesc.Name = "Main Scene TLAS";
        tlasDesc.MaxInstanceCount = 1024;
        pDevice_->CreateTLAS(tlasDesc, &pTLAS_);
        caps_.tlasCreated = pTLAS_ != nullptr;
    }

    RefCntAutoPtr<IRenderDevice> pDevice_;
    RefCntAutoPtr<IBottomLevelAS> pUnitQuadBLAS_;
    RefCntAutoPtr<ITopLevelAS> pTLAS_;
    RefCntAutoPtr<ITexture> pTraceOutputTexture_;
    RefCntAutoPtr<ITextureView> pTraceOutputSRV_;
    RefCntAutoPtr<ITextureView> pTraceOutputUAV_;
    RefCntAutoPtr<IShader> pWarmupRayGen_;
    RefCntAutoPtr<IShader> pWarmupMiss_;
    RefCntAutoPtr<IPipelineState> pRayTracingPSO_;
    RefCntAutoPtr<IShaderBindingTable> pSBT_;
    
    struct BLASNode {
        RefCntAutoPtr<IBottomLevelAS> pBLAS;
        Diligent::float4x4 transform;
    };
    std::map<std::wstring, BLASNode> blasMap_;
    RayTracingCapabilities caps_;
    bool rtSupported_ = false;
};

std::unique_ptr<IRayTracingManager> createRayTracingManager() {
    return std::make_unique<RayTracingManager>();
}

} // namespace ArtifactCore
