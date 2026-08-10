module;
#include <utility>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineState.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/PipelineStateCache.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <QString>
#include <QColor>
#include <QDebug>
#include <QFileInfo>

module Graphics.MeshRenderer;

import std;
import Frame.Debug;
import Graphics.ParticleData;
import Graphics.Compute;
import IO.ImageImporter;
import Image.Raw;

namespace ArtifactCore {

struct MeshCullConstants {
    float viewMatrix[16] = {};
    float projMatrix[16] = {};
    Uint32 inputCount = 0;
    Uint32 outputCapacity = 0;
    float boundsRadius = 0.0f;
    float padding = 0.0f;
};

const char* MeshCullCSSource = R"(
struct InstanceData {
    float4x4 transform;
    float4x4 previousTransform;
    float4 color;
    float weight;
    float timeOffset;
    float2 padding;
};
StructuredBuffer<InstanceData> g_Input : register(t0);
RWStructuredBuffer<InstanceData> g_Output : register(u0);
RWStructuredBuffer<uint> g_Args : register(u1);
cbuffer CullConstants : register(b0) {
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    uint InputCount;
    uint OutputCapacity;
    float BoundsRadius;
    float Padding;
};
[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= InputCount) return;
    InstanceData inst = g_Input[id.x];
    if (inst.weight <= 0.0 || inst.color.a <= 0.0) return;
    float4 worldCenter = mul(float4(0.0, 0.0, 0.0, 1.0), inst.transform);
    float4 viewCenter = mul(worldCenter, ViewMatrix);
    float4 clip = mul(viewCenter, ProjMatrix);
    float3 sx = float3(inst.transform[0][0], inst.transform[0][1], inst.transform[0][2]);
    float3 sy = float3(inst.transform[1][0], inst.transform[1][1], inst.transform[1][2]);
    float3 sz = float3(inst.transform[2][0], inst.transform[2][1], inst.transform[2][2]);
    float scale = max(length(sx), max(length(sy), length(sz)));
    float projectionScale = max(abs(ProjMatrix[0][0]), abs(ProjMatrix[1][1]));
    float margin = max(0.001, BoundsRadius * scale * projectionScale);
    bool visible = clip.w > 0.00001 &&
        clip.x >= -clip.w - margin && clip.x <= clip.w + margin &&
        clip.y >= -clip.w - margin && clip.y <= clip.w + margin &&
        clip.z >= -margin && clip.z <= clip.w + margin;
    if (!visible) return;
    uint dst;
    InterlockedAdd(g_Args[1], 1, dst);
    if (dst < OutputCapacity) g_Output[dst] = inst;
}
)";

namespace {
void transpose4x4(const float* src, float* dst)
{
    if (!src || !dst) {
        return;
    }
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

bool loadLinearTexture(ArtifactCore::GpuContext& context, const QString& path,
                       const char* debugName,
                       Diligent::RefCntAutoPtr<Diligent::ITexture>& texture,
                       Diligent::RefCntAutoPtr<Diligent::ITextureView>& textureSRV)
{
    auto* device = context.RenderDevice();
    if (!device) {
        return false;
    }
    ArtifactCore::ImageImporter importer;
    if (!importer.open(path)) {
        qWarning() << "[MeshRenderer] Failed to open linear texture:" << path;
        return false;
    }
    const ArtifactCore::RawImage rawImage = importer.readImage();
    if (!rawImage.isValid() || rawImage.width <= 0 || rawImage.height <= 0) {
        qWarning() << "[MeshRenderer] Failed to read linear texture:" << path;
        return false;
    }
    QVector<quint8> rgba8 = expandTextureToRgba8(rawImage, false);
    if (rgba8.isEmpty()) {
        qWarning() << "[MeshRenderer] Unsupported linear texture:" << path
                   << rawImage.pixelType;
        return false;
    }

    Diligent::TextureDesc texDesc;
    texDesc.Name = debugName;
    texDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    texDesc.Width = static_cast<Diligent::Uint32>(rawImage.width);
    texDesc.Height = static_cast<Diligent::Uint32>(rawImage.height);
    texDesc.MipLevels = 1;
    texDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    texDesc.Usage = Diligent::USAGE_IMMUTABLE;
    texDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData subRes;
    subRes.pData = rgba8.constData();
    subRes.Stride = static_cast<Diligent::Uint64>(rawImage.width * 4);
    Diligent::TextureData initData;
    initData.pSubResources = &subRes;
    initData.NumSubresources = 1;
    device->CreateTexture(texDesc, &initData, &texture);
    if (!texture) {
        return false;
    }
    textureSRV = texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    return textureSRV.RawPtr() != nullptr;
}
}

const char* MeshVSSource = R"(
struct VSInput {
    float3 pos    : ATTRIB0;
    float3 normal : ATTRIB1;
    float2 uv     : ATTRIB2;
};

struct PSInput {
    float4 Pos   : SV_Position;
    float4 PrevPos : TEXCOORD2;
    float3 Normal : NORMAL;
    float3 WorldPosition : TEXCOORD3;
    float3 WorldNormal : TEXCOORD4;
    float3 ViewPosition : TEXCOORD5;
    float2 UV    : TEXCOORD0;
    float Mode   : TEXCOORD1;
    float4 Color : COLOR;
};

struct InstanceData {
    float4x4 transform;
    float4x4 previousTransform;
    float4 color;
    float weight;
    float timeOffset;
    float2 padding;
};

StructuredBuffer<InstanceData> g_Instances : register(t0);

cbuffer Constants : register(b0) {
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float4x4 PrevViewMatrix;
    float4x4 PrevProjMatrix;
};

PSInput VSMain(VSInput In, uint InstanceID : SV_InstanceID) {
    PSInput Out;
    InstanceData inst = g_Instances[InstanceID];
    
    // Apply instance transform
    float4 worldPos = mul(float4(In.pos, 1.0), inst.transform);
    Out.WorldPosition = worldPos.xyz;
    float4 viewPos = mul(worldPos, ViewMatrix);
    Out.ViewPosition = viewPos.xyz;
    Out.Pos = mul(viewPos, ProjMatrix);
    float4 prevWorldPos = mul(float4(In.pos, 1.0), inst.previousTransform);
    float4 prevViewPos = mul(prevWorldPos, PrevViewMatrix);
    Out.PrevPos = mul(prevViewPos, PrevProjMatrix);
    
    // Transform the normal into view space so studio lighting follows the camera.
    float3x3 rotMat = (float3x3)inst.transform;
    Out.WorldNormal = mul(In.normal, rotMat);
    Out.Normal = mul(Out.WorldNormal, (float3x3)ViewMatrix);
    
    Out.UV = In.uv;
    Out.Mode = inst.timeOffset;
    Out.Color = inst.color * inst.weight; // weight acts as alpha multiplier
    return Out;
}
)";

const char* MeshPSSource = R"(
struct PSInput {
    float4 Pos   : SV_Position;
    float4 PrevPos : TEXCOORD2;
    float3 Normal : NORMAL;
    float3 WorldPosition : TEXCOORD3;
    float3 WorldNormal : TEXCOORD4;
    float3 ViewPosition : TEXCOORD5;
    float2 UV    : TEXCOORD0;
    float Mode   : TEXCOORD1;
    float4 Color : COLOR;
};

