module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <QString>
#include <QDebug>

module Graphics.MeshRenderer;

import std;
import Frame.Debug;
import Graphics.ParticleData;

namespace ArtifactCore {

const char* MeshVSSource = R"(
struct VSInput {
    float3 pos    : POSITION;
    float3 normal : NORMAL;
    float2 uv     : TEXCOORD0;
};

struct PSInput {
    float4 Pos   : SV_Position;
    float3 Normal : NORMAL;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR;
};

struct InstanceData {
    float4x4 transform;
    float4 color;
    float weight;
    float timeOffset;
    float2 padding;
};

StructuredBuffer<InstanceData> g_Instances : register(t0);

cbuffer Constants : register(b0) {
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
};

PSInput VSMain(VSInput In, uint InstanceID : SV_InstanceID) {
    PSInput Out;
    InstanceData inst = g_Instances[InstanceID];
    
    // Apply instance transform
    float4 worldPos = mul(float4(In.pos, 1.0), inst.transform);
    float4 viewPos = mul(worldPos, ViewMatrix);
    Out.Pos = mul(viewPos, ProjMatrix);
    
    // Transform normal (use upper-left 3x3 of transform)
    float3x3 rotMat = (float3x3)inst.transform;
    Out.Normal = mul(In.normal, rotMat);
    
    Out.UV = In.uv;
    Out.Color = inst.color * inst.weight; // weight acts as alpha multiplier
    return Out;
}
)";

const char* MeshPSSource = R"(
struct PSInput {
    float4 Pos   : SV_Position;
    float3 Normal : NORMAL;
    float2 UV    : TEXCOORD0;
    float4 Color : COLOR;
};

float4 PSMain(PSInput In) : SV_Target {
    // Simple lighting
    float3 lightDir = normalize(float3(0.5, 0.8, 1.0));
    float NdotL = max(dot(normalize(In.Normal), lightDir), 0.0);
    float3 litColor = In.Color.rgb * (0.3 + 0.7 * NdotL);
    return float4(litColor, In.Color.a);
}
)";

struct MeshRenderer::Impl {
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pPSO_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> pSRB_;
    
    // Mesh geometry buffers
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pPositionBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pNormalBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pUVBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pIndexBuffer_;
    
    // Instance data buffer
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pInstanceBuffer_;
    
    // Constant buffer for view/proj matrices
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pConstantBuffer_;
    
    size_t vertexCount_ = 0;
    size_t indexCount_ = 0;
    size_t maxInstances_ = 0;
};

MeshRenderer::MeshRenderer(GpuContext& context)
    : context_(context), pImpl_(new Impl())
{
}

MeshRenderer::~MeshRenderer()
{
    delete pImpl_;
}

void MeshRenderer::initialize(size_t maxInstances, size_t vertexCount, size_t indexCount)
{
    maxInstances_ = maxInstances;
    vertexCount_ = vertexCount;
    indexCount_ = indexCount;
    createBuffers();
    createPSO();
}

void MeshRenderer::setFrameCostStats(ArtifactCore::RenderCostStats* stats)
{
    frameCostStats_ = stats;
}

void MeshRenderer::createBuffers()
{
    auto pDevice = context_.D3D12RenderDevice();
    
    // 1. Position buffer (always needed)
    if (vertexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Position Buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.Size              = sizeof(float) * 3 * vertexCount_;
        BuffDesc.BindFlags         = BIND_VERTEX_BUFFER;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pPositionBuffer_);
    }
    
    // 2. Normal buffer (optional, can be created later)
    // 3. UV buffer (optional, can be created later)
    
    // 4. Index buffer (if indexed rendering)
    if (indexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Index Buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.Size              = sizeof(uint32_t) * indexCount_;
        BuffDesc.BindFlags         = BIND_INDEX_BUFFER;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pIndexBuffer_);
    }
    
    // 5. Instance buffer (Structured Buffer)
    if (maxInstances_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Instance Structured Buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.Size              = sizeof(InstanceData) * maxInstances_;
        BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
        BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
        BuffDesc.ElementByteStride = sizeof(InstanceData);
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pInstanceBuffer_);
    }
    
    // 6. Constant buffer
    {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Constants CB";
        BuffDesc.Usage             = USAGE_DYNAMIC;
        BuffDesc.Size              = sizeof(ShaderConstants);
        BuffDesc.BindFlags         = BIND_UNIFORM_BUFFER;
        BuffDesc.CPUAccessFlags    = CPU_ACCESS_WRITE;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pConstantBuffer_);
    }
}

