module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Buffer.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Sampler.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>

module Render.VolumeRenderer;

import Graphics.Compute;
import Graphics.GPUcomputeContext;
import Core.Parallel;

namespace ArtifactCore::RayTrace {

namespace {
constexpr char kVolumeRaymarchShader[] = R"hlsl(
Texture3D<float> gDensity;
Texture3D<float> gTemperature;
Texture3D<float> gReaction;
RWTexture2D<float4> gOutput;
SamplerState gSampler;
cbuffer VolumeParams {
    uint width;
    uint height;
    uint depth;
    uint steps;
    float densityScale;
    float temperatureScale;
    float opacity;
    uint transferKind;
    uint hasReaction;
    float fireReactionScale;
    float smokeLightingStrength;
    float smokeShadowDensity;
    float detailStrength;
    float detailScale;
    float detailTimeSeconds;
    uint detailSeed;
    float viewCenterX;
    float viewCenterY;
    float viewScaleX;
    float viewScaleY;
    float detailOffsetX;
    float detailOffsetY;
    float detailOffsetZ;
    float detailOffsetPadding;
};

uint noiseHash(uint3 cell, uint seed) {
    uint value = cell.x * 1597334677u ^ cell.y * 3812015801u ^
                 cell.z * 2798796415u ^ seed * 2246822519u;
    value ^= value >> 16;
    value *= 2246822519u;
    value ^= value >> 13;
    value *= 3266489917u;
    value ^= value >> 16;
    return value;
}

float latticeNoise(uint3 cell, uint seed) {
    return (float)(noiseHash(cell, seed) >> 8) * (1.0 / 16777215.0);
}

float valueNoise(float3 position, uint seed) {
    float3 base = floor(position);
    float3 f = frac(position);
    f = f * f * (3.0 - 2.0 * f);
    int3 signedCell = (int3)base;
    uint3 cell = (uint3)signedCell;
    float c000 = latticeNoise(cell, seed);
    float c100 = latticeNoise(cell + uint3(1, 0, 0), seed);
    float c010 = latticeNoise(cell + uint3(0, 1, 0), seed);
    float c110 = latticeNoise(cell + uint3(1, 1, 0), seed);
    float c001 = latticeNoise(cell + uint3(0, 0, 1), seed);
    float c101 = latticeNoise(cell + uint3(1, 0, 1), seed);
    float c011 = latticeNoise(cell + uint3(0, 1, 1), seed);
    float c111 = latticeNoise(cell + uint3(1, 1, 1), seed);
    float z0 = lerp(lerp(c000, c100, f.x), lerp(c010, c110, f.x), f.y);
    float z1 = lerp(lerp(c001, c101, f.x), lerp(c011, c111, f.x), f.y);
    return lerp(z0, z1, f.z);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID) {
    if (tid.x >= width || tid.y >= height) return;
    float2 uv = (float2(tid.xy) + 0.5) / float2(width, height);
    float2 viewUV = float2(uv.x, 1.0 - uv.y);
    float2 volumeUV = viewUV;
    if (viewCenterX != 0.5 || viewCenterY != 0.5 ||
        viewScaleX != 1.0 || viewScaleY != 1.0) {
        volumeUV = float2(viewCenterX, viewCenterY) +
                   (viewUV - 0.5) * float2(viewScaleX, viewScaleY);
    }
    if (any(volumeUV < 0.0) || any(volumeUV > 1.0)) {
        gOutput[tid.xy] = 0.0;
        return;
    }
    float3 rayUV = float3(volumeUV, 0.0);
    float3 accumulated = 0.0;
    float transmittance = 1.0;
    float stepLength = 1.0 / max(1.0, (float)steps);
    for (uint i = 0; i < steps && transmittance > 0.01; ++i) {
        rayUV.z = (i + 0.5) * stepLength;
        float d = max(0.0, gDensity.SampleLevel(gSampler, rayUV, 0) * densityScale);
        float t = max(0.0, gTemperature.SampleLevel(gSampler, rayUV, 0) * temperatureScale);
        float reaction = hasReaction != 0
            ? saturate(gReaction.SampleLevel(gSampler, rayUV, 0) * fireReactionScale)
            : 1.0;
        if (detailStrength > 0.0) {
            float3 detailPosition = (rayUV + float3(detailOffsetX, detailOffsetY, detailOffsetZ)) * max(1.0, detailScale) +
                float3(0.23, -0.17, 0.31) * detailTimeSeconds;
            float detail = valueNoise(detailPosition, detailSeed) * 2.0 - 1.0;
            float shaping = max(0.0, 1.0 + detail * saturate(detailStrength));
            d *= shaping;
            t *= lerp(1.0, shaping, 0.65);
        }
        float smokeLighting = 1.0;
        if (smokeLightingStrength > 0.0 && d > 1.0e-5 &&
            (transferKind == 1 || transferKind == 6)) {
            // Fixed-cost lookdev approximation in normalized volume space;
            // this does not sample scene lights or model multiple scattering.
            const float3 lightDirection = normalize(float3(-0.45, 1.0, -0.25));
            const float lightStep = 1.0 / 12.0;
            float opticalDepth = 0.0;
            for (int lightIndex = 0; lightIndex < 12; ++lightIndex) {
                float3 lightUV = rayUV + lightDirection * ((lightIndex + 0.5) * lightStep);
                if (any(lightUV < 0.0) || any(lightUV > 1.0)) break;
                opticalDepth += max(0.0, gDensity.SampleLevel(gSampler, lightUV, 0)) *
                    densityScale * smokeShadowDensity * lightStep;
            }
            float transmittanceToLight = exp(-opticalDepth);
            smokeLighting = lerp(1.0, 0.25 + 1.5 * transmittanceToLight,
                                 saturate(smokeLightingStrength));
        }
        float3 color = 0.0;
        float extinction = 0.0;
        if (transferKind == 0) {
            float heat = saturate(t * 0.5 + d * 0.5);
            color = float3(saturate(heat * 2.0), saturate(heat * 0.8), saturate(heat * 0.15)) * reaction;
            extinction = saturate(d * 8.0) * 20.0;
        } else if (transferKind == 6) {
            float heat = saturate(t * 0.5 + d * 0.5);
            float smoke = saturate(d * 10.0);
            float3 flame = float3(saturate(heat * 2.0),
                                  saturate(heat * 0.8),
                                  saturate(heat * 0.15)) * reaction;
            float3 smokeScatter = smoke * 0.3 * smokeLighting;
            color = flame + smokeScatter;
            extinction = saturate(d * 8.0) * 20.0 + d * 30.0;
        } else if (transferKind == 1) {
            float value = saturate(d * 10.0);
            color = value * 0.3 * smokeLighting;
            extinction = d * 30.0;
        } else if (transferKind == 2) {
            float value = saturate(d * 3.0);
            color = value * float3(0.7, 0.7, 0.75);
            extinction = d * 5.0;
        } else if (transferKind == 3) {
            float fire = saturate(t * 0.7);
            float smoke = saturate(d * 12.0);
            color = float3(fire * 2.0 + smoke * 0.2,
                           fire * 0.6 + smoke * 0.15,
                           fire * 0.05 + smoke * 0.05);
            extinction = (fire + smoke) * 25.0;
        } else if (transferKind == 5) {
            float value = min(0.45, d * 1.2);
            color = value * float3(0.55, 0.58, 0.63);
            extinction = d * 3.5;
        } else {
            color = d * 0.3;
            extinction = d * 10.0;
        }
        float alpha = 1.0 - exp(-extinction * stepLength);
        accumulated += transmittance * alpha * color;
        transmittance *= 1.0 - alpha;
    }
    float outAlpha = saturate((1.0 - transmittance) * opacity);
    float3 straightColor = accumulated / max(1.0e-5, 1.0 - transmittance);
    gOutput[tid.xy] = float4(straightColor, outAlpha);
}
)hlsl";

struct GpuVolumeParams {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t depth = 0;
    std::uint32_t steps = 0;
    float densityScale = 1.0f;
    float temperatureScale = 1.0f;
    float opacity = 1.0f;
    std::uint32_t transferKind = 0;
    std::uint32_t hasReaction = 0;
    float fireReactionScale = 1.5f;
    float smokeLightingStrength = 0.0f;
    float smokeShadowDensity = 20.0f;
    float detailStrength = 0.0f;
    float detailScale = 12.0f;
    float detailTimeSeconds = 0.0f;
    std::uint32_t detailSeed = 0;
    float viewCenterX = 0.5f;
    float viewCenterY = 0.5f;
    float viewScaleX = 1.0f;
    float viewScaleY = 1.0f;
    float detailOffsetX = 0.0f;
    float detailOffsetY = 0.0f;
    float detailOffsetZ = 0.0f;
    float detailOffsetPadding = 0.0f;
};
}