Texture2D g_BaseColorTexture : register(t0);
Texture2D g_OpacityTexture : register(t1);
Texture2D g_EmissionTexture : register(t2);
Texture2D g_MetallicRoughnessTexture : register(t3);
Texture2D g_NormalTexture : register(t4);
Texture2D g_OcclusionTexture : register(t5);
SamplerState g_BaseColorSampler : register(s0);

cbuffer MaterialParams : register(b1) {
    float4 EmissionColorStrength;
    float4 PbrFactors;
    float4 PbrTextureFlags;
};

struct SceneLight {
    float4 PositionType;
    float4 DirectionIntensity;
    float4 ColorAttenuationConstant;
    float4 AttenuationSpot;
    float4 AreaSize;
};

cbuffer SceneLighting : register(b2) {
    SceneLight SceneLights[8];
    uint4 SceneLightingMeta;
    float4 SceneCameraPosition;
};

// Keep this helper name distinct from the shader compiler's internal normal-map
// lowering symbol.  The D3D12 compiler otherwise reports a spurious
// potentially-uninitialized-variable warning for applyNormalMap while creating
// the mesh PSO, and the PSO creation is rejected by the backend.
float3 computeNormalMapped(float3 position, float3 geometricNormal, float2 uv,
                           float3 normalSample, float strength, float enabled) {
    float3 N = normalize(geometricNormal);
    if (enabled < 0.5 || strength <= 0.0) {
        return N;
    }
    float3 dpdx = ddx(position);
    float3 dpdy = ddy(position);
    float2 duvdx = ddx(uv);
    float2 duvdy = ddy(uv);
    float determinant = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
    if (abs(determinant) <= 1e-6) {
        return N;
    }
    float inverseDeterminant = rcp(determinant);
    float3 T = normalize((dpdx * duvdy.y - dpdy * duvdx.y) * inverseDeterminant);
    float3 B = normalize((-dpdx * duvdy.x + dpdy * duvdx.x) * inverseDeterminant);
    float3 tangentNormal = normalSample * 2.0 - 1.0;
    tangentNormal.xy *= strength;
    tangentNormal = normalize(tangentNormal);
    return normalize(tangentNormal.x * T + tangentNormal.y * B +
                     tangentNormal.z * N);
}

float4 PSMain(PSInput In) : SV_Target {
    float4 baseSample = g_BaseColorTexture.Sample(g_BaseColorSampler, In.UV);
    float4 opacitySample = g_OpacityTexture.Sample(g_BaseColorSampler, In.UV);
    float4 emissionSample = g_EmissionTexture.Sample(g_BaseColorSampler, In.UV);
    float4 metallicRoughnessSample =
        g_MetallicRoughnessTexture.Sample(g_BaseColorSampler, In.UV);
    float3 normalSample = g_NormalTexture.Sample(g_BaseColorSampler, In.UV).xyz;
    float occlusionSample = g_OcclusionTexture.Sample(g_BaseColorSampler, In.UV).r;
    float4 baseColor = baseSample * In.Color;
    float emissionStrength = max(EmissionColorStrength.a, 0.0);
    float metallic = saturate(PbrFactors.x *
        lerp(1.0, metallicRoughnessSample.b, PbrTextureFlags.x));
    float roughness = clamp(PbrFactors.y *
        lerp(1.0, metallicRoughnessSample.g, PbrTextureFlags.x), 0.04, 1.0);
    float ao = lerp(1.0,
                    lerp(1.0, occlusionSample, saturate(PbrFactors.w)),
                    PbrTextureFlags.z);
    float3 worldNormal = computeNormalMapped(
        In.WorldPosition, In.WorldNormal, In.UV, normalSample,
        PbrFactors.z, PbrTextureFlags.y);
    float3 viewNormal = computeNormalMapped(
        In.ViewPosition, In.Normal, In.UV, normalSample,
        PbrFactors.z, PbrTextureFlags.y);
    if (In.Mode > 1.5 && In.Mode < 2.5) {
        float3 normalColor = viewNormal * 0.5 + 0.5;
        return float4(normalColor, baseColor.a * opacitySample.a);
    }
    if (In.Mode > 0.5 && In.Mode < 1.5) {
        return float4(baseColor.rgb, baseColor.a * opacitySample.a);
    }
    if (In.Mode > 3.5 && In.Mode < 4.5) {
        float3 emissionColor =
            emissionSample.rgb * EmissionColorStrength.rgb * emissionStrength;
        return float4(emissionColor, baseColor.a * opacitySample.a);
    }
    if (In.Mode > 4.5 && In.Mode < 6.5) {
        return float4(In.Color.rgb, 1.0);
    }
    if (In.Mode > 6.5 && In.Mode < 7.5) {
        float2 currNdc = In.Pos.xy / max(In.Pos.w, 1e-5);
        float2 prevNdc = In.PrevPos.xy / max(In.PrevPos.w, 1e-5);
        float2 velocity = (currNdc - prevNdc) * float2(0.5, -0.5);
        return float4(velocity * 0.5 + 0.5, 0.5, 1.0);
    }

    float4 solidBase = (In.Mode > 7.5 && In.Mode < 8.5) ? In.Color : baseColor;
    if (SceneLightingMeta.x > 0) {
        float3 cameraDelta = SceneCameraPosition.xyz - In.WorldPosition;
        float3 viewDirection = cameraDelta *
            rsqrt(max(dot(cameraDelta, cameraDelta), 1e-8));
        float3 directColor = float3(0.0, 0.0, 0.0);
        float3 ambientColor = float3(0.0, 0.0, 0.0);
        float3 F0 = lerp(float3(0.04, 0.04, 0.04), solidBase.rgb, metallic);
        [loop]
        for (uint lightIndex = 0; lightIndex < min(SceneLightingMeta.x, 8u); ++lightIndex) {
            SceneLight light = SceneLights[lightIndex];
            uint lightType = (uint)(light.PositionType.w + 0.5);
            float3 radiance = light.ColorAttenuationConstant.rgb *
                              max(light.DirectionIntensity.w, 0.0);
            if (lightType == 3u) {
                ambientColor += solidBase.rgb * radiance * ao;
                continue;
            }

            float3 lightDirection = float3(0.0, 0.0, 1.0);
            float attenuation = 1.0;
            if (lightType == 0u) {
                lightDirection = normalize(-light.DirectionIntensity.xyz);
            } else {
                float3 toLight = light.PositionType.xyz - In.WorldPosition;
                float distanceToLight = max(length(toLight), 1e-4);
                lightDirection = toLight / distanceToLight;
                float denominator = light.ColorAttenuationConstant.w +
                    light.AttenuationSpot.x * distanceToLight +
                    light.AttenuationSpot.y * distanceToLight * distanceToLight;
                attenuation = rcp(max(denominator, 1e-4));

                if (lightType == 2u) {
                    float3 lightToSurface = -lightDirection;
                    float spotCos = dot(lightToSurface,
                                        normalize(light.DirectionIntensity.xyz));
                    float innerCos = light.AttenuationSpot.z;
                    float outerCos = light.AttenuationSpot.w;
                    attenuation *= saturate((spotCos - outerCos) /
                                            max(innerCos - outerCos, 1e-4));
                }
                if (lightType == 4u) {
                    float areaScale = light.AreaSize.z > 0.5
                        ? 3.14159265 * light.AreaSize.x * light.AreaSize.x * 0.25
                        : light.AreaSize.x * light.AreaSize.y;
                    areaScale = max(areaScale, 1.0);
                    float distanceScale = max(distanceToLight * distanceToLight, 1.0);
                    attenuation *= min(1.0, areaScale / distanceScale);
                    float3 areaForward = normalize(light.DirectionIntensity.xyz);
                    float facing = saturate(dot(-lightDirection, areaForward));
                    attenuation *= facing;
                }
            }
            float NdotL = saturate(dot(worldNormal, lightDirection));
            if (NdotL <= 0.0) {
                continue;
            }
            float3 halfVector = normalize(viewDirection + lightDirection);
            float NdotV = max(saturate(dot(worldNormal, viewDirection)), 1e-4);
            float NdotH = max(saturate(dot(worldNormal, halfVector)), 1e-4);
            float VdotH = max(saturate(dot(viewDirection, halfVector)), 1e-4);
            float alpha = roughness * roughness;
            float alpha2 = alpha * alpha;
            float denominatorNdf = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
            float distribution = alpha2 /
                max(3.14159265 * denominatorNdf * denominatorNdf, 1e-4);
            float geometryK = (roughness + 1.0) * (roughness + 1.0) * 0.125;
            float geometryV = NdotV / lerp(NdotV, 1.0, geometryK);
            float geometryL = NdotL / lerp(NdotL, 1.0, geometryK);
            float3 fresnel = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
            float3 specular = distribution * geometryV * geometryL * fresnel /
                              max(4.0 * NdotV * NdotL, 1e-4);
            float3 diffuse = (1.0 - fresnel) * (1.0 - metallic) *
                             solidBase.rgb / 3.14159265;
            directColor += (diffuse + specular) * radiance *
                           NdotL * attenuation;
        }
        float3 emissionColor = emissionSample.rgb * EmissionColorStrength.rgb *
                               emissionStrength;
        return float4(saturate(directColor + ambientColor + emissionColor),
                      solidBase.a * opacitySample.a);
    }

    // Preserve the camera-relative studio rig when the composition has no
    // applicable scene lights, including layers with lighting disabled.
    float3 N = viewNormal;
    float key = saturate(dot(N, normalize(float3(-0.35, 0.55, 0.76))));
    float fill = saturate(dot(N, normalize(float3(0.65, -0.20, 0.55))));
    float hemisphere = N.y * 0.5 + 0.5;
    float rim = pow(1.0 - saturate(abs(N.z)), 2.0);
    float normalVariation = length(ddx(N)) + length(ddy(N));
    float cavity = 1.0 - saturate(normalVariation * 0.22);
    float studioLight = 0.24 + key * 0.58 + fill * 0.18 + hemisphere * 0.10;
    float3 rimColor = lerp(solidBase.rgb, float3(1.0, 1.0, 1.0), 0.55);
    float specularPower = lerp(96.0, 4.0, roughness);
    float studioSpecular = pow(saturate(key), specularPower) *
                           lerp(0.08, 0.55, metallic);
    float3 litColor = saturate((solidBase.rgb * studioLight * cavity +
                                rimColor * rim * 0.10 +
                                lerp(float3(1.0, 1.0, 1.0), solidBase.rgb,
                                     metallic) * studioSpecular) * ao);
    return float4(litColor, solidBase.a * opacitySample.a);
}
)";