void MeshRenderer::createPSO()
{
    auto pDevice = context_.D3D12RenderDevice();
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    
    PSOCreateInfo.PSODesc.Name = "Mesh Instancing PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    
    // Triangle list for mesh rendering
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = DefaultParticleRTVFormat; // Reuse particle RTV format
    
    // Alpha blending
    auto& RT0 = PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0];
    RT0.BlendEnable = true;
    RT0.SrcBlend    = BLEND_FACTOR_SRC_ALPHA;
    RT0.DestBlend   = BLEND_FACTOR_INV_SRC_ALPHA;
    RT0.BlendOp     = BLEND_OPERATION_ADD;
    
    // Depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;
    
    // Vertex layout
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = 3;
    std::array<LayoutElement, 3> layoutElements;
    // Position
    layoutElements[0].HLSLSemantic = "POSITION";
    layoutElements[0].InputIndex = 0;
    layoutElements[0].BufferSlot = 0;
    layoutElements[0].NumComponents = 3;
    layoutElements[0].ValueType = VT_FLOAT32;
    layoutElements[0].IsNormalized = false;
    // Normal
    layoutElements[1].HLSLSemantic = "NORMAL";
    layoutElements[1].InputIndex = 1;
    layoutElements[1].BufferSlot = 1;
    layoutElements[1].NumComponents = 3;
    layoutElements[1].ValueType = VT_FLOAT32;
    layoutElements[1].IsNormalized = false;
    // UV
    layoutElements[2].HLSLSemantic = "TEXCOORD";
    layoutElements[2].InputIndex = 2;
    layoutElements[2].BufferSlot = 2;
    layoutElements[2].NumComponents = 2;
    layoutElements[2].ValueType = VT_FLOAT32;
    layoutElements[2].IsNormalized = false;
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = layoutElements.data();
    
    // Compile Shaders
    RefCntAutoPtr<IShader> vs, ps;
    context_.CompileShader(MeshVSSource, SHADER_TYPE_VERTEX, "VSMain", &vs);
    context_.CompileShader(MeshPSSource, SHADER_TYPE_PIXEL,  "PSMain", &ps);
    
    PSOCreateInfo.pVS = vs;
    PSOCreateInfo.pPS = ps;
    
    // Resource layout
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    
    static std::array<ShaderResourceVariableDesc, 1> Vars = {{
        {SHADER_TYPE_VERTEX, "g_Instances", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    }};
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars.data();
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = (Uint32)Vars.size();
    
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pImpl_->pPSO_);
    
    if (!pImpl_->pPSO_) {
        qWarning("[MeshRenderer] PSO creation FAILED");
        return;
    }
    qDebug() << "[MeshRenderer] PSO created successfully";
    
    // Bind constant buffer
    auto* pConstVar = pImpl_->pPSO_->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
    if (pConstVar) {
        pConstVar->Set(pImpl_->pConstantBuffer_);
    }
    
    pImpl_->pPSO_->CreateShaderResourceBinding(&pImpl_->pSRB_, true);
}

