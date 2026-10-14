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
#include <QString>
#include <QFile>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <cstring>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include <limits>
#include <algorithm>

module Graphics.RayTracingManager;

import Graphics;
import Container.NamedVector;
import IO.ImageImporter;
import Image.Raw;
import Utils.String.UniString;

namespace ArtifactCore {

using namespace Diligent;

namespace {
struct RTRayCameraConstants {
    float origin[4];
    float target[4];
    float up[4];
    float params[4];
};

struct RTEnvironmentConstants {
    float settings[4];
};

struct RTSceneLightsConstants {
    Uint32 count[4];
    RTSceneLight lights[8];
};
static_assert(sizeof(RTSceneLight) == sizeof(float) * 20);
static_assert(sizeof(RTSceneLightsConstants) == sizeof(Uint32) * 4 +
              sizeof(RTSceneLight) * 8);

float clampFinite(float value, float minimum, float maximum,
                  float fallback) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum)
                                : fallback;
}

constexpr const char* kWarmupRayGenShaderSource = R"(
RaytracingAccelerationStructure g_TLAS : register(t0);
RWTexture2D<float4> g_OutputTex : register(u0);
cbuffer RayCameraConstants : register(b0)
{
    float4 g_CameraOrigin;
    float4 g_CameraTarget;
    float4 g_CameraUp;
    float4 g_CameraParams;
};
struct Payload { float4 color; float3 rayDirection; uint isShadow; uint blocked; float rayConeWidth; };

[shader("raygeneration")]
void RTWarmup_RayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 size = DispatchRaysDimensions().xy;
    float2 uv = (float2(pixel) + 0.5f) / float2(size);
    float3 forward = normalize(g_CameraTarget.xyz - g_CameraOrigin.xyz);
    float3 right = normalize(cross(g_CameraUp.xyz, forward));
    float3 up = normalize(cross(right, forward));
    float halfHeight = tan(radians(clamp(g_CameraParams.x, 1.0f, 179.0f)) * 0.5f);
    float aspect = float(size.x) / max(float(size.y), 1.0f);
    float2 screen = float2((uv.x * 2.0f - 1.0f) * aspect,
                           uv.y * 2.0f - 1.0f) * halfHeight;
    RayDesc ray;
    ray.Origin = g_CameraOrigin.xyz;
    ray.Direction = normalize(forward + right * screen.x + up * screen.y);
    ray.TMin = 0.001f;
    ray.TMax = 10000.0f;
    Payload payload = (Payload)0;
    payload.rayDirection = ray.Direction;
    payload.rayConeWidth = 2.0f * halfHeight / max(float(size.y), 1.0f);
    TraceRay(g_TLAS, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
    g_OutputTex[pixel] = payload.color;
}
)";

constexpr const char* kWarmupMissShaderSource = R"(
TextureCube<float4> g_RTEnvironment : register(t0);
SamplerState g_RTEnvironmentSampler : register(s0);
cbuffer RTEnvironmentConstants : register(b0)
{
    float4 g_RTEnvironmentSettings;
};
struct Payload { float4 color; float3 rayDirection; uint isShadow; uint blocked; float rayConeWidth; };
[shader("miss")]
void RTWarmup_Miss(inout Payload payload)
{
    if (payload.isShadow != 0) return;
    if (g_RTEnvironmentSettings.x > 0.5f) {
        float rotationSin;
        float rotationCos;
        sincos(g_RTEnvironmentSettings.z, rotationSin, rotationCos);
        const float3 environmentDirection = normalize(float3(
            rotationCos * payload.rayDirection.x +
                rotationSin * payload.rayDirection.z,
            payload.rayDirection.y,
            -rotationSin * payload.rayDirection.x +
                rotationCos * payload.rayDirection.z));
        const float3 environmentColor = g_RTEnvironment.SampleLevel(
            g_RTEnvironmentSampler, environmentDirection, 0.0f).rgb *
            max(g_RTEnvironmentSettings.y, 0.0f);
        payload.color = float4(environmentColor, 1.0f);
        return;
    }
    float skyAmount = saturate(0.5f - payload.rayDirection.y * 1.25f);
    float3 horizon = float3(0.28f, 0.52f, 0.82f);
    float3 zenith = float3(0.035f, 0.07f, 0.16f);
    payload.color = float4(lerp(horizon, zenith, skyAmount), 1.0f);
}
)";