struct MeshRenderer::Impl {
    static constexpr size_t MaxSceneLights = 8;

    struct SceneLightGpu {
        float positionType[4] = {};
        float directionIntensity[4] = {};
        float colorAttenuationConstant[4] = {};
        float attenuationSpot[4] = {};
        float areaSize[4] = {};
    };

    struct SceneLightingConstants {
        SceneLightGpu lights[MaxSceneLights] = {};
        std::uint32_t lightingMeta[4] = {};
        float cameraPosition[4] = {};
    };

    struct MaterialConstants {
        float emissionColorStrength[4] = {};
        float pbrFactors[4] = {0.0f, 0.5f, 1.0f, 1.0f};
        float pbrTextureFlags[4] = {};
    };

    static_assert(sizeof(SceneLightGpu) == sizeof(float) * 20);
    static_assert(sizeof(SceneLightingConstants) ==
                  sizeof(SceneLightGpu) * MaxSceneLights + sizeof(float) * 8);
    static_assert(sizeof(MaterialConstants) == sizeof(float) * 12);

    Diligent::RefCntAutoPtr<Diligent::IPipelineStateCache>    pPSOCache_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pPSO_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> pSRB_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pTransparentPSO_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> pTransparentSRB_;
    struct PipelineSet {
        Diligent::RefCntAutoPtr<Diligent::IPipelineState> opaquePSO;
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> opaqueSRB;
        Diligent::RefCntAutoPtr<Diligent::IPipelineState> transparentPSO;
        Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> transparentSRB;
    };
    std::map<Diligent::TEXTURE_FORMAT, PipelineSet> pipelineSets_;
    bool transparentPass_ = false;
    bool missingPipelineWarningIssued_ = false;
    
    // Mesh geometry buffers
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pPositionBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pNormalBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pUVBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pIndexBuffer_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pBaseColorTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pBaseColorTextureSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pOpacityTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pOpacityTextureSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pEmissionTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pEmissionTextureSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pLinearWhiteTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pLinearWhiteTextureSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pMetallicRoughnessTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pMetallicRoughnessTextureSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pNormalTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pNormalTextureSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pOcclusionTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pOcclusionTextureSRV_;
    Diligent::RefCntAutoPtr<Diligent::ISampler>               pBaseColorSampler_;
    
    // Instance data buffer
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pInstanceBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pIndirectArgsBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pCompactedInstanceBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pCullConstantsBuffer_;
    std::unique_ptr<ComputeExecutor>                           pCullExecutor_;
    bool indirectDrawSupported_ = false;
    bool gpuCullReady_ = false;
    bool gpuCullActive_ = false;
    size_t uploadedInstanceCount_ = 0;
    float geometryBoundsRadius_ = 0.0f;
    
    // Constant buffer for view/proj matrices
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pConstantBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pMaterialBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pSceneLightingBuffer_;
    
    size_t vertexCount_ = 0;
    size_t indexCount_ = 0;
    size_t maxInstances_ = 0;
    QString baseColorTexturePath_;
    QString opacityTexturePath_;
    QString emissionTexturePath_;
    QString metallicRoughnessTexturePath_;
    QString normalTexturePath_;
    QString occlusionTexturePath_;
    MaterialConstants materialConstants_;
    SceneLightingConstants sceneLighting_;
};