void MeshRenderer::updateMeshGeometry(const float* positions, const float* normals, const float* uvs,
                                      const uint32_t* indices)
{
    auto pContext = context_.D3D12DeviceContext();
    
    if (positions && pImpl_->pPositionBuffer_) {
        pContext->UpdateBuffer(pImpl_->pPositionBuffer_, 0, sizeof(float) * 3 * vertexCount_,
                              positions, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (frameCostStats_) ++frameCostStats_->bufferUpdates;
    }
    
    if (indices && pImpl_->pIndexBuffer_) {
        pContext->UpdateBuffer(pImpl_->pIndexBuffer_, 0, sizeof(uint32_t) * indexCount_,
                              indices, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (frameCostStats_) ++frameCostStats_->bufferUpdates;
    }
}

void MeshRenderer::updateInstanceData(const InstanceData* instances, size_t count)
{
    if (!instances || count == 0 || !pImpl_->pInstanceBuffer_) return;
    
    auto pContext = context_.D3D12DeviceContext();
    size_t uploadSize = sizeof(InstanceData) * std::min(count, maxInstances_);
    
    pContext->UpdateBuffer(pImpl_->pInstanceBuffer_, 0, uploadSize,
                          instances, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (frameCostStats_) ++frameCostStats_->bufferUpdates;
}

void MeshRenderer::prepare(IDeviceContext* pContext)
{
    if (!pImpl_->pPSO_ || !pImpl_->pSRB_) {
        qWarning("[MeshRenderer] prepare() called but PSO/SRB is null");
        return;
    }
    
    // Update constants
    void* pData = nullptr;
    pContext->MapBuffer(pImpl_->pConstantBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (pData) {
        memcpy(pData, &constants_, sizeof(ShaderConstants));
        pContext->UnmapBuffer(pImpl_->pConstantBuffer_, MAP_WRITE);
        if (frameCostStats_) ++frameCostStats_->bufferUpdates;
    }
    
    // Bind instance buffer SRV
    if (pImpl_->pSRB_) {
        auto* pVar = pImpl_->pSRB_->GetVariableByName(SHADER_TYPE_VERTEX, "g_Instances");
        if (pVar) {
            pVar->Set(pImpl_->pInstanceBuffer_->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
        }
    }
    
    pContext->SetPipelineState(pImpl_->pPSO_);
    if (frameCostStats_) {
        ++frameCostStats_->psoSwitches;
        ++frameCostStats_->srbCommits;
    }
    pContext->CommitShaderResources(pImpl_->pSRB_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // Set vertex buffers
    if (pImpl_->pPositionBuffer_) {
        IBuffer* pVBs[] = {pImpl_->pPositionBuffer_};
        pContext->SetVertexBuffers(0, 1, pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                  SET_VERTEX_BUFFERS_FLAG_RESET);
    }
    
    // Set index buffer if available
    if (pImpl_->pIndexBuffer_) {
        pContext->SetIndexBuffer(pImpl_->pIndexBuffer_, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
}

void MeshRenderer::draw(IDeviceContext* pContext, size_t instanceCount)
{
    if (instanceCount == 0) return;
    
    if (pImpl_->pIndexBuffer_ && indexCount_ > 0) {
        // Indexed instanced draw
        DrawIndexedAttribs drawAttrs;
        drawAttrs.NumIndices  = (Uint32)indexCount_;
        drawAttrs.NumInstances = (Uint32)instanceCount;
        drawAttrs.IndexType   = VT_UINT32;
        drawAttrs.Flags       = DRAW_FLAG_NONE;
        if (frameCostStats_) ++frameCostStats_->drawCalls;
        pContext->DrawIndexed(drawAttrs);
    } else if (vertexCount_ > 0) {
        // Non-indexed instanced draw
        DrawAttribs drawAttrs;
        drawAttrs.NumVertices  = (Uint32)vertexCount_;
        drawAttrs.NumInstances = (Uint32)instanceCount;
        drawAttrs.Flags        = DRAW_FLAG_NONE;
        if (frameCostStats_) ++frameCostStats_->drawCalls;
        pContext->Draw(drawAttrs);
    }
}

void MeshRenderer::setViewMatrix(const float* matrix)
{
    memcpy(constants_.viewMatrix, matrix, sizeof(float) * 16);
}

void MeshRenderer::setProjectionMatrix(const float* matrix)
{
    memcpy(constants_.projMatrix, matrix, sizeof(float) * 16);
}

} // namespace ArtifactCore