constexpr const char* kWarmupClosestHitShaderSource = R"(
RaytracingAccelerationStructure g_TLAS : register(t0);
struct RTPrimitive { uint i0; uint i1; uint i2; uint materialIndex; };
struct RTMaterial
{
    float4 baseColor;
    float4 surface;
    float4 specularTint;
    float4 thinFilm;
    float4 extras;
    float4 emission;
    float4 sheen;
    float4 occlusion;
    float4 occlusionUv;
    float4 baseColorTexture;
    float4 surfaceTexture;
    float4 normalTexture;
    float4 emissionTexture;
    float4 opacityTexture;
    float4 alpha;
    float4 anisotropy;
    float4 subsurface;
    float4 subsurfaceRadius;
    float4 subsurfaceScattering;
    float4 clearcoatIor;
    float4 clearcoatNormal;
    float4 normalUvOffset;
    float4 clearcoatNormalUvOffset;
    float4 occlusionTextureUvOffset;
    float4 baseColorTextureUvRotation;
    float4 emissionTextureUvRotation;
    float4 surfaceTextureUvRotation;
    float4 opacityTextureUvRotation;
    float4 metallicTexture;
    float4 roughnessTexture;
    float4 metallicTextureUvOffset;
    float4 roughnessTextureUvOffset;
    float4 transmissionMode;
    float4 clearcoatTexture;
    float4 clearcoatTextureUvOffset;
    float4 clearcoatRoughnessTexture;
    float4 clearcoatRoughnessTextureUvOffset;
    float4 sheenColorTexture;
    float4 sheenColorTextureUvOffset;
    float4 sheenRoughnessTexture;
    float4 sheenRoughnessTextureUvOffset;
    float4 anisotropyTexture;
    float4 anisotropyTextureUvOffset;
};
StructuredBuffer<float4> g_RenderVertices : register(t1);
StructuredBuffer<RTPrimitive> g_RenderPrimitives : register(t2);
StructuredBuffer<uint> g_InstancePrimitiveOffsets : register(t3);
StructuredBuffer<float4> g_RenderNormals : register(t4);
StructuredBuffer<RTMaterial> g_RenderMaterials : register(t5);
StructuredBuffer<float2> g_RenderUvs : register(t10);
StructuredBuffer<float4> g_RenderColors : register(t11);
StructuredBuffer<float2> g_RenderUvs1 : register(t12);
Texture2D<float4> g_RTPbrTextures[RT_OCCLUSION_TEXTURE_COUNT] : register(t0, space1);
TextureCube<float4> g_RTEnvironment : register(t6);
TextureCube<float4> g_RTIrradiance : register(t7);
Texture2D<float4> g_RTBrdfLut : register(t8);
TextureCube<float4> g_RTSheenEnvironment : register(t9);
SamplerState g_RTEnvironmentSampler : register(s0);
SamplerState g_RTBrdfSampler : register(s1);
SamplerState g_RTPbrSampler : register(s2);
float4 sampleRTBrdfLut(float2 uv)
{
    const float halfTexel = 0.5f / 128.0f;
    const float specularU = clamp(
        saturate(uv.x) * 0.5f, halfTexel, 0.5f - halfTexel);
    return g_RTBrdfLut.SampleLevel(
        g_RTBrdfSampler, float2(specularU, saturate(uv.y)), 0.0f);
}
float sampleRTDiffuseDirectionalAlbedo(float NdotV, float roughness)
{
    const float halfTexel = 0.5f / 128.0f;
    const float diffuseU = clamp(
        0.5f + saturate(NdotV) * 0.5f,
        0.5f + halfTexel, 1.0f - halfTexel);
    return max(g_RTBrdfLut.SampleLevel(g_RTBrdfSampler,
        float2(diffuseU, saturate(roughness)), 0.0f).r,
        0.0f);
}
cbuffer RTEnvironmentConstants : register(b1)
{
    float4 g_RTEnvironmentSettings;
};
struct RTSceneLight
{
    float4 positionType;
    float4 directionIntensity;
    float4 colorAttenuationConstant;
    float4 attenuationSpot;
    float4 shadowParams;
};
cbuffer RTSceneLightsConstants : register(b2)
{
    uint4 g_RTSceneLightCount;
    RTSceneLight g_RTSceneLights[8];
};
struct Payload { float4 color; float3 rayDirection; uint isShadow; uint blocked; float rayConeWidth; };
float SrgbChannelToLinear(float encoded)
{
    encoded = max(encoded, 0.0f);
    return encoded <= 0.04045f ? encoded / 12.92f
                               : pow((encoded + 0.055f) / 1.055f, 2.4f);
}
float ThinFilmChannelReflectance(float cosIncident, float filmIor,
                                 float substrateIor, float thicknessNm,
                                 float wavelengthNm)
{
    const float n0 = 1.0f;
    const float n1 = max(filmIor, 1.0001f);
    const float n2 = max(substrateIor, 1.0001f);
    const float c0 = saturate(cosIncident);
    const float s0 = sqrt(max(1.0f - c0 * c0, 0.0f));
    const float s1 = min(s0 / n1, 0.9999f);
    const float s2 = min(s0 / n2, 0.9999f);
    const float c1 = sqrt(max(1.0f - s1 * s1, 0.0f));
    const float c2 = sqrt(max(1.0f - s2 * s2, 0.0f));
    const float rs01 = (n0 * c0 - n1 * c1) /
                       max(n0 * c0 + n1 * c1, 1.0e-6f);
    const float rp01 = (n1 * c0 - n0 * c1) /
                       max(n1 * c0 + n0 * c1, 1.0e-6f);
    const float rs12 = (n1 * c1 - n2 * c2) /
                       max(n1 * c1 + n2 * c2, 1.0e-6f);
    const float rp12 = (n2 * c1 - n1 * c2) /
                       max(n2 * c1 + n1 * c2, 1.0e-6f);
    const float phase = 12.5663706144f * n1 * max(thicknessNm, 0.0f) * c1 /
                        max(wavelengthNm, 1.0f);
    const float interference = cos(phase);
    const float rsProduct = rs01 * rs12;
    const float rpProduct = rp01 * rp12;
    const float rs = (rs01 * rs01 + rs12 * rs12 +
                      2.0f * rsProduct * interference) /
                     max(1.0f + rsProduct * rsProduct +
                         2.0f * rsProduct * interference, 1.0e-6f);
    const float rp = (rp01 * rp01 + rp12 * rp12 +
                      2.0f * rpProduct * interference) /
                     max(1.0f + rpProduct * rpProduct +
                         2.0f * rpProduct * interference, 1.0e-6f);
    return saturate(0.5f * (rs + rp));
}
float3 ThinFilmReflectance(float cosIncident, float filmIor,
                           float substrateIor, float thicknessNm)
{
    return float3(
        ThinFilmChannelReflectance(cosIncident, filmIor, substrateIor,
                                   thicknessNm, 610.0f),
        ThinFilmChannelReflectance(cosIncident, filmIor, substrateIor,
                                   thicknessNm, 550.0f),
        ThinFilmChannelReflectance(cosIncident, filmIor, substrateIor,
                                   thicknessNm, 460.0f));
}
float3 CharlieSheen(float roughness, float NdotH, float NdotV, float NdotL)
{
    const float alphaG = max(roughness * roughness, 1.0e-4f);
    const float inverseRoughness = rcp(alphaG);
    const float sin2H = max(1.0f - NdotH * NdotH, 1.0e-6f);
    const float distribution = (2.0f + inverseRoughness) *
        pow(sin2H, 0.5f * inverseRoughness) / (2.0f * 3.14159265f);
    const float oneMinusAlphaSquared = (1.0f - alphaG) * (1.0f - alphaG);
    const float a = lerp(21.5473f, 25.3245f, oneMinusAlphaSquared);
    const float b = lerp(3.82987f, 3.32435f, oneMinusAlphaSquared);
    const float c = lerp(0.19823f, 0.16801f, oneMinusAlphaSquared);
    const float d = lerp(-1.97760f, -1.27393f, oneMinusAlphaSquared);
    const float e = lerp(-4.32054f, -4.85967f, oneMinusAlphaSquared);
    const float lHalf = a / (1.0f + b * pow(0.5f, c)) + d * 0.5f + e;
    const float xV = max(saturate(NdotV), 1.0e-5f);
    const float xL = max(saturate(NdotL), 1.0e-5f);
    const float lV = a / (1.0f + b * pow(xV, c)) + d * xV + e;
    const float lL = a / (1.0f + b * pow(xL, c)) + d * xL + e;
    const float xVComplement = 1.0f - xV;
    const float xLComplement = 1.0f - xL;
    const float lVComplement = a / (1.0f + b * pow(max(xVComplement, 1.0e-5f), c)) +
        d * xVComplement + e;
    const float lLComplement = a / (1.0f + b * pow(max(xLComplement, 1.0e-5f), c)) +
        d * xLComplement + e;
    const float lambdaV = xV < 0.5f ? exp(lV) : exp(2.0f * lHalf - lVComplement);
    const float lambdaL = xL < 0.5f ? exp(lL) : exp(2.0f * lHalf - lLComplement);
    const float visibility = rcp((1.0f + lambdaV + lambdaL) *
        (4.0f * max(NdotV, 1.0e-4f) * max(NdotL, 1.0e-4f)));
    return distribution * visibility;
}
float specularOcclusion(float NdotV, float visibility, float roughness)
{
    const float exponent = exp2(-16.0f * roughness - 1.0f);
    return saturate(pow(max(NdotV + visibility, 0.0f), exponent) - 1.0f + visibility);
}
float2 transformPbrUv(float2 uv, float2 scale, float angle, float2 offset)
{
    float2 scaled = uv * scale;
    float2 rotated = scaled;
    if (abs(angle) > 1.0e-5f) {
        float sine;
        float cosine;
        sincos(angle, sine, cosine);
        float2 centered = scaled - 0.5f;
        rotated = float2(centered.x * cosine - centered.y * sine,
                         centered.x * sine + centered.y * cosine) + 0.5f;
    }
    return rotated + offset;
}
float2 transformBaseColorPbrUv(float2 uv, float scale)
{
    return 0.5f + (uv - 0.5f) * clamp(scale, 0.01f, 10.0f);
}
float2 transformBaseColorTextureUv(float2 uv, float scale, float angle,
                                   float2 offset)
{
    float2 centered = (uv - 0.5f) * clamp(scale, 0.01f, 10.0f);
    if (abs(angle) > 1.0e-5f) {
        float sine;
        float cosine;
        sincos(angle, sine, cosine);
        centered = float2(centered.x * cosine - centered.y * sine,
                          centered.x * sine + centered.y * cosine);
    }
    return centered + 0.5f + offset;
}
float2 transformIndependentPbrTextureUv(float2 uv, float2 scale, float angle,
                                        float2 offset)
{
    float2 safeScale = clamp(
        scale, float2(0.01f, 0.01f), float2(10.0f, 10.0f));
    float2 centered = (uv - 0.5f) * safeScale;
    if (abs(angle) > 1.0e-5f) {
        float sine;
        float cosine;
        sincos(angle, sine, cosine);
        centered = float2(centered.x * cosine - centered.y * sine,
                          centered.x * sine + centered.y * cosine);
    }
    return centered + 0.5f + offset;
}
float2 rotatePbrUv(float2 uv, float angle, float scale, float2 offset)
{
    float sine;
    float cosine;
    sincos(angle, sine, cosine);
    const float2 center = 0.5f + offset;
    const float2 centered = (uv - center) * scale;
    return float2(centered.x * cosine - centered.y * sine,
                  centered.x * sine + centered.y * cosine) + center;
}
float calculatePbrTextureLod(uint textureIndex, float3 worldEdge1,
                             float3 worldEdge2, float2 uvEdge1,
                             float2 uvEdge2, float rayConeRadius)
{
    uint textureWidth = 0;
    uint textureHeight = 0;
    uint mipCount = 0;
    g_RTPbrTextures[NonUniformResourceIndex(textureIndex)].GetDimensions(
        0, textureWidth, textureHeight, mipCount);
    if (textureWidth == 0 || textureHeight == 0 || mipCount <= 1)
        return 0.0f;

    const float edge1Length = length(worldEdge1);
    if (edge1Length <= 1.0e-7f)
        return 0.0f;
    const float3 axis1 = worldEdge1 / edge1Length;
    const float edge2Projection = dot(worldEdge2, axis1);
    const float3 perpendicularEdge2 = worldEdge2 - axis1 * edge2Projection;
    const float edge2Length = length(perpendicularEdge2);
    if (edge2Length <= 1.0e-7f)
        return 0.0f;

    const float2 textureSize = float2(textureWidth, textureHeight);
    const float2 textureEdge1 = uvEdge1 * textureSize;
    const float2 textureEdge2 = uvEdge2 * textureSize;
    const float2 jacobian1 = textureEdge1 / edge1Length;
    const float2 jacobian2 = (textureEdge2 - jacobian1 * edge2Projection) /
                              edge2Length;
    const float a = dot(jacobian1, jacobian1);
    const float b = dot(jacobian1, jacobian2);
    const float c = dot(jacobian2, jacobian2);
    const float discriminant = max((a - c) * (a - c) + 4.0f * b * b, 0.0f);
    const float maximumDensitySquared = 0.5f * (a + c + sqrt(discriminant));
    const float maximumDensity = sqrt(max(maximumDensitySquared, 0.0f));
    const float footprintTexels = max(maximumDensity * rayConeRadius, 1.0f);
    return clamp(log2(footprintTexels), 0.0f, float(mipCount - 1));
}
[shader("closesthit")]
void RTWarmup_ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attributes)
{
    if (payload.isShadow != 0) {
        payload.blocked = 1;
        return;
    }
    uint primitiveOffset = g_InstancePrimitiveOffsets[InstanceID()];
    if (primitiveOffset == 0xFFFFFFFFu) {
        payload.color = float4(0.96f, 0.69f, 0.16f, 1.0f);
        return;
    }
    RTPrimitive primitive = g_RenderPrimitives[primitiveOffset + PrimitiveIndex()];
    float3 p0 = g_RenderVertices[primitive.i0].xyz;
    float3 p1 = g_RenderVertices[primitive.i1].xyz;
    float3 p2 = g_RenderVertices[primitive.i2].xyz;
    float3 faceNormal = normalize(cross(p1 - p0, p2 - p0));
    float3 n0 = g_RenderNormals[primitive.i0].xyz;
    float3 n1 = g_RenderNormals[primitive.i1].xyz;
    float3 n2 = g_RenderNormals[primitive.i2].xyz;
    float3 normal = n0 * (1.0f - attributes.barycentrics.x - attributes.barycentrics.y) +
                    n1 * attributes.barycentrics.x + n2 * attributes.barycentrics.y;
    normal = dot(normal, normal) > 1.0e-8f ? normalize(normal) : faceNormal;
    float2 uv0 = g_RenderUvs[primitive.i0];
    float2 uv1 = g_RenderUvs[primitive.i1];
    float2 uv2 = g_RenderUvs[primitive.i2];
    float4 vertexColor = g_RenderColors[primitive.i0] *
                         (1.0f - attributes.barycentrics.x - attributes.barycentrics.y) +
                         g_RenderColors[primitive.i1] * attributes.barycentrics.x +
                         g_RenderColors[primitive.i2] * attributes.barycentrics.y;
    RTMaterial material = g_RenderMaterials[primitive.materialIndex];
    const float2 baseColorSetUv0 = material.baseColorTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 baseColorSetUv1 = material.baseColorTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 baseColorSetUv2 = material.baseColorTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 normalUv0 = material.normalUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 normalUv1 = material.normalUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 normalUv2 = material.normalUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 surfaceSetUv0 = material.surfaceTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 surfaceSetUv1 = material.surfaceTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 surfaceSetUv2 = material.surfaceTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 metallicSetUv0 = material.metallicTextureUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 metallicSetUv1 = material.metallicTextureUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 metallicSetUv2 = material.metallicTextureUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 roughnessSetUv0 = material.roughnessTextureUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 roughnessSetUv1 = material.roughnessTextureUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 roughnessSetUv2 = material.roughnessTextureUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 occlusionSetUv0 = material.occlusionTextureUvOffset.w > 0.5f
        ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 occlusionSetUv1 = material.occlusionTextureUvOffset.w > 0.5f
        ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 occlusionSetUv2 = material.occlusionTextureUvOffset.w > 0.5f
        ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 emissionSetUv0 = material.emissionTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 emissionSetUv1 = material.emissionTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 emissionSetUv2 = material.emissionTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 coatNormalSetUv0 = material.clearcoatNormalUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 coatNormalSetUv1 = material.clearcoatNormalUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 coatNormalSetUv2 = material.clearcoatNormalUvOffset.z > 0.5f
        ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 clearcoatSetUv0 = material.clearcoatTexture.w > 0.5f
        ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 clearcoatSetUv1 = material.clearcoatTexture.w > 0.5f
        ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 clearcoatSetUv2 = material.clearcoatTexture.w > 0.5f
        ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 clearcoatRoughnessSetUv0 =
        material.clearcoatRoughnessTexture.w > 0.5f
            ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 clearcoatRoughnessSetUv1 =
        material.clearcoatRoughnessTexture.w > 0.5f
            ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 clearcoatRoughnessSetUv2 =
        material.clearcoatRoughnessTexture.w > 0.5f
            ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 sheenColorSetUv0 =
        material.sheenColorTextureUvOffset.w > 0.5f
            ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 sheenColorSetUv1 =
        material.sheenColorTextureUvOffset.w > 0.5f
            ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 sheenColorSetUv2 =
        material.sheenColorTextureUvOffset.w > 0.5f
            ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 normalTransformedUv0 = transformPbrUv(
        normalUv0, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 normalTransformedUv1 = transformPbrUv(
        normalUv1, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 normalTransformedUv2 = transformPbrUv(
        normalUv2, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 baseColorTransformedUv0 = transformPbrUv(
        baseColorSetUv0, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 baseColorTransformedUv1 = transformPbrUv(
        baseColorSetUv1, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 baseColorTransformedUv2 = transformPbrUv(
        baseColorSetUv2, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 surfaceTransformedUv0 = transformPbrUv(
        surfaceSetUv0, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 surfaceTransformedUv1 = transformPbrUv(
        surfaceSetUv1, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 surfaceTransformedUv2 = transformPbrUv(
        surfaceSetUv2, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 metallicTransformedUv0 = transformPbrUv(
        metallicSetUv0, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 metallicTransformedUv1 = transformPbrUv(
        metallicSetUv1, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 metallicTransformedUv2 = transformPbrUv(
        metallicSetUv2, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 roughnessTransformedUv0 = transformPbrUv(
        roughnessSetUv0, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 roughnessTransformedUv1 = transformPbrUv(
        roughnessSetUv1, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 roughnessTransformedUv2 = transformPbrUv(
        roughnessSetUv2, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 occlusionTransformedUv0 = transformPbrUv(
        occlusionSetUv0, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 occlusionTransformedUv1 = transformPbrUv(
        occlusionSetUv1, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 occlusionTransformedUv2 = transformPbrUv(
        occlusionSetUv2, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 emissionTransformedUv0 = transformPbrUv(
        emissionSetUv0, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 emissionTransformedUv1 = transformPbrUv(
        emissionSetUv1, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 emissionTransformedUv2 = transformPbrUv(
        emissionSetUv2, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 coatNormalTransformedUv0 = transformPbrUv(
        coatNormalSetUv0, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 coatNormalTransformedUv1 = transformPbrUv(
        coatNormalSetUv1, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float2 coatNormalTransformedUv2 = transformPbrUv(
        coatNormalSetUv2, material.occlusionUv.xy, material.occlusionUv.z,
        material.occlusion.zw);
    const float clearcoatUvRotation = material.clearcoatTexture.z;
    const float2 clearcoatTransformedUv0 = transformPbrUv(
        clearcoatSetUv0, material.clearcoatTexture.yy,
        clearcoatUvRotation, material.clearcoatTextureUvOffset.xy);
    const float2 clearcoatTransformedUv1 = transformPbrUv(
        clearcoatSetUv1, material.clearcoatTexture.yy,
        clearcoatUvRotation, material.clearcoatTextureUvOffset.xy);
    const float2 clearcoatTransformedUv2 = transformPbrUv(
        clearcoatSetUv2, material.clearcoatTexture.yy,
        clearcoatUvRotation, material.clearcoatTextureUvOffset.xy);
    const float2 clearcoatTextureUv0 = clearcoatTransformedUv0;
    const float2 clearcoatTextureUv1 = clearcoatTransformedUv1;
    const float2 clearcoatTextureUv2 = clearcoatTransformedUv2;
    const float2 clearcoatTextureUvEdge1 =
        clearcoatTextureUv1 - clearcoatTextureUv0;
    const float2 clearcoatTextureUvEdge2 =
        clearcoatTextureUv2 - clearcoatTextureUv0;
    const float2 clearcoatRoughnessUv0 = transformPbrUv(
        clearcoatRoughnessSetUv0, material.clearcoatRoughnessTexture.yy,
        material.clearcoatRoughnessTexture.z,
        material.clearcoatRoughnessTextureUvOffset.xy);
    const float2 clearcoatRoughnessUv1 = transformPbrUv(
        clearcoatRoughnessSetUv1, material.clearcoatRoughnessTexture.yy,
        material.clearcoatRoughnessTexture.z,
        material.clearcoatRoughnessTextureUvOffset.xy);
    const float2 clearcoatRoughnessUv2 = transformPbrUv(
        clearcoatRoughnessSetUv2, material.clearcoatRoughnessTexture.yy,
        material.clearcoatRoughnessTexture.z,
        material.clearcoatRoughnessTextureUvOffset.xy);
    const float2 clearcoatRoughnessUvEdge1 =
        clearcoatRoughnessUv1 - clearcoatRoughnessUv0;
    const float2 clearcoatRoughnessUvEdge2 =
        clearcoatRoughnessUv2 - clearcoatRoughnessUv0;
    const float2 sheenColorUv0 = transformPbrUv(
        sheenColorSetUv0, material.sheenColorTexture.yz,
        material.sheenColorTexture.w,
        material.sheenColorTextureUvOffset.xy);
    const float2 sheenColorUv1 = transformPbrUv(
        sheenColorSetUv1, material.sheenColorTexture.yz,
        material.sheenColorTexture.w,
        material.sheenColorTextureUvOffset.xy);
    const float2 sheenColorUv2 = transformPbrUv(
        sheenColorSetUv2, material.sheenColorTexture.yz,
        material.sheenColorTexture.w,
        material.sheenColorTextureUvOffset.xy);
    const float2 sheenColorUvEdge1 = sheenColorUv1 - sheenColorUv0;
    const float2 sheenColorUvEdge2 = sheenColorUv2 - sheenColorUv0;
    const float2 sheenRoughnessSetUv0 =
        material.sheenRoughnessTextureUvOffset.w > 0.5f
            ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 sheenRoughnessSetUv1 =
        material.sheenRoughnessTextureUvOffset.w > 0.5f
            ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 sheenRoughnessSetUv2 =
        material.sheenRoughnessTextureUvOffset.w > 0.5f
            ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 sheenRoughnessUv0 = transformPbrUv(
        sheenRoughnessSetUv0, material.sheenRoughnessTexture.yz,
        material.sheenRoughnessTexture.w,
        material.sheenRoughnessTextureUvOffset.xy);
    const float2 sheenRoughnessUv1 = transformPbrUv(
        sheenRoughnessSetUv1, material.sheenRoughnessTexture.yz,
        material.sheenRoughnessTexture.w,
        material.sheenRoughnessTextureUvOffset.xy);
    const float2 sheenRoughnessUv2 = transformPbrUv(
        sheenRoughnessSetUv2, material.sheenRoughnessTexture.yz,
        material.sheenRoughnessTexture.w,
        material.sheenRoughnessTextureUvOffset.xy);
    const float2 sheenRoughnessUvEdge1 =
        sheenRoughnessUv1 - sheenRoughnessUv0;
    const float2 sheenRoughnessUvEdge2 =
        sheenRoughnessUv2 - sheenRoughnessUv0;
    const float2 anisotropySetUv0 =
        material.anisotropyTextureUvOffset.w > 0.5f
            ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 anisotropySetUv1 =
        material.anisotropyTextureUvOffset.w > 0.5f
            ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 anisotropySetUv2 =
        material.anisotropyTextureUvOffset.w > 0.5f
            ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 anisotropyUv0 = transformPbrUv(
        anisotropySetUv0, material.anisotropyTexture.yz,
        material.anisotropyTexture.w,
        material.anisotropyTextureUvOffset.xy);
    const float2 anisotropyUv1 = transformPbrUv(
        anisotropySetUv1, material.anisotropyTexture.yz,
        material.anisotropyTexture.w,
        material.anisotropyTextureUvOffset.xy);
    const float2 anisotropyUv2 = transformPbrUv(
        anisotropySetUv2, material.anisotropyTexture.yz,
        material.anisotropyTexture.w,
        material.anisotropyTextureUvOffset.xy);
    const float2 anisotropyUvEdge1 = anisotropyUv1 - anisotropyUv0;
    const float2 anisotropyUvEdge2 = anisotropyUv2 - anisotropyUv0;
    const float baseColorUvRotation =
        material.baseColorTextureUvRotation.x;
    const float surfaceUvRotation = material.surfaceTextureUvRotation.x;
    const float2 baseColorUv0 = transformBaseColorTextureUv(
        baseColorTransformedUv0, material.baseColorTexture.y, baseColorUvRotation,
        material.baseColorTexture.zw);
    const float2 baseColorUv1 = transformBaseColorTextureUv(
        baseColorTransformedUv1, material.baseColorTexture.y, baseColorUvRotation,
        material.baseColorTexture.zw);
    const float2 baseColorUv2 = transformBaseColorTextureUv(
        baseColorTransformedUv2, material.baseColorTexture.y, baseColorUvRotation,
        material.baseColorTexture.zw);
    const float2 baseColorUvEdge1 = baseColorUv1 - baseColorUv0;
    const float2 baseColorUvEdge2 = baseColorUv2 - baseColorUv0;
    const float2 surfaceUv0 = transformBaseColorTextureUv(
        surfaceTransformedUv0, material.surfaceTexture.y, surfaceUvRotation,
        material.surfaceTexture.zw);
    const float2 surfaceUv1 = transformBaseColorTextureUv(
        surfaceTransformedUv1, material.surfaceTexture.y, surfaceUvRotation,
        material.surfaceTexture.zw);
    const float2 surfaceUv2 = transformBaseColorTextureUv(
        surfaceTransformedUv2, material.surfaceTexture.y, surfaceUvRotation,
        material.surfaceTexture.zw);
    const float2 surfaceUvEdge1 = surfaceUv1 - surfaceUv0;
    const float2 surfaceUvEdge2 = surfaceUv2 - surfaceUv0;
    const float2 metallicUv0 = transformIndependentPbrTextureUv(
        metallicTransformedUv0, material.metallicTexture.yz,
        material.metallicTexture.w, material.metallicTextureUvOffset.xy);
    const float2 metallicUv1 = transformIndependentPbrTextureUv(
        metallicTransformedUv1, material.metallicTexture.yz,
        material.metallicTexture.w, material.metallicTextureUvOffset.xy);
    const float2 metallicUv2 = transformIndependentPbrTextureUv(
        metallicTransformedUv2, material.metallicTexture.yz,
        material.metallicTexture.w, material.metallicTextureUvOffset.xy);
    const float2 metallicUvEdge1 = metallicUv1 - metallicUv0;
    const float2 metallicUvEdge2 = metallicUv2 - metallicUv0;
    const float2 roughnessUv0 = transformIndependentPbrTextureUv(
        roughnessTransformedUv0, material.roughnessTexture.yz,
        material.roughnessTexture.w, material.roughnessTextureUvOffset.xy);
    const float2 roughnessUv1 = transformIndependentPbrTextureUv(
        roughnessTransformedUv1, material.roughnessTexture.yz,
        material.roughnessTexture.w, material.roughnessTextureUvOffset.xy);
    const float2 roughnessUv2 = transformIndependentPbrTextureUv(
        roughnessTransformedUv2, material.roughnessTexture.yz,
        material.roughnessTexture.w, material.roughnessTextureUvOffset.xy);
    const float2 roughnessUvEdge1 = roughnessUv1 - roughnessUv0;
    const float2 roughnessUvEdge2 = roughnessUv2 - roughnessUv0;
    const float occlusionTextureUvRotation =
        material.occlusionTextureUvOffset.z;
    const float2 occlusionUv0 = transformBaseColorTextureUv(
        occlusionTransformedUv0, material.occlusionUv.w, occlusionTextureUvRotation,
        material.occlusionTextureUvOffset.xy);
    const float2 occlusionUv1 = transformBaseColorTextureUv(
        occlusionTransformedUv1, material.occlusionUv.w, occlusionTextureUvRotation,
        material.occlusionTextureUvOffset.xy);
    const float2 occlusionUv2 = transformBaseColorTextureUv(
        occlusionTransformedUv2, material.occlusionUv.w, occlusionTextureUvRotation,
        material.occlusionTextureUvOffset.xy);
    const float2 occlusionUvEdge1 = occlusionUv1 - occlusionUv0;
    const float2 occlusionUvEdge2 = occlusionUv2 - occlusionUv0;
    const float emissionUvRotation = material.emissionTextureUvRotation.x;
    const float2 emissionUv0 = transformBaseColorTextureUv(
        emissionTransformedUv0, material.emissionTexture.y, emissionUvRotation,
        material.emissionTexture.zw);
    const float2 emissionUv1 = transformBaseColorTextureUv(
        emissionTransformedUv1, material.emissionTexture.y, emissionUvRotation,
        material.emissionTexture.zw);
    const float2 emissionUv2 = transformBaseColorTextureUv(
        emissionTransformedUv2, material.emissionTexture.y, emissionUvRotation,
        material.emissionTexture.zw);
    const float2 emissionUvEdge1 = emissionUv1 - emissionUv0;
    const float2 emissionUvEdge2 = emissionUv2 - emissionUv0;
    const float2 uvEdge1 = normalTransformedUv1 - normalTransformedUv0;
    const float2 uvEdge2 = normalTransformedUv2 - normalTransformedUv0;
    const float3 worldP0 = mul(ObjectToWorld3x4(), float4(p0, 1.0f));
    const float3 worldP1 = mul(ObjectToWorld3x4(), float4(p1, 1.0f));
    const float3 worldP2 = mul(ObjectToWorld3x4(), float4(p2, 1.0f));
    const float3 worldEdge1 = worldP1 - worldP0;
    const float3 worldEdge2 = worldP2 - worldP0;
    const float rayConeRadius = max(payload.rayConeWidth * RayTCurrent(), 0.0f);
    const float3 worldEdge1 = worldP1 - worldP0;
    const float3 worldEdge2 = worldP2 - worldP0;
    const float rayConeRadius = max(payload.rayConeWidth * RayTCurrent(), 0.0f);
    const float2 baseColorTransformedUv =
        baseColorTransformedUv0 * (1.0f - attributes.barycentrics.x -
                                   attributes.barycentrics.y) +
        baseColorTransformedUv1 * attributes.barycentrics.x +
        baseColorTransformedUv2 * attributes.barycentrics.y;
    const float2 baseColorUv = transformBaseColorTextureUv(
        baseColorTransformedUv, material.baseColorTexture.y, baseColorUvRotation,
        material.baseColorTexture.zw);
    const float2 surfaceTransformedUv =
        surfaceTransformedUv0 * (1.0f - attributes.barycentrics.x -
                                  attributes.barycentrics.y) +
        surfaceTransformedUv1 * attributes.barycentrics.x +
        surfaceTransformedUv2 * attributes.barycentrics.y;
    const float2 surfaceTextureUv = transformBaseColorTextureUv(
        surfaceTransformedUv, material.surfaceTexture.y, surfaceUvRotation,
        material.surfaceTexture.zw);
    const float2 metallicTransformedUv =
        metallicTransformedUv0 * (1.0f - attributes.barycentrics.x -
                                   attributes.barycentrics.y) +
        metallicTransformedUv1 * attributes.barycentrics.x +
        metallicTransformedUv2 * attributes.barycentrics.y;
    const float2 metallicTextureUv = transformIndependentPbrTextureUv(
        metallicTransformedUv, material.metallicTexture.yz,
        material.metallicTexture.w, material.metallicTextureUvOffset.xy);
    const float2 roughnessTransformedUv =
        roughnessTransformedUv0 * (1.0f - attributes.barycentrics.x -
                                    attributes.barycentrics.y) +
        roughnessTransformedUv1 * attributes.barycentrics.x +
        roughnessTransformedUv2 * attributes.barycentrics.y;
    const float2 roughnessTextureUv = transformIndependentPbrTextureUv(
        roughnessTransformedUv, material.roughnessTexture.yz,
        material.roughnessTexture.w, material.roughnessTextureUvOffset.xy);
    const float2 occlusionTransformedUv =
        occlusionTransformedUv0 * (1.0f - attributes.barycentrics.x -
                                    attributes.barycentrics.y) +
        occlusionTransformedUv1 * attributes.barycentrics.x +
        occlusionTransformedUv2 * attributes.barycentrics.y;
    const float2 occlusionTextureUv = transformBaseColorTextureUv(
        occlusionTransformedUv, material.occlusionUv.w,
        occlusionTextureUvRotation,
        material.occlusionTextureUvOffset.xy);
    const float2 emissionTransformedUv =
        emissionTransformedUv0 * (1.0f - attributes.barycentrics.x -
                                   attributes.barycentrics.y) +
        emissionTransformedUv1 * attributes.barycentrics.x +
        emissionTransformedUv2 * attributes.barycentrics.y;
    const float2 emissionTextureUv = transformBaseColorTextureUv(
        emissionTransformedUv, material.emissionTexture.y, emissionUvRotation,
        material.emissionTexture.zw);
    float3 worldNormal = normalize(mul(transpose((float3x3)WorldToObject3x4()), normal));
    if (dot(worldNormal, WorldRayDirection()) > 0.0f) worldNormal = -worldNormal;
    const float3 geometricWorldNormal = worldNormal;
    const float2 deltaUv1 = uvEdge1;
    const float2 deltaUv2 = uvEdge2;
    const float uvDeterminant = deltaUv1.x * deltaUv2.y -
                                deltaUv1.y * deltaUv2.x;
    float3 worldTangent;
    float3 worldBitangent;
    if (abs(uvDeterminant) > 1.0e-6f) {
        const float inverseDeterminant = rcp(uvDeterminant);
        const float3 edge1 = p1 - p0;
        const float3 edge2 = p2 - p0;
        const float3 localTangent = normalize(
            (edge1 * deltaUv2.y - edge2 * deltaUv1.y) * inverseDeterminant);
        const float3 localBitangent = normalize(
            (-edge1 * deltaUv2.x + edge2 * deltaUv1.x) * inverseDeterminant);
        const float3 transformedTangent = mul(
            (float3x3)ObjectToWorld3x4(), localTangent);
        const float3 transformedBitangent = mul(
            (float3x3)ObjectToWorld3x4(), localBitangent);
        worldTangent = normalize(
            transformedTangent - worldNormal * dot(transformedTangent, worldNormal));
        const float handedness = dot(cross(worldNormal, worldTangent),
                                      transformedBitangent) < 0.0f ? -1.0f : 1.0f;
        worldBitangent = normalize(cross(worldNormal, worldTangent)) * handedness;
    } else {
        worldTangent = normalize(cross(
            abs(worldNormal.y) < 0.9f ? float3(0.0f, 1.0f, 0.0f)
                                     : float3(1.0f, 0.0f, 0.0f), worldNormal));
        worldBitangent = normalize(cross(worldNormal, worldTangent));
    }
    const float normalUvRotation = material.normalTexture.z;
    const float normalUvScale = clamp(material.normalTexture.w, 0.01f, 10.0f);
    const float2 normalTransformedUv =
        normalTransformedUv0 * (1.0f - attributes.barycentrics.x -
                                 attributes.barycentrics.y) +
        normalTransformedUv1 * attributes.barycentrics.x +
        normalTransformedUv2 * attributes.barycentrics.y;
    const float2 normalMapUv = rotatePbrUv(
        normalTransformedUv, normalUvRotation, normalUvScale,
        material.occlusion.zw) + material.normalUvOffset.xy;
    const float2 normalMapUvEdge1 = uvEdge1 * normalUvScale;
    const float2 normalMapUvEdge2 = uvEdge2 * normalUvScale;
    float normalUvSine;
    float normalUvCosine;
    sincos(normalUvRotation, normalUvSine, normalUvCosine);
    const float3 normalMapTangent = normalize(
        worldTangent * normalUvCosine + worldBitangent * normalUvSine);
    const float3 normalMapBitangent = normalize(
        -worldTangent * normalUvSine + worldBitangent * normalUvCosine);
    float normalMapAverageLength = 1.0f;
    if (material.normalTexture.x >= 0.0f && material.normalTexture.y > 0.0f) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.normalTexture.x);
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            normalMapUvEdge1, normalMapUvEdge2,
            rayConeRadius);
        const float4 normalSample = g_RTPbrTextures[textureIndex].SampleLevel(
            g_RTPbrSampler, normalMapUv, textureLod);
        normalMapAverageLength = saturate(normalSample.a);
        float3 tangentNormal = normalSample.xyz * 2.0f - 1.0f;
        tangentNormal.xy *= material.normalTexture.y;
        tangentNormal = normalize(tangentNormal);
        if (abs(uvDeterminant) > 1.0e-6f) {
            worldNormal = normalize(tangentNormal.x * normalMapTangent +
                                    tangentNormal.y * normalMapBitangent +
                                    tangentNormal.z * worldNormal);
            worldTangent = normalize(worldTangent -
                worldNormal * dot(worldTangent, worldNormal));
            worldBitangent = normalize(cross(worldNormal, worldTangent)) *
                (dot(cross(worldNormal, worldTangent), worldBitangent) < 0.0f
                    ? -1.0f : 1.0f);
        }
    }
    float coatNormalMapAverageLength = normalMapAverageLength;
    float3 coatNormal = normalize(lerp(
        geometricWorldNormal, worldNormal,
        saturate(material.clearcoatNormal.x)));
    if (material.clearcoatNormal.y >= 0.0f) {
        const float coatNormalUvRotation = material.clearcoatNormal.z;
        const float coatNormalUvScale =
            clamp(material.clearcoatNormal.w, 0.01f, 10.0f);
        const float2 coatNormalTransformedUv =
            coatNormalTransformedUv0 * (1.0f - attributes.barycentrics.x -
                                         attributes.barycentrics.y) +
            coatNormalTransformedUv1 * attributes.barycentrics.x +
            coatNormalTransformedUv2 * attributes.barycentrics.y;
        const float2 coatNormalUv = rotatePbrUv(coatNormalTransformedUv,
            coatNormalUvRotation, coatNormalUvScale, material.occlusion.zw) +
            material.clearcoatNormalUvOffset.xy;
        const float2 coatNormalUv0 = rotatePbrUv(coatNormalTransformedUv0,
            coatNormalUvRotation, coatNormalUvScale, material.occlusion.zw);
        const float2 coatNormalUv1 = rotatePbrUv(coatNormalTransformedUv1,
            coatNormalUvRotation, coatNormalUvScale, material.occlusion.zw);
        const float2 coatNormalUv2 = rotatePbrUv(coatNormalTransformedUv2,
            coatNormalUvRotation, coatNormalUvScale, material.occlusion.zw);
        const float2 coatNormalUvEdge1 = coatNormalUv1 - coatNormalUv0;
        const float2 coatNormalUvEdge2 = coatNormalUv2 - coatNormalUv0;
        const float coatUvDeterminant =
            coatNormalUvEdge1.x * coatNormalUvEdge2.y -
            coatNormalUvEdge1.y * coatNormalUvEdge2.x;
        if (abs(coatUvDeterminant) <= 1.0e-6f) {
            coatNormalMapAverageLength = normalMapAverageLength;
        } else {
            const float inverseCoatUvDeterminant = rcp(coatUvDeterminant);
            const float3 coatLocalTangent = normalize(
                ((p1 - p0) * coatNormalUvEdge2.y -
                 (p2 - p0) * coatNormalUvEdge1.y) *
                inverseCoatUvDeterminant);
            const float3 coatLocalBitangent = normalize(
                (-(p1 - p0) * coatNormalUvEdge2.x +
                 (p2 - p0) * coatNormalUvEdge1.x) *
                inverseCoatUvDeterminant);
            const float3 coatTransformedTangent = mul(
                (float3x3)ObjectToWorld3x4(), coatLocalTangent);
            const float3 coatTransformedBitangent = mul(
                (float3x3)ObjectToWorld3x4(), coatLocalBitangent);
            const float3 coatNormalTangent = normalize(
                coatTransformedTangent - geometricWorldNormal *
                    dot(coatTransformedTangent, geometricWorldNormal));
            const float coatHandedness =
                dot(cross(geometricWorldNormal, coatNormalTangent),
                    coatTransformedBitangent) < 0.0f ? -1.0f : 1.0f;
            const float3 coatNormalBitangent = normalize(
                cross(geometricWorldNormal, coatNormalTangent)) *
                coatHandedness;
            const uint textureIndex = NonUniformResourceIndex(
                (uint)material.clearcoatNormal.y);
            const float textureLod = calculatePbrTextureLod(
                textureIndex, worldEdge1, worldEdge2, coatNormalUvEdge1,
                coatNormalUvEdge2,
                rayConeRadius);
            const float4 coatNormalSample =
                g_RTPbrTextures[textureIndex].SampleLevel(
                    g_RTPbrSampler, coatNormalUv, textureLod);
            coatNormalMapAverageLength = saturate(coatNormalSample.a);
            float3 tangentCoatNormal = coatNormalSample.xyz * 2.0f - 1.0f;
            tangentCoatNormal.xy *= saturate(material.clearcoatNormal.x);
            tangentCoatNormal = normalize(tangentCoatNormal);
            coatNormal = normalize(tangentCoatNormal.x * coatNormalTangent +
                tangentCoatNormal.y * coatNormalBitangent +
                tangentCoatNormal.z * geometricWorldNormal);
        }
    }
    float anisotropyRotationSin;
    float anisotropyRotationCos;
    sincos(material.anisotropy.y, anisotropyRotationSin,
          anisotropyRotationCos);
    const float3 unrotatedTangent = worldTangent;
    const float3 unrotatedBitangent = worldBitangent;
    worldTangent = unrotatedTangent * anisotropyRotationCos +
                   unrotatedBitangent * anisotropyRotationSin;
    worldBitangent = -unrotatedTangent * anisotropyRotationSin +
                     unrotatedBitangent * anisotropyRotationCos;
    float3 keyLightDirection = normalize(float3(-0.65f, 0.55f, -0.75f));
    float keyNdotL = saturate(dot(worldNormal, keyLightDirection));
    float keyVisibility = 1.0f;
    if (g_RTSceneLightCount.x == 0 && keyNdotL > 0.0f) {
        float3 tangent = normalize(cross(abs(keyLightDirection.y) < 0.9f
                                             ? float3(0.0f, 1.0f, 0.0f)
                                             : float3(1.0f, 0.0f, 0.0f),
                                         keyLightDirection));
        float3 bitangent = normalize(cross(keyLightDirection, tangent));
        float3 hitPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
        float sampleRotation = frac(sin(dot(hitPosition, float3(12.9898f, 78.233f, 37.719f))) *
                                    43758.5453f) * 6.2831853f;
        float rotationSin;
        float rotationCos;
        sincos(sampleRotation, rotationSin, rotationCos);
        const float2 sampleOffsets[16] = {
            float2(-0.75f, -0.75f), float2(-0.25f, -0.75f),
            float2(0.25f, -0.75f), float2(0.75f, -0.75f),
            float2(-0.75f, -0.25f), float2(-0.25f, -0.25f),
            float2(0.25f, -0.25f), float2(0.75f, -0.25f),
            float2(-0.75f, 0.25f), float2(-0.25f, 0.25f),
            float2(0.25f, 0.25f), float2(0.75f, 0.25f),
            float2(-0.75f, 0.75f), float2(-0.25f, 0.75f),
            float2(0.25f, 0.75f), float2(0.75f, 0.75f)};
        float visibleSamples = 0.0f;
        [unroll]
        for (uint sampleIndex = 0; sampleIndex < 16; ++sampleIndex) {
            float2 rotatedOffset = float2(
                rotationCos * sampleOffsets[sampleIndex].x - rotationSin * sampleOffsets[sampleIndex].y,
                rotationSin * sampleOffsets[sampleIndex].x + rotationCos * sampleOffsets[sampleIndex].y);
            float3 sampleDirection = normalize(
                keyLightDirection + 0.14f * (tangent * rotatedOffset.x + bitangent * rotatedOffset.y));
            RayDesc shadowRay;
            shadowRay.Origin = hitPosition + sampleDirection * 0.005f;
            shadowRay.Direction = sampleDirection;
            shadowRay.TMin = 0.001f;
            shadowRay.TMax = 10000.0f;
            Payload shadowPayload = (Payload)0;
            shadowPayload.isShadow = 1;
            TraceRay(g_TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 1, 0,
                     shadowRay, shadowPayload);
            visibleSamples += shadowPayload.blocked != 0 ? 0.0f : 1.0f;
        }
        keyVisibility = visibleSamples * (1.0f / 16.0f);
    }
    float3 baseColor = float3(SrgbChannelToLinear(material.baseColor.r),
                              SrgbChannelToLinear(material.baseColor.g),
                              SrgbChannelToLinear(material.baseColor.b));
    baseColor *= float3(SrgbChannelToLinear(saturate(vertexColor.r)),
                        SrgbChannelToLinear(saturate(vertexColor.g)),
                        SrgbChannelToLinear(saturate(vertexColor.b)));
    if (material.baseColorTexture.x >= 0.0f) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.baseColorTexture.x);
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            baseColorUvEdge1, baseColorUvEdge2,
            rayConeRadius);
        baseColor *= g_RTPbrTextures[textureIndex].SampleLevel(
            g_RTPbrSampler, baseColorUv, textureLod).rgb;
    }
    float3 viewDirection = normalize(-WorldRayDirection());
    float metallic = saturate(material.surface.x);
    float roughness = clamp(material.surface.y, 0.04f, 1.0f);
    const bool hasIndependentMetallicTexture =
        material.metallicTexture.x >= 0.0f;
    const bool hasIndependentRoughnessTexture =
        material.roughnessTexture.x >= 0.0f;
    if (material.surfaceTexture.x >= 0.0f &&
        !hasIndependentMetallicTexture && !hasIndependentRoughnessTexture) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.surfaceTexture.x);
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            surfaceUvEdge1, surfaceUvEdge2,
            rayConeRadius);
        const float4 surfaceSample = g_RTPbrTextures[textureIndex].SampleLevel(
            g_RTPbrSampler, surfaceTextureUv, textureLod);
        metallic = saturate(metallic * surfaceSample.b);
        roughness = clamp(roughness * surfaceSample.g, 0.04f, 1.0f);
    }
    if (hasIndependentMetallicTexture) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.metallicTexture.x);
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            metallicUvEdge1, metallicUvEdge2, rayConeRadius);
        const float metallicSample = g_RTPbrTextures[textureIndex].SampleLevel(
            g_RTPbrSampler, metallicTextureUv, textureLod).r;
        metallic = saturate(metallic * metallicSample);
    }
    if (hasIndependentRoughnessTexture) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.roughnessTexture.x);
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            roughnessUvEdge1, roughnessUvEdge2, rayConeRadius);
        const float roughnessSample = g_RTPbrTextures[textureIndex].SampleLevel(
            g_RTPbrSampler, roughnessTextureUv, textureLod).r;
        roughness = clamp(roughness * roughnessSample, 0.04f, 1.0f);
    }
    if (material.normalTexture.x >= 0.0f && material.normalTexture.y > 0.0f) {
        float normalVariance = max(1.0f - normalMapAverageLength, 0.0f) /
                              max(normalMapAverageLength, 1.0e-4f);
        normalVariance *= material.normalTexture.y * material.normalTexture.y;
        const float alpha = roughness * roughness;
        const float filteredAlphaSquared = saturate(
            alpha * alpha + min(2.0f * normalVariance, 0.2f));
        roughness = sqrt(sqrt(filteredAlphaSquared));
    }
    float iorRatio = (max(material.surface.w, 1.0f) - 1.0f) /
                     (max(material.surface.w, 1.0f) + 1.0f);
    float dielectricF0 = iorRatio * iorRatio *
                         (saturate(material.surface.z) / 0.5f);
    float3 specularTint = float3(
        SrgbChannelToLinear(material.specularTint.x),
        SrgbChannelToLinear(material.specularTint.y),
        SrgbChannelToLinear(material.specularTint.z));
    const float specularScale = saturate(material.surface.z) / 0.5f;
    float transmission = saturate(material.extras.x) * (1.0f - metallic);
    const bool thinWallEnabled = material.transmissionMode.x > 0.5f &&
                                 transmission > 1.0e-4f;
    float clearcoat = saturate(material.extras.y);
    if (material.clearcoatTexture.x >= 0.0f) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.clearcoatTexture.x);
        const float2 clearcoatTextureUv =
            clearcoatTextureUv0 *
                (1.0f - attributes.barycentrics.x -
                 attributes.barycentrics.y) +
            clearcoatTextureUv1 * attributes.barycentrics.x +
            clearcoatTextureUv2 * attributes.barycentrics.y;
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            clearcoatTextureUvEdge1, clearcoatTextureUvEdge2,
            rayConeRadius);
        clearcoat = saturate(clearcoat *
            g_RTPbrTextures[textureIndex].SampleLevel(
                g_RTPbrSampler, clearcoatTextureUv, textureLod).r);
    }
    float clearcoatRoughness = clamp(material.extras.z, 0.04f, 1.0f);
    if (material.clearcoatRoughnessTexture.x >= 0.0f) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.clearcoatRoughnessTexture.x);
        const float2 clearcoatRoughnessUv =
            clearcoatRoughnessUv0 *
                (1.0f - attributes.barycentrics.x -
                 attributes.barycentrics.y) +
            clearcoatRoughnessUv1 * attributes.barycentrics.x +
            clearcoatRoughnessUv2 * attributes.barycentrics.y;
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            clearcoatRoughnessUvEdge1, clearcoatRoughnessUvEdge2,
            rayConeRadius);
        const float roughnessMap = g_RTPbrTextures[textureIndex].SampleLevel(
            g_RTPbrSampler, clearcoatRoughnessUv, textureLod).g;
        clearcoatRoughness = clamp(
            clearcoatRoughness * roughnessMap, 0.04f, 1.0f);
    }
    if ((material.clearcoatNormal.y >= 0.0f ||
         material.normalTexture.x >= 0.0f) &&
        material.clearcoatNormal.x > 0.0f) {
        float coatNormalVariance = max(
            1.0f - coatNormalMapAverageLength, 0.0f) /
            max(coatNormalMapAverageLength, 1.0e-4f);
        coatNormalVariance *= material.clearcoatNormal.x *
                              material.clearcoatNormal.x;
        const float coatAlpha = clearcoatRoughness * clearcoatRoughness;
        const float filteredCoatAlphaSquared = saturate(
            coatAlpha * coatAlpha + min(2.0f * coatNormalVariance, 0.2f));
        clearcoatRoughness = sqrt(sqrt(filteredCoatAlphaSquared));
    }
    float clearcoatIor = max(material.clearcoatIor.x, 1.0f);
    float clearcoatIorRatio = (clearcoatIor - 1.0f) /
                              (clearcoatIor + 1.0f);
    float clearcoatF0 = clearcoatIorRatio * clearcoatIorRatio;
    float3 clearcoatTint = float3(
        SrgbChannelToLinear(material.clearcoatIor.y),
        SrgbChannelToLinear(material.clearcoatIor.z),
        SrgbChannelToLinear(material.clearcoatIor.w));
    float sheen = saturate(material.extras.w) * (1.0f - metallic) *
                  (1.0f - transmission);
    float sheenRoughness = clamp(material.sheen.x, 0.0f, 1.0f);
    if (material.sheenRoughnessTexture.x >= 0.0f) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.sheenRoughnessTexture.x);
        const float2 sheenRoughnessUv =
            sheenRoughnessUv0 *
                (1.0f - attributes.barycentrics.x -
                 attributes.barycentrics.y) +
            sheenRoughnessUv1 * attributes.barycentrics.x +
            sheenRoughnessUv2 * attributes.barycentrics.y;
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            sheenRoughnessUvEdge1, sheenRoughnessUvEdge2, rayConeRadius);
        const float sampledSheenRoughness =
            g_RTPbrTextures[textureIndex].SampleLevel(
                g_RTPbrSampler, sheenRoughnessUv, textureLod).r;
        sheenRoughness = saturate(sheenRoughness * sampledSheenRoughness);
    }
    float3 sheenTint = float3(SrgbChannelToLinear(material.sheen.y),
                              SrgbChannelToLinear(material.sheen.z),
                              SrgbChannelToLinear(material.sheen.w));
    if (material.sheenColorTexture.x >= 0.0f) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.sheenColorTexture.x);
        const float2 sheenColorUv =
            sheenColorUv0 *
                (1.0f - attributes.barycentrics.x -
                 attributes.barycentrics.y) +
            sheenColorUv1 * attributes.barycentrics.x +
            sheenColorUv2 * attributes.barycentrics.y;
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            sheenColorUvEdge1, sheenColorUvEdge2, rayConeRadius);
        const float3 sampledSheenColor =
            g_RTPbrTextures[textureIndex].SampleLevel(
                g_RTPbrSampler, sheenColorUv, textureLod).rgb;
        sheenTint *= float3(SrgbChannelToLinear(sampledSheenColor.x),
                            SrgbChannelToLinear(sampledSheenColor.y),
                            SrgbChannelToLinear(sampledSheenColor.z));
    }
    float ao = 1.0f;
    if (material.occlusion.x >= 0.0f) {
        const uint textureIndex = NonUniformResourceIndex((uint)material.occlusion.x);
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            occlusionUvEdge1, occlusionUvEdge2,
            rayConeRadius);
        const float aoSample = g_RTPbrTextures[textureIndex].SampleLevel(
            g_RTPbrSampler, occlusionTextureUv, textureLod).r;
        ao = lerp(1.0f, saturate(aoSample), saturate(material.occlusion.y));
    }
    float3 dielectricSpecularF0 =
        float3(dielectricF0, dielectricF0, dielectricF0) * specularTint;
    if (thinWallEnabled) {
        dielectricSpecularF0 = 2.0f * dielectricSpecularF0 /
                               (1.0f + dielectricSpecularF0);
    }
    float3 F0 = lerp(dielectricSpecularF0, baseColor, metallic);
    if (material.thinFilm.x > 0.0f) {
        float3 filmF0 = ThinFilmReflectance(
            1.0f, material.thinFilm.y, material.surface.w,
            material.thinFilm.x) * specularScale * specularTint;
        if (thinWallEnabled) {
            filmF0 = 2.0f * filmF0 / (1.0f + filmF0);
        }
        F0 = lerp(filmF0, baseColor, metallic);
    }
    const float directTransmissionIor = max(material.surface.w, 1.0f);
    const float directTransmissionF0 = dielectricF0;
    float3 directTransmissionFresnel = directTransmissionF0 +
        (1.0f - directTransmissionF0) *
        pow(1.0f - saturate(dot(worldNormal, viewDirection)), 5.0f);
    if (material.thinFilm.x > 0.0f) {
        directTransmissionFresnel = ThinFilmReflectance(
            saturate(dot(worldNormal, viewDirection)), material.thinFilm.y,
            directTransmissionIor, material.thinFilm.x) * specularScale *
            specularTint;
    }
    if (thinWallEnabled) {
        directTransmissionFresnel = 2.0f * directTransmissionFresnel /
                                    (1.0f + directTransmissionFresnel);
    }
    const float3 directTransmissionDirection = thinWallEnabled
        ? -viewDirection
        : refract(-viewDirection, worldNormal, rcp(directTransmissionIor));
    const bool transmissionIsTir = !thinWallEnabled &&
        dot(directTransmissionDirection, directTransmissionDirection) <=
            1.0e-6f;
    if (transmissionIsTir) {
        directTransmissionFresnel = float3(1.0f, 1.0f, 1.0f);
    }
    const float directCoatNdotV = saturate(dot(coatNormal, viewDirection));
    const float directCoatViewFresnel = clearcoatF0 +
        (1.0f - clearcoatF0) * pow(1.0f - directCoatNdotV, 5.0f);
    const float directCoatViewCosInternal = sqrt(saturate(
        1.0f - (1.0f - directCoatNdotV * directCoatNdotV) /
            (clearcoatIor * clearcoatIor)));
    const float3 directCoatTintPath = pow(max(clearcoatTint, 1.0e-4f),
        rcp(max(directCoatViewCosInternal, 1.0e-3f)));
    const float3 directCoatTransmissionEnergy =
        (1.0f - clearcoat * directCoatViewFresnel) *
        lerp(float3(1.0f, 1.0f, 1.0f), directCoatTintPath, clearcoat);
    float3 directLighting = float3(0.0f, 0.0f, 0.0f);
    float3 ambientLighting = float3(0.0f, 0.0f, 0.0f);
    const bool useFallbackLights = g_RTSceneLightCount.x == 0;
    const uint lightCount = useFallbackLights ? 2 : min(g_RTSceneLightCount.x, 8);
    float3 hitPosition = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float anisotropyAmount = clamp(material.anisotropy.x, -1.0f, 1.0f);
    if (material.anisotropyTexture.x >= 0.0f) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.anisotropyTexture.x);
        const float2 anisotropyUv =
            anisotropyUv0 * (1.0f - attributes.barycentrics.x -
                             attributes.barycentrics.y) +
            anisotropyUv1 * attributes.barycentrics.x +
            anisotropyUv2 * attributes.barycentrics.y;
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            anisotropyUvEdge1, anisotropyUvEdge2, rayConeRadius);
        const float anisotropySample = g_RTPbrTextures[textureIndex].SampleLevel(
            g_RTPbrSampler, anisotropyUv, textureLod).r;
        anisotropyAmount *= saturate(anisotropySample);
    }
    [loop]
    for (uint lightIndex = 0; lightIndex < lightCount; ++lightIndex) {
        RTSceneLight sceneLight = (RTSceneLight)0;
        float3 lightDirection;
        float3 radiance;
        float attenuation = 1.0f;
        float spotFactor = 1.0f;
        float shadowDistance = 10000.0f;
        uint lightType = 0;
        if (useFallbackLights) {
            lightDirection = lightIndex == 0
                ? keyLightDirection
                : normalize(float3(-0.35f, 0.55f, -0.75f));
            radiance = lightIndex == 0
                ? float3(0.70f, 0.70f, 0.70f)
                : float3(0.22f, 0.22f, 0.22f);
        } else {
            sceneLight = g_RTSceneLights[lightIndex];
            lightType = (uint)round(sceneLight.positionType.w);
            radiance = sceneLight.colorAttenuationConstant.rgb *
                       max(sceneLight.directionIntensity.w, 0.0f);
            if (lightType == 0) {
                // Directional light direction describes the direction rays travel.
                lightDirection = -normalize(sceneLight.directionIntensity.xyz);
            } else if (lightType == 3) {
                ambientLighting += radiance;
                continue;
            } else {
                float3 toLight = sceneLight.positionType.xyz - hitPosition;
                float distanceToLight = length(toLight);
                shadowDistance = max(distanceToLight - 0.01f, 0.001f);
                lightDirection = distanceToLight > 1.0e-5f
                    ? toLight / distanceToLight : worldNormal;
                float denominator = sceneLight.colorAttenuationConstant.w +
                    sceneLight.attenuationSpot.x * distanceToLight +
                    sceneLight.attenuationSpot.y * distanceToLight * distanceToLight;
                attenuation = rcp(max(denominator, 1.0e-4f));
                if (lightType == 2) {
                    float spotCosine = dot(normalize(sceneLight.directionIntensity.xyz),
                                           -lightDirection);
                    spotFactor = saturate(
                        (spotCosine - sceneLight.attenuationSpot.w) /
                        max(sceneLight.attenuationSpot.z -
                            sceneLight.attenuationSpot.w, 1.0e-4f));
                }
            }
        }
        const float softness = useFallbackLights
            ? 0.0f : clamp(sceneLight.shadowParams.x, 0.0f, 2.0f);
        const bool castsSceneShadow = !useFallbackLights && lightType != 3 &&
                                      sceneLight.shadowParams.x >= 0.0f;
        const bool areaSampling = !useFallbackLights && lightType == 4u &&
            sceneLight.shadowParams.y > 0.0f && sceneLight.shadowParams.z > 0.0f;
        const uint directSampleCount = areaSampling ? 4u : 1u;
        const float2 samplePattern[4] = {
            float2(-0.5f, -0.5f), float2(0.5f, -0.5f),
            float2(-0.5f, 0.5f), float2(0.5f, 0.5f)};
        float3 areaNormal = float3(0.0f, 0.0f, 1.0f);
        float3 areaTangent = float3(1.0f, 0.0f, 0.0f);
        float3 areaBitangent = float3(0.0f, 1.0f, 0.0f);
        if (areaSampling) {
            areaNormal = normalize(sceneLight.directionIntensity.xyz);
            areaTangent = normalize(cross(
                abs(areaNormal.y) < 0.9f ? float3(0.0f, 1.0f, 0.0f)
                                        : float3(1.0f, 0.0f, 0.0f),
                areaNormal));
            areaBitangent = normalize(cross(areaNormal, areaTangent));
        }
        float3 tangent = normalize(cross(
            abs(lightDirection.y) < 0.9f ? float3(0.0f, 1.0f, 0.0f)
                                        : float3(1.0f, 0.0f, 0.0f),
            lightDirection));
        float3 bitangent = normalize(cross(lightDirection, tangent));
        [unroll]
        for (uint sampleIndex = 0; sampleIndex < directSampleCount;
             ++sampleIndex) {
            float3 sampleLightDirection = lightDirection;
            float sampleDistance = shadowDistance;
            float sampleAttenuation = attenuation;
            float sampleSpotFactor = spotFactor;
            float3 sampledAreaPosition = sceneLight.positionType.xyz;
            if (areaSampling) {
                const float2 pattern = samplePattern[sampleIndex];
                const float2 areaOffset = sceneLight.shadowParams.w > 0.5f
                    ? pattern * (max(sceneLight.shadowParams.y,
                                     sceneLight.shadowParams.z) * 0.35355339f)
                    : pattern * float2(sceneLight.shadowParams.y,
                                       sceneLight.shadowParams.z);
                sampledAreaPosition += areaTangent * areaOffset.x +
                                       areaBitangent * areaOffset.y;
                const float3 toSample = sampledAreaPosition - hitPosition;
                sampleDistance = length(toSample);
                sampleLightDirection = sampleDistance > 1.0e-5f
                    ? toSample / sampleDistance : worldNormal;
                const float denominator = sceneLight.colorAttenuationConstant.w +
                    sceneLight.attenuationSpot.x * sampleDistance +
                    sceneLight.attenuationSpot.y * sampleDistance * sampleDistance;
                sampleAttenuation = rcp(max(denominator, 1.0e-4f));
                const float areaScale = sceneLight.shadowParams.w > 0.5f
                    ? 3.14159265f * max(sceneLight.shadowParams.y,
                                       sceneLight.shadowParams.z) *
                        max(sceneLight.shadowParams.y,
                            sceneLight.shadowParams.z) * 0.25f
                    : sceneLight.shadowParams.y * sceneLight.shadowParams.z;
                sampleAttenuation *= min(
                    1.0f, max(areaScale, 1.0f) /
                        max(sampleDistance * sampleDistance, 1.0f));
                sampleAttenuation *= saturate(dot(
                    -sampleLightDirection, areaNormal));
                sampleDistance = max(sampleDistance - 0.01f, 0.001f);
            }
            const float NdotL = saturate(dot(worldNormal, sampleLightDirection));
            if (NdotL <= 0.0f && transmission <= 0.0f) continue;
            float sampleVisibility = 1.0f;
            if (castsSceneShadow) {
                const uint visibilitySampleCount = areaSampling
                    ? 1u : (softness > 0.0f ? 4u : 1u);
                float visibleSamples = 0.0f;
                [unroll]
                for (uint visibilityIndex = 0;
                     visibilityIndex < visibilitySampleCount; ++visibilityIndex) {
                    float3 visibilityDirection = sampleLightDirection;
                    float visibilityDistance = sampleDistance;
                    if (!areaSampling && softness > 0.0f) {
                        const float2 offset = samplePattern[visibilityIndex] *
                                              (0.08f * softness);
                        visibilityDirection = normalize(sampleLightDirection +
                            tangent * offset.x + bitangent * offset.y);
                    }
                    RayDesc shadowRay;
                    const float3 shadowOriginNormal = NdotL > 0.0f
                        ? worldNormal : -worldNormal;
                    shadowRay.Origin = hitPosition + shadowOriginNormal * 0.005f;
                    shadowRay.Direction = visibilityDirection;
                    shadowRay.TMin = 0.001f;
                    shadowRay.TMax = visibilityDistance;
                    Payload shadowPayload = (Payload)0;
                    shadowPayload.isShadow = 1;
                    TraceRay(g_TLAS, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                             0xFF, 0, 1, 0, shadowRay, shadowPayload);
                    visibleSamples += shadowPayload.blocked != 0 ? 0.0f : 1.0f;
                }
                sampleVisibility = visibleSamples /
                                   float(visibilitySampleCount);
            }
            if (NdotL <= 0.0f) {
                const float backLight = saturate(
                    -dot(worldNormal, sampleLightDirection));
                directLighting += baseColor * transmission * backLight *
                    (1.0f - saturate(directTransmissionFresnel)) *
                    directCoatTransmissionEnergy * radiance *
                    sampleAttenuation * sampleSpotFactor * sampleVisibility /
                    float(directSampleCount);
                continue;
            }
            const float3 halfVector = normalize(viewDirection + sampleLightDirection);
            const float NdotV = max(saturate(dot(worldNormal, viewDirection)), 1.0e-4f);
            const float NdotH = max(saturate(dot(worldNormal, halfVector)), 1.0e-4f);
            const float VdotH = max(saturate(dot(viewDirection, halfVector)), 1.0e-4f);
            const float anisotropy = anisotropyAmount;
            const float alpha = roughness * roughness;
            const float alphaT = max(alpha * (1.0f + anisotropy), 1.0e-3f);
            const float alphaB = max(alpha * (1.0f - anisotropy), 1.0e-3f);
            const float3 scaledHalf = float3(
                dot(halfVector, worldTangent) / alphaT,
                dot(halfVector, worldBitangent) / alphaB, NdotH);
            const float scaledHalfLengthSquared = dot(scaledHalf, scaledHalf);
            const float distribution = rcp(3.14159265f * alphaT * alphaB *
                scaledHalfLengthSquared * scaledHalfLengthSquared);
            const float ToV = dot(viewDirection, worldTangent);
            const float BoV = dot(viewDirection, worldBitangent);
            const float ToL = dot(sampleLightDirection, worldTangent);
            const float BoL = dot(sampleLightDirection, worldBitangent);
            const float lambdaV = NdotL * length(float3(alphaT * ToV,
                                                         alphaB * BoV, NdotV));
            const float lambdaL = NdotV * length(float3(alphaT * ToL,
                                                         alphaB * BoL, NdotL));
            const float visibility = 0.5f / max(lambdaV + lambdaL, 1.0e-6f);
            float3 fresnel = F0 + (1.0f - F0) * pow(1.0f - VdotH, 5.0f);
            if (material.thinFilm.x > 0.0f) {
                const float3 filmFresnel = ThinFilmReflectance(
                    VdotH, material.thinFilm.y, material.surface.w,
                    material.thinFilm.x) * specularScale * specularTint;
                fresnel = lerp(filmFresnel, baseColor, metallic);
            }
            const float coatFresnel = clearcoatF0 +
                (1.0f - clearcoatF0) * pow(1.0f - VdotH, 5.0f);
            const float coatNdotV = max(
                saturate(dot(coatNormal, viewDirection)), 1.0e-4f);
            const float coatNdotL = max(
                saturate(dot(coatNormal, sampleLightDirection)), 1.0e-4f);
            const float coatNdotH = max(
                saturate(dot(coatNormal, halfVector)), 1.0e-4f);
            const float viewCosInternal = sqrt(saturate(
                1.0f - (1.0f - coatNdotV * coatNdotV) /
                    (clearcoatIor * clearcoatIor)));
            const float lightCosInternal = sqrt(saturate(
                1.0f - (1.0f - coatNdotL * coatNdotL) /
                    (clearcoatIor * clearcoatIor)));
            const float coatPathLength = 0.5f * (
                rcp(max(viewCosInternal, 1.0e-3f)) +
                rcp(max(lightCosInternal, 1.0e-3f)));
            const float3 coatTintPath = pow(max(clearcoatTint, 1.0e-4f),
                                            coatPathLength);
            const float3 coatEnergy = (1.0f - clearcoat * coatFresnel) *
                lerp(float3(1.0f, 1.0f, 1.0f), coatTintPath, clearcoat);
            float3 specular = distribution * visibility * fresnel;
            if (transmission <= 1.0e-4f) {
                const float4 viewLut = sampleRTBrdfLut(float2(NdotV, roughness));
                const float4 lightLut = sampleRTBrdfLut(float2(NdotL, roughness));
                const float Eavg = saturate(viewLut.a);
                const float EssV = saturate(viewLut.r + viewLut.g);
                const float EssL = saturate(lightLut.r + lightLut.g);
                const float3 Fss = F0 + (1.0f - F0) * (1.0f / 21.0f);
                const float3 Fms = Fss * Eavg /
                    max(1.0f - Fss * (1.0f - Eavg), 1.0e-4f);
                specular += Fms * ((1.0f - EssV) * (1.0f - EssL)) /
                    max(3.14159265f * (1.0f - Eavg), 1.0e-4f);
            }
            specular *= coatEnergy;
            const float diffuseF90 = 0.5f + 2.0f * roughness * VdotH * VdotH;
            const float diffuseLightScatter = 1.0f + (diffuseF90 - 1.0f) *
                pow(1.0f - NdotL, 5.0f);
            const float diffuseViewScatter = 1.0f + (diffuseF90 - 1.0f) *
                pow(1.0f - NdotV, 5.0f);
            float3 burleyDiffuse = baseColor * (1.0f - fresnel) *
                (1.0f - metallic) * (1.0f - transmission) *
                (diffuseLightScatter * diffuseViewScatter / 3.14159265f) *
                coatEnergy;
            const float diffuseSigma = saturate(
                material.subsurfaceScattering.y);
            if (diffuseSigma > 1.0e-5f) {
                // Match the raster path's Burley-weighted Oren-Nayar response.
                const float diffuseOrenA = rcp(3.14159265f + diffuseSigma *
                    (1.57079633f - 0.66666667f));
                const float diffuseOrenB = diffuseSigma * diffuseOrenA;
                float diffuseOrenT = dot(sampleLightDirection, viewDirection) -
                                     NdotL * NdotV;
                if (diffuseOrenT > 0.0f) {
                    diffuseOrenT /= max(max(NdotL, NdotV), 1.0e-4f);
                }
                const float diffuseNdotL = saturate(NdotL);
                const float diffuseNdotV = saturate(NdotV);
                const float diffuseSinL = sqrt(max(
                    1.0f - diffuseNdotL * diffuseNdotL, 0.0f));
                const float diffuseSinV = sqrt(max(
                    1.0f - diffuseNdotV * diffuseNdotV, 0.0f));
                float diffuseGL;
                if (diffuseNdotL < 1.0e-6f) {
                    diffuseGL = 0.90412966f - diffuseNdotL;
                } else {
                    const float diffuseThetaL = acos(diffuseNdotL);
                    diffuseGL = diffuseSinL * (diffuseThetaL - 0.66666667f -
                        diffuseSinL * diffuseNdotL) +
                        0.66666667f * (diffuseSinL / diffuseNdotL) *
                        (1.0f - diffuseSinL * diffuseSinL * diffuseSinL);
                }
                float diffuseGV;
                if (diffuseNdotV < 1.0e-6f) {
                    diffuseGV = 0.90412966f - diffuseNdotV;
                } else {
                    const float diffuseThetaV = acos(diffuseNdotV);
                    diffuseGV = diffuseSinV * (diffuseThetaV - 0.66666667f -
                        diffuseSinV * diffuseNdotV) +
                        0.66666667f * (diffuseSinV / diffuseNdotV) *
                        (1.0f - diffuseSinV * diffuseSinV * diffuseSinV);
                }
                const float diffuseAverageEnergy =
                    diffuseOrenA * 3.14159265f +
                    ((6.28318531f - 5.6f) / 3.0f) * diffuseOrenB;
                const float diffuseEnergyL = diffuseOrenA * 3.14159265f +
                                             diffuseOrenB * diffuseGL;
                const float diffuseEnergyV = diffuseOrenA * 3.14159265f +
                                             diffuseOrenB * diffuseGV;
                const float3 diffuseAlbedo = saturate(baseColor);
                float3 diffuseMultiScatter =
                    (diffuseAlbedo * diffuseAlbedo) /
                    (1.0f - diffuseAlbedo *
                        (1.0f - diffuseAverageEnergy));
                diffuseMultiScatter *= (diffuseAverageEnergy /
                    max(1.0f - diffuseAverageEnergy, 1.0e-7f)) /
                    3.14159265f;
                const float3 diffuseOren = diffuseOrenA +
                    diffuseOrenB * diffuseOrenT + diffuseMultiScatter *
                    (1.0f - diffuseEnergyV) * (1.0f - diffuseEnergyL);
                burleyDiffuse *=
                    diffuseOren * 3.14159265f;
            }
            const float subsurfaceWeight = saturate(material.subsurface.x);
            const float subsurfaceFss90 = roughness * VdotH * VdotH;
            const float subsurfaceFss =
                lerp(1.0f, subsurfaceFss90, 1.0f - NdotL) *
                lerp(1.0f, subsurfaceFss90, 1.0f - NdotV);
            const float subsurfaceScatter = 1.25f *
                (subsurfaceFss *
                    (rcp(max(NdotL + NdotV, 1.0e-4f)) - 0.5f) + 0.5f);
            const float subsurfaceAnisotropy = clamp(
                material.subsurfaceScattering.x, -0.9f, 0.9f);
            const float3 subsurfacePhaseDirection =
                sampleLightDirection + viewDirection;
            const float subsurfacePhaseDirectionLengthSquared =
                dot(subsurfacePhaseDirection, subsurfacePhaseDirection);
            const float subsurfaceCosTheta = clamp(
                subsurfacePhaseDirectionLengthSquared > 1.0e-8f
                    ? dot(-sampleLightDirection, viewDirection) /
                          sqrt(subsurfacePhaseDirectionLengthSquared)
                    : 0.0f,
                -1.0f, 1.0f);
            const float subsurfacePhaseDenominator = max(
                1.0f + subsurfaceAnisotropy * subsurfaceAnisotropy -
                    2.0f * subsurfaceAnisotropy * subsurfaceCosTheta,
                0.05f);
            const float subsurfacePhase = clamp(
                (1.0f - subsurfaceAnisotropy * subsurfaceAnisotropy) /
                    pow(subsurfacePhaseDenominator, 1.5f),
                0.05f, 4.0f);
            const float subsurfaceNeutralPhase = 1.0f /
                pow(max(1.0f + subsurfaceCosTheta * subsurfaceCosTheta, 0.05f),
                    1.5f);
            const float subsurfacePhaseScale = subsurfacePhase /
                max(subsurfaceNeutralPhase, 1.0e-3f);
            const float3 subsurfaceColor = float3(
                SrgbChannelToLinear(material.subsurface.y),
                SrgbChannelToLinear(material.subsurface.z),
                SrgbChannelToLinear(material.subsurface.w));
            const float subsurfaceIorRatio =
                (max(material.subsurfaceRadius.w, 1.0f) - 1.0f) /
                (max(material.subsurfaceRadius.w, 1.0f) + 1.0f);
            const float subsurfaceF0 = subsurfaceIorRatio * subsurfaceIorRatio;
            const float3 subsurfaceFresnel = subsurfaceF0 +
                (1.0f - subsurfaceF0) * pow(1.0f - VdotH, 5.0f);
            float3 subsurfaceDiffuse = subsurfaceColor *
                (1.0f - subsurfaceFresnel) *
                (1.0f - metallic) * (1.0f - transmission) *
                (subsurfaceScatter * subsurfacePhaseScale / 3.14159265f) * coatEnergy *
                max(material.subsurfaceRadius.rgb, 0.0f);
            float3 diffuse = lerp(burleyDiffuse, subsurfaceDiffuse,
                                  subsurfaceWeight);
            const float sheenViewAlbedo = saturate(sampleRTBrdfLut(
                float2(NdotV, sheenRoughness)).b);
            const float sheenLightAlbedo = saturate(sampleRTBrdfLut(
                float2(NdotL, sheenRoughness)).b);
            const float sheenBaseEnergy = saturate(1.0f - sheen *
                max(sheenTint.x, max(sheenTint.y, sheenTint.z)) *
                max(sheenViewAlbedo, sheenLightAlbedo));
            float3 sheenLobe = sheenTint * sheen *
                CharlieSheen(sheenRoughness, NdotH, NdotV, NdotL);
            diffuse *= sheenBaseEnergy;
            specular *= sheenBaseEnergy;
            sheenLobe *= coatEnergy;
            const float coatAlpha = clearcoatRoughness * clearcoatRoughness;
            const float coatAlpha2 = coatAlpha * coatAlpha;
            const float coatGeometryK = (clearcoatRoughness + 1.0f) *
                (clearcoatRoughness + 1.0f) * 0.125f;
            const float coatGeometryV = coatNdotV /
                lerp(coatNdotV, 1.0f, coatGeometryK);
            const float coatGeometryL = coatNdotL /
                lerp(coatNdotL, 1.0f, coatGeometryK);
            const float coatDenominator =
                coatNdotH * coatNdotH * (coatAlpha2 - 1.0f) + 1.0f;
            const float coatDistribution = coatAlpha2 /
                max(3.14159265f * coatDenominator * coatDenominator, 1.0e-4f);
            const float coatSpecular = coatDistribution *
                coatGeometryV * coatGeometryL /
                max(4.0f * coatNdotV * coatNdotL, 1.0e-4f);
            specular += coatSpecular * clearcoat * coatFresnel * coatTintPath;
            const float visibility = useFallbackLights && lightIndex == 0
                ? keyVisibility : sampleVisibility;
            const float3 effectiveRadiance = radiance * sampleAttenuation *
                                             sampleSpotFactor;
            directLighting += (diffuse + specular + sheenLobe) * effectiveRadiance *
                NdotL * visibility / float(directSampleCount);
        }
    }
    float3 reflectedDirection = reflect(-viewDirection, worldNormal);
    float3 coatReflectedDirection = reflect(-viewDirection, coatNormal);
    const float3 anisotropyDirection = anisotropyAmount >= 0.0f
        ? worldBitangent : worldTangent;
    const float3 anisotropyTangent = cross(anisotropyDirection, viewDirection);
    float3 bentNormal = worldNormal;
    if (dot(anisotropyTangent, anisotropyTangent) > 1.0e-6f) {
        const float3 anisotropicNormal = normalize(
            cross(anisotropyTangent, anisotropyDirection));
        bentNormal = normalize(lerp(worldNormal, anisotropicNormal,
                                    anisotropyAmount));
    }
    const float3 anisotropicReflectedDirection =
        reflect(-viewDirection, bentNormal);
    float3 reflectedSky = lerp(float3(0.16f, 0.20f, 0.28f),
                               float3(0.72f, 0.78f, 0.90f),
                               saturate(reflectedDirection.y * 0.5f + 0.5f));
    reflectedSky = lerp(reflectedSky, float3(0.28f, 0.32f, 0.40f), roughness);
    float3 coatReflectedSky = lerp(
        float3(0.16f, 0.20f, 0.28f), float3(0.72f, 0.78f, 0.90f),
        saturate(coatReflectedDirection.y * 0.5f + 0.5f));
    coatReflectedSky = lerp(coatReflectedSky,
                            float3(0.28f, 0.32f, 0.40f), roughness);
    float3 anisotropicReflectedSky = lerp(
        float3(0.16f, 0.20f, 0.28f), float3(0.72f, 0.78f, 0.90f),
        saturate(anisotropicReflectedDirection.y * 0.5f + 0.5f));
    anisotropicReflectedSky = lerp(anisotropicReflectedSky,
                                  float3(0.28f, 0.32f, 0.40f), roughness);
    const float environmentNdotV = saturate(dot(worldNormal, viewDirection));
    float3 environmentF0 = F0;
    if (material.thinFilm.x > 0.0f) {
        const float3 filmViewFresnel = ThinFilmReflectance(
            environmentNdotV, material.thinFilm.y, material.surface.w,
            material.thinFilm.x) * specularScale * specularTint;
        environmentF0 = lerp(filmViewFresnel, baseColor, metallic);
    }
    const float coatEnvironmentNdotV = saturate(
        dot(coatNormal, viewDirection));
    const float coatViewFresnel = clearcoatF0 + (1.0f - clearcoatF0) *
        pow(1.0f - coatEnvironmentNdotV, 5.0f);
    const float coatViewCosInternal = sqrt(saturate(
        1.0f - (1.0f - coatEnvironmentNdotV * coatEnvironmentNdotV) /
            (clearcoatIor * clearcoatIor)));
    const float3 coatViewTintPath = pow(max(clearcoatTint, 1.0e-4f),
        rcp(max(coatViewCosInternal, 1.0e-3f)));
    float3 specularEnvironment = environmentF0 * anisotropicReflectedSky * 0.50f;
    float3 coatSpecularEnvironment =
        clearcoat * coatViewFresnel * coatViewTintPath *
        coatReflectedSky * 0.50f;
    float3 sheenEnvironment = float3(0.0f, 0.0f, 0.0f);
    if (g_RTEnvironmentSettings.x > 0.5f) {
        float rotationSin;
        float rotationCos;
        sincos(g_RTEnvironmentSettings.z, rotationSin, rotationCos);
        float3 environmentDirection = float3(
            rotationCos * reflectedDirection.x + rotationSin * reflectedDirection.z,
            reflectedDirection.y,
            -rotationSin * reflectedDirection.x + rotationCos * reflectedDirection.z);
        float3 coatEnvironmentDirection = float3(
            rotationCos * coatReflectedDirection.x +
                rotationSin * coatReflectedDirection.z,
            coatReflectedDirection.y,
            -rotationSin * coatReflectedDirection.x +
                rotationCos * coatReflectedDirection.z);
        float3 anisotropicEnvironmentDirection = float3(
            rotationCos * anisotropicReflectedDirection.x +
                rotationSin * anisotropicReflectedDirection.z,
            anisotropicReflectedDirection.y,
            -rotationSin * anisotropicReflectedDirection.x +
                rotationCos * anisotropicReflectedDirection.z);
        float3 environmentTangent = float3(
            rotationCos * worldTangent.x + rotationSin * worldTangent.z,
            worldTangent.y,
            -rotationSin * worldTangent.x + rotationCos * worldTangent.z);
        float3 environmentBitangent = float3(
            rotationCos * worldBitangent.x + rotationSin * worldBitangent.z,
            worldBitangent.y,
            -rotationSin * worldBitangent.x +
                rotationCos * worldBitangent.z);
        uint environmentWidth;
        uint environmentHeight;
        uint environmentMipCount;
        g_RTEnvironment.GetDimensions(0, environmentWidth, environmentHeight,
                                      environmentMipCount);
        const float environmentMip = roughness *
            max(float(environmentMipCount) - 1.0f, 0.0f);
        float3 prefilteredSpecular;
        const float anisotropyMagnitude = saturate(abs(anisotropyAmount));
        if (anisotropyMagnitude > 1.0e-4f) {
            float3 broadAxis = anisotropyAmount >= 0.0f
                ? environmentTangent : environmentBitangent;
            broadAxis -= anisotropicEnvironmentDirection *
                dot(broadAxis, anisotropicEnvironmentDirection);
            const float broadAxisLengthSquared = dot(broadAxis, broadAxis);
            if (broadAxisLengthSquared > 1.0e-6f) {
                broadAxis *= rsqrt(broadAxisLengthSquared);
                const float narrowRoughness = saturate(
                    roughness * sqrt(1.0f - anisotropyMagnitude));
                const float broadRoughness = saturate(
                    roughness * sqrt(1.0f + anisotropyMagnitude));
                const float narrowMip = narrowRoughness *
                    max(float(environmentMipCount) - 1.0f, 0.0f);
                const float broadMip = broadRoughness *
                    max(float(environmentMipCount) - 1.0f, 0.0f);
                const float angularOffset =
                    roughness * anisotropyMagnitude * 0.35f;
                const float3 narrowTap = g_RTEnvironment.SampleLevel(
                    g_RTEnvironmentSampler,
                    anisotropicEnvironmentDirection, narrowMip).rgb;
                const float3 positiveTap = g_RTEnvironment.SampleLevel(
                    g_RTEnvironmentSampler,
                    normalize(anisotropicEnvironmentDirection +
                              broadAxis * angularOffset), broadMip).rgb;
                const float3 negativeTap = g_RTEnvironment.SampleLevel(
                    g_RTEnvironmentSampler,
                    normalize(anisotropicEnvironmentDirection -
                              broadAxis * angularOffset), broadMip).rgb;
                const float2 narrowBrdf = sampleRTBrdfLut(
                    float2(environmentNdotV, narrowRoughness)).rg;
                const float2 broadBrdf = sampleRTBrdfLut(
                    float2(environmentNdotV, broadRoughness)).rg;
                prefilteredSpecular = narrowTap *
                        (environmentF0 * narrowBrdf.x + narrowBrdf.y) * 0.5f +
                    (positiveTap + negativeTap) *
                        (environmentF0 * broadBrdf.x + broadBrdf.y) * 0.25f;
            } else {
                float3 prefiltered = g_RTEnvironment.SampleLevel(
                    g_RTEnvironmentSampler, anisotropicEnvironmentDirection,
                    environmentMip).rgb;
                const float2 brdf = sampleRTBrdfLut(
                    float2(environmentNdotV, roughness)).rg;
                prefilteredSpecular = prefiltered *
                    (environmentF0 * brdf.x + brdf.y);
            }
        } else {
            float3 prefiltered = g_RTEnvironment.SampleLevel(
                g_RTEnvironmentSampler, anisotropicEnvironmentDirection,
                environmentMip).rgb;
            const float2 brdf = sampleRTBrdfLut(
                float2(environmentNdotV, roughness)).rg;
            prefilteredSpecular = prefiltered *
                (environmentF0 * brdf.x + brdf.y);
        }
        specularEnvironment = prefilteredSpecular *
            max(g_RTEnvironmentSettings.y, 0.0f);
        const float sheenMip = sheenRoughness *
            max(float(environmentMipCount) - 1.0f, 0.0f);
        const float3 sheenPrefiltered = g_RTSheenEnvironment.SampleLevel(
            g_RTEnvironmentSampler, environmentDirection, sheenMip).rgb *
            max(g_RTEnvironmentSettings.y, 0.0f);
        const float sheenDirectionalAlbedo = saturate(sampleRTBrdfLut(
            float2(environmentNdotV, sheenRoughness)).b);
        sheenEnvironment = sheenPrefiltered * sheenTint *
                           (sheen * sheenDirectionalAlbedo *
                            specularOcclusion(environmentNdotV, ao,
                                              sheenRoughness));
        if (clearcoat > 0.0f) {
            const float coatRoughness = clamp(clearcoatRoughness, 0.04f, 1.0f);
            const float coatMip = coatRoughness *
                max(float(environmentMipCount) - 1.0f, 0.0f);
            const float3 coatPrefiltered = g_RTEnvironment.SampleLevel(
                g_RTEnvironmentSampler, coatEnvironmentDirection, coatMip).rgb *
                max(g_RTEnvironmentSettings.y, 0.0f);
            const float2 coatBrdf = sampleRTBrdfLut(
                float2(coatEnvironmentNdotV, coatRoughness)).rg;
            // The LUT stores the Schlick split-sum coefficients A and B.
            // Apply the coat IOR-derived F0 once through the split-sum fit.
            coatSpecularEnvironment = clearcoat * coatPrefiltered *
                (clearcoatF0 * coatBrdf.x + coatBrdf.y) * coatViewTintPath;
        }
    }
    specularEnvironment *= specularOcclusion(environmentNdotV, ao, roughness);
    // Approximate thin-surface transmission by sampling the prefiltered
    // environment along the refracted view direction. This preserves the
    // Principled IOR/Fresnel controls without pretending that the current
    // single-surface shading data contains a closed-volume thickness model.
    float3 refractedDirection = directTransmissionDirection;
    if (transmissionIsTir) {
        refractedDirection = reflectedDirection;
    }
    float3 transmittedSky = lerp(float3(0.16f, 0.20f, 0.28f),
                                 float3(0.72f, 0.78f, 0.90f),
                                 saturate(refractedDirection.y * 0.5f + 0.5f));
    if (g_RTEnvironmentSettings.x > 0.5f) {
        float rotationSin;
        float rotationCos;
        sincos(g_RTEnvironmentSettings.z, rotationSin, rotationCos);
        float3 environmentDirection = float3(
            rotationCos * refractedDirection.x + rotationSin * refractedDirection.z,
            refractedDirection.y,
            -rotationSin * refractedDirection.x + rotationCos * refractedDirection.z);
        uint environmentWidth;
        uint environmentHeight;
        uint environmentMipCount;
        g_RTEnvironment.GetDimensions(0, environmentWidth, environmentHeight,
                                      environmentMipCount);
        const float environmentMip = roughness *
            max(float(environmentMipCount) - 1.0f, 0.0f);
        transmittedSky = g_RTEnvironment.SampleLevel(g_RTEnvironmentSampler,
            environmentDirection, environmentMip).rgb *
            max(g_RTEnvironmentSettings.y, 0.0f);
    }
    float3 transmissionFresnel = dielectricF0 + (1.0f - dielectricF0) *
        pow(1.0f - saturate(dot(worldNormal, viewDirection)), 5.0f);
    if (material.thinFilm.x > 0.0f) {
        transmissionFresnel = ThinFilmReflectance(
            saturate(dot(worldNormal, viewDirection)), material.thinFilm.y,
            material.surface.w, material.thinFilm.x) * specularScale *
            specularTint;
    }
    if (thinWallEnabled) {
        transmissionFresnel = 2.0f * transmissionFresnel /
                              (1.0f + transmissionFresnel);
    }
    if (transmissionIsTir) {
        transmissionFresnel = float3(1.0f, 1.0f, 1.0f);
    }
    float3 transmissionEnvironment = baseColor * transmittedSky * transmission *
                                     (1.0f - saturate(transmissionFresnel)) * ao;
    // Use the same cosine-convolved irradiance cube as the forward PBR path.
    float3 diffuseEnvironment = float3(0.16f, 0.16f, 0.16f);
    if (g_RTEnvironmentSettings.x > 0.5f) {
        float rotationSin;
        float rotationCos;
        sincos(g_RTEnvironmentSettings.z, rotationSin, rotationCos);
        float3 diffuseDirection = float3(
            rotationCos * worldNormal.x + rotationSin * worldNormal.z,
            worldNormal.y,
            -rotationSin * worldNormal.x + rotationCos * worldNormal.z);
        uint irradianceWidth;
        uint irradianceHeight;
        uint irradianceMipCount;
        g_RTIrradiance.GetDimensions(0, irradianceWidth, irradianceHeight,
                                     irradianceMipCount);
        diffuseEnvironment = g_RTIrradiance.SampleLevel(
            g_RTEnvironmentSampler, diffuseDirection,
            max(float(irradianceMipCount) - 1.0f, 0.0f)).rgb *
            max(g_RTEnvironmentSettings.y, 0.0f);
    }
    diffuseEnvironment *= ao;
    const float diffuseDirectionalAlbedo = sampleRTDiffuseDirectionalAlbedo(
        environmentNdotV, roughness);
    const float subsurfaceDirectionalAlbedo = lerp(
        diffuseDirectionalAlbedo, 1.0f, saturate(material.subsurface.x));
    const float3 subsurfaceColor = float3(
        SrgbChannelToLinear(material.subsurface.y),
        SrgbChannelToLinear(material.subsurface.z),
        SrgbChannelToLinear(material.subsurface.w));
    const float subsurfaceIorRatio =
        (max(material.subsurfaceRadius.w, 1.0f) - 1.0f) /
        (max(material.subsurfaceRadius.w, 1.0f) + 1.0f);
    const float subsurfaceF0 = subsurfaceIorRatio * subsurfaceIorRatio;
    const float3 subsurfaceViewFresnel = subsurfaceF0 +
        (1.0f - subsurfaceF0) * pow(1.0f - environmentNdotV, 5.0f);
    const float3 subsurfaceDiffuseEnergy =
        (1.0f - subsurfaceViewFresnel) * (1.0f - metallic) *
        (1.0f - transmission);
    const float3 diffuseColor = lerp(
        baseColor, subsurfaceColor * max(material.subsurfaceRadius.rgb, 0.0f),
        saturate(material.subsurface.x));
    ambientLighting *= ao;
    if (g_RTEnvironmentSettings.x > 0.5f && transmission <= 1.0e-4f) {
        const float4 viewLut = sampleRTBrdfLut(
            float2(environmentNdotV, roughness));
        const float Eavg = saturate(viewLut.a);
        const float EssV = saturate(viewLut.r + viewLut.g);
        const float3 Fss = environmentF0 +
            (1.0f - environmentF0) * (1.0f / 21.0f);
        const float3 Fms = Fss * Eavg /
            max(1.0f - Fss * (1.0f - Eavg), 1.0e-4f);
        // Approximate incoming-direction Ess by its cosine-weighted mean;
        // diffuseEnvironment already contains the cosine convolution and AO.
        const float3 multipleScatterIbl = diffuseEnvironment * Fms *
            (1.0f - EssV) / 3.14159265f;
        specularEnvironment += multipleScatterIbl *
            specularOcclusion(environmentNdotV, ao, roughness);
    }
    float3 diffuseEnergy = (1.0f - environmentF0) * (1.0f - metallic) *
                          (1.0f - transmission);
    diffuseEnergy = lerp(diffuseEnergy, subsurfaceDiffuseEnergy,
                         saturate(material.subsurface.x));
    float sheenNdotV = saturate(dot(worldNormal, viewDirection));
    float sheenDirectionalAlbedo = saturate(sampleRTBrdfLut(
        float2(sheenNdotV, sheenRoughness)).b);
    float sheenBaseEnergy = saturate(1.0f - sheen *
        max(sheenTint.x, max(sheenTint.y, sheenTint.z)) *
        sheenDirectionalAlbedo);
    float3 baseEnvironment = diffuseColor * diffuseEnergy *
                             (diffuseEnvironment * subsurfaceDirectionalAlbedo +
                              ambientLighting) *
                             sheenBaseEnergy +
                             specularEnvironment * sheenBaseEnergy;
    const float3 coatEnvironmentEnergy =
        (1.0f - clearcoat * coatViewFresnel) *
        lerp(float3(1.0f, 1.0f, 1.0f), coatViewTintPath, clearcoat);
    float3 environmentFill =
                             (transmissionEnvironment + sheenEnvironment) *
                                 coatEnvironmentEnergy +
                             baseEnvironment * coatEnvironmentEnergy +
                             coatSpecularEnvironment;
    float3 emissionColor = float3(SrgbChannelToLinear(material.emission.r),
                                  SrgbChannelToLinear(material.emission.g),
                                  SrgbChannelToLinear(material.emission.b));
    if (material.emissionTexture.x >= 0.0f) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.emissionTexture.x);
        const float textureLod = calculatePbrTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            emissionUvEdge1, emissionUvEdge2,
            rayConeRadius);
        emissionColor *= g_RTPbrTextures[textureIndex].SampleLevel(
            g_RTPbrSampler, emissionTextureUv, textureLod).rgb;
    }
    float3 emission = emissionColor * max(material.emission.a, 0.0f);
    // Emission is below the coat and sheen lobes in the Principled stack.
    emission *= coatEnvironmentEnergy * sheenBaseEnergy;
    // Match the forward MeshRenderer's fixed studio ambient fill. Keep this
    // additive term outside diffuse-energy and clearcoat weighting, as in the
    // raster PBR path.
    payload.color = float4(directLighting + environmentFill +
                               baseColor * (0.24f * ao) + emission,
                           1.0f);
}
)";