MeshRenderer::MeshRenderer(GpuContext& context)
    : context_(context), pImpl_(new Impl())
{
    pImpl_->pCullExecutor_ = std::make_unique<ComputeExecutor>(context_);
    static const float identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    transpose4x4(identity, constants_.viewMatrix);
    transpose4x4(identity, constants_.projMatrix);
    transpose4x4(identity, constants_.prevViewMatrix);
    transpose4x4(identity, constants_.prevProjMatrix);
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

void MeshRenderer::setRenderTargetFormat(TEXTURE_FORMAT format)
{
    if (format == TEX_FORMAT_UNKNOWN || renderTargetFormat_ == format) {
        return;
    }

    prepared_ = false;
    renderTargetFormat_ = format;
    if (const auto cached = pImpl_->pipelineSets_.find(format);
        cached != pImpl_->pipelineSets_.end()) {
        pImpl_->pPSO_ = cached->second.opaquePSO;
        pImpl_->pSRB_ = cached->second.opaqueSRB;
        pImpl_->pTransparentPSO_ = cached->second.transparentPSO;
        pImpl_->pTransparentSRB_ = cached->second.transparentSRB;
        return;
    }
    if (maxInstances_ > 0) {
        createPSO();
    }
}

void MeshRenderer::setFrameCostStats(ArtifactCore::RenderCostStats* stats)
{
    frameCostStats_ = stats;
}

void MeshRenderer::setPipelineStateCache(IPipelineStateCache* cache)
{
    pImpl_->pPSOCache_ = cache;
}

void MeshRenderer::createBuffers()
{
    prepared_ = false;
    auto pDevice = context_.RenderDevice();
    if (!pDevice) {
        return;
    }

    // initialize() may be called again when the mesh topology changes.
    // Diligent requires every output RefCntAutoPtr to be empty before a new
    // device object is written into it.
    pImpl_->pPositionBuffer_.Release();
    pImpl_->pNormalBuffer_.Release();
    pImpl_->pUVBuffer_.Release();
    pImpl_->pIndexBuffer_.Release();
    pImpl_->pInstanceBuffer_.Release();
    pImpl_->pCompactedInstanceBuffer_.Release();
    pImpl_->pIndirectArgsBuffer_.Release();
    pImpl_->pCullConstantsBuffer_.Release();
    pImpl_->pConstantBuffer_.Release();
    pImpl_->pMaterialBuffer_.Release();
    pImpl_->pSceneLightingBuffer_.Release();
    // Cached SRBs contain static bindings to the constant buffers above.
    // They must not survive a buffer rebuild.
    pImpl_->pipelineSets_.clear();

    pImpl_->gpuCullReady_ = false;
    pImpl_->gpuCullActive_ = false;
    
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
        BuffDesc.Name = "Compacted Instance Structured Buffer";
        BuffDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        pDevice->CreateBuffer(
            BuffDesc, nullptr, &pImpl_->pCompactedInstanceBuffer_);
    }
    pImpl_->indirectDrawSupported_ =
        (pDevice->GetAdapterInfo().DrawCommand.CapFlags &
         DRAW_COMMAND_CAP_FLAG_DRAW_INDIRECT) != 0;
    if (pImpl_->indirectDrawSupported_) {
        BufferDesc BuffDesc;
        BuffDesc.Name = "Mesh Indirect Draw Args";
        BuffDesc.Usage = USAGE_DEFAULT;
        BuffDesc.Size = sizeof(Uint32) * 5;
        BuffDesc.BindFlags =
            BIND_INDIRECT_DRAW_ARGS | BIND_UNORDERED_ACCESS;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_NONE;
        BuffDesc.Mode = BUFFER_MODE_STRUCTURED;
        BuffDesc.ElementByteStride = sizeof(Uint32);
        pDevice->CreateBuffer(
            BuffDesc, nullptr, &pImpl_->pIndirectArgsBuffer_);
        pImpl_->indirectDrawSupported_ =
            pImpl_->pIndirectArgsBuffer_ != nullptr;
    }
    {
        BufferDesc BuffDesc;
        BuffDesc.Name = "Mesh Cull Constants";
        BuffDesc.Usage = USAGE_DYNAMIC;
        BuffDesc.Size = sizeof(MeshCullConstants);
        BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        pDevice->CreateBuffer(
            BuffDesc, nullptr, &pImpl_->pCullConstantsBuffer_);
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
    {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Material CB";
        BuffDesc.Usage             = USAGE_DYNAMIC;
        BuffDesc.Size              = sizeof(Impl::MaterialConstants);
        BuffDesc.BindFlags         = BIND_UNIFORM_BUFFER;
        BuffDesc.CPUAccessFlags    = CPU_ACCESS_WRITE;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pMaterialBuffer_);
    }
    {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Scene Lighting CB";
        BuffDesc.Usage             = USAGE_DYNAMIC;
        BuffDesc.Size              = sizeof(Impl::SceneLightingConstants);
        BuffDesc.BindFlags         = BIND_UNIFORM_BUFFER;
        BuffDesc.CPUAccessFlags    = CPU_ACCESS_WRITE;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pSceneLightingBuffer_);
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
    if (!pImpl_->pEmissionTexture_) {
        const Uint8 whitePixel[4] = {255, 255, 255, 255};
        TextureDesc texDesc;
        texDesc.Name = "MeshRenderer_EmissionWhiteTexture";
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
        pDevice->CreateTexture(texDesc, &initData, &pImpl_->pEmissionTexture_);
        if (pImpl_->pEmissionTexture_) {
            pImpl_->pEmissionTextureSRV_ =
                pImpl_->pEmissionTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
    }
    if (!pImpl_->pLinearWhiteTexture_) {
        const Uint8 whitePixel[4] = {255, 255, 255, 255};
        TextureDesc texDesc;
        texDesc.Name = "MeshRenderer_LinearWhiteTexture";
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = TEX_FORMAT_RGBA8_UNORM;
        texDesc.Usage = USAGE_IMMUTABLE;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;
        TextureSubResData subRes;
        subRes.pData = whitePixel;
        subRes.Stride = 4;
        TextureData initData;
        initData.pSubResources = &subRes;
        initData.NumSubresources = 1;
        pDevice->CreateTexture(texDesc, &initData, &pImpl_->pLinearWhiteTexture_);
        if (pImpl_->pLinearWhiteTexture_) {
            pImpl_->pLinearWhiteTextureSRV_ =
                pImpl_->pLinearWhiteTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
    }
    if (!pImpl_->pMetallicRoughnessTextureSRV_) {
        pImpl_->pMetallicRoughnessTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
    }
    if (!pImpl_->pNormalTextureSRV_) {
        pImpl_->pNormalTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
    }
    if (!pImpl_->pOcclusionTextureSRV_) {
        pImpl_->pOcclusionTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
    }
}

void MeshRenderer::createPSO()
{
    auto pDevice = context_.RenderDevice();
    if (!pDevice) {
        return;
    }

    // Diligent output parameters must be empty. createPSO() is also called
    // when the active render-target format changes, so release the currently
    // selected set before writing the new references.
    pImpl_->pSRB_.Release();
    pImpl_->pTransparentSRB_.Release();
    pImpl_->pPSO_.Release();
    pImpl_->pTransparentPSO_.Release();

    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    
    PSOCreateInfo.PSODesc.Name = "Mesh Instancing PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.pPSOCache = pImpl_->pPSOCache_.RawPtr();
    
    // Triangle list for mesh rendering
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    // Meshes are drawn both to the SDR swap-chain surface and to the floating
    // point composition pipeline. A PSO must match the currently bound color
    // attachment; reusing the particle's fixed SDR format breaks the latter.
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = renderTargetFormat_;
    PSOCreateInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
    
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
    // D3D input layouts keep the semantic name and semantic index separate.
    // The shader's ATTRIB0/1/2 declarations compile to name "ATTRIB" with
    // indices 0/1/2.  Using "ATTRIB0" here creates a different semantic and
    // makes CreateGraphicsPipelineState fail on D3D12.
    layoutElements[0].HLSLSemantic = "ATTRIB";
    layoutElements[0].InputIndex = 0;
    layoutElements[0].BufferSlot = 0;
    layoutElements[0].NumComponents = 3;
    layoutElements[0].ValueType = VT_FLOAT32;
    layoutElements[0].IsNormalized = false;
    // Normal
    layoutElements[1].HLSLSemantic = "ATTRIB";
    layoutElements[1].InputIndex = 1;
    layoutElements[1].BufferSlot = 1;
    layoutElements[1].NumComponents = 3;
    layoutElements[1].ValueType = VT_FLOAT32;
    layoutElements[1].IsNormalized = false;
    // UV
    layoutElements[2].HLSLSemantic = "ATTRIB";
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
    
    static std::array<ShaderResourceVariableDesc, 8> Vars = {{
        {SHADER_TYPE_VERTEX, "g_Instances", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_BaseColorTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_OpacityTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_EmissionTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_MetallicRoughnessTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_NormalTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_OcclusionTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_BaseColorSampler", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
    }};
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars.data();
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = (Uint32)Vars.size();
    
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pImpl_->pPSO_);

    PSOCreateInfo.PSODesc.Name = "Mesh Transparent Instancing PSO";
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pImpl_->pTransparentPSO_);
    
    if (!pImpl_->pPSO_) {
        qWarning("[MeshRenderer] PSO creation FAILED");
        return;
    }
    if (!pImpl_->pTransparentPSO_) {
        qWarning("[MeshRenderer] transparent PSO creation failed; using opaque fallback");
    }
    qDebug() << "[MeshRenderer] PSO created successfully";
    
    const auto initializeBindings = [&](IPipelineState* pso,
                                        RefCntAutoPtr<IShaderResourceBinding>& srb) {
        if (!pso) {
            return;
        }
        if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")) {
            var->Set(pImpl_->pConstantBuffer_);
        }
        if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "MaterialParams")) {
            var->Set(pImpl_->pMaterialBuffer_);
        }
        if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "SceneLighting")) {
            var->Set(pImpl_->pSceneLightingBuffer_);
        }
        if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_BaseColorSampler")) {
            var->Set(pImpl_->pBaseColorSampler_);
        }
        pso->CreateShaderResourceBinding(&srb, true);
    };
    initializeBindings(pImpl_->pPSO_, pImpl_->pSRB_);
    initializeBindings(pImpl_->pTransparentPSO_, pImpl_->pTransparentSRB_);
    if (!pImpl_->gpuCullReady_) {
        static std::array<ShaderResourceVariableDesc, 3> CullVars = {{
            {SHADER_TYPE_COMPUTE, "g_Input", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_COMPUTE, "g_Output", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_COMPUTE, "g_Args", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
        }};
        ComputePipelineDesc cullDesc;
        cullDesc.name = "Mesh GPU Visibility Cull";
        cullDesc.shaderSource = MeshCullCSSource;
        cullDesc.entryPoint = "CSMain";
        cullDesc.variables = CullVars.data();
        cullDesc.variableCount = static_cast<Uint32>(CullVars.size());
        cullDesc.defaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        pImpl_->gpuCullReady_ =
            pImpl_->pCullExecutor_ &&
            pImpl_->pCullExecutor_->build(cullDesc) &&
            pImpl_->pCullExecutor_->setBuffer(
                "CullConstants", pImpl_->pCullConstantsBuffer_) &&
            pImpl_->pCullExecutor_->createShaderResourceBinding(true);
    }
    pImpl_->pipelineSets_[renderTargetFormat_] = {
        pImpl_->pPSO_, pImpl_->pSRB_, pImpl_->pTransparentPSO_,
        pImpl_->pTransparentSRB_};
}

