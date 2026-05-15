module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <QString>
#include <QDebug>
#include <QFileInfo>

module Graphics.MeshRenderer;

import std;
import Frame.Debug;
import Graphics.ParticleData;
import IO.ImageImporter;
import Image.Raw;

namespace ArtifactCore {

namespace {
void transpose4x4(const float* src, float* dst)
{
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            dst[row * 4 + col] = src[col * 4 + row];
        }
    }
}

QVector<quint8> expandTextureToRgba8(const ArtifactCore::RawImage& rawImage,
                                    bool alphaFromLuminance)
{
    QVector<quint8> rgba8;
    const int pixelCount = rawImage.width * rawImage.height;
    if (pixelCount <= 0 || rawImage.channels <= 0) {
        return rgba8;
    }

    const int srcPixelSize = rawImage.getPixelTypeSizeInBytes();
    if (srcPixelSize <= 0) {
        return rgba8;
    }

    rgba8.resize(pixelCount * 4);
    const quint8* srcBytes = rawImage.data.constData();
    const int srcStride = rawImage.channels * srcPixelSize;

    auto sampleChannel = [&](int pixelIndex, int channelIndex) -> quint8 {
        const int srcIndex = pixelIndex * srcStride + channelIndex * srcPixelSize;
        if (rawImage.pixelType == QStringLiteral("uint8")) {
            if (srcIndex < 0 || srcIndex >= rawImage.data.size()) return 0;
            return rawImage.data[static_cast<size_t>(srcIndex)];
        }
        if (rawImage.pixelType == QStringLiteral("uint16")) {
            quint16 value = 0;
            std::memcpy(&value, srcBytes + srcIndex, sizeof(quint16));
            return static_cast<quint8>(value / 257u);
        }
        if (rawImage.pixelType == QStringLiteral("float")) {
            float value = 0.0f;
            std::memcpy(&value, srcBytes + srcIndex, sizeof(float));
            value = std::clamp(value, 0.0f, 1.0f);
            return static_cast<quint8>(std::lround(value * 255.0f));
        }
        return 0;
    };

    for (int i = 0; i < pixelCount; ++i) {
        const quint8 c0 = sampleChannel(i, 0);
        const quint8 c1 = (rawImage.channels > 1) ? sampleChannel(i, 1) : c0;
        const quint8 c2 = (rawImage.channels > 2) ? sampleChannel(i, 2) : c0;
        const quint8 c3 = (rawImage.channels > 3) ? sampleChannel(i, 3) : 255;
        const quint8 alpha = alphaFromLuminance
                                 ? static_cast<quint8>((static_cast<int>(c0) + static_cast<int>(c1) + static_cast<int>(c2)) / 3)
                                 : c3;
        rgba8[i * 4 + 0] = c0;
        rgba8[i * 4 + 1] = c1;
        rgba8[i * 4 + 2] = c2;
        rgba8[i * 4 + 3] = alpha;
    }

    return rgba8;
}
}

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

Texture2D g_BaseColorTexture : register(t0);
Texture2D g_OpacityTexture : register(t1);
SamplerState g_BaseColorSampler : register(s0);

