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
RaytracingAccelerationStructure g_TLAS : register(t0);
RWTexture2D<float4> g_OutputTex : register(u0);
struct Payload { float4 color; };

[shader("raygeneration")]
void RTWarmup_RayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 size = DispatchRaysDimensions().xy;
    float2 uv = (float2(pixel) + 0.5f) / float2(size);
    RayDesc ray;
    ray.Origin = float3(uv * 2.0f - 1.0f, -2.0f);
    ray.Direction = float3(0.0f, 0.0f, 1.0f);
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;
    Payload payload = (Payload)0;
    TraceRay(g_TLAS, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    g_OutputTex[pixel] = payload.color;
}

[shader("miss")]
void RTWarmup_Miss(inout Payload payload)
{
    payload.color = float4(0.2f, 0.45f, 0.9f, 1.0f);
}

[shader("closesthit")]
void RTWarmup_ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    payload.color = float4(0.9f, 0.75f, 0.2f, 1.0f);
}
)";

constexpr const char* kWarmupRayGenName = "RTWarmup_RayGen";
constexpr const char* kWarmupMissName = "RTWarmup_Miss";
constexpr const char* kWarmupHitName = "RTWarmup_ClosestHit";

void bindWarmupOutput(IPipelineState* pPSO, ITextureView* pUAV)
{
    if (!pPSO || !pUAV) {
        return;
    }
    if (auto* pVar = pPSO->GetStaticVariableByName(SHADER_TYPE_RAY_GEN, "g_OutputTex")) {
        pVar->Set(pUAV);
    }
}