struct GpuVolumeRenderer::Impl {
    ArtifactCore::GpuContext gpuContext;
    ArtifactCore::ComputeExecutor executor;
    Diligent::RefCntAutoPtr<Diligent::ITexture> densityTexture;
    Diligent::RefCntAutoPtr<Diligent::ITexture> temperatureTexture;
    Diligent::RefCntAutoPtr<Diligent::ITexture> reactionTexture;
    Diligent::RefCntAutoPtr<Diligent::ITexture> outputTexture;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> densitySRV;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> temperatureSRV;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> reactionSRV;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> outputSRV;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> outputUAV;
    Diligent::RefCntAutoPtr<Diligent::ISampler> sampler;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> paramsBuffer;
    Diligent::IRenderDevice* device = nullptr;
    int volumeWidth = 0;
    int volumeHeight = 0;
    int volumeDepth = 0;
    int outputWidth = 0;
    int outputHeight = 0;

    Impl(Diligent::IRenderDevice* renderDevice,
         Diligent::IDeviceContext* context)
        : gpuContext(renderDevice, context), executor(gpuContext),
          device(renderDevice) {}

    bool createVolumeTextures(const int width, const int height,
                              const int depth) {
        const auto createScalar = [&](const char* name,
                                      Diligent::RefCntAutoPtr<Diligent::ITexture>& texture,
                                      Diligent::RefCntAutoPtr<Diligent::ITextureView>& srv) {
            Diligent::TextureDesc desc;
            desc.Name = name;
            desc.Type = Diligent::RESOURCE_DIM_TEX_3D;
            desc.Width = static_cast<Diligent::Uint32>(width);
            desc.Height = static_cast<Diligent::Uint32>(height);
            desc.Depth = static_cast<Diligent::Uint32>(depth);
            desc.MipLevels = 1;
            desc.Format = Diligent::TEX_FORMAT_R32_FLOAT;
            desc.Usage = Diligent::USAGE_DEFAULT;
            desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
            texture.Release();
            srv.Release();
            device->CreateTexture(desc, nullptr, &texture);
            if (!texture) return false;
            srv = texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
            return srv != nullptr;
        };
        if (!createScalar("PyroVolume/Density3D", densityTexture, densitySRV) ||
            !createScalar("PyroVolume/Temperature3D", temperatureTexture,
                          temperatureSRV) ||
            !createScalar("PyroVolume/Reaction3D", reactionTexture,
                          reactionSRV)) return false;
        volumeWidth = width;
        volumeHeight = height;
        volumeDepth = depth;
        return true;
    }