constexpr const char* kWarmupAnyHitShaderSource = R"(
struct RTPrimitive { uint i0; uint i1; uint i2; uint materialIndex; };
struct RTMaterial
{
    float4 baseColor;
    float4 surface;
    float4 specularTint;
    float4 thinFilm;
    float4 extras;
    float4 emission;
    float4 sheen;
    float4 occlusion;
    float4 occlusionUv;
    float4 baseColorTexture;
    float4 surfaceTexture;
    float4 normalTexture;
    float4 emissionTexture;
    float4 opacityTexture;
    float4 alpha;
    float4 anisotropy;
    float4 subsurface;
    float4 subsurfaceRadius;
    float4 subsurfaceScattering;
    float4 clearcoatIor;
    float4 clearcoatNormal;
    float4 normalUvOffset;
    float4 clearcoatNormalUvOffset;
    float4 occlusionTextureUvOffset;
    float4 baseColorTextureUvRotation;
    float4 emissionTextureUvRotation;
    float4 surfaceTextureUvRotation;
    float4 opacityTextureUvRotation;
    float4 metallicTexture;
    float4 roughnessTexture;
    float4 metallicTextureUvOffset;
    float4 roughnessTextureUvOffset;
    float4 transmissionMode;
    float4 clearcoatTexture;
    float4 clearcoatTextureUvOffset;
    float4 clearcoatRoughnessTexture;
    float4 clearcoatRoughnessTextureUvOffset;
    float4 sheenColorTexture;
    float4 sheenColorTextureUvOffset;
    float4 sheenRoughnessTexture;
    float4 sheenRoughnessTextureUvOffset;
    float4 anisotropyTexture;
    float4 anisotropyTextureUvOffset;
};
StructuredBuffer<float4> g_RenderVertices : register(t1);
StructuredBuffer<RTPrimitive> g_RenderPrimitives : register(t2);
StructuredBuffer<uint> g_InstancePrimitiveOffsets : register(t3);
StructuredBuffer<RTMaterial> g_RenderMaterials : register(t5);
StructuredBuffer<float2> g_RenderUvs : register(t10);
StructuredBuffer<float4> g_RenderColors : register(t11);
StructuredBuffer<float2> g_RenderUvs1 : register(t12);
Texture2D<float4> g_RTPbrTextures[RT_OCCLUSION_TEXTURE_COUNT] : register(t0, space1);
SamplerState g_RTPbrSampler : register(s2);
struct Payload { float4 color; float3 rayDirection; uint isShadow; uint blocked; float rayConeWidth; };
float2 transformOpacityUv(float2 uv, float2 scale, float angle, float2 offset)
{
    float2 scaled = uv * scale;
    float sine;
    float cosine;
    sincos(angle, sine, cosine);
    const float2 centered = scaled - 0.5f;
    return float2(centered.x * cosine - centered.y * sine,
                  centered.x * sine + centered.y * cosine) + 0.5f + offset;
}
float2 transformBaseColorTextureOpacityUv(float2 uv, float scale,
                                          float angle, float2 offset)
{
    float2 centered = (uv - 0.5f) * clamp(scale, 0.01f, 10.0f);
    if (abs(angle) > 1.0e-5f) {
        float sine;
        float cosine;
        sincos(angle, sine, cosine);
        centered = float2(centered.x * cosine - centered.y * sine,
                          centered.x * sine + centered.y * cosine);
    }
    return centered + 0.5f + offset;
}
float2 transformOpacityMapUv(float2 uv, float scale, float angle,
                             float2 offset)
{
    return transformBaseColorTextureOpacityUv(uv, scale, angle, offset);
}
float calculateOpacityTextureLod(uint textureIndex, float3 worldEdge1,
                                 float3 worldEdge2, float2 uvEdge1,
                                 float2 uvEdge2, float rayConeRadius)
{
    uint textureWidth = 0;
    uint textureHeight = 0;
    uint mipCount = 0;
    g_RTPbrTextures[NonUniformResourceIndex(textureIndex)].GetDimensions(
        0, textureWidth, textureHeight, mipCount);
    if (textureWidth == 0 || textureHeight == 0 || mipCount <= 1)
        return 0.0f;
    const float edge1Length = length(worldEdge1);
    if (edge1Length <= 1.0e-7f) return 0.0f;
    const float3 axis1 = worldEdge1 / edge1Length;
    const float edge2Projection = dot(worldEdge2, axis1);
    const float3 perpendicularEdge2 = worldEdge2 - axis1 * edge2Projection;
    const float edge2Length = length(perpendicularEdge2);
    if (edge2Length <= 1.0e-7f) return 0.0f;
    const float2 textureSize = float2(textureWidth, textureHeight);
    const float2 jacobian1 = uvEdge1 * textureSize / edge1Length;
    const float2 jacobian2 = (uvEdge2 * textureSize -
        jacobian1 * edge2Projection) / edge2Length;
    const float a = dot(jacobian1, jacobian1);
    const float b = dot(jacobian1, jacobian2);
    const float c = dot(jacobian2, jacobian2);
    const float discriminant = max((a - c) * (a - c) + 4.0f * b * b, 0.0f);
    const float density = sqrt(max(0.5f * (a + c + sqrt(discriminant)), 0.0f));
    return clamp(log2(max(density * rayConeRadius, 1.0f)),
                 0.0f, float(mipCount - 1));
}
[shader("anyhit")]
void RTWarmup_AnyHit(inout Payload payload,
                     in BuiltInTriangleIntersectionAttributes attributes)
{
    const uint primitiveOffset = g_InstancePrimitiveOffsets[InstanceID()];
    if (primitiveOffset == 0xFFFFFFFFu) return;
    const RTPrimitive primitive =
        g_RenderPrimitives[primitiveOffset + PrimitiveIndex()];
    const RTMaterial material = g_RenderMaterials[primitive.materialIndex];
    if (material.alpha.z < 0.5f || material.alpha.z >= 1.5f) return;

    const float barycentric0 = 1.0f - attributes.barycentrics.x -
                               attributes.barycentrics.y;
    const float2 uv0 = g_RenderUvs[primitive.i0];
    const float2 uv1 = g_RenderUvs[primitive.i1];
    const float2 uv2 = g_RenderUvs[primitive.i2];
    const float2 baseColorSetUv0 = material.baseColorTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 baseColorSetUv1 = material.baseColorTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 baseColorSetUv2 = material.baseColorTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i2] : uv2;
    const float2 opacitySetUv0 = material.opacityTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i0] : uv0;
    const float2 opacitySetUv1 = material.opacityTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i1] : uv1;
    const float2 opacitySetUv2 = material.opacityTextureUvRotation.y > 0.5f
        ? g_RenderUvs1[primitive.i2] : uv2;
    const float4 color0 = g_RenderColors[primitive.i0];
    const float4 color1 = g_RenderColors[primitive.i1];
    const float4 color2 = g_RenderColors[primitive.i2];
    const float vertexAlpha = color0.a * barycentric0 +
                              color1.a * attributes.barycentrics.x +
                              color2.a * attributes.barycentrics.y;
    const float angle = material.occlusionUv.z;
    const float2 transformedUv = transformOpacityUv(
        opacitySetUv0 * barycentric0 + opacitySetUv1 * attributes.barycentrics.x +
            opacitySetUv2 * attributes.barycentrics.y,
        material.occlusionUv.xy, angle, material.occlusion.zw);
    const float2 transformedUv0 = transformOpacityUv(
        opacitySetUv0, material.occlusionUv.xy, angle, material.occlusion.zw);
    const float2 transformedUv1 = transformOpacityUv(
        opacitySetUv1, material.occlusionUv.xy, angle, material.occlusion.zw);
    const float2 transformedUv2 = transformOpacityUv(
        opacitySetUv2, material.occlusionUv.xy, angle, material.occlusion.zw);
    const float2 baseColorTransformedUv0 = transformOpacityUv(
        baseColorSetUv0, material.occlusionUv.xy, angle, material.occlusion.zw);
    const float2 baseColorTransformedUv1 = transformOpacityUv(
        baseColorSetUv1, material.occlusionUv.xy, angle, material.occlusion.zw);
    const float2 baseColorTransformedUv2 = transformOpacityUv(
        baseColorSetUv2, material.occlusionUv.xy, angle, material.occlusion.zw);
    const float baseColorUvRotation =
        material.baseColorTextureUvRotation.x;
    const float2 baseColorTransformedUv =
        baseColorTransformedUv0 * barycentric0 +
        baseColorTransformedUv1 * attributes.barycentrics.x +
        baseColorTransformedUv2 * attributes.barycentrics.y;
    const float2 baseColorUv = transformBaseColorTextureOpacityUv(
        baseColorTransformedUv, material.baseColorTexture.y, baseColorUvRotation,
        material.baseColorTexture.zw);
    const float2 baseColorUv0 = transformBaseColorTextureOpacityUv(
        baseColorTransformedUv0, material.baseColorTexture.y, baseColorUvRotation,
        material.baseColorTexture.zw);
    const float2 baseColorUv1 = transformBaseColorTextureOpacityUv(
        baseColorTransformedUv1, material.baseColorTexture.y, baseColorUvRotation,
        material.baseColorTexture.zw);
    const float2 baseColorUv2 = transformBaseColorTextureOpacityUv(
        baseColorTransformedUv2, material.baseColorTexture.y, baseColorUvRotation,
        material.baseColorTexture.zw);
    const float opacityUvRotation = material.opacityTextureUvRotation.x;
    const float2 opacityUv = transformOpacityMapUv(
        transformedUv, material.opacityTexture.y, opacityUvRotation,
        material.opacityTexture.zw);
    const float2 opacityUv0 = transformOpacityMapUv(
        transformedUv0, material.opacityTexture.y, opacityUvRotation,
        material.opacityTexture.zw);
    const float2 opacityUv1 = transformOpacityMapUv(
        transformedUv1, material.opacityTexture.y, opacityUvRotation,
        material.opacityTexture.zw);
    const float2 opacityUv2 = transformOpacityMapUv(
        transformedUv2, material.opacityTexture.y, opacityUvRotation,
        material.opacityTexture.zw);
    const float3 p0 = g_RenderVertices[primitive.i0].xyz;
    const float3 p1 = g_RenderVertices[primitive.i1].xyz;
    const float3 p2 = g_RenderVertices[primitive.i2].xyz;
    const float3 worldP0 = mul(ObjectToWorld3x4(), float4(p0, 1.0f));
    const float3 worldP1 = mul(ObjectToWorld3x4(), float4(p1, 1.0f));
    const float3 worldP2 = mul(ObjectToWorld3x4(), float4(p2, 1.0f));
    const float3 worldEdge1 = worldP1 - worldP0;
    const float3 worldEdge2 = worldP2 - worldP0;
    const float2 opacityUvEdge1 = opacityUv1 - opacityUv0;
    const float2 opacityUvEdge2 = opacityUv2 - opacityUv0;
    const float rayConeRadius = max(payload.rayConeWidth * RayTCurrent(), 0.0f);

    float alpha = material.baseColor.a * material.alpha.x * vertexAlpha;
    if (material.baseColorTexture.x >= 0.0f) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.baseColorTexture.x);
        const float lod = calculateOpacityTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            baseColorUv1 - baseColorUv0, baseColorUv2 - baseColorUv0,
            rayConeRadius);
        alpha *= g_RTPbrTextures[textureIndex].SampleLevel(
            g_RTPbrSampler, baseColorUv, lod).a;
    }
    if (material.opacityTexture.x >= 0.0f) {
        const uint textureIndex = NonUniformResourceIndex(
            (uint)material.opacityTexture.x);
        const float lod = calculateOpacityTextureLod(
            textureIndex, worldEdge1, worldEdge2,
            opacityUvEdge1, opacityUvEdge2,
            rayConeRadius);
        alpha *= g_RTPbrTextures[textureIndex].SampleLevel(
            g_RTPbrSampler, opacityUv, lod).a;
    }
    if (alpha < material.alpha.y) IgnoreHit();
}
)";