void MeshRenderer::updateMeshGeometry(const float* positions, const float* normals, const float* uvs,
                                      const uint32_t* indices)
{
    auto pContext = context_.DeviceContext();
    if (!pContext) {
        qWarning("[MeshRenderer] updateMeshGeometry skipped because device context is unavailable");
        return;
    }
    
    if (positions && pImpl_->pPositionBuffer_) {
        pContext->UpdateBuffer(pImpl_->pPositionBuffer_, 0, sizeof(float) * 3 * vertexCount_,
                              positions, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if (frameCostStats_) ++frameCostStats_->bufferUpdates;
        float maxRadiusSquared = 0.0f;
        for (size_t i = 0; i < vertexCount_; ++i) {
            const float x = positions[i * 3u + 0u];
            const float y = positions[i * 3u + 1u];
            const float z = positions[i * 3u + 2u];
            maxRadiusSquared =
                std::max(maxRadiusSquared, x * x + y * y + z * z);
        }
        pImpl_->geometryBoundsRadius_ = std::sqrt(maxRadiusSquared);
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
    if (!pContext) {
        qWarning("[MeshRenderer] updateInstanceData skipped because device context is unavailable");
        return;
    }
    size_t uploadSize = sizeof(InstanceData) * std::min(count, maxInstances_);
    pImpl_->uploadedInstanceCount_ = std::min(count, maxInstances_);
    
    pContext->UpdateBuffer(pImpl_->pInstanceBuffer_, 0, uploadSize,
                          instances, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (frameCostStats_) ++frameCostStats_->bufferUpdates;
}

void MeshRenderer::prepare(IDeviceContext* pContext)
{
    prepared_ = false;
    if (!pContext) {
        if (!pImpl_->missingPipelineWarningIssued_) {
            qWarning("[MeshRenderer] prepare skipped because device context is unavailable");
            pImpl_->missingPipelineWarningIssued_ = true;
        }
        return;
    }
    IPipelineState* activePSO =
        pImpl_->transparentPass_ && pImpl_->pTransparentPSO_
            ? pImpl_->pTransparentPSO_.RawPtr()
            : pImpl_->pPSO_.RawPtr();
    IShaderResourceBinding* activeSRB =
        pImpl_->transparentPass_ && pImpl_->pTransparentSRB_
            ? pImpl_->pTransparentSRB_.RawPtr()
            : pImpl_->pSRB_.RawPtr();
    if (!activePSO || !activeSRB || !pImpl_->pConstantBuffer_ ||
        !pImpl_->pMaterialBuffer_ || !pImpl_->pSceneLightingBuffer_ ||
        !pImpl_->pInstanceBuffer_) {
        if (!pImpl_->missingPipelineWarningIssued_) {
            qWarning("[MeshRenderer] prepare skipped because pipeline or buffer resources are unavailable");
            pImpl_->missingPipelineWarningIssued_ = true;
        }
        return;
    }
    pImpl_->missingPipelineWarningIssued_ = false;
    
    // Update constants
    void* pData = nullptr;
    pContext->MapBuffer(pImpl_->pConstantBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (!pData) {
        qWarning("[MeshRenderer] prepare skipped because constant buffer mapping failed");
        return;
    }
    memcpy(pData, &constants_, sizeof(ShaderConstants));
    pContext->UnmapBuffer(pImpl_->pConstantBuffer_, MAP_WRITE);
    if (frameCostStats_) ++frameCostStats_->bufferUpdates;
    pData = nullptr;
    pContext->MapBuffer(pImpl_->pMaterialBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (!pData) {
        qWarning("[MeshRenderer] prepare skipped because material buffer mapping failed");
        return;
    }
    memcpy(pData, &pImpl_->materialConstants_, sizeof(Impl::MaterialConstants));
    pContext->UnmapBuffer(pImpl_->pMaterialBuffer_, MAP_WRITE);
    if (frameCostStats_) ++frameCostStats_->bufferUpdates;
    pData = nullptr;
    pContext->MapBuffer(pImpl_->pSceneLightingBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    if (!pData) {
        qWarning("[MeshRenderer] prepare skipped because scene lighting buffer mapping failed");
        return;
    }
    memcpy(pData, &pImpl_->sceneLighting_, sizeof(Impl::SceneLightingConstants));
    pContext->UnmapBuffer(pImpl_->pSceneLightingBuffer_, MAP_WRITE);
    if (frameCostStats_) ++frameCostStats_->bufferUpdates;
    
    pImpl_->gpuCullActive_ =
        pImpl_->gpuCullReady_ && pImpl_->indirectDrawSupported_ &&
        !pImpl_->transparentPass_ && pImpl_->pCompactedInstanceBuffer_ &&
        pImpl_->pIndirectArgsBuffer_ &&
        pImpl_->uploadedInstanceCount_ >= 64 &&
        pImpl_->geometryBoundsRadius_ > 0.0f;
    if (pImpl_->gpuCullActive_) {
        const Uint32 args[5] = {
            static_cast<Uint32>(
                pImpl_->pIndexBuffer_ && indexCount_ > 0
                    ? indexCount_ : vertexCount_),
            0u, 0u, 0u, 0u
        };
        pContext->UpdateBuffer(
            pImpl_->pIndirectArgsBuffer_, 0, sizeof(args), args,
            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        MeshCullConstants cullConstants;
        std::memcpy(cullConstants.viewMatrix, constants_.viewMatrix,
                    sizeof(cullConstants.viewMatrix));
        std::memcpy(cullConstants.projMatrix, constants_.projMatrix,
                    sizeof(cullConstants.projMatrix));
        cullConstants.inputCount =
            static_cast<Uint32>(pImpl_->uploadedInstanceCount_);
        cullConstants.outputCapacity = static_cast<Uint32>(maxInstances_);
        cullConstants.boundsRadius = pImpl_->geometryBoundsRadius_;
        void* cullData = nullptr;
        pContext->MapBuffer(
            pImpl_->pCullConstantsBuffer_, MAP_WRITE, MAP_FLAG_DISCARD,
            cullData);
        if (cullData) {
            std::memcpy(cullData, &cullConstants, sizeof(cullConstants));
            pContext->UnmapBuffer(
                pImpl_->pCullConstantsBuffer_, MAP_WRITE);
        } else {
            pImpl_->gpuCullActive_ = false;
        }
        if (pImpl_->gpuCullActive_) {
            const bool inputBound = pImpl_->pCullExecutor_->setBufferView(
                "g_Input", pImpl_->pInstanceBuffer_->GetDefaultView(
                               BUFFER_VIEW_SHADER_RESOURCE));
            const bool outputBound = pImpl_->pCullExecutor_->setBufferView(
                "g_Output", pImpl_->pCompactedInstanceBuffer_->GetDefaultView(
                                BUFFER_VIEW_UNORDERED_ACCESS));
            const bool argsBound = pImpl_->pCullExecutor_->setBufferView(
                "g_Args", pImpl_->pIndirectArgsBuffer_->GetDefaultView(
                              BUFFER_VIEW_UNORDERED_ACCESS));
            pImpl_->gpuCullActive_ =
                inputBound && outputBound && argsBound;
            if (pImpl_->gpuCullActive_) {
                DispatchComputeAttribs dispatch;
                dispatch.ThreadGroupCountX =
                    (static_cast<Uint32>(pImpl_->uploadedInstanceCount_) +
                     63u) / 64u;
                pImpl_->pCullExecutor_->dispatch(
                    pContext, dispatch,
                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }
        }
    }

    // Bind instance buffer SRV
    if (activeSRB) {
        auto* pVar = activeSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_Instances");
        if (pVar) {
            auto* drawInstanceBuffer = pImpl_->gpuCullActive_
                ? pImpl_->pCompactedInstanceBuffer_.RawPtr()
                : pImpl_->pInstanceBuffer_.RawPtr();
            pVar->Set(drawInstanceBuffer->GetDefaultView(
                BUFFER_VIEW_SHADER_RESOURCE));
        }
        if (auto* texVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_BaseColorTexture")) {
            texVar->Set(pImpl_->pBaseColorTextureSRV_);
        }
        if (auto* opacityVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_OpacityTexture")) {
            opacityVar->Set(pImpl_->pOpacityTextureSRV_);
        }
        if (auto* emissionVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_EmissionTexture")) {
            emissionVar->Set(pImpl_->pEmissionTextureSRV_);
        }
        if (auto* pbrVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_MetallicRoughnessTexture")) {
            pbrVar->Set(pImpl_->pMetallicRoughnessTextureSRV_);
        }
        if (auto* normalVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_NormalTexture")) {
            normalVar->Set(pImpl_->pNormalTextureSRV_);
        }
        if (auto* occlusionVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_OcclusionTexture")) {
            occlusionVar->Set(pImpl_->pOcclusionTextureSRV_);
        }
        if (auto* sampVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_BaseColorSampler")) {
            sampVar->Set(pImpl_->pBaseColorSampler_);
        }
    }
    
    pContext->SetPipelineState(activePSO);
    if (frameCostStats_) {
        ++frameCostStats_->psoSwitches;
        ++frameCostStats_->srbCommits;
    }
    pContext->CommitShaderResources(activeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
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
    prepared_ = true;
}

void MeshRenderer::draw(IDeviceContext* pContext, size_t instanceCount)
{
    if (!pContext || !prepared_ || instanceCount == 0) return;

    const IPipelineState* activePSO =
        pImpl_->transparentPass_ && pImpl_->pTransparentPSO_
            ? pImpl_->pTransparentPSO_.RawPtr()
            : pImpl_->pPSO_.RawPtr();
    const IShaderResourceBinding* activeSRB =
        pImpl_->transparentPass_ && pImpl_->pTransparentSRB_
            ? pImpl_->pTransparentSRB_.RawPtr()
            : pImpl_->pSRB_.RawPtr();
    if (!activePSO || !activeSRB) {
        if (!pImpl_->missingPipelineWarningIssued_) {
            qWarning("[MeshRenderer] draw skipped because PSO/SRB is unavailable");
            pImpl_->missingPipelineWarningIssued_ = true;
        }
        return;
    }

    instanceCount = std::min(instanceCount, maxInstances_);
    const bool useIndirect =
        pImpl_->indirectDrawSupported_ && pImpl_->pIndirectArgsBuffer_ &&
        instanceCount >= 64;
    
    if (pImpl_->pIndexBuffer_ && indexCount_ > 0) {
        if (frameCostStats_) ++frameCostStats_->drawCalls;
        if (pImpl_->gpuCullActive_) {
            DrawIndexedIndirectAttribs drawAttrs{
                VT_UINT32, pImpl_->pIndirectArgsBuffer_, DRAW_FLAG_NONE,
                1, 0, sizeof(Uint32) * 5,
                RESOURCE_STATE_TRANSITION_MODE_TRANSITION};
            pContext->DrawIndexedIndirect(drawAttrs);
        } else if (useIndirect) {
            const Uint32 args[5] = {
                static_cast<Uint32>(indexCount_),
                static_cast<Uint32>(instanceCount), 0u, 0u, 0u
            };
            pContext->UpdateBuffer(
                pImpl_->pIndirectArgsBuffer_, 0, sizeof(args), args,
                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            if (frameCostStats_) ++frameCostStats_->bufferUpdates;
            DrawIndexedIndirectAttribs drawAttrs{
                VT_UINT32, pImpl_->pIndirectArgsBuffer_, DRAW_FLAG_NONE,
                1, 0, sizeof(args), RESOURCE_STATE_TRANSITION_MODE_TRANSITION};
            pContext->DrawIndexedIndirect(drawAttrs);
        } else {
            DrawIndexedAttribs drawAttrs;
            drawAttrs.NumIndices = static_cast<Uint32>(indexCount_);
            drawAttrs.NumInstances = static_cast<Uint32>(instanceCount);
            drawAttrs.IndexType = VT_UINT32;
            drawAttrs.Flags = DRAW_FLAG_NONE;
            pContext->DrawIndexed(drawAttrs);
        }
    } else if (vertexCount_ > 0) {
        if (frameCostStats_) ++frameCostStats_->drawCalls;
        if (pImpl_->gpuCullActive_) {
            DrawIndirectAttribs drawAttrs{
                pImpl_->pIndirectArgsBuffer_, DRAW_FLAG_NONE, 1, 0,
                sizeof(Uint32) * 4,
                RESOURCE_STATE_TRANSITION_MODE_TRANSITION};
            pContext->DrawIndirect(drawAttrs);
        } else if (useIndirect) {
            const Uint32 args[4] = {
                static_cast<Uint32>(vertexCount_),
                static_cast<Uint32>(instanceCount), 0u, 0u
            };
            pContext->UpdateBuffer(
                pImpl_->pIndirectArgsBuffer_, 0, sizeof(args), args,
                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            if (frameCostStats_) ++frameCostStats_->bufferUpdates;
            DrawIndirectAttribs drawAttrs{
                pImpl_->pIndirectArgsBuffer_, DRAW_FLAG_NONE, 1, 0,
                sizeof(args), RESOURCE_STATE_TRANSITION_MODE_TRANSITION};
            pContext->DrawIndirect(drawAttrs);
        } else {
            DrawAttribs drawAttrs;
            drawAttrs.NumVertices = static_cast<Uint32>(vertexCount_);
            drawAttrs.NumInstances = static_cast<Uint32>(instanceCount);
            drawAttrs.Flags = DRAW_FLAG_NONE;
            pContext->Draw(drawAttrs);
        }
    }
}

void MeshRenderer::setViewMatrix(const float* matrix)
{
    transpose4x4(matrix, constants_.viewMatrix);
    if (matrix) {
        const float tx = matrix[12];
        const float ty = matrix[13];
        const float tz = matrix[14];
        pImpl_->sceneLighting_.cameraPosition[0] =
            -(matrix[0] * tx + matrix[1] * ty + matrix[2] * tz);
        pImpl_->sceneLighting_.cameraPosition[1] =
            -(matrix[4] * tx + matrix[5] * ty + matrix[6] * tz);
        pImpl_->sceneLighting_.cameraPosition[2] =
            -(matrix[8] * tx + matrix[9] * ty + matrix[10] * tz);
        pImpl_->sceneLighting_.cameraPosition[3] = 0.0f;
    }
}

void MeshRenderer::setProjectionMatrix(const float* matrix)
{
    transpose4x4(matrix, constants_.projMatrix);
}

void MeshRenderer::setPreviousViewMatrix(const float* matrix)
{
    transpose4x4(matrix, constants_.prevViewMatrix);
}

void MeshRenderer::setPreviousProjectionMatrix(const float* matrix)
{
    transpose4x4(matrix, constants_.prevProjMatrix);
}

void MeshRenderer::setSceneLights(const std::vector<Light>& lights)
{
    std::fill(std::begin(pImpl_->sceneLighting_.lights),
              std::end(pImpl_->sceneLighting_.lights), Impl::SceneLightGpu{});
    pImpl_->sceneLighting_.lightingMeta[0] = 0;
    for (const auto& light : lights) {
        if (!light.enabled() ||
            pImpl_->sceneLighting_.lightingMeta[0] >= Impl::MaxSceneLights) {
            continue;
        }

        auto& gpuLight =
            pImpl_->sceneLighting_.lights[pImpl_->sceneLighting_.lightingMeta[0]++];
        const auto position = light.position();
        const auto direction = light.direction();
        const auto color = light.color();
        gpuLight.positionType[0] = position.x;
        gpuLight.positionType[1] = position.y;
        gpuLight.positionType[2] = position.z;
        gpuLight.positionType[3] = static_cast<float>(light.type());
        gpuLight.directionIntensity[0] = direction.x;
        gpuLight.directionIntensity[1] = direction.y;
        gpuLight.directionIntensity[2] = direction.z;
        gpuLight.directionIntensity[3] = std::max(light.intensity(), 0.0f);
        gpuLight.colorAttenuationConstant[0] = color.x;
        gpuLight.colorAttenuationConstant[1] = color.y;
        gpuLight.colorAttenuationConstant[2] = color.z;
        gpuLight.colorAttenuationConstant[3] = light.attenuationConstant();
        gpuLight.attenuationSpot[0] = light.attenuationLinear();
        gpuLight.attenuationSpot[1] = light.attenuationQuadratic();
        gpuLight.attenuationSpot[2] =
            std::cos(light.spotInnerCutoff() *
                     std::numbers::pi_v<float> / 180.0f);
        gpuLight.attenuationSpot[3] =
            std::cos(light.spotOuterCutoff() *
                     std::numbers::pi_v<float> / 180.0f);
        gpuLight.areaSize[0] = light.areaWidth();
        gpuLight.areaSize[1] = light.areaHeight();
        gpuLight.areaSize[2] = static_cast<float>(light.areaShape());
    }
}

void MeshRenderer::setTransparentPass(bool transparent)
{
    if (pImpl_->transparentPass_ == transparent) {
        return;
    }
    pImpl_->transparentPass_ = transparent;
    prepared_ = false;
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

void MeshRenderer::setEmissionTexture(const QString& path)
{
    const QString newPath = path.trimmed();
    auto pDevice = context_.RenderDevice();
    if (newPath == pImpl_->emissionTexturePath_ && pImpl_->pEmissionTextureSRV_) {
        return;
    }

    if (newPath.isEmpty() || !pDevice) {
        clearEmissionTexture();
        return;
    }

    pImpl_->emissionTexturePath_ = newPath;

    ArtifactCore::ImageImporter importer;
    if (!importer.open(pImpl_->emissionTexturePath_)) {
        qWarning() << "[MeshRenderer] Failed to open emission texture path:" << pImpl_->emissionTexturePath_;
        clearEmissionTexture();
        return;
    }

    const ArtifactCore::RawImage rawImage = importer.readImage();
    if (!rawImage.isValid() || rawImage.width <= 0 || rawImage.height <= 0) {
        qWarning() << "[MeshRenderer] Failed to read emission texture image:" << pImpl_->emissionTexturePath_;
        clearEmissionTexture();
        return;
    }

    TextureDesc texDesc;
    texDesc.Name = "MeshRenderer_EmissionTexture";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = static_cast<Uint32>(rawImage.width);
    texDesc.Height = static_cast<Uint32>(rawImage.height);
    texDesc.MipLevels = 1;
    texDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
    texDesc.Usage = USAGE_IMMUTABLE;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;

    QVector<quint8> rgba8 = expandTextureToRgba8(rawImage, false);
    if (rgba8.isEmpty()) {
        qWarning() << "[MeshRenderer] Unsupported emission texture pixel type:"
                   << pImpl_->emissionTexturePath_
                   << rawImage.pixelType;
        clearEmissionTexture();
        return;
    }

    TextureSubResData subRes;
    subRes.pData = rgba8.constData();
    subRes.Stride = static_cast<Uint64>(rawImage.width * 4);

    TextureData initData;
    initData.pSubResources = &subRes;
    initData.NumSubresources = 1;

    pDevice->CreateTexture(texDesc, &initData, &pImpl_->pEmissionTexture_);
    if (pImpl_->pEmissionTexture_) {
        pImpl_->pEmissionTextureSRV_ =
            pImpl_->pEmissionTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
}

void MeshRenderer::clearEmissionTexture()
{
    if (pImpl_->emissionTexturePath_.isEmpty() && pImpl_->pEmissionTextureSRV_) {
        return;
    }
    pImpl_->emissionTexturePath_.clear();
    auto pDevice = context_.RenderDevice();
    if (!pDevice) {
        pImpl_->pEmissionTexture_ = nullptr;
        pImpl_->pEmissionTextureSRV_ = nullptr;
        return;
    }

    const Uint8 whitePixel[4] = {255, 255, 255, 255};
    TextureDesc texDesc;
    texDesc.Name = "MeshRenderer_EmissionWhiteTexture";
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
    pDevice->CreateTexture(texDesc, &initData, &pImpl_->pEmissionTexture_);
    if (pImpl_->pEmissionTexture_) {
        pImpl_->pEmissionTextureSRV_ =
            pImpl_->pEmissionTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
}

void MeshRenderer::setEmissionColor(const QColor& color, float strength)
{
    pImpl_->materialConstants_.emissionColorStrength[0] = color.redF();
    pImpl_->materialConstants_.emissionColorStrength[1] = color.greenF();
    pImpl_->materialConstants_.emissionColorStrength[2] = color.blueF();
    pImpl_->materialConstants_.emissionColorStrength[3] =
        std::max(strength, 0.0f);
}

void MeshRenderer::setPbrFactors(float metallic, float roughness,
                                 float normalStrength,
                                 float occlusionStrength)
{
    pImpl_->materialConstants_.pbrFactors[0] = std::clamp(metallic, 0.0f, 1.0f);
    pImpl_->materialConstants_.pbrFactors[1] = std::clamp(roughness, 0.0f, 1.0f);
    pImpl_->materialConstants_.pbrFactors[2] = std::max(normalStrength, 0.0f);
    pImpl_->materialConstants_.pbrFactors[3] =
        std::clamp(occlusionStrength, 0.0f, 1.0f);
}

void MeshRenderer::setMetallicRoughnessTexture(const QString& path)
{
    const QString newPath = path.trimmed();
    if (newPath.isEmpty() && pImpl_->metallicRoughnessTexturePath_.isEmpty() &&
        pImpl_->materialConstants_.pbrTextureFlags[0] < 0.5f) {
        return;
    }
    if (newPath == pImpl_->metallicRoughnessTexturePath_ &&
        pImpl_->materialConstants_.pbrTextureFlags[0] > 0.5f) {
        return;
    }
    pImpl_->pMetallicRoughnessTexture_ = nullptr;
    pImpl_->pMetallicRoughnessTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
    pImpl_->metallicRoughnessTexturePath_.clear();
    pImpl_->materialConstants_.pbrTextureFlags[0] = 0.0f;
    if (newPath.isEmpty()) {
        return;
    }
    if (loadLinearTexture(context_, newPath, "MeshRenderer_MetallicRoughnessTexture",
                          pImpl_->pMetallicRoughnessTexture_,
                          pImpl_->pMetallicRoughnessTextureSRV_)) {
        pImpl_->metallicRoughnessTexturePath_ = newPath;
        pImpl_->materialConstants_.pbrTextureFlags[0] = 1.0f;
    } else {
        pImpl_->pMetallicRoughnessTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
    }
}

void MeshRenderer::setNormalTexture(const QString& path)
{
    const QString newPath = path.trimmed();
    if (newPath.isEmpty() && pImpl_->normalTexturePath_.isEmpty() &&
        pImpl_->materialConstants_.pbrTextureFlags[1] < 0.5f) {
        return;
    }
    if (newPath == pImpl_->normalTexturePath_ &&
        pImpl_->materialConstants_.pbrTextureFlags[1] > 0.5f) {
        return;
    }
    pImpl_->pNormalTexture_ = nullptr;
    pImpl_->pNormalTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
    pImpl_->normalTexturePath_.clear();
    pImpl_->materialConstants_.pbrTextureFlags[1] = 0.0f;
    if (newPath.isEmpty()) {
        return;
    }
    if (loadLinearTexture(context_, newPath, "MeshRenderer_NormalTexture",
                          pImpl_->pNormalTexture_, pImpl_->pNormalTextureSRV_)) {
        pImpl_->normalTexturePath_ = newPath;
        pImpl_->materialConstants_.pbrTextureFlags[1] = 1.0f;
    } else {
        pImpl_->pNormalTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
    }
}

void MeshRenderer::setOcclusionTexture(const QString& path)
{
    const QString newPath = path.trimmed();
    if (newPath.isEmpty() && pImpl_->occlusionTexturePath_.isEmpty() &&
        pImpl_->materialConstants_.pbrTextureFlags[2] < 0.5f) {
        return;
    }
    if (newPath == pImpl_->occlusionTexturePath_ &&
        pImpl_->materialConstants_.pbrTextureFlags[2] > 0.5f) {
        return;
    }
    pImpl_->pOcclusionTexture_ = nullptr;
    pImpl_->pOcclusionTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
    pImpl_->occlusionTexturePath_.clear();
    pImpl_->materialConstants_.pbrTextureFlags[2] = 0.0f;
    if (newPath.isEmpty()) {
        return;
    }
    if (loadLinearTexture(context_, newPath, "MeshRenderer_OcclusionTexture",
                          pImpl_->pOcclusionTexture_,
                          pImpl_->pOcclusionTextureSRV_)) {
        pImpl_->occlusionTexturePath_ = newPath;
        pImpl_->materialConstants_.pbrTextureFlags[2] = 1.0f;
    } else {
        pImpl_->pOcclusionTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
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