    bool createOutputTexture(const int width, const int height) {
        Diligent::TextureDesc desc;
        desc.Name = "PyroVolume/RaymarchOutput";
        desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
        desc.Width = static_cast<Diligent::Uint32>(width);
        desc.Height = static_cast<Diligent::Uint32>(height);
        desc.MipLevels = 1;
        desc.Format = Diligent::TEX_FORMAT_RGBA16_FLOAT;
        desc.Usage = Diligent::USAGE_DEFAULT;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE |
                         Diligent::BIND_UNORDERED_ACCESS;
        outputTexture.Release();
        outputSRV.Release();
        outputUAV.Release();
        device->CreateTexture(desc, nullptr, &outputTexture);
        if (!outputTexture) return false;
        outputSRV = outputTexture->GetDefaultView(
            Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
        outputUAV = outputTexture->GetDefaultView(
            Diligent::TEXTURE_VIEW_UNORDERED_ACCESS);
        outputWidth = width;
        outputHeight = height;
        return outputSRV != nullptr && outputUAV != nullptr;
    }
};

GpuVolumeRenderer::GpuVolumeRenderer() noexcept = default;
GpuVolumeRenderer::~GpuVolumeRenderer() { delete impl_; }

bool GpuVolumeRenderer::initialize(Diligent::IRenderDevice* device,
                                   Diligent::IDeviceContext* context) {
    if (!device || !context) return false;
    delete impl_;
    impl_ = new Impl(device, context);
    const Diligent::ShaderResourceVariableDesc variables[]{
        {Diligent::SHADER_TYPE_COMPUTE, "gDensity",
         Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_COMPUTE, "gTemperature",
         Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_COMPUTE, "gReaction",
         Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_COMPUTE, "gOutput",
         Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_COMPUTE, "gSampler",
         Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {Diligent::SHADER_TYPE_COMPUTE, "VolumeParams",
         Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    const ArtifactCore::ComputePipelineDesc pipeline{
        .name = "PyroVolume/Raymarch",
        .shaderSource = kVolumeRaymarchShader,
        .entryPoint = "CSMain",
        .sourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL,
        .variables = variables,
        .variableCount = 6,
        .defaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE};
    if (!impl_->executor.build(pipeline) ||
        !impl_->executor.createShaderResourceBinding()) return false;

    Diligent::SamplerDesc samplerDesc;
    samplerDesc.MinFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = Diligent::FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = Diligent::TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = Diligent::TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = Diligent::TEXTURE_ADDRESS_CLAMP;
    device->CreateSampler(samplerDesc, &impl_->sampler);

    Diligent::BufferDesc paramsDesc;
    paramsDesc.Name = "PyroVolume/Params";
    paramsDesc.Size = sizeof(GpuVolumeParams);
    paramsDesc.Usage = Diligent::USAGE_DYNAMIC;
    paramsDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
    paramsDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
    device->CreateBuffer(paramsDesc, nullptr, &impl_->paramsBuffer);
    return impl_->sampler != nullptr && impl_->paramsBuffer != nullptr;
}

bool GpuVolumeRenderer::render(Diligent::IDeviceContext* context,
                               const VolumeFieldSet& fields,
                               const TransferFunction& transfer,
                               const GpuVolumeRenderSettings& settings) {
    if (!ready() || !context || fields.density.empty() ||
        fields.temperature.empty()) return false;
    const auto resolution = fields.density.resolution;
    if (fields.temperature.resolution.width != resolution.width ||
        fields.temperature.resolution.height != resolution.height ||
        fields.temperature.resolution.depth != resolution.depth) return false;
    if (!fields.reaction.empty() &&
        (fields.reaction.resolution.width != resolution.width ||
         fields.reaction.resolution.height != resolution.height ||
         fields.reaction.resolution.depth != resolution.depth)) return false;
    if (resolution.width != impl_->volumeWidth ||
        resolution.height != impl_->volumeHeight ||
        resolution.depth != impl_->volumeDepth) {
        if (!impl_->createVolumeTextures(resolution.width, resolution.height,
                                         resolution.depth)) return false;
    }
    const int width = std::clamp(settings.outputWidth, 1, 4096);
    const int height = std::clamp(settings.outputHeight, 1, 4096);
    if (width != impl_->outputWidth || height != impl_->outputHeight) {
        if (!impl_->createOutputTexture(width, height)) return false;
    }
    const Diligent::Box volumeBox{
        0, static_cast<Diligent::Uint32>(resolution.width),
        0, static_cast<Diligent::Uint32>(resolution.height),
        0, static_cast<Diligent::Uint32>(resolution.depth)};
    const auto rowStride = static_cast<Diligent::Uint64>(resolution.width) *
                           sizeof(float);
    const auto depthStride = rowStride *
                             static_cast<Diligent::Uint64>(resolution.height);
    Diligent::TextureSubResData densityData;
    densityData.pData = fields.density.data;
    densityData.Stride = rowStride;
    densityData.DepthStride = depthStride;
    Diligent::TextureSubResData temperatureData;
    temperatureData.pData = fields.temperature.data;
    temperatureData.Stride = rowStride;
    temperatureData.DepthStride = depthStride;
    context->UpdateTexture(impl_->densityTexture, 0, 0, volumeBox,
        densityData, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->UpdateTexture(impl_->temperatureTexture, 0, 0, volumeBox,
        temperatureData, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (!fields.reaction.empty()) {
        Diligent::TextureSubResData reactionData;
        reactionData.pData = fields.reaction.data;
        reactionData.Stride = rowStride;
        reactionData.DepthStride = depthStride;
        context->UpdateTexture(impl_->reactionTexture, 0, 0, volumeBox,
            reactionData, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    void* mapped = nullptr;
    context->MapBuffer(impl_->paramsBuffer, Diligent::MAP_WRITE,
        Diligent::MAP_FLAG_DISCARD, mapped);
    if (!mapped) return false;
    const GpuVolumeParams params{
        static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height),
        static_cast<std::uint32_t>(resolution.depth),
        static_cast<std::uint32_t>(std::clamp(settings.raySteps, 1, 1024)),
        std::max(0.0f, settings.densityScale),
        std::max(0.0f, settings.temperatureScale),
        std::clamp(settings.opacity, 0.0f, 1.0f),
        static_cast<std::uint32_t>(transfer.kind),
        fields.reaction.empty() ? 0u : 1u,
        std::max(0.0f, settings.fireReactionScale),
        std::isfinite(settings.smokeLightingStrength)
            ? std::clamp(settings.smokeLightingStrength, 0.0f, 1.0f) : 0.0f,
        std::isfinite(settings.smokeShadowDensity)
            ? std::clamp(settings.smokeShadowDensity, 0.0f, 100.0f) : 20.0f,
        std::clamp(settings.detailStrength, 0.0f, 1.0f),
        std::max(1.0f, settings.detailScale),
        std::isfinite(settings.detailTimeSeconds)
            ? settings.detailTimeSeconds : 0.0f,
        settings.detailSeed,
        std::isfinite(settings.viewCenterX) ? std::clamp(settings.viewCenterX, 0.0f, 1.0f) : 0.5f,
        std::isfinite(settings.viewCenterY) ? std::clamp(settings.viewCenterY, 0.0f, 1.0f) : 0.5f,
        std::isfinite(settings.viewScaleX) ? std::clamp(settings.viewScaleX, 0.001f, 2.0f) : 1.0f,
        std::isfinite(settings.viewScaleY) ? std::clamp(settings.viewScaleY, 0.001f, 2.0f) : 1.0f,
        std::isfinite(settings.detailOffsetX) ? settings.detailOffsetX : 0.0f,
        std::isfinite(settings.detailOffsetY) ? settings.detailOffsetY : 0.0f,
        std::isfinite(settings.detailOffsetZ) ? settings.detailOffsetZ : 0.0f};
    std::memcpy(mapped, &params, sizeof(params));
    context->UnmapBuffer(impl_->paramsBuffer, Diligent::MAP_WRITE);
    if (!impl_->executor.setTextureView("gDensity", impl_->densitySRV) ||
        !impl_->executor.setTextureView("gTemperature", impl_->temperatureSRV) ||
        !impl_->executor.setTextureView("gReaction", impl_->reactionSRV) ||
        !impl_->executor.setTextureView("gOutput", impl_->outputUAV) ||
        !impl_->executor.setSampler("gSampler", impl_->sampler) ||
        !impl_->executor.setBuffer("VolumeParams", impl_->paramsBuffer)) return false;
    impl_->executor.dispatch(context,
        ArtifactCore::ComputeExecutor::makeDispatchAttribs(
            static_cast<Diligent::Uint32>(width),
            static_cast<Diligent::Uint32>(height), 1, 8, 8, 1));
    return true;
}

Diligent::ITextureView* GpuVolumeRenderer::outputSRV() const noexcept {
    return impl_ ? impl_->outputSRV.RawPtr() : nullptr;
}

bool GpuVolumeRenderer::ready() const noexcept {
    return impl_ && impl_->device && impl_->executor.ready();
}

namespace {

inline float clamp01(float v) noexcept {
    return std::clamp(v, 0.0f, 1.0f);
}

inline Color lerpColor(const Color& a, const Color& b, float t) noexcept {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

inline Vec3 safeNormalize(const Vec3& v) noexcept {
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq < 1e-12f) return {0.0f, 0.0f, 0.0f};
    const float invLen = 1.0f / std::sqrt(lenSq);
    return {v.x * invLen, v.y * invLen, v.z * invLen};
}

inline float dotColor(const Color& a, const Color& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

}

float VolumeScalarField::sampleTrilinear(const Vec3& voxelCoord) const noexcept {
    if (empty()) return 0.0f;

    const int w = resolution.width;
    const int h = resolution.height;
    const int d = resolution.depth;

    const float fx = std::clamp(voxelCoord.x, 0.0f, static_cast<float>(w - 1));
    const float fy = std::clamp(voxelCoord.y, 0.0f, static_cast<float>(h - 1));
    const float fz = std::clamp(voxelCoord.z, 0.0f, static_cast<float>(d - 1));

    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const int z0 = static_cast<int>(std::floor(fz));
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const int z1 = std::min(z0 + 1, d - 1);

    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);
    const float tz = fz - static_cast<float>(z0);

    const float c000 = at(x0, y0, z0);
    const float c100 = at(x1, y0, z0);
    const float c010 = at(x0, y1, z0);
    const float c110 = at(x1, y1, z0);
    const float c001 = at(x0, y0, z1);
    const float c101 = at(x1, y0, z1);
    const float c011 = at(x0, y1, z1);
    const float c111 = at(x1, y1, z1);

    const float c00 = c000 + (c100 - c000) * tx;
    const float c10 = c010 + (c110 - c010) * tx;
    const float c01 = c001 + (c101 - c001) * tx;
    const float c11 = c011 + (c111 - c011) * tx;
    const float c0 = c00 + (c10 - c00) * ty;
    const float c1 = c01 + (c11 - c01) * ty;
    return c0 + (c1 - c0) * tz;
}

TransferFunctionSample TransferFunction::evaluate(float density, float temperature) const noexcept {
    const float d = density * densityScale;
    const float t = temperature * temperatureScale;

    if (customCallback) {
        return customCallback(d, t);
    }

    TransferFunctionSample sample{};
    sample.densityValue = d;

    switch (kind) {
    case TransferFunctionKind::Fire: {
        const float flameOpacity = std::clamp(d * 8.0f, 0.0f, 1.0f);
        const float heatFactor = clamp01(t * 0.5f + d * 0.5f);
        sample.emission = {
            clamp01(heatFactor * 2.0f),
            clamp01(heatFactor * 0.8f),
            clamp01(heatFactor * 0.15f)
        };
        sample.absorption = flameOpacity * 20.0f;
        break;
    }
    case TransferFunctionKind::FireAndSmoke: {
        const float flameOpacity = std::clamp(d * 8.0f, 0.0f, 1.0f);
        const float heatFactor = clamp01(t * 0.5f + d * 0.5f);
        const float smoke = clamp01(d * 10.0f);
        sample.emission = {smoke * 0.3f, smoke * 0.3f, smoke * 0.3f};
        sample.fireEmission = {
            clamp01(heatFactor * 2.0f),
            clamp01(heatFactor * 0.8f),
            clamp01(heatFactor * 0.15f)
        };
        sample.absorption = flameOpacity * 20.0f + d * 30.0f;
        break;
    }
    case TransferFunctionKind::Smoke: {
        const float opacity = clamp01(d * 10.0f);
        sample.emission = {opacity * 0.3f, opacity * 0.3f, opacity * 0.3f};
        sample.absorption = d * 30.0f;
        break;
    }
    case TransferFunctionKind::Steam: {
        const float opacity = clamp01(d * 3.0f);
        sample.emission = {opacity * 0.7f, opacity * 0.7f, opacity * 0.75f};
        sample.absorption = d * 5.0f;
        break;
    }
    case TransferFunctionKind::Explosion: {
        const float fire = clamp01(t * 0.7f);
        const float smoke = clamp01(d * 12.0f);
        sample.emission = {
            fire * 2.0f + smoke * 0.2f,
            fire * 0.6f + smoke * 0.15f,
            fire * 0.05f + smoke * 0.05f
        };
        sample.absorption = (fire + smoke) * 25.0f;
        break;
    }
    case TransferFunctionKind::Fog: {
        const float opacity = std::clamp(d * 1.2f, 0.0f, 0.45f);
        sample.emission = {
            opacity * 0.55f,
            opacity * 0.58f,
            opacity * 0.63f
        };
        sample.absorption = d * 3.5f;
        break;
    }
    case TransferFunctionKind::Custom:
    default: {
        sample.emission = {d * 0.5f, d * 0.5f, d * 0.5f};
        sample.absorption = d * 10.0f;
        break;
    }
    }
    return sample;
}

TransferFunction TransferFunction::makeFire() {
    TransferFunction tf;
    tf.kind = TransferFunctionKind::Fire;
    return tf;
}

TransferFunction TransferFunction::makeSmoke() {
    TransferFunction tf;
    tf.kind = TransferFunctionKind::Smoke;
    return tf;
}

TransferFunction TransferFunction::makeSteam() {
    TransferFunction tf;
    tf.kind = TransferFunctionKind::Steam;
    return tf;
}

TransferFunction TransferFunction::makeExplosion() {
    TransferFunction tf;
    tf.kind = TransferFunctionKind::Explosion;
    return tf;
}

TransferFunction TransferFunction::makeFog() {
    TransferFunction tf;
    tf.kind = TransferFunctionKind::Fog;
    return tf;
}

TransferFunction TransferFunction::makeFireAndSmoke() {
    TransferFunction tf;
    tf.kind = TransferFunctionKind::FireAndSmoke;
    return tf;
}

void CPUVolumeRenderer::setVolumeData(const VolumeAABB& aabb, const VolumeFieldSet& fieldSet) {
    bounds = aabb;
    const auto finiteOr = [](const float value, const float fallback) {
        return std::isfinite(value) ? value : fallback;
    };
    bounds.min.x = finiteOr(bounds.min.x, 0.0f);
    bounds.min.y = finiteOr(bounds.min.y, 0.0f);
    bounds.min.z = finiteOr(bounds.min.z, 0.0f);
    bounds.max.x = finiteOr(bounds.max.x, bounds.min.x + 1.0f);
    bounds.max.y = finiteOr(bounds.max.y, bounds.min.y + 1.0f);
    bounds.max.z = finiteOr(bounds.max.z, bounds.min.z + 1.0f);
    if (bounds.max.x <= bounds.min.x) bounds.max.x = bounds.min.x + 1.0f;
    if (bounds.max.y <= bounds.min.y) bounds.max.y = bounds.min.y + 1.0f;
    if (bounds.max.z <= bounds.min.z) bounds.max.z = bounds.min.z + 1.0f;
    fields = fieldSet;
}

void CPUVolumeRenderer::setTransferFunction(const TransferFunction& tf) {
    transferFunction_ = tf;
}

void CPUVolumeRenderer::addLight(const VolumetricLight& light) {
    lights.push_back(light);
}

void CPUVolumeRenderer::clearLights() {
    lights.clear();
}

Color CPUVolumeRenderer::evaluateVolumetricLights(const Vec3& worldPos, const Vec3& sampleDir) const noexcept {
    Color accumulated{0.0f, 0.0f, 0.0f};

    for (const auto& light : lights) {
        if (!light.enabled) continue;
        if (!std::isfinite(light.range) || light.range <= 0.0f) continue;

        const Vec3 toLight{
            light.position.x - worldPos.x,
            light.position.y - worldPos.y,
            light.position.z - worldPos.z
        };
        const float distToLight = std::sqrt(toLight.x * toLight.x + toLight.y * toLight.y + toLight.z * toLight.z);

        if (distToLight > light.range) continue;

        const float rangeFactor = std::pow(std::max(0.0f, 1.0f - distToLight / light.range), light.falloffExponent);
        if (rangeFactor < 0.001f) continue;

        float directionalFactor = 1.0f;
        if (light.type == VolumeLightType::Spot) {
            const Vec3 lightForward = safeNormalize(light.direction);
            const Vec3 dirToLight = safeNormalize(toLight);
            const float cosAngle = -(lightForward.x * dirToLight.x + lightForward.y * dirToLight.y + lightForward.z * dirToLight.z);

            const float innerRad = light.coneInnerAngle * 3.14159265f / 180.0f;
            const float outerRad = light.coneOuterAngle * 3.14159265f / 180.0f;
            const float cosInner = std::cos(innerRad * 0.5f);
            const float cosOuter = std::cos(outerRad * 0.5f);

            if (cosAngle < cosOuter) continue;

            if (cosAngle < cosInner) {
                const float t = (cosAngle - cosOuter) / (cosInner - cosOuter);
                directionalFactor = std::pow(t, light.coneSoftness);
            }
        }

        const float viewScatter = std::max(0.0f, sampleDir.x * sampleDir.x + sampleDir.y * sampleDir.y + sampleDir.z * sampleDir.z) * 0.1f + 0.9f;
        const float intensity = light.intensity * rangeFactor * directionalFactor * viewScatter;

        accumulated.x += light.color.x * intensity;
        accumulated.y += light.color.y * intensity;
        accumulated.z += light.color.z * intensity;
    }

    return accumulated;
}

bool CPUVolumeRenderer::intersectAABB(const Ray& ray, float& tNear, float& tFar) const noexcept {
    float tmin = -std::numeric_limits<float>::infinity();
    float tmax = std::numeric_limits<float>::infinity();

    for (int i = 0; i < 3; ++i) {
        const float origin = (i == 0) ? ray.origin.x : (i == 1) ? ray.origin.y : ray.origin.z;
        const float direction = (i == 0) ? ray.dir.x : (i == 1) ? ray.dir.y : ray.dir.z;
        const float bmin = (i == 0) ? bounds.min.x : (i == 1) ? bounds.min.y : bounds.min.z;
        const float bmax = (i == 0) ? bounds.max.x : (i == 1) ? bounds.max.y : bounds.max.z;

        if (std::abs(direction) < 1e-8f) {
            if (origin < bmin || origin > bmax) return false;
            continue;
        }

        const float invDir = 1.0f / direction;
        float t0 = (bmin - origin) * invDir;
        float t1 = (bmax - origin) * invDir;
        if (t0 > t1) std::swap(t0, t1);

        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmin > tmax) return false;
    }

    tNear = std::max(0.0f, tmin);
    tFar = tmax;
    return tNear <= tFar;
}

Vec3 CPUVolumeRenderer::worldToVoxel(const Vec3& worldPos) const noexcept {
    const auto e = bounds.extent();
    return {
        (worldPos.x - bounds.min.x) / (e.x > 0.0f ? e.x : 1.0f) * static_cast<float>(fields.density.resolution.width),
        (worldPos.y - bounds.min.y) / (e.y > 0.0f ? e.y : 1.0f) * static_cast<float>(fields.density.resolution.height),
        (worldPos.z - bounds.min.z) / (e.z > 0.0f ? e.z : 1.0f) * static_cast<float>(fields.density.resolution.depth),
    };
}

float CPUVolumeRenderer::sampleDensity(const Vec3& voxelPos) const noexcept {
    if (fields.density.empty()) return 0.0f;
    return fields.density.sampleTrilinear(voxelPos);
}

float CPUVolumeRenderer::phaseHenyeyGreenstein(float cosTheta, float g) const noexcept {
    const float gSq = g * g;
    const float denom = 1.0f + gSq - 2.0f * g * cosTheta;
    return (1.0f - gSq) / (4.0f * 3.14159265f * std::pow(std::max(denom, 0.001f), 1.5f));
}

float CPUVolumeRenderer::multipleScatteringBoost(float density, float depth) const noexcept {
    if (settings.multipleScatteringStrength <= 0.0f) return 1.0f;
    const float di = std::clamp(density * settings.multipleScatteringSpread, 0.0f, 1.0f);
    const float dp = std::clamp(depth * settings.multipleScatteringSpread * 0.1f, 0.0f, 1.0f);
    float boost = 0.0f;
    float weightSum = 0.0f;
    float freq = 1.0f;
    for (int i = 0; i < settings.multipleScatteringOctaves; ++i) {
        boost += di * freq;
        weightSum += freq;
        freq *= 0.5f;
    }
    boost = boost / std::max(weightSum, 1e-10f);
    return 1.0f + boost * settings.multipleScatteringStrength * (1.0f + dp);
}

Color CPUVolumeRenderer::sampleLight(const Vec3& voxelPos) const noexcept {
    if (settings.shadowIntensity <= 0.0f || fields.density.empty()) return {1.0f, 1.0f, 1.0f};

    const Vec3 lightDir = safeNormalize(settings.lightDirection);
    float t = 0.0f;
    float accumulatedDensity = 0.0f;

    for (int i = 0; i < settings.shadowMaxSteps; ++i) {
        t += settings.shadowStepSize;
        const Vec3 samplePos{
            voxelPos.x + lightDir.x * t,
            voxelPos.y + lightDir.y * t,
            voxelPos.z + lightDir.z * t
        };
        accumulatedDensity += sampleDensity(samplePos) * settings.shadowDensityScale;

        if (accumulatedDensity > 8.0f) break;
    }

    const float transmittance = std::exp(-accumulatedDensity);
    return {transmittance, transmittance, transmittance};
}

Color CPUVolumeRenderer::raymarch(const Ray& ray) const noexcept {
    float tNear = 0.0f;
    float tFar = 0.0f;

    if (!intersectAABB(ray, tNear, tFar)) {
        return settings.backgroundColor;
    }

    if (fields.density.empty()) {
        return settings.backgroundColor;
    }

    const Vec3 lightDir = safeNormalize(settings.lightDirection);
    float t = tNear;
    float transmittance = 1.0f;
    Color accumulated{0.0f, 0.0f, 0.0f};

    for (int i = 0; i < settings.maxSteps && t < tFar; ++i) {
        if (transmittance < 0.01f) break;

        const Vec3 worldPos = ray.at(t);
        const Vec3 voxelCoord = worldToVoxel(worldPos);

        const float density = sampleDensity(voxelCoord);
        if (density <= 0.0f) {
            t += settings.stepSize;
            continue;
        }

        float temperature = 0.0f;
        if (settings.useTemperatureEmission && !fields.temperature.empty()) {
            temperature = fields.temperature.sampleTrilinear(voxelCoord);
        }

        auto tfSample = transferFunction_.evaluate(density * settings.densityScale, temperature);
        if ((transferFunction_.kind == TransferFunctionKind::Fire ||
             transferFunction_.kind == TransferFunctionKind::FireAndSmoke) &&
            !fields.reaction.empty()) {
            const float reactionGate = clamp01(
                fields.reaction.sampleTrilinear(voxelCoord) * settings.fireReactionScale);
            // Combustion controls fire emission; smoke extinction stays density-driven.
            auto& flame = transferFunction_.kind == TransferFunctionKind::Fire
                ? tfSample.emission : tfSample.fireEmission;
            flame.x *= reactionGate;
            flame.y *= reactionGate;
            flame.z *= reactionGate;
        }
        if (transferFunction_.kind == TransferFunctionKind::Fire ||
            transferFunction_.kind == TransferFunctionKind::FireAndSmoke) {
            const float intensity = std::max(0.0f, settings.fireIntensityScale);
            auto& flame = transferFunction_.kind == TransferFunctionKind::Fire
                ? tfSample.emission : tfSample.fireEmission;
            flame.x *= intensity;
            flame.y *= intensity;
            flame.z *= intensity;
        }
        const float absorption = tfSample.absorption * settings.stepSize;

        const float ambient = std::clamp(settings.ambientLightIntensity, 0.0f, 1.0f);
        Color lighting{1.0f, 1.0f, 1.0f};
        if (settings.lightIntensity > 0.0f) {
            const auto shadow = sampleLight(voxelCoord);
            const float shadowAmount = std::clamp(settings.shadowIntensity, 0.0f, 1.0f);
            const float shadowFactor = 1.0f - shadowAmount * (1.0f - shadow.x);
            const float NdotL = std::max(0.0f, -settings.lightDirection.y);
            const float henyey = phaseHenyeyGreenstein(NdotL, settings.phaseAnisotropy);
            lighting = {
                ambient + shadowFactor * (0.1f + henyey * 0.9f) * settings.lightIntensity,
                ambient + shadowFactor * (0.1f + henyey * 0.9f) * settings.lightIntensity,
                ambient + shadowFactor * (0.1f + henyey * 0.9f) * settings.lightIntensity
            };
        } else if (ambient > 0.0f) {
            lighting = {ambient, ambient, ambient};
        }

        const auto volumetricLightContrib = evaluateVolumetricLights(worldPos, ray.dir);
        const float scatteringBoost = multipleScatteringBoost(density * settings.densityScale, t - tNear);

        const float alpha = 1.0f - std::exp(-absorption);
        accumulated.x += transmittance * alpha * (((tfSample.emission.x + tfSample.fireEmission.x) * lighting.x + volumetricLightContrib.x) * scatteringBoost);
        accumulated.y += transmittance * alpha * (((tfSample.emission.y + tfSample.fireEmission.y) * lighting.y + volumetricLightContrib.y) * scatteringBoost);
        accumulated.z += transmittance * alpha * (((tfSample.emission.z + tfSample.fireEmission.z) * lighting.z + volumetricLightContrib.z) * scatteringBoost);
        transmittance *= (1.0f - alpha);

        t += settings.stepSize;
    }

    accumulated.x += settings.backgroundColor.x * transmittance;
    accumulated.y += settings.backgroundColor.y * transmittance;
    accumulated.z += settings.backgroundColor.z * transmittance;

    return accumulated;
}

ImageBuffer CPUVolumeRenderer::render(int width, int height) const {
    if (width <= 0 || height <= 0) {
        return ImageBuffer(0, 0);
    }

    ImageBuffer buffer(width, height);
    Camera renderCamera = camera;
    renderCamera.setViewport(static_cast<float>(width), static_cast<float>(height));

    const int dofSamples = renderCamera.aperture > 0.0f ? 4 : 1;

    const auto renderRow = [&](int y) {
        std::uint8_t* row = buffer.pixels.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 3u;
        for (int x = 0; x < width; ++x) {
            Color pixelColor{0.0f, 0.0f, 0.0f};

            for (int s = 0; s < dofSamples; ++s) {
                const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
                const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);

                if (dofSamples == 1) {
                    pixelColor = raymarch(renderCamera.getRay(u, v));
                } else {
                    const float goldenAngle = 2.39996323f;
                    const float theta = static_cast<float>(s) * goldenAngle;
                    const float radius = std::sqrt(static_cast<float>(s) + 0.5f) / std::max(std::sqrt(static_cast<float>(dofSamples)), 1.0f);
                    float lensU = std::cos(theta) * radius;
                    float lensV = std::sin(theta) * radius;
                    const auto sample = raymarch(renderCamera.getRayDOF(u, v, lensU, lensV));
                    pixelColor.x += sample.x;
                    pixelColor.y += sample.y;
                    pixelColor.z += sample.z;
                }
            }

            if (dofSamples > 1) {
                const float invSamples = 1.0f / static_cast<float>(dofSamples);
                pixelColor.x *= invSamples;
                pixelColor.y *= invSamples;
                pixelColor.z *= invSamples;
            }

            std::uint8_t* pixel = row + static_cast<std::size_t>(x) * 3u;
            pixel[0] = static_cast<std::uint8_t>(std::clamp(pixelColor.x * 255.999f, 0.0f, 255.0f));
            pixel[1] = static_cast<std::uint8_t>(std::clamp(pixelColor.y * 255.999f, 0.0f, 255.0f));
            pixel[2] = static_cast<std::uint8_t>(std::clamp(pixelColor.z * 255.999f, 0.0f, 255.0f));
        }
    };

    if (transferFunction_.customCallback) {
        for (int y = 0; y < height; ++y) {
            renderRow(y);
        }
    } else {
        Parallel::ForPixels(0, height, width, height, renderRow);
    }

    return buffer;
}

float normalizeStepToVoxel(float stepSize, const VolumeAABB& bounds, const VolumeResolution& resolution) noexcept {
    if (!std::isfinite(stepSize) || stepSize <= 0.0f) stepSize = 0.01f;
    if (!resolution.valid()) return stepSize;
    const auto vs = bounds.voxelSize(resolution);
    const float voxelDiag = std::sqrt(vs.x * vs.x + vs.y * vs.y + vs.z * vs.z);
    return std::max(stepSize, voxelDiag * 0.25f);
}

} // namespace ArtifactCore::RayTrace