void bindWarmupTLAS(IPipelineState* pPSO, ITopLevelAS* pTLAS)
{
    if (!pPSO || !pTLAS) return;
    if (auto* pVar = pPSO->GetStaticVariableByName(SHADER_TYPE_RAY_GEN, "g_TLAS")) {
        pVar->Set(pTLAS);
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
        pWarmupClosestHit_.Release();
        pUnitQuadBLAS_.Release();
        pTLAS_.Release();
        pBLASScratchBuffer_.Release();
        pTLASScratchBuffer_.Release();
        pTLASInstanceBuffer_.Release();
        blasMap_.clear();
        tlasInstanceCount_ = 0;
        pDevice_.Release();
        caps_ = {};
    }

    bool createOrUpdateBLAS(const UniString& id, const RTGeometryData& data) override {
        if (!rtSupported_ || !pDevice_ || !data.pVertexBuffer || data.vertexCount < 3 ||
            (data.pIndexBuffer && data.indexCount < 3)) return false;
        if ((data.pVertexBuffer->GetDesc().BindFlags & BIND_RAY_TRACING) == 0) {
            return false;
        }
        if (data.pIndexBuffer &&
            (data.pIndexBuffer->GetDesc().BindFlags & BIND_RAY_TRACING) == 0) {
            return false;
        }
        const auto key = id.toStdWString();
        auto& node = blasMap_[key];
        const bool geometryLayoutChanged =
            node.data.pVertexBuffer != data.pVertexBuffer ||
            node.data.pIndexBuffer != data.pIndexBuffer ||
            node.data.vertexCount != data.vertexCount ||
            node.data.indexCount != data.indexCount;
        if (geometryLayoutChanged) node.pBLAS.Release();
        node.data = data;
        node.dirty = geometryLayoutChanged || !node.pBLAS;
        node.active = true;
        if (!node.pBLAS) {
            BLASTriangleDesc triangles;
            triangles.GeometryName = key.c_str();
            triangles.VertexValueType = VT_FLOAT32;
            triangles.VertexComponentCount = 3;
            triangles.MaxVertexCount = data.vertexCount;
            triangles.IndexType = data.pIndexBuffer ? VT_UINT32 : VT_UNDEFINED;
            triangles.MaxPrimitiveCount = data.pIndexBuffer ? data.indexCount / 3 : data.vertexCount / 3;
            BottomLevelASDesc desc;
            desc.Name = key.c_str();
            desc.Flags = RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
            desc.pTriangles = &triangles;
            desc.TriangleCount = 1;
            pDevice_->CreateBLAS(desc, &node.pBLAS);
        }
        node.transform = data.transform;
        caps_.registeredBLASCount = 0;
        for (const auto& [name, registered] : blasMap_) {
            if (registered.pBLAS) ++caps_.registeredBLASCount;
        }
        return node.pBLAS != nullptr;
    }

    bool buildBLAS(IDeviceContext* pContext) override {
        if (!rtSupported_ || !pDevice_ || !pContext) return false;
        for (auto& [name, node] : blasMap_) {
            if (!node.dirty) continue;
            if (!node.pBLAS || !node.data.pVertexBuffer) return false;
            const auto& vertexDesc = node.data.pVertexBuffer->GetDesc();
            BLASBuildTriangleData triangle;
            triangle.GeometryName = name.c_str();
            triangle.pVertexBuffer = node.data.pVertexBuffer;
            triangle.VertexStride = vertexDesc.ElementByteStride;
            triangle.VertexCount = node.data.vertexCount;
            triangle.VertexValueType = VT_FLOAT32;
            triangle.VertexComponentCount = 3;
            triangle.pIndexBuffer = node.data.pIndexBuffer;
            triangle.IndexType = node.data.pIndexBuffer ? VT_UINT32 : VT_UNDEFINED;
            triangle.PrimitiveCount = node.data.pIndexBuffer ? node.data.indexCount / 3 : node.data.vertexCount / 3;
            triangle.Flags = RAYTRACING_GEOMETRY_FLAG_OPAQUE;
            BuildBLASAttribs attribs;
            attribs.pBLAS = node.pBLAS;
            attribs.pTriangleData = &triangle;
            attribs.TriangleDataCount = 1;
            if (!pBLASScratchBuffer_ || pBLASScratchBuffer_->GetDesc().Size < node.pBLAS->GetScratchBufferSizes().Build) {
                BufferDesc scratchDesc;
                scratchDesc.Name = "RayTracing BLAS Scratch";
                scratchDesc.Usage = USAGE_DEFAULT;
                scratchDesc.BindFlags = BIND_RAY_TRACING;
            scratchDesc.Size = node.pBLAS->GetScratchBufferSizes().Build;
                pBLASScratchBuffer_.Release();
                pDevice_->CreateBuffer(scratchDesc, nullptr, &pBLASScratchBuffer_);
            }
            if (!pBLASScratchBuffer_) return false;
            attribs.pScratchBuffer = pBLASScratchBuffer_;
            attribs.BLASTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            attribs.GeometryTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            pContext->BuildBLAS(attribs);
            ++caps_.blasBuildCount;
            node.dirty = false;
        }
        return true;
    }

    bool updateInstanceTransform(const UniString& id,
                                 const Diligent::float4x4& transform) override {
        const auto it = blasMap_.find(id.toStdWString());
        if (it == blasMap_.end()) return false;
        auto& current = it->second.transform;
        const bool transformChanged =
            std::memcmp(current.Data(), transform.Data(), sizeof(float) * 16) != 0;
        const bool wasInactive = !it->second.active;
        if (!transformChanged && !wasInactive) {
            return false;
        }
        current = transform;
        it->second.active = true;
        return true;
    }

    bool hasBLAS(const UniString& id) const override {
        const auto it = blasMap_.find(id.toStdWString());
        return it != blasMap_.end() && it->second.pBLAS != nullptr;
    }

    bool setInstanceActive(const UniString& id, bool active) override {
        const auto it = blasMap_.find(id.toStdWString());
        if (it == blasMap_.end()) return false;
        if (it->second.active == active) return false;
        it->second.active = active;
        return true;
    }

    bool buildTLAS(IDeviceContext* pContext) override {
        caps_.lastBuildSucceeded = false;
        if (!rtSupported_ || !pDevice_ || !pContext || !pTLAS_) return false;

        if (!buildBLAS(pContext)) {
            caps_.lastBuildSucceeded = false;
            return false;
        }
        std::vector<TLASBuildInstanceData> instances;
        std::vector<std::string> names;
        instances.reserve(blasMap_.size());
        names.reserve(blasMap_.size());
        for (const auto& [name, node] : blasMap_) {
            if (!node.pBLAS) continue;
            names.push_back(std::string(name.begin(), name.end()));
            TLASBuildInstanceData instance;
            instance.InstanceName = names.back().c_str();
            instance.pBLAS = node.pBLAS;
            instance.Mask = node.active ? 0xFF : 0;
            instance.Transform.SetRotation(node.transform.Data(), 4);
            instance.Transform.SetTranslation(node.transform.m30, node.transform.m31, node.transform.m32);
            instances.push_back(instance);
        }
        if (instances.empty()) return false;
        if (instances.size() > caps_.maxInstancesPerTLAS) return false;
        BuildTLASAttribs attribs;
        attribs.pTLAS = pTLAS_;
        attribs.pInstances = instances.data();
        attribs.InstanceCount = static_cast<Uint32>(instances.size());
        const auto requiredScratchSize = std::max(
            pTLAS_->GetScratchBufferSizes().Build,
            pTLAS_->GetScratchBufferSizes().Update);
        if (!pTLASScratchBuffer_ || pTLASScratchBuffer_->GetDesc().Size < requiredScratchSize) {
            BufferDesc scratchDesc;
            scratchDesc.Name = "RayTracing TLAS Scratch";
            scratchDesc.Usage = USAGE_DEFAULT;
            scratchDesc.BindFlags = BIND_RAY_TRACING;
            scratchDesc.Size = requiredScratchSize;
            pTLASScratchBuffer_.Release();
            pDevice_->CreateBuffer(scratchDesc, nullptr, &pTLASScratchBuffer_);
        }
        if (!pTLASScratchBuffer_) return false;
        if (!pTLASInstanceBuffer_ || pTLASInstanceBuffer_->GetDesc().Size < TLAS_INSTANCE_DATA_SIZE * instances.size()) {
            BufferDesc instanceDesc;
            instanceDesc.Name = "RayTracing TLAS Instances";
            instanceDesc.Usage = USAGE_DEFAULT;
            instanceDesc.BindFlags = BIND_RAY_TRACING;
            instanceDesc.Size = TLAS_INSTANCE_DATA_SIZE * instances.size();
            pTLASInstanceBuffer_.Release();
            pDevice_->CreateBuffer(instanceDesc, nullptr, &pTLASInstanceBuffer_);
        }
        if (!pTLASInstanceBuffer_) return false;
        attribs.pScratchBuffer = pTLASScratchBuffer_;
        attribs.pInstanceBuffer = pTLASInstanceBuffer_;
        const auto instanceCount = static_cast<Uint32>(instances.size());
        attribs.Update = caps_.tlasBuilt && tlasInstanceCount_ == instanceCount;
        attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        attribs.InstanceBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        attribs.TLASTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        attribs.BLASTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        pContext->BuildTLAS(attribs);
        ++caps_.tlasBuildCount;
        caps_.tlasBuilt = true;
        tlasInstanceCount_ = instanceCount;
        caps_.activeInstanceCount = 0;
        for (const auto& [name, node] : blasMap_) {
            if (node.active && node.pBLAS) ++caps_.activeInstanceCount;
        }
        caps_.lastBuildSucceeded = true;
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

            shaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
            shaderCI.Desc.Name = "RT Warmup Closest Hit";
            shaderCI.EntryPoint = kWarmupHitName;
            pDevice_->CreateShader(shaderCI, &pWarmupClosestHit_);

            if (!pWarmupRayGen_ || !pWarmupMiss_ || !pWarmupClosestHit_) {
                return false;
            }

            RayTracingPipelineStateCreateInfoX psoCI("RT Warmup PSO");
            psoCI.AddGeneralShader(kWarmupRayGenName, pWarmupRayGen_);
            psoCI.AddGeneralShader(kWarmupMissName, pWarmupMiss_);
            psoCI.AddTriangleHitShader(kWarmupHitName, pWarmupClosestHit_);
            psoCI.RayTracingPipeline.MaxRecursionDepth = 1;
            psoCI.RayTracingPipeline.ShaderRecordSize = 0;
            psoCI.MaxAttributeSize = 8;
            psoCI.MaxPayloadSize = 16;
            psoCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            ShaderResourceVariableDesc Vars[] = 
            {
                {SHADER_TYPE_RAY_GEN, "g_OutputTex", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                {SHADER_TYPE_RAY_GEN, "g_TLAS", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
            };
            psoCI.PSODesc.ResourceLayout.Variables    = Vars;
            psoCI.PSODesc.ResourceLayout.NumVariables = 2;

            pDevice_->CreateRayTracingPipelineState(psoCI, &pRayTracingPSO_);
            if (pRayTracingPSO_) {
                bindWarmupOutput(pRayTracingPSO_, pTraceOutputUAV_);
                bindWarmupTLAS(pRayTracingPSO_, pTLAS_);
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
                pSBT_->BindHitGroupForTLAS(pTLAS_, 0, kWarmupHitName);
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
        if (!rtSupported_ || !pDevice_ || !caps_.tlasBuilt) {
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
        tlasDesc.Flags = RAYTRACING_BUILD_AS_ALLOW_UPDATE |
                         RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
        pDevice_->CreateTLAS(tlasDesc, &pTLAS_);
        caps_.tlasCreated = pTLAS_ != nullptr;
    }

    RefCntAutoPtr<IRenderDevice> pDevice_;
    RefCntAutoPtr<IBottomLevelAS> pUnitQuadBLAS_;
    RefCntAutoPtr<ITopLevelAS> pTLAS_;
    RefCntAutoPtr<IBuffer> pBLASScratchBuffer_;
    RefCntAutoPtr<IBuffer> pTLASScratchBuffer_;
    RefCntAutoPtr<IBuffer> pTLASInstanceBuffer_;
    RefCntAutoPtr<ITexture> pTraceOutputTexture_;
    RefCntAutoPtr<ITextureView> pTraceOutputSRV_;
    RefCntAutoPtr<ITextureView> pTraceOutputUAV_;
    RefCntAutoPtr<IShader> pWarmupRayGen_;
    RefCntAutoPtr<IShader> pWarmupMiss_;
    RefCntAutoPtr<IShader> pWarmupClosestHit_;
    RefCntAutoPtr<IPipelineState> pRayTracingPSO_;
    RefCntAutoPtr<IShaderBindingTable> pSBT_;
    
    struct BLASNode {
        RefCntAutoPtr<IBottomLevelAS> pBLAS;
        Diligent::float4x4 transform;
        RTGeometryData data;
        bool dirty = false;
        bool active = true;
    };
    std::map<std::wstring, BLASNode> blasMap_;
    Uint32 tlasInstanceCount_ = 0;
    RayTracingCapabilities caps_;
    bool rtSupported_ = false;
};

std::unique_ptr<IRayTracingManager> createRayTracingManager() {
    return std::make_unique<RayTracingManager>();
}

} // namespace ArtifactCore