float4 PSMain(PSInput In) : SV_Target {
    float4 baseSample = g_BaseColorTexture.Sample(g_BaseColorSampler, In.UV);
    float4 opacitySample = g_OpacityTexture.Sample(g_BaseColorSampler, In.UV);
    float4 baseColor = baseSample * In.Color;

    // Simple lighting
    float3 lightDir = normalize(float3(0.5, 0.8, 1.0));
    float NdotL = max(dot(normalize(In.Normal), lightDir), 0.0);
    float3 litColor = baseColor.rgb * (0.3 + 0.7 * NdotL);
    return float4(litColor, baseColor.a * opacitySample.a);
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
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pBaseColorTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pBaseColorTextureSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pOpacityTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pOpacityTextureSRV_;
    Diligent::RefCntAutoPtr<Diligent::ISampler>               pBaseColorSampler_;
    
    // Instance data buffer
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pInstanceBuffer_;
    
    // Constant buffer for view/proj matrices
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pConstantBuffer_;
    
    size_t vertexCount_ = 0;
    size_t indexCount_ = 0;
    size_t maxInstances_ = 0;
    QString baseColorTexturePath_;
    QString opacityTexturePath_;
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
    auto pDevice = context_.RenderDevice();
    
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
    
    // 2. Normal buffer
    if (vertexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Normal Buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.Size              = sizeof(float) * 3 * vertexCount_;
        BuffDesc.BindFlags         = BIND_VERTEX_BUFFER;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pNormalBuffer_);
    }
    // 3. UV buffer
    if (vertexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh UV Buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.Size              = sizeof(float) * 2 * vertexCount_;
        BuffDesc.BindFlags         = BIND_VERTEX_BUFFER;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pUVBuffer_);
    }
    
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

    if (!pImpl_->pBaseColorSampler_) {
        SamplerDesc samplerDesc;
        samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
        samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
        samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
        samplerDesc.AddressU = TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = TEXTURE_ADDRESS_WRAP;
        samplerDesc.ComparisonFunc = COMPARISON_FUNC_ALWAYS;
        samplerDesc.MaxAnisotropy = 1;
        pDevice->CreateSampler(samplerDesc, &pImpl_->pBaseColorSampler_);
    }

    if (!pImpl_->pBaseColorTexture_) {
        const Uint8 whitePixel[4] = {255, 255, 255, 255};
        TextureDesc texDesc;
        texDesc.Name = "MeshRenderer_WhiteTexture";
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
        texDesc.Usage = USAGE_IMMUTABLE;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;
        TextureSubResData subRes;
        subRes.pData = whitePixel;
        subRes.Stride = 4;
        TextureData initData;
        initData.pSubResources = &subRes;
        initData.NumSubresources = 1;
        pDevice->CreateTexture(texDesc, &initData, &pImpl_->pBaseColorTexture_);
        if (pImpl_->pBaseColorTexture_) {
            pImpl_->pBaseColorTextureSRV_ =
                pImpl_->pBaseColorTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
    }

    if (!pImpl_->pOpacityTexture_) {
        const Uint8 whitePixel[4] = {255, 255, 255, 255};
        TextureDesc texDesc;
        texDesc.Name = "MeshRenderer_OpacityWhiteTexture";
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
        texDesc.Usage = USAGE_IMMUTABLE;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;
        TextureSubResData subRes;
        subRes.pData = whitePixel;
        subRes.Stride = 4;
        TextureData initData;
        initData.pSubResources = &subRes;
        initData.NumSubresources = 1;
        pDevice->CreateTexture(texDesc, &initData, &pImpl_->pOpacityTexture_);
        if (pImpl_->pOpacityTexture_) {
            pImpl_->pOpacityTextureSRV_ =
                pImpl_->pOpacityTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
    }
}

void MeshRenderer::createPSO()
{
    auto pDevice = context_.RenderDevice();
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
    
    static std::array<ShaderResourceVariableDesc, 4> Vars = {{
        {SHADER_TYPE_VERTEX, "g_Instances", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_BaseColorTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_OpacityTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_BaseColorSampler", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
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
    if (auto* texVar = pImpl_->pPSO_->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_BaseColorTexture")) {
        texVar->Set(pImpl_->pBaseColorTextureSRV_);
    }
    if (auto* opacityVar = pImpl_->pPSO_->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_OpacityTexture")) {
        opacityVar->Set(pImpl_->pOpacityTextureSRV_);
    }
    if (auto* sampVar = pImpl_->pPSO_->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_BaseColorSampler")) {
        sampVar->Set(pImpl_->pBaseColorSampler_);
    }
    
    pImpl_->pPSO_->CreateShaderResourceBinding(&pImpl_->pSRB_, true);
}

void MeshRenderer::updateMeshGeometry(const float* positions, const float* normals, const float* uvs,
                                      const uint32_t* indices)
{
    auto pContext = context_.DeviceContext();
    
    if (positions && pImpl_->pPositionBuffer_) {
        pContext->UpdateBuffer(pImpl_->pPositionBuffer_, 0, sizeof(float) * 3 * vertexCount_,
                              positions, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (frameCostStats_) ++frameCostStats_->bufferUpdates;
    }

    if (normals && pImpl_->pNormalBuffer_) {
        pContext->UpdateBuffer(pImpl_->pNormalBuffer_, 0, sizeof(float) * 3 * vertexCount_,
                              normals, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (frameCostStats_) ++frameCostStats_->bufferUpdates;
    }

    if (uvs && pImpl_->pUVBuffer_) {
        pContext->UpdateBuffer(pImpl_->pUVBuffer_, 0, sizeof(float) * 2 * vertexCount_,
                              uvs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
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
    
    auto pContext = context_.DeviceContext();
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
        if (auto* texVar = pImpl_->pSRB_->GetVariableByName(SHADER_TYPE_PIXEL, "g_BaseColorTexture")) {
            texVar->Set(pImpl_->pBaseColorTextureSRV_);
        }
        if (auto* opacityVar = pImpl_->pSRB_->GetVariableByName(SHADER_TYPE_PIXEL, "g_OpacityTexture")) {
            opacityVar->Set(pImpl_->pOpacityTextureSRV_);
        }
        if (auto* sampVar = pImpl_->pSRB_->GetVariableByName(SHADER_TYPE_PIXEL, "g_BaseColorSampler")) {
            sampVar->Set(pImpl_->pBaseColorSampler_);
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
        IBuffer* pVBs[] = {pImpl_->pPositionBuffer_, pImpl_->pNormalBuffer_, pImpl_->pUVBuffer_};
        pContext->SetVertexBuffers(0, 3, pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
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
    transpose4x4(matrix, constants_.viewMatrix);
}

void MeshRenderer::setProjectionMatrix(const float* matrix)
{
    transpose4x4(matrix, constants_.projMatrix);
}

void MeshRenderer::setBaseColorTexture(const QString& path)
{
    const QString newPath = path.trimmed();
    auto pDevice = context_.RenderDevice();
    if (newPath == pImpl_->baseColorTexturePath_ && pImpl_->pBaseColorTextureSRV_) {
        return;
    }

    if (newPath.isEmpty() || !pDevice) {
        clearBaseColorTexture();
        return;
    }

    pImpl_->baseColorTexturePath_ = newPath;

    ArtifactCore::ImageImporter importer;
    if (!importer.open(pImpl_->baseColorTexturePath_)) {
        qWarning() << "[MeshRenderer] Failed to open texture path:" << pImpl_->baseColorTexturePath_;
        clearBaseColorTexture();
        return;
    }

    const ArtifactCore::RawImage rawImage = importer.readImage();
    if (!rawImage.isValid() || rawImage.width <= 0 || rawImage.height <= 0) {
        qWarning() << "[MeshRenderer] Failed to read texture image:" << pImpl_->baseColorTexturePath_;
        clearBaseColorTexture();
        return;
    }

    TextureDesc texDesc;
    texDesc.Name = "MeshRenderer_BaseColorTexture";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = static_cast<Uint32>(rawImage.width);
    texDesc.Height = static_cast<Uint32>(rawImage.height);
    texDesc.MipLevels = 1;
    texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
    texDesc.Usage = USAGE_IMMUTABLE;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;

    QVector<quint8> rgba8 = expandTextureToRgba8(rawImage, false);
    if (rgba8.isEmpty()) {
        qWarning() << "[MeshRenderer] Unsupported texture pixel type:"
                   << pImpl_->baseColorTexturePath_
                   << rawImage.pixelType;
        clearBaseColorTexture();
        return;
    }

    TextureSubResData subRes;
    subRes.pData = rgba8.constData();
    subRes.Stride = static_cast<Uint64>(rawImage.width * 4);

    TextureData initData;
    initData.pSubResources = &subRes;
    initData.NumSubresources = 1;

    pDevice->CreateTexture(texDesc, &initData, &pImpl_->pBaseColorTexture_);
    if (pImpl_->pBaseColorTexture_) {
        pImpl_->pBaseColorTextureSRV_ =
            pImpl_->pBaseColorTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
}

void MeshRenderer::clearBaseColorTexture()
{
    if (pImpl_->baseColorTexturePath_.isEmpty() && pImpl_->pBaseColorTextureSRV_) {
        return;
    }
    pImpl_->baseColorTexturePath_.clear();
    auto pDevice = context_.RenderDevice();
    if (!pDevice) {
        pImpl_->pBaseColorTexture_ = nullptr;
        pImpl_->pBaseColorTextureSRV_ = nullptr;
        return;
    }

    const Uint8 whitePixel[4] = {255, 255, 255, 255};
    TextureDesc texDesc;
    texDesc.Name = "MeshRenderer_WhiteTexture";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
    texDesc.Usage = USAGE_IMMUTABLE;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;
    TextureSubResData subRes;
    subRes.pData = whitePixel;
    subRes.Stride = 4;
    TextureData initData;
    initData.pSubResources = &subRes;
    initData.NumSubresources = 1;
    pDevice->CreateTexture(texDesc, &initData, &pImpl_->pBaseColorTexture_);
    if (pImpl_->pBaseColorTexture_) {
        pImpl_->pBaseColorTextureSRV_ =
            pImpl_->pBaseColorTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
}

void MeshRenderer::setOpacityTexture(const QString& path)
{
    const QString newPath = path.trimmed();
    auto pDevice = context_.RenderDevice();
    if (newPath == pImpl_->opacityTexturePath_ && pImpl_->pOpacityTextureSRV_) {
        return;
    }

    if (newPath.isEmpty() || !pDevice) {
        clearOpacityTexture();
        return;
    }

    pImpl_->opacityTexturePath_ = newPath;

    ArtifactCore::ImageImporter importer;
    if (!importer.open(pImpl_->opacityTexturePath_)) {
        qWarning() << "[MeshRenderer] Failed to open opacity texture path:" << pImpl_->opacityTexturePath_;
        clearOpacityTexture();
        return;
    }

    const ArtifactCore::RawImage rawImage = importer.readImage();
    if (!rawImage.isValid() || rawImage.width <= 0 || rawImage.height <= 0) {
        qWarning() << "[MeshRenderer] Failed to read opacity texture image:" << pImpl_->opacityTexturePath_;
        clearOpacityTexture();
        return;
    }

    TextureDesc texDesc;
    texDesc.Name = "MeshRenderer_OpacityTexture";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = static_cast<Uint32>(rawImage.width);
    texDesc.Height = static_cast<Uint32>(rawImage.height);
    texDesc.MipLevels = 1;
    texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
    texDesc.Usage = USAGE_IMMUTABLE;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;

    QVector<quint8> rgba8 = expandTextureToRgba8(rawImage, true);
    if (rgba8.isEmpty()) {
        qWarning() << "[MeshRenderer] Unsupported opacity texture pixel type:"
                   << pImpl_->opacityTexturePath_
                   << rawImage.pixelType;
        clearOpacityTexture();
        return;
    }

    TextureSubResData subRes;
    subRes.pData = rgba8.constData();
    subRes.Stride = static_cast<Uint64>(rawImage.width * 4);

    TextureData initData;
    initData.pSubResources = &subRes;
    initData.NumSubresources = 1;

    pDevice->CreateTexture(texDesc, &initData, &pImpl_->pOpacityTexture_);
    if (pImpl_->pOpacityTexture_) {
        pImpl_->pOpacityTextureSRV_ =
            pImpl_->pOpacityTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
}

void MeshRenderer::clearOpacityTexture()
{
    if (pImpl_->opacityTexturePath_.isEmpty() && pImpl_->pOpacityTextureSRV_) {
        return;
    }
    pImpl_->opacityTexturePath_.clear();
    auto pDevice = context_.RenderDevice();
    if (!pDevice) {
        pImpl_->pOpacityTexture_ = nullptr;
        pImpl_->pOpacityTextureSRV_ = nullptr;
        return;
    }

    const Uint8 whitePixel[4] = {255, 255, 255, 255};
    TextureDesc texDesc;
    texDesc.Name = "MeshRenderer_OpacityWhiteTexture";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
    texDesc.Usage = USAGE_IMMUTABLE;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;
    TextureSubResData subRes;
    subRes.pData = whitePixel;
    subRes.Stride = 4;
    TextureData initData;
    initData.pSubResources = &subRes;
    initData.NumSubresources = 1;
    pDevice->CreateTexture(texDesc, &initData, &pImpl_->pOpacityTexture_);
    if (pImpl_->pOpacityTexture_) {
        pImpl_->pOpacityTextureSRV_ =
            pImpl_->pOpacityTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
}

} // namespace ArtifactCore