constexpr const char* kWarmupRayGenName = "RTWarmup_RayGen";
constexpr const char* kWarmupMissName = "RTWarmup_Miss";
constexpr const char* kWarmupHitName = "RTWarmup_ClosestHit";
constexpr const char* kWarmupAnyHitName = "RTWarmup_AnyHit";

void bindWarmupOutput(IPipelineState* pPSO, ITextureView* pUAV)
{
    if (!pPSO || !pUAV) {
        return;
    }
    if (auto* pVar = pPSO->GetStaticVariableByName(SHADER_TYPE_RAY_GEN, "g_OutputTex")) {
        pVar->Set(pUAV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
    }
}

void bindWarmupTLAS(IPipelineState* pPSO, ITopLevelAS* pTLAS)
{
    if (!pPSO || !pTLAS) return;
    if (auto* pVar = pPSO->GetStaticVariableByName(SHADER_TYPE_RAY_GEN, "g_TLAS")) {
        pVar->Set(pTLAS);
    }
}

void bindWarmupSceneGeometry(IShaderResourceBinding* pSRB, IBuffer* pVertices,
                             IBuffer* pPrimitives, IBuffer* pOffsets, IBuffer* pNormals,
                             IBuffer* pUvs, IBuffer* pUvs1, IBuffer* pColors,
                             IBuffer* pMaterials)
{
    if (!pSRB || !pVertices || !pPrimitives || !pOffsets || !pNormals ||
        !pUvs || !pUvs1 || !pColors || !pMaterials) return;
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderVertices")) {
        pVar->Set(pVertices->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderPrimitives")) {
        pVar->Set(pPrimitives->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_InstancePrimitiveOffsets")) {
        pVar->Set(pOffsets->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderNormals")) {
        pVar->Set(pNormals->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderUvs")) {
        pVar->Set(pUvs->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderUvs1")) {
        pVar->Set(pUvs1->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderColors")) {
        pVar->Set(pColors->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderMaterials")) {
        pVar->Set(pMaterials->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_ANY_HIT, "g_RenderVertices")) {
        pVar->Set(pVertices->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_ANY_HIT, "g_RenderPrimitives")) {
        pVar->Set(pPrimitives->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_ANY_HIT, "g_InstancePrimitiveOffsets")) {
        pVar->Set(pOffsets->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_ANY_HIT, "g_RenderUvs")) {
        pVar->Set(pUvs->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_ANY_HIT, "g_RenderUvs1")) {
        pVar->Set(pUvs1->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_ANY_HIT, "g_RenderColors")) {
        pVar->Set(pColors->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    }
    if (auto* pVar = pSRB->GetVariableByName(SHADER_TYPE_RAY_ANY_HIT, "g_RenderMaterials")) {
        pVar->Set(pMaterials->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
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
        if (!createFallbackEnvironment() || !createFallbackOcclusionTexture()) {
            return false;
        }

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
            createTLAS();
        }

        return true;
    }

    void destroy() override {
        pTraceOutputSRV_.Release();
        pTraceOutputUAV_.Release();
        pTraceOutputTexture_.Release();
        pEnvironmentSRV_.Release();
        pFallbackEnvironmentSRV_.Release();
        pFallbackEnvironmentTexture_.Release();
        pFallbackOcclusionSRV_.Release();
        pFallbackOcclusionTexture_.Release();
        pEnvironmentConstantsBuffer_.Release();
        pSceneLightsConstantsBuffer_.Release();
        pRayTracingPSO_.Release();
        pRayTracingSRB_.Release();
        pSBT_.Release();
        pWarmupRayGen_.Release();
        pWarmupMiss_.Release();
        pWarmupClosestHit_.Release();
        pWarmupAnyHit_.Release();
        pTLAS_.Release();
        pBLASScratchBuffer_.Release();
        pTLASScratchBuffer_.Release();
        pTLASInstanceBuffer_.Release();
        pRenderVertices_.Release();
        pRenderNormals_.Release();
        pRenderUvs_.Release();
        pRenderUvs1_.Release();
        pRenderColors_.Release();
        pRenderIndices_.Release();
        pInstancePrimitiveOffsets_.Release();
        pRayCameraBuffer_.Release();
        blasMap_.clear();
        pbrTextureCache_.clear();
        rtPbrTextureViews_.clear();
        pbrTextureCapacity_ = 0;
        tlasInstanceCount_ = 0;
        environmentEnabled_ = false;
        environmentIntensity_ = 1.0f;
        environmentRotationRadians_ = 0.0f;
        environmentSettingsUploaded_ = false;
        sbtHitGroupBound_ = false;
        pDevice_.Release();
        caps_ = {};
    }

    bool createOrUpdateBLAS(const UniString& id, const RTGeometryData& data) override {
        if (!rtSupported_ || !pDevice_ || id.toQString().isEmpty() ||
            !data.pVertexBuffer || data.vertexCount < 3) return false;
        const auto& vertexDesc = data.pVertexBuffer->GetDesc();
        const Uint64 minimumVertexStride = sizeof(float) * 3u;
        if (vertexDesc.ElementByteStride < minimumVertexStride ||
            (vertexDesc.ElementByteStride % sizeof(float)) != 0 ||
            static_cast<Uint64>(data.vertexCount) * vertexDesc.ElementByteStride >
                vertexDesc.Size) {
            return false;
        }
        const Uint32 primitiveCount = data.pIndexBuffer
            ? data.indexCount / 3u
            : data.vertexCount / 3u;
        if ((data.pIndexBuffer &&
             (data.indexCount < 3 || (data.indexCount % 3u) != 0 ||
              static_cast<Uint64>(data.indexCount) * sizeof(Uint32) >
                  data.pIndexBuffer->GetDesc().Size)) ||
            (!data.pIndexBuffer && data.indexCount != 0) ||
            primitiveCount == 0 ||
            (caps_.maxPrimitivesPerBLAS > 0 &&
             primitiveCount > caps_.maxPrimitivesPerBLAS)) {
            return false;
        }
        if ((data.pVertexBuffer->GetDesc().BindFlags & BIND_RAY_TRACING) == 0) {
            return false;
        }
        if (data.pIndexBuffer &&
            (data.pIndexBuffer->GetDesc().BindFlags & BIND_RAY_TRACING) == 0) {
            return false;
        }
        const auto key = id.toQString().toStdWString();
        const auto keyUtf8 = id.toQString().toStdString();
        auto& node = blasMap_[key];
        const bool geometryLayoutChanged =
            node.data.pVertexBuffer != data.pVertexBuffer ||
            node.data.pIndexBuffer != data.pIndexBuffer ||
            node.data.vertexCount != data.vertexCount ||
            node.data.indexCount != data.indexCount;
        const bool geometryContentChanged =
            geometryLayoutChanged || !node.pBLAS ||
            node.data.geometryRevision != data.geometryRevision ||
            node.data.topologyRevision != data.topologyRevision;
        const bool topologyChanged =
            geometryLayoutChanged || !node.pBLAS ||
            node.data.topologyRevision != data.topologyRevision;
        const bool transformChanged =
            !node.pBLAS ||
            std::memcmp(node.transform.Data(), data.transform.Data(),
                        sizeof(float) * 16) != 0;
        const bool wasInactive = !node.active;
        const bool hadPendingGeometryWork = node.dirty || node.requiresRebuild;
        RefCntAutoPtr<IBottomLevelAS> replacementBLAS;
        if (topologyChanged) {
            BLASTriangleDesc triangles;
            triangles.GeometryName = keyUtf8.c_str();
            triangles.VertexValueType = VT_FLOAT32;
            triangles.VertexComponentCount = 3;
            triangles.MaxVertexCount = data.vertexCount;
            triangles.IndexType = data.pIndexBuffer ? VT_UINT32 : VT_UNDEFINED;
            triangles.MaxPrimitiveCount = primitiveCount;
            BottomLevelASDesc desc;
            desc.Name = keyUtf8.c_str();
            desc.Flags = RAYTRACING_BUILD_AS_ALLOW_UPDATE |
                         RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
            desc.pTriangles = &triangles;
            desc.TriangleCount = 1;
            pDevice_->CreateBLAS(desc, &replacementBLAS);
            if (!replacementBLAS) return false;
        }
        node.data = data;
        node.dirty = geometryContentChanged || hadPendingGeometryWork;
        node.requiresRebuild = topologyChanged || node.requiresRebuild;
        node.active = true;
        if (topologyChanged) {
            node.pBLAS = std::move(replacementBLAS);
            node.built = false;
        }
        node.transform = data.transform;
        caps_.registeredBLASCount = 0;
        for (const auto& [name, registered] : blasMap_) {
            if (registered.pBLAS) ++caps_.registeredBLASCount;
        }
        return node.pBLAS != nullptr &&
               (geometryContentChanged || hadPendingGeometryWork ||
                transformChanged || wasInactive);
    }

    bool updateShadingGeometry(const UniString& id, const float* vertices,
                               Uint32 vertexCount, const Uint32* indices,
                               Uint32 indexCount, const float* vertexNormals,
                               const Uint32* primitiveMaterialIndices,
                               const float* vertexUvs,
                               const float* vertexColors,
                               const float* vertexUvs1) override {
        const auto it = blasMap_.find(id.toQString().toStdWString());
        if (it == blasMap_.end() || !vertices || vertexCount < 3 ||
            indexCount < 3 || (indexCount % 3u) != 0 || !indices) return false;
        for (Uint32 i = 0; i < indexCount; ++i) {
            if (indices[i] >= vertexCount) return false;
        }
        auto& node = it->second;
        node.shadingVertices.assign(vertices, vertices + static_cast<size_t>(vertexCount) * 3u);
        node.shadingIndices.assign(indices, indices + indexCount);
        if (vertexNormals) {
            node.shadingNormals.assign(vertexNormals,
                                       vertexNormals + static_cast<size_t>(vertexCount) * 3u);
        } else {
            node.shadingNormals.clear();
        }
        if (vertexUvs) {
            node.shadingUvs.assign(vertexUvs,
                                   vertexUvs + static_cast<size_t>(vertexCount) * 2u);
        } else {
            node.shadingUvs.clear();
        }
        if (vertexUvs1) {
            node.shadingUvs1.assign(
                vertexUvs1, vertexUvs1 + static_cast<size_t>(vertexCount) * 2u);
        } else {
            node.shadingUvs1.clear();
        }
        if (vertexColors) {
            node.shadingColors.assign(vertexColors,
                                      vertexColors + static_cast<size_t>(vertexCount) * 4u);
        } else {
            node.shadingColors.clear();
        }
        const size_t primitiveCount = static_cast<size_t>(indexCount / 3u);
        if (primitiveMaterialIndices) {
            node.shadingMaterialIndices.assign(
                primitiveMaterialIndices,
                primitiveMaterialIndices + primitiveCount);
        } else {
            node.shadingMaterialIndices.clear();
        }
        shadingPoolsDirty_ = true;
        return true;
    }

    bool setInstancePbrMaterial(const UniString& id,
                                const RTPbrMaterial& material,
                                Uint32 materialIndex) override {
        const auto it = blasMap_.find(id.toQString().toStdWString());
        if (it == blasMap_.end()) return false;
        auto& node = it->second;
        RTMaterial nextMaterial{
            {clampFinite(material.baseColor[0], 0.0f, 1.0f, 1.0f),
             clampFinite(material.baseColor[1], 0.0f, 1.0f, 1.0f),
             clampFinite(material.baseColor[2], 0.0f, 1.0f, 1.0f),
             clampFinite(material.baseColorAlpha, 0.0f, 1.0f, 1.0f)},
            {clampFinite(material.metallic, 0.0f, 1.0f, 0.0f),
             clampFinite(material.roughness, 0.0f, 1.0f, 0.5f),
             clampFinite(material.specular, 0.0f, 1.0f, 0.5f),
             clampFinite(material.ior, 1.0f, 3.0f, 1.5f)},
            {clampFinite(material.specularTint[0], 0.0f, 1.0f, 1.0f),
             clampFinite(material.specularTint[1], 0.0f, 1.0f, 1.0f),
             clampFinite(material.specularTint[2], 0.0f, 1.0f, 1.0f), 0.0f},
            {clampFinite(material.thinFilmThickness, 0.0f, 1000.0f, 0.0f),
             clampFinite(material.thinFilmIOR, 1.0f, 2.5f, 1.33f), 0.0f, 0.0f},
            {clampFinite(material.transmission, 0.0f, 1.0f, 0.0f),
             clampFinite(material.clearcoat, 0.0f, 1.0f, 0.0f),
             clampFinite(material.clearcoatRoughness, 0.0f, 1.0f, 0.03f),
             clampFinite(material.sheen, 0.0f, 1.0f, 0.0f)},
            {clampFinite(material.emissionColor[0], 0.0f, 1.0f, 0.0f),
             clampFinite(material.emissionColor[1], 0.0f, 1.0f, 0.0f),
             clampFinite(material.emissionColor[2], 0.0f, 1.0f, 0.0f),
             std::isfinite(material.emissionStrength)
                 ? std::max(material.emissionStrength, 0.0f) : 0.0f},
            {clampFinite(material.sheenRoughness, 0.0f, 1.0f, 0.5f),
             clampFinite(material.sheenTint[0], 0.0f, 1.0f, 1.0f),
             clampFinite(material.sheenTint[1], 0.0f, 1.0f, 1.0f),
             clampFinite(material.sheenTint[2], 0.0f, 1.0f, 1.0f)},
            {-1.0f, clampFinite(material.occlusionStrength, 0.0f, 1.0f, 1.0f),
             clampFinite(material.uvOffsetU, -10.0f, 10.0f, 0.0f),
             clampFinite(material.uvOffsetV, -10.0f, 10.0f, 0.0f)},
            {clampFinite(material.uvScaleU, 0.01f, 10.0f, 1.0f),
             clampFinite(material.uvScaleV, 0.01f, 10.0f, 1.0f),
             clampFinite(material.uvRotationDegrees, -360.0f, 360.0f, 0.0f) *
                 (3.14159265358979323846f / 180.0f),
             0.0f},
            {-1.0f, 0.0f, 0.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f, 0.0f},
            {-1.0f,
             std::isfinite(material.normalStrength)
                 ? std::max(material.normalStrength, 0.0f) : 1.0f,
             clampFinite(material.normalUvRotationDegrees, -360.0f, 360.0f,
                         0.0f) * (3.14159265358979323846f / 180.0f),
             clampFinite(material.normalUvScale, 0.01f, 10.0f, 1.0f)},
            {-1.0f, 0.0f, 0.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f, 0.0f},
            {clampFinite(material.opacity, 0.0f, 1.0f, 1.0f),
             clampFinite(material.alphaCutoff, 0.0f, 1.0f, 0.5f),
             static_cast<float>(std::min(material.alphaMode, 2u)), 0.0f},
            {clampFinite(material.anisotropy, -1.0f, 1.0f, 0.0f),
             clampFinite(material.anisotropyRotationDegrees, -180.0f, 180.0f,
                         0.0f) * 3.14159265358979323846f / 180.0f,
             0.0f, 0.0f},
            {clampFinite(material.subsurface, 0.0f, 1.0f, 0.0f),
             clampFinite(material.subsurfaceColor[0], 0.0f, 1.0f, 1.0f),
             clampFinite(material.subsurfaceColor[1], 0.0f, 1.0f, 1.0f),
             clampFinite(material.subsurfaceColor[2], 0.0f, 1.0f, 1.0f)},
            {clampFinite(material.subsurfaceRadius[0], 0.0f, 10.0f, 1.0f),
             clampFinite(material.subsurfaceRadius[1], 0.0f, 10.0f, 0.2f),
             clampFinite(material.subsurfaceRadius[2], 0.0f, 10.0f, 0.1f),
             clampFinite(material.subsurfaceIOR, 1.0f, 2.0f, 1.4f)},
            {clampFinite(material.subsurfaceAnisotropy, -0.9f, 0.9f, 0.0f),
             clampFinite(material.diffuseRoughness, 0.0f, 1.0f, 0.0f),
             0.0f, 0.0f},
            {clampFinite(material.clearcoatIOR, 1.0f, 2.0f, 1.5f),
             clampFinite(material.clearcoatTint[0], 0.0f, 1.0f, 1.0f),
             clampFinite(material.clearcoatTint[1], 0.0f, 1.0f, 1.0f),
             clampFinite(material.clearcoatTint[2], 0.0f, 1.0f, 1.0f)},
            {clampFinite(material.clearcoatNormalStrength, 0.0f, 1.0f, 1.0f),
             -1.0f,
             clampFinite(material.clearcoatNormalUvRotationDegrees,
                         -360.0f, 360.0f, 0.0f) *
                 (3.14159265358979323846f / 180.0f),
             clampFinite(material.clearcoatNormalUvScale, 0.01f, 10.0f,
                         1.0f)}};
        nextMaterial.baseColorTexture[1] =
            clampFinite(material.baseColorUvScale, 0.01f, 10.0f, 1.0f);
        nextMaterial.transmissionMode[0] = clampFinite(
            material.thinWall, 0.0f, 1.0f, 0.0f) > 0.5f ? 1.0f : 0.0f;
        nextMaterial.baseColorTexture[2] = clampFinite(
            material.baseColorUvOffsetU, -10.0f, 10.0f, 0.0f);
        nextMaterial.baseColorTexture[3] = clampFinite(
            material.baseColorUvOffsetV, -10.0f, 10.0f, 0.0f);
        nextMaterial.baseColorTextureUvRotation[0] = clampFinite(
            material.baseColorUvRotationDegrees, -360.0f, 360.0f, 0.0f) *
            (3.14159265358979323846f / 180.0f);
        nextMaterial.baseColorTextureUvRotation[1] =
            material.baseColorUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.surfaceTexture[1] = clampFinite(
            material.metallicRoughnessUvScale, 0.01f, 10.0f, 1.0f);
        nextMaterial.surfaceTexture[2] = clampFinite(
            material.metallicRoughnessUvOffsetU, -10.0f, 10.0f, 0.0f);
        nextMaterial.surfaceTexture[3] = clampFinite(
            material.metallicRoughnessUvOffsetV, -10.0f, 10.0f, 0.0f);
        nextMaterial.surfaceTextureUvRotation[0] = clampFinite(
            material.metallicRoughnessUvRotationDegrees, -360.0f, 360.0f,
            0.0f) * (3.14159265358979323846f / 180.0f);
        nextMaterial.surfaceTextureUvRotation[1] =
            material.metallicRoughnessUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.metallicTexture[1] = clampFinite(
            material.metallicTextureUvScaleU, 0.01f, 10.0f, 1.0f);
        nextMaterial.metallicTexture[2] = clampFinite(
            material.metallicTextureUvScaleV, 0.01f, 10.0f, 1.0f);
        nextMaterial.metallicTexture[3] = clampFinite(
            material.metallicTextureUvRotationDegrees, -360.0f, 360.0f,
            0.0f) * (3.14159265358979323846f / 180.0f);
        nextMaterial.metallicTextureUvOffset[0] = clampFinite(
            material.metallicTextureUvOffsetU, -10.0f, 10.0f, 0.0f);
        nextMaterial.metallicTextureUvOffset[1] = clampFinite(
            material.metallicTextureUvOffsetV, -10.0f, 10.0f, 0.0f);
        nextMaterial.metallicTextureUvOffset[2] =
            material.metallicTextureUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.roughnessTexture[1] = clampFinite(
            material.roughnessTextureUvScaleU, 0.01f, 10.0f, 1.0f);
        nextMaterial.roughnessTexture[2] = clampFinite(
            material.roughnessTextureUvScaleV, 0.01f, 10.0f, 1.0f);
        nextMaterial.roughnessTexture[3] = clampFinite(
            material.roughnessTextureUvRotationDegrees, -360.0f, 360.0f,
            0.0f) * (3.14159265358979323846f / 180.0f);
        nextMaterial.roughnessTextureUvOffset[0] = clampFinite(
            material.roughnessTextureUvOffsetU, -10.0f, 10.0f, 0.0f);
        nextMaterial.roughnessTextureUvOffset[1] = clampFinite(
            material.roughnessTextureUvOffsetV, -10.0f, 10.0f, 0.0f);
        nextMaterial.roughnessTextureUvOffset[2] =
            material.roughnessTextureUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.normalUvOffset[0] =
            clampFinite(material.normalUvOffsetU, -10.0f, 10.0f, 0.0f);
        nextMaterial.normalUvOffset[1] =
            clampFinite(material.normalUvOffsetV, -10.0f, 10.0f, 0.0f);
        nextMaterial.normalUvOffset[2] = material.normalUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.occlusionUv[3] = clampFinite(
            material.occlusionTextureUvScale, 0.01f, 10.0f, 1.0f);
        nextMaterial.occlusionTextureUvOffset[0] = clampFinite(
            material.occlusionTextureUvOffsetU, -10.0f, 10.0f, 0.0f);
        nextMaterial.occlusionTextureUvOffset[1] = clampFinite(
            material.occlusionTextureUvOffsetV, -10.0f, 10.0f, 0.0f);
        nextMaterial.occlusionTextureUvOffset[2] = clampFinite(
            material.occlusionTextureUvRotationDegrees, -360.0f, 360.0f,
            0.0f) * (3.14159265358979323846f / 180.0f);
        nextMaterial.occlusionTextureUvOffset[3] =
            material.occlusionTextureUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.emissionTexture[1] = clampFinite(
            material.emissionTextureUvScale, 0.01f, 10.0f, 1.0f);
        nextMaterial.emissionTexture[2] = clampFinite(
            material.emissionTextureUvOffsetU, -10.0f, 10.0f, 0.0f);
        nextMaterial.emissionTexture[3] = clampFinite(
            material.emissionTextureUvOffsetV, -10.0f, 10.0f, 0.0f);
        nextMaterial.emissionTextureUvRotation[0] = clampFinite(
            material.emissionTextureUvRotationDegrees, -360.0f, 360.0f,
            0.0f) * (3.14159265358979323846f / 180.0f);
        nextMaterial.emissionTextureUvRotation[1] =
            material.emissionTextureUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.opacityTexture[1] = clampFinite(
            material.opacityTextureUvScale, 0.01f, 10.0f, 1.0f);
        nextMaterial.opacityTexture[2] = clampFinite(
            material.opacityTextureUvOffsetU, -10.0f, 10.0f, 0.0f);
        nextMaterial.opacityTexture[3] = clampFinite(
            material.opacityTextureUvOffsetV, -10.0f, 10.0f, 0.0f);
        nextMaterial.opacityTextureUvRotation[0] = clampFinite(
            material.opacityTextureUvRotationDegrees, -360.0f, 360.0f,
            0.0f) * (3.14159265358979323846f / 180.0f);
        nextMaterial.opacityTextureUvRotation[1] =
            material.opacityTextureUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.clearcoatNormalUvOffset[0] = clampFinite(
            material.clearcoatNormalUvOffsetU, -10.0f, 10.0f, 0.0f);
        nextMaterial.clearcoatNormalUvOffset[1] = clampFinite(
            material.clearcoatNormalUvOffsetV, -10.0f, 10.0f, 0.0f);
        nextMaterial.clearcoatNormalUvOffset[2] =
            material.clearcoatNormalUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.clearcoatTexture[1] = clampFinite(
            material.clearcoatTextureUvScale, 0.01f, 10.0f, 1.0f);
        nextMaterial.clearcoatTexture[2] = clampFinite(
            material.clearcoatTextureUvRotationDegrees, -360.0f, 360.0f,
            0.0f) * (3.14159265358979323846f / 180.0f);
        nextMaterial.clearcoatTexture[3] =
            material.clearcoatTextureUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.clearcoatTextureUvOffset[0] = clampFinite(
            material.clearcoatTextureUvOffsetU, -10.0f, 10.0f, 0.0f);
        nextMaterial.clearcoatTextureUvOffset[1] = clampFinite(
            material.clearcoatTextureUvOffsetV, -10.0f, 10.0f, 0.0f);
        nextMaterial.clearcoatRoughnessTexture[1] = clampFinite(
            material.clearcoatRoughnessTextureUvScale, 0.01f, 10.0f, 1.0f);
        nextMaterial.clearcoatRoughnessTexture[2] = clampFinite(
            material.clearcoatRoughnessTextureUvRotationDegrees, -360.0f,
            360.0f, 0.0f) * (3.14159265358979323846f / 180.0f);
        nextMaterial.clearcoatRoughnessTexture[3] =
            material.clearcoatRoughnessTextureUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.clearcoatRoughnessTextureUvOffset[0] = clampFinite(
            material.clearcoatRoughnessTextureUvOffsetU, -10.0f, 10.0f, 0.0f);
        nextMaterial.clearcoatRoughnessTextureUvOffset[1] = clampFinite(
            material.clearcoatRoughnessTextureUvOffsetV, -10.0f, 10.0f, 0.0f);
        nextMaterial.sheenColorTexture[1] = clampFinite(
            material.sheenColorTextureUvScaleU, 0.0001f, 10000.0f, 1.0f);
        nextMaterial.sheenColorTexture[2] = clampFinite(
            material.sheenColorTextureUvScaleV, 0.0001f, 10000.0f, 1.0f);
        nextMaterial.sheenColorTexture[3] = clampFinite(
            material.sheenColorTextureUvRotationDegrees, -360.0f, 360.0f,
            0.0f) * (3.14159265358979323846f / 180.0f);
        nextMaterial.sheenColorTextureUvOffset[1] = clampFinite(
            material.sheenColorTextureUvOffsetU, -1000000.0f, 1000000.0f, 0.0f);
        nextMaterial.sheenColorTextureUvOffset[2] = clampFinite(
            material.sheenColorTextureUvOffsetV, -1000000.0f, 1000000.0f, 0.0f);
        nextMaterial.sheenColorTextureUvOffset[3] =
            material.sheenColorTextureUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.sheenRoughnessTexture[1] = clampFinite(
            material.sheenRoughnessTextureUvScaleU, 0.0001f, 10000.0f, 1.0f);
        nextMaterial.sheenRoughnessTexture[2] = clampFinite(
            material.sheenRoughnessTextureUvScaleV, 0.0001f, 10000.0f, 1.0f);
        nextMaterial.sheenRoughnessTexture[3] = clampFinite(
            material.sheenRoughnessTextureUvRotationDegrees, -360.0f, 360.0f,
            0.0f) * (3.14159265358979323846f / 180.0f);
        nextMaterial.sheenRoughnessTextureUvOffset[1] = clampFinite(
            material.sheenRoughnessTextureUvOffsetU, -1000000.0f,
            1000000.0f, 0.0f);
        nextMaterial.sheenRoughnessTextureUvOffset[2] = clampFinite(
            material.sheenRoughnessTextureUvOffsetV, -1000000.0f,
            1000000.0f, 0.0f);
        nextMaterial.sheenRoughnessTextureUvOffset[3] =
            material.sheenRoughnessTextureUvSet == 1 ? 1.0f : 0.0f;
        nextMaterial.anisotropyTexture[1] = clampFinite(
            material.anisotropyTextureUvScaleU, 0.0001f, 10000.0f, 1.0f);
        nextMaterial.anisotropyTexture[2] = clampFinite(
            material.anisotropyTextureUvScaleV, 0.0001f, 10000.0f, 1.0f);
        nextMaterial.anisotropyTexture[3] = clampFinite(
            material.anisotropyTextureUvRotationDegrees, -360.0f, 360.0f,
            0.0f) * (3.14159265358979323846f / 180.0f);
        nextMaterial.anisotropyTextureUvOffset[1] = clampFinite(
            material.anisotropyTextureUvOffsetU, -1000000.0f,
            1000000.0f, 0.0f);
        nextMaterial.anisotropyTextureUvOffset[2] = clampFinite(
            material.anisotropyTextureUvOffsetV, -1000000.0f,
            1000000.0f, 0.0f);
        nextMaterial.anisotropyTextureUvOffset[3] =
            material.anisotropyTextureUvSet == 1 ? 1.0f : 0.0f;
        if (materialIndex >= node.pbrMaterials.size()) {
            node.pbrMaterials.resize(static_cast<size_t>(materialIndex) + 1u,
                                     node.pbrMaterials.front());
            node.baseColorTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.metallicRoughnessTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.metallicTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.roughnessTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.normalTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.clearcoatTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.clearcoatRoughnessTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.clearcoatNormalTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.occlusionTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.emissionTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.opacityTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.sheenColorTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.sheenRoughnessTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
            node.anisotropyTexturePaths.resize(
                static_cast<size_t>(materialIndex) + 1u);
        }
        bool textureChanged = false;
        const auto updateTexturePath = [&](std::vector<QString>& paths,
                                           const UniString& value,
                                           PbrTextureKind kind) {
            const QString path = value.toQString().trimmed();
            if (paths[materialIndex] == path) return;
            paths[materialIndex] = path;
            textureChanged = true;
            if (path.isEmpty()) return;
            const QString cacheKey = pbrTextureCacheKey(path, kind);
            if (pbrTextureCache_.find(cacheKey) != pbrTextureCache_.end()) return;
            RTPbrTexture texture;
            const bool loaded = kind == PbrTextureKind::Occlusion
                ? loadOcclusionTexture(path, texture)
                : loadRgbaPbrTexture(path, kind, texture);
            if (loaded) {
                pbrTextureCache_.emplace(cacheKey, std::move(texture));
            }
        };
        updateTexturePath(node.baseColorTexturePaths,
                          material.baseColorTexture, PbrTextureKind::BaseColor);
        updateTexturePath(node.metallicRoughnessTexturePaths,
                          material.metallicRoughnessTexture,
                          PbrTextureKind::MetallicRoughness);
        updateTexturePath(node.metallicTexturePaths, material.metallicTexture,
                          PbrTextureKind::Metallic);
        updateTexturePath(node.roughnessTexturePaths, material.roughnessTexture,
                          PbrTextureKind::Roughness);
        updateTexturePath(node.normalTexturePaths, material.normalTexture,
                          PbrTextureKind::Normal);
        updateTexturePath(node.clearcoatTexturePaths,
                          material.clearcoatTexture,
                          PbrTextureKind::Clearcoat);
        updateTexturePath(node.clearcoatRoughnessTexturePaths,
                          material.clearcoatRoughnessTexture,
                          PbrTextureKind::ClearcoatRoughness);
        updateTexturePath(node.clearcoatNormalTexturePaths,
                          material.clearcoatNormalTexture,
                          PbrTextureKind::ClearcoatNormal);
        updateTexturePath(node.occlusionTexturePaths,
                          material.occlusionTexture, PbrTextureKind::Occlusion);
        updateTexturePath(node.emissionTexturePaths,
                          material.emissionTexture, PbrTextureKind::Emission);
        updateTexturePath(node.opacityTexturePaths,
                          material.opacityTexture, PbrTextureKind::Opacity);
        updateTexturePath(node.sheenColorTexturePaths,
                          material.sheenColorTexture, PbrTextureKind::SheenColor);
        updateTexturePath(node.sheenRoughnessTexturePaths,
                          material.sheenRoughnessTexture,
                          PbrTextureKind::SheenRoughness);
        updateTexturePath(node.anisotropyTexturePaths,
                          material.anisotropyTexture,
                          PbrTextureKind::Anisotropy);
        auto& current = node.pbrMaterials[materialIndex];
        if (std::memcmp(&current, &nextMaterial, sizeof(RTMaterial)) == 0) {
            if (!textureChanged) return false;
        } else {
            current = nextMaterial;
        }
        const bool wasAlphaTestEnabled = node.alphaTestEnabled;
        node.alphaTestEnabled = false;
        for (const auto& pbrMaterial : node.pbrMaterials) {
            node.alphaTestEnabled = node.alphaTestEnabled ||
                pbrMaterial.alpha[2] >= 0.5f && pbrMaterial.alpha[2] < 1.5f;
        }
        if (wasAlphaTestEnabled != node.alphaTestEnabled && node.built) {
            node.dirty = true;
            node.requiresRebuild = true;
        }
        node.materialDirty = false;
        shadingPoolsDirty_ = true;
        return true;
    }

    void setEnvironmentLighting(IDeviceContext* pContext,
                                ITextureView* irradianceEnvironment,
                                ITextureView* prefilteredEnvironment,
                                ITextureView* brdfLut,
                                bool enabled, float intensity,
                                float rotationDegrees,
                                ITextureView* sheenPrefilteredEnvironment) override {
        const bool nextEnabled = enabled && irradianceEnvironment &&
                                 prefilteredEnvironment && brdfLut;
        const float nextIntensity = std::isfinite(intensity)
            ? std::max(intensity, 0.0f) : 0.0f;
        const float nextRotation = std::isfinite(rotationDegrees)
            ? rotationDegrees * (3.14159265358979323846f / 180.0f) : 0.0f;
        ITextureView* nextEnvironment = prefilteredEnvironment
            ? prefilteredEnvironment : pFallbackEnvironmentSRV_.RawPtr();
        ITextureView* nextIrradiance = irradianceEnvironment
            ? irradianceEnvironment : pFallbackEnvironmentSRV_.RawPtr();
        ITextureView* nextBrdfLut = brdfLut
            ? brdfLut : pFallbackBrdfLutSRV_.RawPtr();
        ITextureView* nextSheenEnvironment = sheenPrefilteredEnvironment
            ? sheenPrefilteredEnvironment : nextEnvironment;
        const bool resourceChanged = pEnvironmentSRV_.RawPtr() != nextEnvironment;
        const bool irradianceChanged =
            pIrradianceEnvironmentSRV_.RawPtr() != nextIrradiance;
        const bool brdfLutChanged = pBrdfLutSRV_.RawPtr() != nextBrdfLut;
        const bool sheenEnvironmentChanged =
            pSheenEnvironmentSRV_.RawPtr() != nextSheenEnvironment;
        const bool settingsChanged = environmentEnabled_ != nextEnabled ||
            environmentIntensity_ != nextIntensity ||
            environmentRotationRadians_ != nextRotation;
        environmentEnabled_ = nextEnabled;
        environmentIntensity_ = nextIntensity;
        environmentRotationRadians_ = nextRotation;
        if (resourceChanged) pEnvironmentSRV_ = nextEnvironment;
        if (irradianceChanged) pIrradianceEnvironmentSRV_ = nextIrradiance;
        if (brdfLutChanged) pBrdfLutSRV_ = nextBrdfLut;
        if (sheenEnvironmentChanged) pSheenEnvironmentSRV_ = nextSheenEnvironment;
        if (settingsChanged && pEnvironmentConstantsBuffer_) {
            updateEnvironmentSettings(pContext);
        }
        if (resourceChanged && pRayTracingSRB_) {
            if (auto* variable = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTEnvironment")) {
                variable->Set(pEnvironmentSRV_);
            }
            if (auto* variable = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_MISS, "g_RTEnvironment")) {
                variable->Set(pEnvironmentSRV_);
            }
        }
        if (irradianceChanged && pRayTracingSRB_) {
            if (auto* variable = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTIrradiance")) {
                variable->Set(pIrradianceEnvironmentSRV_);
            }
        }
        if (brdfLutChanged && pRayTracingSRB_) {
            if (auto* variable = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTBrdfLut")) {
                variable->Set(pBrdfLutSRV_);
            }
        }
        if (sheenEnvironmentChanged && pRayTracingSRB_) {
            if (auto* variable = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTSheenEnvironment")) {
                variable->Set(pSheenEnvironmentSRV_);
            }
        }
    }

    void setSceneLights(IDeviceContext* pContext, const RTSceneLight* lights,
                        Uint32 lightCount) override {
        RTSceneLightsConstants next{};
        const Uint32 count = lights ? std::min(lightCount, 8u) : 0u;
        next.count[0] = count;
        if (count > 0) {
            std::memcpy(next.lights, lights,
                        static_cast<size_t>(count) * sizeof(RTSceneLight));
        }
        if (std::memcmp(&next, &sceneLightsConstants_, sizeof(next)) == 0) {
            return;
        }
        sceneLightsConstants_ = next;
        if (pSceneLightsConstantsBuffer_) {
            updateSceneLightsBuffer(pContext);
        }
    }

    bool buildBLAS(IDeviceContext* pContext) override {
        if (!rtSupported_ || !pDevice_ || !pContext) return false;
        for (auto& [name, node] : blasMap_) {
            if (!node.dirty) continue;
            if (!node.pBLAS || !node.data.pVertexBuffer) return false;
            const auto nameUtf8 = QString::fromStdWString(name).toUtf8().toStdString();
            const auto& vertexDesc = node.data.pVertexBuffer->GetDesc();
            BLASBuildTriangleData triangle;
            triangle.GeometryName = nameUtf8.c_str();
            triangle.pVertexBuffer = node.data.pVertexBuffer;
            triangle.VertexStride = vertexDesc.ElementByteStride;
            triangle.VertexCount = node.data.vertexCount;
            triangle.VertexValueType = VT_FLOAT32;
            triangle.VertexComponentCount = 3;
            triangle.pIndexBuffer = node.data.pIndexBuffer;
            triangle.IndexType = node.data.pIndexBuffer ? VT_UINT32 : VT_UNDEFINED;
            triangle.PrimitiveCount = node.data.pIndexBuffer ? node.data.indexCount / 3 : node.data.vertexCount / 3;
            triangle.Flags = node.alphaTestEnabled
                ? RAYTRACING_GEOMETRY_FLAG_NONE
                : RAYTRACING_GEOMETRY_FLAG_OPAQUE;
            BuildBLASAttribs attribs;
            attribs.pBLAS = node.pBLAS;
            attribs.pTriangleData = &triangle;
            attribs.TriangleDataCount = 1;
            attribs.Update = node.built && !node.requiresRebuild;
            const auto scratchSize = attribs.Update
                ? node.pBLAS->GetScratchBufferSizes().Update
                : node.pBLAS->GetScratchBufferSizes().Build;
            if (!pBLASScratchBuffer_ || pBLASScratchBuffer_->GetDesc().Size < scratchSize) {
                BufferDesc scratchDesc;
                scratchDesc.Name = "RayTracing BLAS Scratch";
                scratchDesc.Usage = USAGE_DEFAULT;
                scratchDesc.BindFlags = BIND_RAY_TRACING;
                scratchDesc.Size = scratchSize;
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
            if (attribs.Update) {
                ++caps_.blasUpdateCount;
            } else {
                ++caps_.blasRebuildCount;
            }
            node.dirty = false;
            node.built = true;
            node.requiresRebuild = false;
        }
        return true;
    }

    bool updateInstanceTransform(const UniString& id,
                                 const Diligent::float4x4& transform) override {
    const auto it = blasMap_.find(id.toQString().toStdWString());
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
    const auto it = blasMap_.find(id.toQString().toStdWString());
        return it != blasMap_.end() && it->second.pBLAS != nullptr;
    }

    bool setInstanceActive(const UniString& id, bool active) override {
        const auto it = blasMap_.find(id.toQString().toStdWString());
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
        if (!rebuildShadingPools(pContext)) return false;
        std::vector<TLASBuildInstanceData> instances;
        NamedVector<std::string> names;
        instances.reserve(blasMap_.size());
        names.reserve(blasMap_.size());
        for (const auto& [name, node] : blasMap_) {
            if (!node.pBLAS) continue;
            names.append(std::string(name.begin(), name.end()));
            TLASBuildInstanceData instance;
            instance.InstanceName = names.at(names.size() - 1)->c_str();
            instance.pBLAS = node.pBLAS;
            instance.CustomId = node.shadingInstanceIndex;
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
        attribs.BindingMode = HIT_GROUP_BINDING_MODE_PER_TLAS;
        attribs.HitGroupStride = 1;
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
        if (attribs.Update) {
            ++caps_.tlasUpdateCount;
        } else {
            ++caps_.tlasRebuildCount;
        }
        attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        attribs.InstanceBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        attribs.TLASTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        attribs.BLASTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        pContext->BuildTLAS(attribs);
        sbtHitGroupBound_ = false;
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
        if (!pRayCameraBuffer_ && !createCameraBuffer()) return false;
        if (!pEnvironmentConstantsBuffer_ && !createEnvironmentConstantsBuffer()) return false;
        if (!pSceneLightsConstantsBuffer_ && !createSceneLightsConstantsBuffer()) return false;
        if (!environmentSettingsUploaded_ && !updateEnvironmentSettings(pContext)) return false;
        const Uint32 requiredTextureCount = static_cast<Uint32>(
            std::max<size_t>(1u, rtPbrTextureViews_.size()));
        if (requiredTextureCount > pbrTextureCapacity_) {
            Uint32 nextCapacity = std::max(1u, pbrTextureCapacity_);
            while (nextCapacity < requiredTextureCount) nextCapacity *= 2u;
            pbrTextureCapacity_ = nextCapacity;
            pSBT_.Release();
            pRayTracingSRB_.Release();
            pRayTracingPSO_.Release();
            pWarmupRayGen_.Release();
            pWarmupMiss_.Release();
            pWarmupClosestHit_.Release();
            pWarmupAnyHit_.Release();
            sbtHitGroupBound_ = false;
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
            shaderCI.Source = kWarmupRayGenShaderSource;
            shaderCI.SourceLength = std::strlen(kWarmupRayGenShaderSource);
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
            shaderCI.Source = kWarmupMissShaderSource;
            shaderCI.SourceLength = std::strlen(kWarmupMissShaderSource);
            pDevice_->CreateShader(shaderCI, &pWarmupMiss_);

            shaderCI.Desc.ShaderType = SHADER_TYPE_RAY_CLOSEST_HIT;
            shaderCI.Desc.Name = "RT Warmup Closest Hit";
            shaderCI.EntryPoint = kWarmupHitName;
            const std::string closestHitShaderSource =
                "#define RT_OCCLUSION_TEXTURE_COUNT " +
                std::to_string(std::max(1u, pbrTextureCapacity_)) + "\n" +
                kWarmupClosestHitShaderSource;
            shaderCI.Source = closestHitShaderSource.c_str();
            shaderCI.SourceLength = closestHitShaderSource.size();
            pDevice_->CreateShader(shaderCI, &pWarmupClosestHit_);

            shaderCI.Desc.ShaderType = SHADER_TYPE_RAY_ANY_HIT;
            shaderCI.Desc.Name = "RT Warmup Any Hit";
            shaderCI.EntryPoint = kWarmupAnyHitName;
            const std::string anyHitShaderSource =
                "#define RT_OCCLUSION_TEXTURE_COUNT " +
                std::to_string(std::max(1u, pbrTextureCapacity_)) + "\n" +
                kWarmupAnyHitShaderSource;
            shaderCI.Source = anyHitShaderSource.c_str();
            shaderCI.SourceLength = anyHitShaderSource.size();
            pDevice_->CreateShader(shaderCI, &pWarmupAnyHit_);

            if (!pWarmupRayGen_ || !pWarmupMiss_ || !pWarmupClosestHit_ ||
                !pWarmupAnyHit_) {
                return false;
            }

            RayTracingPipelineStateCreateInfoX psoCI("RT Warmup PSO");
            psoCI.AddGeneralShader(kWarmupRayGenName, pWarmupRayGen_);
            psoCI.AddGeneralShader(kWarmupMissName, pWarmupMiss_);
            psoCI.AddTriangleHitShader(kWarmupHitName, pWarmupClosestHit_,
                                       pWarmupAnyHit_);
            psoCI.RayTracingPipeline.MaxRecursionDepth = 2;
            psoCI.RayTracingPipeline.ShaderRecordSize = 0;
            psoCI.MaxAttributeSize = 8;
            psoCI.MaxPayloadSize = 48;
            psoCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
            ShaderResourceVariableDesc Vars[] = 
            {
                {SHADER_TYPE_RAY_GEN, "g_OutputTex", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                {SHADER_TYPE_RAY_GEN, "RayCameraConstants", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_GEN, "g_TLAS", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_TLAS", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderVertices", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderPrimitives", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_InstancePrimitiveOffsets", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderNormals", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderUvs", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderUvs1", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderColors", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RenderMaterials", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTPbrTextures", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTEnvironment", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTSheenEnvironment", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTIrradiance", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTBrdfLut", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "RTEnvironmentConstants", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "RTSceneLightsConstants", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_MISS, "g_RTEnvironment", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {SHADER_TYPE_RAY_MISS, "RTEnvironmentConstants", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_ANY_HIT, "g_RenderVertices", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_ANY_HIT, "g_RenderPrimitives", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_ANY_HIT, "g_InstancePrimitiveOffsets", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_ANY_HIT, "g_RenderUvs", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_ANY_HIT, "g_RenderUvs1", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_ANY_HIT, "g_RenderColors", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_ANY_HIT, "g_RenderMaterials", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
                {SHADER_TYPE_RAY_ANY_HIT, "g_RTPbrTextures", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
            };
            const SamplerDesc environmentSamplerDesc{
                FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
                TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};
            SamplerDesc pbrTextureSamplerDesc{
                FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
                TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP,
                TEXTURE_ADDRESS_WRAP};
            const Uint32 supportedAnisotropy =
                pDevice_->GetAdapterInfo().Sampler.MaxAnisotropy;
            if (supportedAnisotropy > 1) {
                pbrTextureSamplerDesc.MinFilter = FILTER_TYPE_ANISOTROPIC;
                pbrTextureSamplerDesc.MagFilter = FILTER_TYPE_ANISOTROPIC;
                pbrTextureSamplerDesc.MipFilter = FILTER_TYPE_ANISOTROPIC;
                pbrTextureSamplerDesc.MaxAnisotropy =
                    std::min<Uint32>(16, supportedAnisotropy);
            }
            const ImmutableSamplerDesc environmentSamplers[] = {
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTEnvironmentSampler",
                 environmentSamplerDesc},
                {SHADER_TYPE_RAY_MISS, "g_RTEnvironmentSampler",
                 environmentSamplerDesc},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTBrdfSampler",
                 environmentSamplerDesc},
                {SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTPbrSampler",
                 pbrTextureSamplerDesc},
                {SHADER_TYPE_RAY_ANY_HIT, "g_RTPbrSampler",
                 pbrTextureSamplerDesc}};
            psoCI.PSODesc.ResourceLayout.Variables    = Vars;
            psoCI.PSODesc.ResourceLayout.NumVariables = 25;
            psoCI.PSODesc.ResourceLayout.ImmutableSamplers = environmentSamplers;
            psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 5;

            pDevice_->CreateRayTracingPipelineState(psoCI, &pRayTracingPSO_);
            if (pRayTracingPSO_) {
                bindWarmupOutput(pRayTracingPSO_, pTraceOutputUAV_);
                bindWarmupTLAS(pRayTracingPSO_, pTLAS_);
                if (auto* pVar = pRayTracingPSO_->GetStaticVariableByName(
                        SHADER_TYPE_RAY_CLOSEST_HIT, "g_TLAS")) {
                    pVar->Set(pTLAS_);
                }
                rebuildResourceBinding();
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
                caps_.sbtCreated = true;
            }
        }

        if (pSBT_ && caps_.tlasBuilt && !sbtHitGroupBound_) {
            const TLASBuildInfo buildInfo = pTLAS_->GetBuildInfo();
            if (buildInfo.HitGroupStride > 0) {
                pSBT_->BindHitGroupForTLAS(pTLAS_, 0, kWarmupHitName);
                sbtHitGroupBound_ = true;
            }
        }
        if (pSBT_ && pContext) {
            pContext->UpdateSBT(pSBT_);
            caps_.sbtBound = true;
        }

        return caps_.pipelineCreated && caps_.sbtCreated && caps_.outputResourcesBound;
    }

    bool traceUnitQuad(IDeviceContext* pContext, Uint32 width, Uint32 height) override {
        return traceCamera(pContext, width, height, RTRayCamera{});
    }

    bool traceCamera(IDeviceContext* pContext, Uint32 width, Uint32 height,
                     const RTRayCamera& camera) override {
        if (!rtSupported_ || !pDevice_ || !caps_.tlasBuilt) {
            return false;
        }
        const auto finiteVector = [](const float* v) {
            return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
        };
        if (!finiteVector(camera.origin) || !finiteVector(camera.target) ||
            !finiteVector(camera.up) || !std::isfinite(camera.verticalFovDegrees)) {
            return false;
        }
        const float forward[3] = {camera.target[0] - camera.origin[0],
                                  camera.target[1] - camera.origin[1],
                                  camera.target[2] - camera.origin[2]};
        const float right[3] = {camera.up[1] * forward[2] - camera.up[2] * forward[1],
                                camera.up[2] * forward[0] - camera.up[0] * forward[2],
                                camera.up[0] * forward[1] - camera.up[1] * forward[0]};
        const float forwardLengthSquared = forward[0] * forward[0] +
                                           forward[1] * forward[1] +
                                           forward[2] * forward[2];
        const float rightLengthSquared = right[0] * right[0] + right[1] * right[1] +
                                         right[2] * right[2];
        if (forwardLengthSquared < 1.0e-8f || rightLengthSquared < 1.0e-8f) return false;

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
            rebuildResourceBinding();
        }

        if (!pRayTracingPSO_ || !pRayTracingSRB_ || !pSBT_ || !sbtHitGroupBound_ || !pContext) {
            return false;
        }

        if (!updateCameraBuffer(pContext, camera)) return false;
        pContext->SetPipelineState(pRayTracingPSO_);
        pContext->CommitShaderResources(pRayTracingSRB_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
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
    struct RTPrimitive { Uint32 i0; Uint32 i1; Uint32 i2; Uint32 materialIndex; };
    struct RTMaterial {
        float baseColor[4];
        float surface[4];
        float specularTint[4];
        float thinFilm[4];
        float extras[4];
        float emission[4];
        float sheen[4];
        float occlusion[4];
        float occlusionUv[4];
        float baseColorTexture[4];
        float surfaceTexture[4];
        float normalTexture[4];
        float emissionTexture[4];
        float opacityTexture[4];
        float alpha[4];
        float anisotropy[4];
        float subsurface[4];
        float subsurfaceRadius[4];
        float subsurfaceScattering[4];
        float clearcoatIor[4];
        float clearcoatNormal[4];
        float normalUvOffset[4];
        float clearcoatNormalUvOffset[4];
        float occlusionTextureUvOffset[4];
        float baseColorTextureUvRotation[4];
        float emissionTextureUvRotation[4];
        float surfaceTextureUvRotation[4];
        float opacityTextureUvRotation[4];
        float metallicTexture[4]{-1.0f, 1.0f, 1.0f, 0.0f};
        float roughnessTexture[4]{-1.0f, 1.0f, 1.0f, 0.0f};
        float metallicTextureUvOffset[4]{};
        float roughnessTextureUvOffset[4]{};
        float transmissionMode[4]{};
        float clearcoatTexture[4]{-1.0f, 1.0f, 0.0f, 0.0f};
        float clearcoatTextureUvOffset[4]{};
        float clearcoatRoughnessTexture[4]{-1.0f, 1.0f, 0.0f, 0.0f};
        float clearcoatRoughnessTextureUvOffset[4]{};
        float sheenColorTexture[4]{-1.0f, 1.0f, 1.0f, 0.0f};
        float sheenColorTextureUvOffset[4]{};
        float sheenRoughnessTexture[4]{-1.0f, 1.0f, 1.0f, 0.0f};
        float sheenRoughnessTextureUvOffset[4]{};
        float anisotropyTexture[4]{-1.0f, 1.0f, 1.0f, 0.0f};
        float anisotropyTextureUvOffset[4]{};
    };
    static_assert(sizeof(RTMaterial) == 43u * 4u * sizeof(float));
    struct RTPbrTexture {
        RefCntAutoPtr<ITexture> texture;
        RefCntAutoPtr<ITextureView> view;
    };
    enum class PbrTextureKind : Uint8 {
        BaseColor, MetallicRoughness, Metallic, Roughness, Normal,
        Clearcoat, ClearcoatNormal, Occlusion, Emission, Opacity,
        ClearcoatRoughness, SheenColor, SheenRoughness, Anisotropy
    };
    struct BLASNode {
        RefCntAutoPtr<IBottomLevelAS> pBLAS;
        Diligent::float4x4 transform;
        RTGeometryData data;
        bool dirty = false;
        bool built = false;
        bool requiresRebuild = false;
        bool active = true;
        bool alphaTestEnabled = false;
        std::vector<float> shadingVertices;
        std::vector<float> shadingNormals;
        std::vector<float> shadingUvs;
        std::vector<float> shadingUvs1;
        std::vector<float> shadingColors;
        std::vector<Uint32> shadingIndices;
        std::vector<Uint32> shadingMaterialIndices;
        Uint32 shadingVertexOffset = 0;
        Uint32 shadingPrimitiveOffset = 0;
        Uint32 shadingInstanceIndex = 0;
        Uint32 shadingMaterialOffset = 0;
        std::vector<RTMaterial> pbrMaterials{
            RTMaterial{{0.96f, 0.69f, 0.16f, 1.0f},
                       {0.0f, 0.5f, 0.5f, 1.5f},
                       {0.0f, 0.0f, 0.03f, 0.0f},
                       {0.0f, 0.0f, 0.0f, 0.0f},
                       {0.5f, 1.0f, 1.0f, 1.0f},
                       {-1.0f, 1.0f, 0.0f, 0.0f},
                       {1.0f, 1.0f, 0.0f, 0.0f},
                       {-1.0f, 0.0f, 0.0f, 0.0f},
                       {-1.0f, 0.0f, 0.0f, 0.0f},
                       {-1.0f, 1.0f, 0.0f, 0.0f},
                       {-1.0f, 0.0f, 0.0f, 0.0f},
                       {-1.0f, 0.0f, 0.0f, 0.0f},
                       {1.0f, 0.5f, 0.0f, 0.0f},
                       {0.0f, -1.0f, 0.0f, 0.0f}}};
        std::vector<QString> baseColorTexturePaths{QString()};
        std::vector<QString> metallicRoughnessTexturePaths{QString()};
        std::vector<QString> metallicTexturePaths{QString()};
        std::vector<QString> roughnessTexturePaths{QString()};
        std::vector<QString> normalTexturePaths{QString()};
        std::vector<QString> clearcoatTexturePaths{QString()};
        std::vector<QString> clearcoatRoughnessTexturePaths{QString()};
        std::vector<QString> clearcoatNormalTexturePaths{QString()};
        std::vector<QString> occlusionTexturePaths{QString()};
        std::vector<QString> emissionTexturePaths{QString()};
        std::vector<QString> opacityTexturePaths{QString()};
        std::vector<QString> sheenColorTexturePaths{QString()};
        std::vector<QString> sheenRoughnessTexturePaths{QString()};
        std::vector<QString> anisotropyTexturePaths{QString()};
        bool materialDirty = false;
    };

    bool loadOcclusionTexture(const QString& path,
                              RTPbrTexture& output) {
        if (!pDevice_ || path.isEmpty()) return false;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        const QByteArray encoded = file.readAll();
        if (encoded.isEmpty()) return false;
        const std::vector<uchar> encodedBytes(encoded.begin(), encoded.end());
        const cv::Mat image = cv::imdecode(encodedBytes, cv::IMREAD_UNCHANGED);
        if (image.empty() || image.cols <= 0 || image.rows <= 0 ||
            image.channels() <= 0) return false;
        const size_t pixelCount = static_cast<size_t>(image.cols) *
                                  static_cast<size_t>(image.rows);

        QVector<QVector<quint8>> mipData;
        mipData.push_back(QVector<quint8>(static_cast<qsizetype>(pixelCount)));
        for (int y = 0; y < image.rows; ++y) {
            const auto* row = image.ptr<uchar>(y);
            for (int x = 0; x < image.cols; ++x) {
                const auto* sample = row +
                    static_cast<size_t>(x) * image.elemSize();
                quint8 encodedValue = 255;
                if (image.depth() == CV_8U) {
                    encodedValue = sample[0];
                } else if (image.depth() == CV_16U) {
                    quint16 value = 0;
                    std::memcpy(&value, sample, sizeof(value));
                    encodedValue = static_cast<quint8>(value / 257u);
                } else if (image.depth() == CV_32F) {
                    float value = 1.0f;
                    std::memcpy(&value, sample, sizeof(value));
                    encodedValue = static_cast<quint8>(std::lround(
                        std::clamp(std::isfinite(value) ? value : 1.0f,
                                   0.0f, 1.0f) * 255.0f));
                } else {
                    return false;
                }
                mipData.front()[static_cast<qsizetype>(
                    static_cast<size_t>(y) * static_cast<size_t>(image.cols) +
                    static_cast<size_t>(x))] = encodedValue;
            }
        }

        int mipWidth = image.cols;
        int mipHeight = image.rows;
        while (mipWidth > 1 || mipHeight > 1) {
            const int nextWidth = std::max(1, mipWidth / 2);
            const int nextHeight = std::max(1, mipHeight / 2);
            const auto& previous = mipData.back();
            QVector<quint8> next(nextWidth * nextHeight);
            for (int y = 0; y < nextHeight; ++y) {
                for (int x = 0; x < nextWidth; ++x) {
                    Uint32 sum = 0;
                    Uint32 samples = 0;
                    for (int dy = 0; dy < 2; ++dy) {
                        for (int dx = 0; dx < 2; ++dx) {
                            const int sourceX = std::min(x * 2 + dx, mipWidth - 1);
                            const int sourceY = std::min(y * 2 + dy, mipHeight - 1);
                            sum += previous[sourceY * mipWidth + sourceX];
                            ++samples;
                        }
                    }
                    next[y * nextWidth + x] =
                        static_cast<quint8>((sum + samples / 2u) / samples);
                }
            }
            mipData.push_back(std::move(next));
            mipWidth = nextWidth;
            mipHeight = nextHeight;
        }

        TextureDesc desc;
        desc.Name = "RayTracing Occlusion Texture";
        desc.Type = RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Uint32>(image.cols);
        desc.Height = static_cast<Uint32>(image.rows);
        desc.MipLevels = static_cast<Uint32>(mipData.size());
        desc.Format = TEX_FORMAT_R8_UNORM;
        desc.Usage = USAGE_IMMUTABLE;
        desc.BindFlags = BIND_SHADER_RESOURCE;
        QVector<TextureSubResData> subresources(mipData.size());
        mipWidth = image.cols;
        for (int mip = 0; mip < mipData.size(); ++mip) {
            subresources[mip].pData = mipData[mip].constData();
            subresources[mip].Stride = static_cast<Uint64>(mipWidth);
            mipWidth = std::max(1, mipWidth / 2);
        }
        TextureData initialData;
        initialData.pSubResources = subresources.data();
        initialData.NumSubresources = static_cast<Uint32>(subresources.size());
        pDevice_->CreateTexture(desc, &initialData, &output.texture);
        if (!output.texture) return false;
        output.view = output.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        return output.view != nullptr;
    }

    QString pbrTextureCacheKey(const QString& path, PbrTextureKind kind) const {
        const QChar suffix = kind == PbrTextureKind::BaseColor ? QLatin1Char('c')
            : kind == PbrTextureKind::MetallicRoughness ? QLatin1Char('m')
            : kind == PbrTextureKind::Metallic ||
              kind == PbrTextureKind::Roughness ? QLatin1Char('d')
            : kind == PbrTextureKind::Normal ? QLatin1Char('n')
            : kind == PbrTextureKind::ClearcoatNormal ? QLatin1Char('q')
            : kind == PbrTextureKind::ClearcoatRoughness ? QLatin1Char('k')
            : kind == PbrTextureKind::Occlusion ? QLatin1Char('o')
            : kind == PbrTextureKind::Emission ? QLatin1Char('e')
            : kind == PbrTextureKind::SheenColor ? QLatin1Char('h')
            : kind == PbrTextureKind::SheenRoughness ? QLatin1Char('j')
            : kind == PbrTextureKind::Anisotropy ? QLatin1Char('l')
                                               : QLatin1Char('a');
        return path + QLatin1Char('\x1f') + suffix;
    }

    bool loadRgbaPbrTexture(const QString& path, PbrTextureKind kind,
                            RTPbrTexture& output) {
        if (!pDevice_ || path.isEmpty() || kind == PbrTextureKind::Occlusion)
            return false;
        bool floatLinearSrgbBaseColor = false;
        if (kind == PbrTextureKind::BaseColor) {
            ImageImporter metadataImporter;
            if (metadataImporter.open(path)) {
                const RawImage metadata = metadataImporter.readImage();
                if (metadata.isLogEncoded) return false;
                if (metadata.pixelType == QStringLiteral("float")) {
                    const QString colorSpace =
                        metadata.colorSpace.trimmed().toLower();
                    const QString transfer =
                        metadata.transferFunction.trimmed().toLower();
                    const QString description =
                        colorSpace + QLatin1Char(' ') + transfer;
                    const bool linearTransfer =
                        ((transfer.contains(QStringLiteral("linear")) &&
                          !transfer.contains(QStringLiteral("nonlinear"))) ||
                         (colorSpace.contains(QStringLiteral("linear")) &&
                          !colorSpace.contains(QStringLiteral("nonlinear"))));
                    const bool srgbTransfer = !linearTransfer &&
                        (transfer.contains(QStringLiteral("srgb")) ||
                         colorSpace.contains(QStringLiteral("srgb")));
                    const bool unsupportedPrimaries =
                        description.contains(QStringLiteral("aces")) ||
                        description.contains(QStringLiteral("2020")) ||
                        description.contains(QStringLiteral("p3")) ||
                        description.contains(QStringLiteral("xyz")) ||
                        description.contains(QStringLiteral("adobe")) ||
                        description.contains(QStringLiteral("prophoto")) ||
                        description.contains(QStringLiteral("wide gamut"));
                    floatLinearSrgbBaseColor = !srgbTransfer &&
                        !unsupportedPrimaries &&
                        (description.trimmed().isEmpty() || linearTransfer);
                }
            }
        }
        const auto encodeLinearSrgb = [](float value) {
            value = std::isfinite(value)
                ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
            return value <= 0.0031308f ? value * 12.92f
                : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
        };
        if (kind == PbrTextureKind::Emission) {
            ImageImporter importer;
            if (importer.open(path)) {
                const RawImage image = importer.readImage();
                const qsizetype pixelCount = static_cast<qsizetype>(image.width) * image.height;
                const int pixelSize = image.getPixelTypeSizeInBytes();
                const qsizetype requiredBytes = pixelCount * image.channels * pixelSize;
                if (image.isValid() && image.isLogEncoded) return false;
                if (image.isValid() && pixelCount > 0 && image.channels > 0 &&
                    pixelSize > 0 && image.data.size() >= requiredBytes &&
                    (image.pixelType == QStringLiteral("uint8") ||
                     image.pixelType == QStringLiteral("uint16") ||
                     image.pixelType == QStringLiteral("float"))) {
                    const bool floatSource = image.pixelType == QStringLiteral("float");
                    const bool floatSrgb = floatSource &&
                        (image.colorSpace.contains(QStringLiteral("srgb"), Qt::CaseInsensitive) ||
                         image.transferFunction.contains(QStringLiteral("srgb"), Qt::CaseInsensitive));
                    const auto srgbToLinear = [](float value) {
                        value = std::clamp(value, 0.0f, 1.0f);
                        return value <= 0.04045f ? value / 12.92f
                            : std::pow((value + 0.055f) / 1.055f, 2.4f);
                    };
                    const Uint8* bytes = image.data.constData();
                    const qsizetype sourceStride = static_cast<qsizetype>(image.channels) * pixelSize;
                    QVector<QVector<float>> mips;
                    QVector<float> base(static_cast<qsizetype>(pixelCount) * 4, 1.0f);
                    for (qsizetype pixel = 0; pixel < pixelCount; ++pixel) {
                        for (int channel = 0; channel < 4; ++channel) {
                            float value = channel == 3 ? 1.0f : 0.0f;
                            if (channel < image.channels) {
                                const Uint8* src = bytes + pixel * sourceStride + channel * pixelSize;
                                if (floatSource) {
                                    std::memcpy(&value, src, sizeof(value));
                                    value = std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
                                } else if (image.pixelType == QStringLiteral("uint16")) {
                                    Uint16 sample = 0;
                                    std::memcpy(&sample, src, sizeof(sample));
                                    value = sample / 65535.0f;
                                } else {
                                    value = *src / 255.0f;
                                }
                                if ((!floatSource || floatSrgb) && channel < 3)
                                    value = srgbToLinear(value);
                            } else if (channel < 3 && image.channels == 1) {
                                    value = base[pixel * 4];
                            }
                            base[pixel * 4 + channel] = value;
                        }
                    }
                    mips.push_back(std::move(base));
                    int mipWidth = image.width;
                    int mipHeight = image.height;
                    while (mipWidth > 1 || mipHeight > 1) {
                        const int nextWidth = std::max(1, mipWidth / 2);
                        const int nextHeight = std::max(1, mipHeight / 2);
                        const auto& previous = mips.back();
                        QVector<float> next(static_cast<qsizetype>(nextWidth) * nextHeight * 4, 0.0f);
                        for (int y = 0; y < nextHeight; ++y) for (int x = 0; x < nextWidth; ++x) {
                            float sums[4]{};
                            int samples = 0;
                            for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
                                const int sx = std::min(x * 2 + dx, mipWidth - 1);
                                const int sy = std::min(y * 2 + dy, mipHeight - 1);
                                const qsizetype src = (static_cast<qsizetype>(sy) * mipWidth + sx) * 4;
                                for (int c = 0; c < 4; ++c) sums[c] += previous[src + c];
                                ++samples;
                            }
                            const qsizetype dst = (static_cast<qsizetype>(y) * nextWidth + x) * 4;
                            for (int c = 0; c < 4; ++c) next[dst + c] = sums[c] / samples;
                        }
                        mips.push_back(std::move(next));
                        mipWidth = nextWidth;
                        mipHeight = nextHeight;
                    }
                    TextureDesc desc;
                    desc.Name = "RayTracing Emission Texture";
                    desc.Type = RESOURCE_DIM_TEX_2D;
                    desc.Width = static_cast<Uint32>(image.width);
                    desc.Height = static_cast<Uint32>(image.height);
                    desc.MipLevels = static_cast<Uint32>(mips.size());
                    desc.Format = TEX_FORMAT_RGBA32_FLOAT;
                    desc.Usage = USAGE_IMMUTABLE;
                    desc.BindFlags = BIND_SHADER_RESOURCE;
                    QVector<TextureSubResData> subresources(mips.size());
                    mipWidth = image.width;
                    for (int mip = 0; mip < mips.size(); ++mip) {
                        subresources[mip].pData = mips[mip].constData();
                        subresources[mip].Stride = static_cast<Uint64>(mipWidth) * 4u * sizeof(float);
                        mipWidth = std::max(1, mipWidth / 2);
                    }
                    TextureData initialData;
                    initialData.pSubResources = subresources.data();
                    initialData.NumSubresources = static_cast<Uint32>(subresources.size());
                    pDevice_->CreateTexture(desc, &initialData, &output.texture);
                    if (!output.texture) return false;
                    output.view = output.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                    return output.view != nullptr;
                }
            }
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        const QByteArray encoded = file.readAll();
        if (encoded.isEmpty()) return false;
        const std::vector<uchar> encodedBytes(encoded.begin(), encoded.end());
        const cv::Mat source = cv::imdecode(encodedBytes, cv::IMREAD_UNCHANGED);
        if (source.empty() || source.cols <= 0 || source.rows <= 0 ||
            source.channels() <= 0 || source.channels() > 4) return false;
        cv::Mat rgba;
        if (source.channels() == 1) {
            cv::cvtColor(source, rgba, cv::COLOR_GRAY2RGBA);
        } else if (source.channels() == 3) {
            cv::cvtColor(source, rgba, cv::COLOR_BGR2RGBA);
        } else {
            cv::cvtColor(source, rgba, cv::COLOR_BGRA2RGBA);
        }
        QVector<quint8> base(static_cast<qsizetype>(source.cols) *
                             source.rows * 4);
        for (int y = 0; y < rgba.rows; ++y) {
            const auto* row = rgba.ptr<uchar>(y);
            for (int x = 0; x < rgba.cols; ++x) {
                const auto* pixel = row + static_cast<size_t>(x) * rgba.elemSize();
                for (int channel = 0; channel < 4; ++channel) {
                    quint8 value = 255;
                    if (rgba.depth() == CV_8U) {
                        value = pixel[channel];
                    } else if (rgba.depth() == CV_16U) {
                        quint16 wide = 0;
                        std::memcpy(&wide, pixel + channel * sizeof(wide), sizeof(wide));
                        value = static_cast<quint8>(wide / 257u);
                    } else if (rgba.depth() == CV_32F) {
                        float wide = 1.0f;
                        std::memcpy(&wide, pixel + channel * sizeof(wide), sizeof(wide));
                        if (kind == PbrTextureKind::BaseColor &&
                            floatLinearSrgbBaseColor && channel < 3) {
                            wide = encodeLinearSrgb(wide);
                        }
                        value = static_cast<quint8>(std::lround(
                            std::clamp(std::isfinite(wide) ? wide : 1.0f,
                                       0.0f, 1.0f) * 255.0f));
                    } else {
                        return false;
                    }
                    base[(static_cast<qsizetype>(y) * rgba.cols + x) * 4 +
                         channel] = value;
                }
            }
        }
        if (kind == PbrTextureKind::Opacity) {
            for (qsizetype pixel = 0; pixel < base.size() / 4; ++pixel) {
                const qsizetype offset = pixel * 4;
                base[offset + 3] = static_cast<quint8>(
                    (static_cast<Uint32>(base[offset]) +
                     base[offset + 1] + base[offset + 2]) / 3u);
            }
        }

        const bool srgb = kind == PbrTextureKind::BaseColor ||
                          kind == PbrTextureKind::Emission ||
                          kind == PbrTextureKind::SheenColor;
        const bool renormalizeNormal = kind == PbrTextureKind::Normal ||
                                       kind == PbrTextureKind::ClearcoatNormal;
        const auto decodeSrgb = [](float value) {
            return value <= 0.04045f ? value / 12.92f
                : std::pow((value + 0.055f) / 1.055f, 2.4f);
        };
        const auto encodeSrgb = [](float value) {
            return value <= 0.0031308f ? value * 12.92f
                : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
        };
        const bool alphaWeightedBaseColor =
            kind == PbrTextureKind::BaseColor;
        QVector<QVector<quint8>> mipData{std::move(base)};
        int mipWidth = rgba.cols;
        int mipHeight = rgba.rows;
        if (renormalizeNormal) {
            auto& baseLevel = mipData[0];
            for (int pixel = 0; pixel < mipWidth * mipHeight; ++pixel) {
                const int offset = pixel * 4;
                const float nx = baseLevel[offset + 0] / 127.5f - 1.0f;
                const float ny = baseLevel[offset + 1] / 127.5f - 1.0f;
                const float nz = baseLevel[offset + 2] / 127.5f - 1.0f;
                const float normalLength = std::clamp(
                    std::sqrt(nx * nx + ny * ny + nz * nz), 0.0f, 1.0f);
                baseLevel[offset + 3] = static_cast<quint8>(
                    std::lround(normalLength * 255.0f));
            }
        }
        while (mipWidth > 1 || mipHeight > 1) {
            const int nextWidth = std::max(1, mipWidth / 2);
            const int nextHeight = std::max(1, mipHeight / 2);
            const auto& previous = mipData.back();
            QVector<quint8> next(nextWidth * nextHeight * 4);
            for (int y = 0; y < nextHeight; ++y) {
                for (int x = 0; x < nextWidth; ++x) {
                    float sums[4]{};
                    float totalWeight = 0.0f;
                    const float sourceX0 =
                        static_cast<float>(x) * mipWidth / nextWidth;
                    const float sourceX1 =
                        static_cast<float>(x + 1) * mipWidth / nextWidth;
                    const float sourceY0 =
                        static_cast<float>(y) * mipHeight / nextHeight;
                    const float sourceY1 =
                        static_cast<float>(y + 1) * mipHeight / nextHeight;
                    for (int sy = static_cast<int>(std::floor(sourceY0));
                         sy < static_cast<int>(std::ceil(sourceY1)); ++sy) {
                        const float weightY = std::max(0.0f,
                            std::min(sourceY1, static_cast<float>(sy + 1)) -
                            std::max(sourceY0, static_cast<float>(sy)));
                        for (int sx = static_cast<int>(std::floor(sourceX0));
                             sx < static_cast<int>(std::ceil(sourceX1)); ++sx) {
                            const float weightX = std::max(0.0f,
                                std::min(sourceX1, static_cast<float>(sx + 1)) -
                                std::max(sourceX0, static_cast<float>(sx)));
                            const float weight = weightX * weightY;
                            const int sourceOffset = (sy * mipWidth + sx) * 4;
                            const float alpha =
                                previous[sourceOffset + 3] / 255.0f;
                            if (renormalizeNormal) {
                                const float previousLength =
                                    previous[sourceOffset + 3] / 255.0f;
                                for (int channel = 0; channel < 3; ++channel) {
                                    const float component =
                                        previous[sourceOffset + channel] / 127.5f - 1.0f;
                                    sums[channel] +=
                                        (component * previousLength + 1.0f) *
                                        (0.5f * weight);
                                }
                            } else {
                                for (int channel = 0; channel < 3; ++channel) {
                                    const float value =
                                        previous[sourceOffset + channel] / 255.0f;
                                    sums[channel] +=
                                        (srgb ? decodeSrgb(value) : value) *
                                        (alphaWeightedBaseColor ? alpha : 1.0f) *
                                        weight;
                                }
                            }
                            sums[3] += alpha * weight;
                            totalWeight += weight;
                        }
                    }
                    const int destinationOffset = (y * nextWidth + x) * 4;
                    if (renormalizeNormal) {
                        float nx = sums[0] / totalWeight * 2.0f - 1.0f;
                        float ny = sums[1] / totalWeight * 2.0f - 1.0f;
                        float nz = sums[2] / totalWeight * 2.0f - 1.0f;
                        const float length = std::sqrt(std::max(
                            nx * nx + ny * ny + nz * nz, 1.0e-8f));
                        next[destinationOffset + 3] = static_cast<quint8>(
                            std::lround(std::clamp(length, 0.0f, 1.0f) * 255.0f));
                        next[destinationOffset] = static_cast<quint8>(
                            std::clamp((nx / length + 1.0f) * 127.5f, 0.0f, 255.0f));
                        next[destinationOffset + 1] = static_cast<quint8>(
                            std::clamp((ny / length + 1.0f) * 127.5f, 0.0f, 255.0f));
                        next[destinationOffset + 2] = static_cast<quint8>(
                            std::clamp((nz / length + 1.0f) * 127.5f, 0.0f, 255.0f));
                    } else {
                        for (int channel = 0; channel < 3; ++channel) {
                            const float average = alphaWeightedBaseColor
                                ? (sums[3] > 1.0e-8f
                                    ? sums[channel] / sums[3] : 0.0f)
                                    : sums[channel] / totalWeight;
                            const float encodedValue = srgb ? encodeSrgb(average) : average;
                            next[destinationOffset + channel] = static_cast<quint8>(
                                std::lround(std::clamp(encodedValue, 0.0f, 1.0f) * 255.0f));
                        }
                    }
                    if (!renormalizeNormal) {
                        next[destinationOffset + 3] = static_cast<quint8>(
                            std::lround(std::clamp(
                                sums[3] / totalWeight, 0.0f, 1.0f) * 255.0f));
                    }
                }
            }
            mipData.push_back(std::move(next));
            mipWidth = nextWidth;
            mipHeight = nextHeight;
        }

        TextureDesc desc;
        desc.Name = kind == PbrTextureKind::BaseColor
            ? "RayTracing Base Color Texture"
            : kind == PbrTextureKind::MetallicRoughness
                ? "RayTracing Metallic Roughness Texture"
                : kind == PbrTextureKind::Metallic ||
                  kind == PbrTextureKind::Roughness ||
                  kind == PbrTextureKind::ClearcoatRoughness
                    ? "RayTracing Metallic or Roughness Texture"
                : kind == PbrTextureKind::Emission
                    ? "RayTracing Emission Texture"
                    : kind == PbrTextureKind::SheenRoughness
                        ? "RayTracing Sheen Roughness Texture"
                    : kind == PbrTextureKind::Anisotropy
                        ? "RayTracing Anisotropy Texture"
                    : kind == PbrTextureKind::Opacity
                        ? "RayTracing Opacity Texture"
                        : "RayTracing Normal Texture";
        desc.Type = RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Uint32>(rgba.cols);
        desc.Height = static_cast<Uint32>(rgba.rows);
        desc.MipLevels = static_cast<Uint32>(mipData.size());
        desc.Format = srgb ? TEX_FORMAT_RGBA8_UNORM_SRGB : TEX_FORMAT_RGBA8_UNORM;
        desc.Usage = USAGE_IMMUTABLE;
        desc.BindFlags = BIND_SHADER_RESOURCE;
        QVector<TextureSubResData> subresources(mipData.size());
        mipWidth = rgba.cols;
        for (int mip = 0; mip < mipData.size(); ++mip) {
            subresources[mip].pData = mipData[mip].constData();
            subresources[mip].Stride = static_cast<Uint64>(mipWidth) * 4u;
            mipWidth = std::max(1, mipWidth / 2);
        }
        TextureData initialData;
        initialData.pSubResources = subresources.data();
        initialData.NumSubresources = static_cast<Uint32>(subresources.size());
        pDevice_->CreateTexture(desc, &initialData, &output.texture);
        if (!output.texture) return false;
        output.view = output.texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        return output.view != nullptr;
    }

    bool createFallbackOcclusionTexture() {
        if (!pDevice_) return false;
        const Uint8 white = 255;
        TextureDesc desc;
        desc.Name = "RayTracing White Occlusion Fallback";
        desc.Type = RESOURCE_DIM_TEX_2D;
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.Format = TEX_FORMAT_R8_UNORM;
        desc.Usage = USAGE_IMMUTABLE;
        desc.BindFlags = BIND_SHADER_RESOURCE;
        TextureSubResData subresource{};
        subresource.pData = &white;
        subresource.Stride = 1;
        TextureData initialData{};
        initialData.pSubResources = &subresource;
        initialData.NumSubresources = 1;
        pDevice_->CreateTexture(desc, &initialData,
                                &pFallbackOcclusionTexture_);
        if (!pFallbackOcclusionTexture_) return false;
        pFallbackOcclusionSRV_ = pFallbackOcclusionTexture_->GetDefaultView(
            TEXTURE_VIEW_SHADER_RESOURCE);
        return pFallbackOcclusionSRV_ != nullptr;
    }

    bool updateCameraBuffer(IDeviceContext* pContext, const RTRayCamera& camera) {
        if (!pContext || !pRayCameraBuffer_) return false;
        RTRayCameraConstants constants{};
        std::copy(std::begin(camera.origin), std::end(camera.origin), constants.origin);
        std::copy(std::begin(camera.target), std::end(camera.target), constants.target);
        std::copy(std::begin(camera.up), std::end(camera.up), constants.up);
        constants.params[0] = std::clamp(camera.verticalFovDegrees, 1.0f, 179.0f);
        constants.params[1] = constants.params[2] = constants.params[3] = 0.0f;
        void* mapped = nullptr;
        pContext->MapBuffer(pRayCameraBuffer_, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
        if (!mapped) return false;
        std::memcpy(mapped, &constants, sizeof(constants));
        pContext->UnmapBuffer(pRayCameraBuffer_, MAP_WRITE);
        return true;
    }

    bool createCameraBuffer() {
        BufferDesc desc;
        desc.Name = "RayTracing Camera Constants";
        desc.Size = sizeof(RTRayCameraConstants);
        desc.Usage = USAGE_DYNAMIC;
        desc.BindFlags = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        pDevice_->CreateBuffer(desc, nullptr, &pRayCameraBuffer_);
        return pRayCameraBuffer_ != nullptr;
    }

    bool createFallbackEnvironment() {
        const float neutralFace[4] = {0.18f, 0.20f, 0.24f, 1.0f};
        TextureDesc desc;
        desc.Name = "RayTracing/NeutralEnvironment";
        desc.Type = RESOURCE_DIM_TEX_CUBE;
        desc.Width = 1;
        desc.Height = 1;
        desc.ArraySize = 6;
        desc.MipLevels = 1;
        desc.Format = TEX_FORMAT_RGBA32_FLOAT;
        desc.Usage = USAGE_IMMUTABLE;
        desc.BindFlags = BIND_SHADER_RESOURCE;
        TextureSubResData faces[6] = {};
        for (auto& face : faces) {
            face.pData = neutralFace;
            face.Stride = sizeof(neutralFace);
        }
        TextureData data;
        data.pSubResources = faces;
        data.NumSubresources = 6;
        pDevice_->CreateTexture(desc, &data, &pFallbackEnvironmentTexture_);
        if (!pFallbackEnvironmentTexture_) return false;
        pFallbackEnvironmentSRV_ = pFallbackEnvironmentTexture_->GetDefaultView(
            TEXTURE_VIEW_SHADER_RESOURCE);
        pEnvironmentSRV_ = pFallbackEnvironmentSRV_;
        pSheenEnvironmentSRV_ = pFallbackEnvironmentSRV_;
        pIrradianceEnvironmentSRV_ = pFallbackEnvironmentSRV_;

        const float neutralBrdf[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        TextureDesc brdfDesc;
        brdfDesc.Name = "RayTracing/NeutralBRDFLUT";
        brdfDesc.Type = RESOURCE_DIM_TEX_2D;
        brdfDesc.Width = 1;
        brdfDesc.Height = 1;
        brdfDesc.MipLevels = 1;
        brdfDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
        brdfDesc.Usage = USAGE_IMMUTABLE;
        brdfDesc.BindFlags = BIND_SHADER_RESOURCE;
        TextureSubResData brdfSubresource{};
        brdfSubresource.pData = neutralBrdf;
        brdfSubresource.Stride = sizeof(neutralBrdf);
        TextureData brdfData;
        brdfData.pSubResources = &brdfSubresource;
        brdfData.NumSubresources = 1;
        pDevice_->CreateTexture(brdfDesc, &brdfData, &pFallbackBrdfLutTexture_);
        if (!pFallbackBrdfLutTexture_) return false;
        pFallbackBrdfLutSRV_ = pFallbackBrdfLutTexture_->GetDefaultView(
            TEXTURE_VIEW_SHADER_RESOURCE);
        pBrdfLutSRV_ = pFallbackBrdfLutSRV_;
        return pEnvironmentSRV_ && pIrradianceEnvironmentSRV_ && pBrdfLutSRV_;
    }

    bool createEnvironmentConstantsBuffer() {
        BufferDesc desc;
        desc.Name = "RayTracing Environment Constants";
        desc.Size = sizeof(RTEnvironmentConstants);
        desc.Usage = USAGE_DYNAMIC;
        desc.BindFlags = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        pDevice_->CreateBuffer(desc, nullptr, &pEnvironmentConstantsBuffer_);
        return pEnvironmentConstantsBuffer_ != nullptr;
    }

    bool createSceneLightsConstantsBuffer() {
        BufferDesc desc;
        desc.Name = "RayTracing Scene Lights Constants";
        desc.Size = sizeof(RTSceneLightsConstants);
        desc.Usage = USAGE_DYNAMIC;
        desc.BindFlags = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_WRITE;
        pDevice_->CreateBuffer(desc, nullptr, &pSceneLightsConstantsBuffer_);
        if (!pSceneLightsConstantsBuffer_) return false;
        return updateSceneLightsBuffer(pImmediateContext_);
    }

    bool updateSceneLightsBuffer(IDeviceContext* pContext) {
        if (!pContext || !pSceneLightsConstantsBuffer_) return false;
        void* mapped = nullptr;
        pContext->MapBuffer(pSceneLightsConstantsBuffer_, MAP_WRITE,
                            MAP_FLAG_DISCARD, mapped);
        if (!mapped) return false;
        std::memcpy(mapped, &sceneLightsConstants_, sizeof(sceneLightsConstants_));
        pContext->UnmapBuffer(pSceneLightsConstantsBuffer_, MAP_WRITE);
        return true;
    }

    bool updateEnvironmentSettings(IDeviceContext* pContext) {
        if (!pContext || !pEnvironmentConstantsBuffer_) return false;
        RTEnvironmentConstants constants{};
        constants.settings[0] = environmentEnabled_ ? 1.0f : 0.0f;
        constants.settings[1] = environmentIntensity_;
        constants.settings[2] = environmentRotationRadians_;
        void* mapped = nullptr;
        pContext->MapBuffer(pEnvironmentConstantsBuffer_, MAP_WRITE,
                            MAP_FLAG_DISCARD, mapped);
        if (!mapped) return false;
        std::memcpy(mapped, &constants, sizeof(constants));
        pContext->UnmapBuffer(pEnvironmentConstantsBuffer_, MAP_WRITE);
        environmentSettingsUploaded_ = true;
        return true;
    }

    bool createStructuredBuffer(const char* name, const void* data, Uint64 size,
                                Uint32 stride, RefCntAutoPtr<IBuffer>& buffer) {
        BufferDesc desc;
        desc.Name = name;
        desc.Size = size;
        desc.Usage = USAGE_DEFAULT;
        desc.BindFlags = BIND_SHADER_RESOURCE | BIND_RAY_TRACING;
        desc.Mode = BUFFER_MODE_STRUCTURED;
        desc.ElementByteStride = stride;
        BufferData initialData{data, size, pImmediateContext_};
        pDevice_->CreateBuffer(desc, data ? &initialData : nullptr, &buffer);
        return buffer != nullptr;
    }

    bool rebuildShadingPools(IDeviceContext* pContext) {
        if (!shadingPoolsDirty_ && pRenderVertices_ && pRenderIndices_ &&
            pInstancePrimitiveOffsets_ && pRenderMaterials_) return true;
        if (!pContext) return false;
        pImmediateContext_ = pContext;
        std::vector<float> vertices;
        std::vector<float> normals;
        std::vector<float> uvs;
        std::vector<float> uvs1;
        std::vector<float> colors;
        std::vector<RTPrimitive> primitives;
        std::vector<RTMaterial> materials;
        std::vector<Uint32> offsets;
        rtPbrTextureViews_.clear();
        vertices.reserve(1024);
        normals.reserve(1024);
        uvs.reserve(512);
        uvs1.reserve(512);
        primitives.reserve(1024);
        offsets.reserve(blasMap_.size());
        for (const auto& [name, node] : blasMap_) {
            (void)name;
            if (node.pBLAS) {
                offsets.push_back(std::numeric_limits<Uint32>::max());
            }
        }
        Uint32 instanceIndex = 0;
        for (auto& [name, node] : blasMap_) {
            if (!node.pBLAS) continue;
            node.shadingVertexOffset = static_cast<Uint32>(vertices.size() / 4u);
            node.shadingPrimitiveOffset = static_cast<Uint32>(primitives.size());
            node.shadingInstanceIndex = instanceIndex++;
            node.shadingMaterialOffset = static_cast<Uint32>(materials.size());
            const auto resolveTextureIndex = [&](const QString& path,
                                                 PbrTextureKind kind) {
                if (path.isEmpty()) return -1.0f;
                const auto textureIt = pbrTextureCache_.find(
                    pbrTextureCacheKey(path, kind));
                if (textureIt == pbrTextureCache_.end() ||
                    !textureIt->second.view) return -1.0f;
                const auto viewIt = std::find(
                    rtPbrTextureViews_.begin(), rtPbrTextureViews_.end(),
                    textureIt->second.view.RawPtr());
                if (viewIt != rtPbrTextureViews_.end()) {
                    return static_cast<float>(std::distance(
                        rtPbrTextureViews_.begin(), viewIt));
                }
                const float textureIndex = static_cast<float>(
                    rtPbrTextureViews_.size());
                rtPbrTextureViews_.push_back(textureIt->second.view.RawPtr());
                return textureIndex;
            };
            for (size_t materialIndex = 0;
                 materialIndex < node.pbrMaterials.size(); ++materialIndex) {
                RTMaterial gpuMaterial = node.pbrMaterials[materialIndex];
                if (materialIndex < node.baseColorTexturePaths.size()) {
                    gpuMaterial.baseColorTexture[0] = resolveTextureIndex(
                        node.baseColorTexturePaths[materialIndex],
                        PbrTextureKind::BaseColor);
                }
                if (materialIndex < node.metallicRoughnessTexturePaths.size()) {
                    gpuMaterial.surfaceTexture[0] = resolveTextureIndex(
                        node.metallicRoughnessTexturePaths[materialIndex],
                        PbrTextureKind::MetallicRoughness);
                }
                if (materialIndex < node.metallicTexturePaths.size()) {
                    gpuMaterial.metallicTexture[0] = resolveTextureIndex(
                        node.metallicTexturePaths[materialIndex],
                        PbrTextureKind::Metallic);
                }
                if (materialIndex < node.roughnessTexturePaths.size()) {
                    gpuMaterial.roughnessTexture[0] = resolveTextureIndex(
                        node.roughnessTexturePaths[materialIndex],
                        PbrTextureKind::Roughness);
                }
                if (materialIndex < node.normalTexturePaths.size()) {
                    gpuMaterial.normalTexture[0] = resolveTextureIndex(
                        node.normalTexturePaths[materialIndex],
                        PbrTextureKind::Normal);
                }
                if (materialIndex < node.clearcoatTexturePaths.size()) {
                    gpuMaterial.clearcoatTexture[0] = resolveTextureIndex(
                        node.clearcoatTexturePaths[materialIndex],
                        PbrTextureKind::Clearcoat);
                }
                if (materialIndex < node.clearcoatRoughnessTexturePaths.size()) {
                    gpuMaterial.clearcoatRoughnessTexture[0] =
                        resolveTextureIndex(
                            node.clearcoatRoughnessTexturePaths[materialIndex],
                            PbrTextureKind::ClearcoatRoughness);
                }
                if (materialIndex < node.clearcoatNormalTexturePaths.size()) {
                    gpuMaterial.clearcoatNormal[1] = resolveTextureIndex(
                        node.clearcoatNormalTexturePaths[materialIndex],
                        PbrTextureKind::ClearcoatNormal);
                }
                if (materialIndex < node.occlusionTexturePaths.size()) {
                    gpuMaterial.occlusion[0] = resolveTextureIndex(
                        node.occlusionTexturePaths[materialIndex],
                        PbrTextureKind::Occlusion);
                }
                if (materialIndex < node.emissionTexturePaths.size()) {
                    gpuMaterial.emissionTexture[0] = resolveTextureIndex(
                        node.emissionTexturePaths[materialIndex],
                        PbrTextureKind::Emission);
                }
                if (materialIndex < node.opacityTexturePaths.size()) {
                    gpuMaterial.opacityTexture[0] = resolveTextureIndex(
                        node.opacityTexturePaths[materialIndex],
                        PbrTextureKind::Opacity);
                }
                if (materialIndex < node.sheenColorTexturePaths.size()) {
                    gpuMaterial.sheenColorTexture[0] = resolveTextureIndex(
                        node.sheenColorTexturePaths[materialIndex],
                        PbrTextureKind::SheenColor);
                }
                if (materialIndex < node.sheenRoughnessTexturePaths.size()) {
                    gpuMaterial.sheenRoughnessTexture[0] = resolveTextureIndex(
                        node.sheenRoughnessTexturePaths[materialIndex],
                        PbrTextureKind::SheenRoughness);
                }
                if (materialIndex < node.anisotropyTexturePaths.size()) {
                    gpuMaterial.anisotropyTexture[0] = resolveTextureIndex(
                        node.anisotropyTexturePaths[materialIndex],
                        PbrTextureKind::Anisotropy);
                }
                materials.push_back(gpuMaterial);
            }
            offsets.at(node.shadingInstanceIndex) = node.shadingVertices.empty() || node.shadingIndices.empty()
                ? std::numeric_limits<Uint32>::max() : node.shadingPrimitiveOffset;
            if (node.shadingVertices.empty() || node.shadingIndices.empty()) continue;
            for (size_t vertex = 0; vertex + 2 < node.shadingVertices.size(); vertex += 3) {
                vertices.push_back(node.shadingVertices[vertex]);
                vertices.push_back(node.shadingVertices[vertex + 1]);
                vertices.push_back(node.shadingVertices[vertex + 2]);
                vertices.push_back(0.0f);
                if (node.shadingNormals.size() == node.shadingVertices.size()) {
                    normals.push_back(node.shadingNormals[vertex]);
                    normals.push_back(node.shadingNormals[vertex + 1]);
                    normals.push_back(node.shadingNormals[vertex + 2]);
                } else {
                    normals.push_back(0.0f);
                    normals.push_back(0.0f);
                    normals.push_back(0.0f);
                }
                normals.push_back(0.0f);
                if (node.shadingUvs.size() == node.shadingVertices.size() / 3u * 2u) {
                    const size_t uvOffset = vertex / 3u * 2u;
                    uvs.push_back(node.shadingUvs[uvOffset]);
                    uvs.push_back(node.shadingUvs[uvOffset + 1u]);
                } else {
                    uvs.push_back(0.0f);
                    uvs.push_back(0.0f);
                }
                if (node.shadingUvs1.size() == node.shadingVertices.size() / 3u * 2u) {
                    const size_t uvOffset = vertex / 3u * 2u;
                    uvs1.push_back(node.shadingUvs1[uvOffset]);
                    uvs1.push_back(node.shadingUvs1[uvOffset + 1u]);
                } else {
                    const size_t uvOffset = vertex / 3u * 2u;
                    uvs1.push_back(uvs[uvOffset]);
                    uvs1.push_back(uvs[uvOffset + 1u]);
                }
                if (node.shadingColors.size() == node.shadingVertices.size() / 3u * 4u) {
                    const size_t colorOffset = vertex / 3u * 4u;
                    colors.insert(colors.end(), node.shadingColors.begin() + colorOffset,
                                  node.shadingColors.begin() + colorOffset + 4u);
                } else {
                    colors.insert(colors.end(), {1.0f, 1.0f, 1.0f, 1.0f});
                }
            }
            for (size_t i = 0; i + 2 < node.shadingIndices.size(); i += 3) {
                const size_t primitiveIndex = i / 3u;
                const Uint32 localMaterialIndex =
                    primitiveIndex < node.shadingMaterialIndices.size()
                        ? node.shadingMaterialIndices[primitiveIndex]
                        : 0u;
                primitives.push_back({node.shadingVertexOffset + node.shadingIndices[i],
                                      node.shadingVertexOffset + node.shadingIndices[i + 1],
                                      node.shadingVertexOffset + node.shadingIndices[i + 2],
                                      node.shadingMaterialOffset + static_cast<Uint32>(
                                          std::min<size_t>(localMaterialIndex,
                                              node.pbrMaterials.size() - 1u))});
            }
        }
        const float fallbackVertex[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        const float fallbackNormal[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        const float fallbackUv[2] = {0.0f, 0.0f};
        const float fallbackColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        const RTPrimitive fallbackPrimitive{0, 0, 0, 0};
        const RTMaterial fallbackMaterial{{0.96f, 0.69f, 0.16f, 1.0f},
                                          {0.0f, 0.5f, 0.5f, 1.5f},
                                          {0.0f, 0.0f, 0.03f, 0.0f},
                                          {0.0f, 0.0f, 0.0f, 0.0f},
                                          {0.5f, 1.0f, 1.0f, 1.0f},
                                          {-1.0f, 1.0f, 0.0f, 0.0f},
                                          {1.0f, 1.0f, 0.0f, 0.0f},
                                          {-1.0f, 0.0f, 0.0f, 0.0f},
                                          {-1.0f, 0.0f, 0.0f, 0.0f},
                                          {-1.0f, 1.0f, 0.0f, 0.0f},
                                          {-1.0f, 0.0f, 0.0f, 0.0f},
                                          {-1.0f, 0.0f, 0.0f, 0.0f},
                                          {1.0f, 0.5f, 0.0f, 0.0f},
                                          {0.0f, 0.0f, 0.0f, 0.0f}};
        const Uint32 fallbackOffset = 0;
        if (vertices.empty()) vertices.insert(vertices.end(), std::begin(fallbackVertex), std::end(fallbackVertex));
        if (normals.empty()) normals.insert(normals.end(), std::begin(fallbackNormal), std::end(fallbackNormal));
        if (uvs.empty()) uvs.insert(uvs.end(), std::begin(fallbackUv), std::end(fallbackUv));
        if (uvs1.empty()) uvs1.insert(uvs1.end(), uvs.begin(), uvs.end());
        if (colors.empty()) colors.insert(colors.end(), std::begin(fallbackColor), std::end(fallbackColor));
        if (primitives.empty()) primitives.push_back(fallbackPrimitive);
        if (materials.empty()) materials.push_back(fallbackMaterial);
        if (offsets.empty()) offsets.push_back(fallbackOffset);
        RefCntAutoPtr<IBuffer> newVertices, newNormals, newUvs, newUvs1, newColors, newPrimitives, newOffsets, newMaterials;
        if (!createStructuredBuffer("RayTracing Shading Vertices", vertices.data(),
                                    vertices.size() * sizeof(float), 4u * sizeof(float), newVertices) ||
            !createStructuredBuffer("RayTracing Shading Normals", normals.data(),
                                    normals.size() * sizeof(float), 4u * sizeof(float), newNormals) ||
            !createStructuredBuffer("RayTracing Shading UVs", uvs.data(),
                                    uvs.size() * sizeof(float), 2u * sizeof(float), newUvs) ||
            !createStructuredBuffer("RayTracing Shading UV1s", uvs1.data(),
                                    uvs1.size() * sizeof(float), 2u * sizeof(float), newUvs1) ||
            !createStructuredBuffer("RayTracing Shading Colors", colors.data(),
                                    colors.size() * sizeof(float), 4u * sizeof(float), newColors) ||
            !createStructuredBuffer("RayTracing Shading Primitives", primitives.data(),
                                    primitives.size() * sizeof(RTPrimitive), sizeof(RTPrimitive), newPrimitives) ||
            !createStructuredBuffer("RayTracing Shading Materials", materials.data(),
                                    materials.size() * sizeof(RTMaterial), sizeof(RTMaterial), newMaterials) ||
            !createStructuredBuffer("RayTracing Instance Primitive Offsets", offsets.data(),
                                    offsets.size() * sizeof(Uint32), sizeof(Uint32), newOffsets)) return false;
        pRenderVertices_ = std::move(newVertices);
        pRenderNormals_ = std::move(newNormals);
        pRenderUvs_ = std::move(newUvs);
        pRenderUvs1_ = std::move(newUvs1);
        pRenderColors_ = std::move(newColors);
        pRenderIndices_ = std::move(newPrimitives);
        pRenderMaterials_ = std::move(newMaterials);
        pInstancePrimitiveOffsets_ = std::move(newOffsets);
        shadingPoolsDirty_ = false;
        for (auto& [name, node] : blasMap_) {
            (void)name;
            node.materialDirty = false;
        }
        rebuildResourceBinding();
        bindWarmupSceneGeometry(pRayTracingSRB_, pRenderVertices_, pRenderIndices_,
                                pInstancePrimitiveOffsets_, pRenderNormals_, pRenderUvs_,
                                pRenderUvs1_, pRenderColors_, pRenderMaterials_);
        return true;
    }

    void rebuildResourceBinding() {
        pRayTracingSRB_.Release();
        if (pRayTracingPSO_) {
            pRayTracingPSO_->CreateShaderResourceBinding(&pRayTracingSRB_, true);
            bindWarmupSceneGeometry(pRayTracingSRB_, pRenderVertices_, pRenderIndices_,
                                    pInstancePrimitiveOffsets_, pRenderNormals_, pRenderUvs_,
                                    pRenderUvs1_, pRenderColors_, pRenderMaterials_);
            std::vector<IDeviceObject*> textureViews;
            textureViews.reserve(pbrTextureCapacity_);
            for (Uint32 i = 0; i < pbrTextureCapacity_; ++i) {
                textureViews.push_back(i < rtPbrTextureViews_.size()
                    ? static_cast<IDeviceObject*>(rtPbrTextureViews_[i])
                    : static_cast<IDeviceObject*>(pFallbackOcclusionSRV_.RawPtr()));
            }
            if (!textureViews.empty()) {
                constexpr SHADER_TYPE textureShaderStages[] = {
                    SHADER_TYPE_RAY_CLOSEST_HIT, SHADER_TYPE_RAY_ANY_HIT};
                for (const auto stage : textureShaderStages) {
                    if (auto* textureArrayVar = pRayTracingSRB_->GetVariableByName(
                            stage, "g_RTPbrTextures")) {
                        textureArrayVar->SetArray(textureViews.data(), 0,
                            static_cast<Uint32>(textureViews.size()));
                    }
                }
            }
            if (auto* cameraVar = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_GEN, "RayCameraConstants")) {
                cameraVar->Set(pRayCameraBuffer_);
            }
            if (auto* environmentVar = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTEnvironment")) {
                environmentVar->Set(pEnvironmentSRV_);
            }
            if (auto* missEnvironmentVar = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_MISS, "g_RTEnvironment")) {
                missEnvironmentVar->Set(pEnvironmentSRV_);
            }
            if (auto* sheenEnvironmentVar = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTSheenEnvironment")) {
                sheenEnvironmentVar->Set(pSheenEnvironmentSRV_);
            }
            if (auto* irradianceVar = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTIrradiance")) {
                irradianceVar->Set(pIrradianceEnvironmentSRV_);
            }
            if (auto* brdfLutVar = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_CLOSEST_HIT, "g_RTBrdfLut")) {
                brdfLutVar->Set(pBrdfLutSRV_);
            }
            if (auto* environmentConstantsVar = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_CLOSEST_HIT, "RTEnvironmentConstants")) {
                environmentConstantsVar->Set(pEnvironmentConstantsBuffer_);
            }
            if (auto* missEnvironmentConstantsVar = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_MISS, "RTEnvironmentConstants")) {
                missEnvironmentConstantsVar->Set(pEnvironmentConstantsBuffer_);
            }
            if (auto* sceneLightsConstantsVar = pRayTracingSRB_->GetVariableByName(
                    SHADER_TYPE_RAY_CLOSEST_HIT, "RTSceneLightsConstants")) {
                sceneLightsConstantsVar->Set(pSceneLightsConstantsBuffer_);
            }
        }
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
    IDeviceContext* pImmediateContext_ = nullptr;
    RefCntAutoPtr<ITopLevelAS> pTLAS_;
    RefCntAutoPtr<IBuffer> pBLASScratchBuffer_;
    RefCntAutoPtr<IBuffer> pTLASScratchBuffer_;
    RefCntAutoPtr<IBuffer> pTLASInstanceBuffer_;
    RefCntAutoPtr<IBuffer> pRenderVertices_;
    RefCntAutoPtr<IBuffer> pRenderNormals_;
    RefCntAutoPtr<IBuffer> pRenderUvs_;
    RefCntAutoPtr<IBuffer> pRenderUvs1_;
    RefCntAutoPtr<IBuffer> pRenderColors_;
    RefCntAutoPtr<IBuffer> pRenderIndices_;
    RefCntAutoPtr<IBuffer> pRenderMaterials_;
    RefCntAutoPtr<IBuffer> pRayCameraBuffer_;
    RefCntAutoPtr<IBuffer> pEnvironmentConstantsBuffer_;
    RefCntAutoPtr<IBuffer> pSceneLightsConstantsBuffer_;
    RefCntAutoPtr<IBuffer> pInstancePrimitiveOffsets_;
    RefCntAutoPtr<ITexture> pTraceOutputTexture_;
    RefCntAutoPtr<ITextureView> pTraceOutputSRV_;
    RefCntAutoPtr<ITextureView> pTraceOutputUAV_;
    RefCntAutoPtr<ITexture> pFallbackEnvironmentTexture_;
    RefCntAutoPtr<ITexture> pFallbackBrdfLutTexture_;
    RefCntAutoPtr<ITextureView> pFallbackEnvironmentSRV_;
    RefCntAutoPtr<ITextureView> pFallbackBrdfLutSRV_;
    RefCntAutoPtr<ITexture> pFallbackOcclusionTexture_;
    RefCntAutoPtr<ITextureView> pFallbackOcclusionSRV_;
    RefCntAutoPtr<ITextureView> pEnvironmentSRV_;
    RefCntAutoPtr<ITextureView> pSheenEnvironmentSRV_;
    RefCntAutoPtr<ITextureView> pIrradianceEnvironmentSRV_;
    RefCntAutoPtr<ITextureView> pBrdfLutSRV_;
    RefCntAutoPtr<IShader> pWarmupRayGen_;
    RefCntAutoPtr<IShader> pWarmupMiss_;
    RefCntAutoPtr<IShader> pWarmupClosestHit_;
    RefCntAutoPtr<IShader> pWarmupAnyHit_;
    RefCntAutoPtr<IPipelineState> pRayTracingPSO_;
    RefCntAutoPtr<IShaderResourceBinding> pRayTracingSRB_;
    RefCntAutoPtr<IShaderBindingTable> pSBT_;
    
    std::map<std::wstring, BLASNode> blasMap_;
    std::map<QString, RTPbrTexture> pbrTextureCache_;
    std::vector<ITextureView*> rtPbrTextureViews_;
    Uint32 pbrTextureCapacity_ = 0;
    Uint32 tlasInstanceCount_ = 0;
    bool sbtHitGroupBound_ = false;
    bool shadingPoolsDirty_ = true;
    bool environmentEnabled_ = false;
    float environmentIntensity_ = 1.0f;
    float environmentRotationRadians_ = 0.0f;
    bool environmentSettingsUploaded_ = false;
    RTSceneLightsConstants sceneLightsConstants_{};
    RayTracingCapabilities caps_;
    bool rtSupported_ = false;
};

std::unique_ptr<IRayTracingManager> createRayTracingManager() {
    return std::make_unique<RayTracingManager>();
}

} // namespace ArtifactCore
