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

struct EnvironmentFloat3 {
    float x;
    float y;
    float z;
};

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

QVector<QVector<quint8>> buildRgba8MipChain(const QVector<quint8>& base,
                                            int width,
                                            int height,
                                            bool srgb,
                                            bool renormalizeNormal = false)
{
    QVector<QVector<quint8>> mipData;
    mipData.push_back(base);
    int mipWidth = width;
    int mipHeight = height;
    const auto decodeSrgb = [](float value) {
        return value <= 0.04045f ? value / 12.92f
                                 : std::pow((value + 0.055f) / 1.055f, 2.4f);
    };
    const auto encodeSrgb = [](float value) {
        return value <= 0.0031308f ? value * 12.92f
                                   : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    };

    while (mipWidth > 1 || mipHeight > 1) {
        const int nextWidth = std::max(1, mipWidth / 2);
        const int nextHeight = std::max(1, mipHeight / 2);
        const auto& previous = mipData.back();
        QVector<quint8> next(nextWidth * nextHeight * 4, 0);
        for (int y = 0; y < nextHeight; ++y) {
            for (int x = 0; x < nextWidth; ++x) {
                float sum[4] = {};
                int samples = 0;
                for (int sy = 0; sy < 2; ++sy) {
                    for (int sx = 0; sx < 2; ++sx) {
                        const int px = std::min(x * 2 + sx, mipWidth - 1);
                        const int py = std::min(y * 2 + sy, mipHeight - 1);
                        const int source = (py * mipWidth + px) * 4;
                        for (int channel = 0; channel < 3; ++channel) {
                            const float value = previous[source + channel] / 255.0f;
                            sum[channel] += srgb ? decodeSrgb(value) : value;
                        }
                        sum[3] += previous[source + 3] / 255.0f;
                        ++samples;
                    }
                }
                const int destination = (y * nextWidth + x) * 4;
                if (renormalizeNormal) {
                    float nx = sum[0] / samples * 2.0f - 1.0f;
                    float ny = sum[1] / samples * 2.0f - 1.0f;
                    float nz = sum[2] / samples * 2.0f - 1.0f;
                    const float length = std::sqrt(std::max(nx * nx + ny * ny + nz * nz, 1e-8f));
                    next[destination + 0] = static_cast<quint8>(std::clamp((nx / length + 1.0f) * 127.5f, 0.0f, 255.0f));
                    next[destination + 1] = static_cast<quint8>(std::clamp((ny / length + 1.0f) * 127.5f, 0.0f, 255.0f));
                    next[destination + 2] = static_cast<quint8>(std::clamp((nz / length + 1.0f) * 127.5f, 0.0f, 255.0f));
                } else {
                    for (int channel = 0; channel < 3; ++channel) {
                        const float value = sum[channel] / samples;
                        const float encoded = srgb ? encodeSrgb(value) : value;
                        next[destination + channel] = static_cast<quint8>(std::clamp(encoded * 255.0f, 0.0f, 255.0f));
                    }
                }
                next[destination + 3] = static_cast<quint8>(std::clamp(sum[3] / samples * 255.0f, 0.0f, 255.0f));
            }
        }
        mipData.push_back(std::move(next));
        mipWidth = nextWidth;
        mipHeight = nextHeight;
    }
    return mipData;
}

bool loadLinearTexture(ArtifactCore::GpuContext& context, const QString& path,
                       const char* debugName,
                       Diligent::RefCntAutoPtr<Diligent::ITexture>& texture,
                       Diligent::RefCntAutoPtr<Diligent::ITextureView>& textureSRV,
                       bool renormalizeNormal = false)
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

    QVector<QVector<quint8>> mipData = buildRgba8MipChain(
        rgba8, rawImage.width, rawImage.height, false, renormalizeNormal);

    Diligent::TextureDesc texDesc;
    texDesc.Name = debugName;
    texDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    texDesc.Width = static_cast<Diligent::Uint32>(rawImage.width);
    texDesc.Height = static_cast<Diligent::Uint32>(rawImage.height);
    texDesc.MipLevels = static_cast<Diligent::Uint32>(mipData.size());
    texDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    texDesc.Usage = Diligent::USAGE_IMMUTABLE;
    texDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    QVector<Diligent::TextureSubResData> subResources;
    subResources.resize(mipData.size());
    int levelWidth = rawImage.width;
    for (int level = 0; level < mipData.size(); ++level) {
        subResources[level].pData = mipData[level].constData();
        subResources[level].Stride = static_cast<Diligent::Uint64>(levelWidth * 4);
        levelWidth = std::max(1, levelWidth / 2);
    }
    Diligent::TextureData initData;
    initData.pSubResources = subResources.constData();
    initData.NumSubresources = static_cast<Diligent::Uint32>(subResources.size());
    device->CreateTexture(texDesc, &initData, &texture);
    if (!texture) {
        return false;
    }
    textureSRV = texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    return textureSRV.RawPtr() != nullptr;
}

float rawEnvironmentChannel(const ArtifactCore::RawImage& image,
                            int pixelIndex, int channel)
{
    const int size = image.getPixelTypeSizeInBytes();
    if (size <= 0 || pixelIndex < 0 || channel < 0 || channel >= image.channels)
        return 0.0f;
    const int offset = (pixelIndex * image.channels + channel) * size;
    if (offset < 0 || offset + size > image.data.size()) return 0.0f;
    const auto* bytes = image.data.constData() + offset;
    if (image.pixelType == QStringLiteral("float")) {
        float value = 0.0f;
        std::memcpy(&value, bytes, sizeof(value));
        return std::isfinite(value) ? std::max(value, 0.0f) : 0.0f;
    }
    if (image.pixelType == QStringLiteral("uint16")) {
        std::uint16_t value = 0;
        std::memcpy(&value, bytes, sizeof(value));
        return static_cast<float>(value) / 65535.0f;
    }
    const float value = static_cast<float>(*bytes) / 255.0f;
    if (channel >= 3) {
        return value;
    }
    return value <= 0.04045f
        ? value / 12.92f
        : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

bool createEnvironmentCube(ArtifactCore::GpuContext& context,
                           const QString& path,
                           Diligent::RefCntAutoPtr<Diligent::ITexture>& texture,
                           Diligent::RefCntAutoPtr<Diligent::ITextureView>& view,
                           Diligent::RefCntAutoPtr<Diligent::ITexture>& irradianceTexture,
                           Diligent::RefCntAutoPtr<Diligent::ITextureView>& irradianceView)
{
    auto* device = context.RenderDevice();
    ArtifactCore::ImageImporter importer;
    if (!device || !importer.open(path)) return false;
    const auto image = importer.readImage();
    if (!image.isValid() || image.width < 2 || image.height < 2 || image.channels < 3)
        return false;

    constexpr int faceSize = 64;
    std::vector<float> cube(static_cast<std::size_t>(faceSize) * faceSize * 6 * 4,
                            0.0f);
    auto directionForFace = [](int face, float u, float v) {
        switch (face) {
        case 0: return EnvironmentFloat3{1.0f, -v, -u};
        case 1: return EnvironmentFloat3{-1.0f, -v, u};
        case 2: return EnvironmentFloat3{u, 1.0f, v};
        case 3: return EnvironmentFloat3{u, -1.0f, -v};
        case 4: return EnvironmentFloat3{u, -v, 1.0f};
        default: return EnvironmentFloat3{-u, -v, -1.0f};
        }
    };
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < faceSize; ++y) {
            for (int x = 0; x < faceSize; ++x) {
                const float u = 2.0f * (static_cast<float>(x) + 0.5f) / faceSize - 1.0f;
                const float v = 2.0f * (static_cast<float>(y) + 0.5f) / faceSize - 1.0f;
                auto direction = directionForFace(face, u, v);
                const float length = std::sqrt(direction.x * direction.x +
                                               direction.y * direction.y +
                                               direction.z * direction.z);
                direction.x /= length; direction.y /= length; direction.z /= length;
                const float longitude = std::atan2(direction.z, direction.x);
                const float latitude = std::asin(std::clamp(direction.y, -1.0f, 1.0f));
                const float imageU = (longitude / (2.0f * std::numbers::pi_v<float>) + 0.5f) * image.width - 0.5f;
                const float imageV = (0.5f - latitude / std::numbers::pi_v<float>) * image.height - 0.5f;
                const int x0 = static_cast<int>(std::floor(imageU));
                const int y0 = static_cast<int>(std::floor(imageV));
                const float tx = imageU - x0;
                const float ty = imageV - y0;
                const auto wrapX = [width = image.width](int value) {
                    value %= width;
                    return value < 0 ? value + width : value;
                };
                const auto clampY = [height = image.height](int value) {
                    return std::clamp(value, 0, height - 1);
                };
                const std::size_t destination =
                    (static_cast<std::size_t>(face) * faceSize * faceSize +
                     static_cast<std::size_t>(y) * faceSize + x) * 4;
                for (int channel = 0; channel < 4; ++channel) {
                    float value = 0.0f;
                    for (int dy = 0; dy < 2; ++dy) {
                        for (int dx = 0; dx < 2; ++dx) {
                            const int pixel = clampY(y0 + dy) * image.width + wrapX(x0 + dx);
                            const float weight = (dx == 0 ? 1.0f - tx : tx) *
                                                 (dy == 0 ? 1.0f - ty : ty);
                            value += (channel < 3 || image.channels > channel)
                                ? rawEnvironmentChannel(image, pixel, channel) * weight
                                : weight;
                        }
                    }
                    cube[destination + channel] = channel < 3 || image.channels > 3
                        ? value : 1.0f;
                }
            }
        }
    }

    constexpr int mipCount = 7;
    std::vector<std::vector<float>> mipData;
    mipData.push_back(std::move(cube));
    auto sampleEquirectangular = [&](const EnvironmentFloat3& direction) {
        const float longitude = std::atan2(direction.z, direction.x);
        const float latitude = std::asin(std::clamp(direction.y, -1.0f, 1.0f));
        const float imageU = (longitude / (2.0f * std::numbers::pi_v<float>) + 0.5f) * image.width - 0.5f;
        const float imageV = (0.5f - latitude / std::numbers::pi_v<float>) * image.height - 0.5f;
        const int x0 = static_cast<int>(std::floor(imageU));
        const int y0 = static_cast<int>(std::floor(imageV));
        const float tx = imageU - x0;
        const float ty = imageV - y0;
        const auto wrapX = [width = image.width](int x) {
            x %= width;
            return x < 0 ? x + width : x;
        };
        const auto clampY = [height = image.height](int y) {
            return std::clamp(y, 0, height - 1);
        };
        EnvironmentFloat3 result{0.0f, 0.0f, 0.0f};
        for (int dy = 0; dy < 2; ++dy) {
            for (int dx = 0; dx < 2; ++dx) {
                const int pixel = clampY(y0 + dy) * image.width + wrapX(x0 + dx);
                const float weight = (dx == 0 ? 1.0f - tx : tx) *
                                     (dy == 0 ? 1.0f - ty : ty);
                result.x += rawEnvironmentChannel(image, pixel, 0) * weight;
                result.y += rawEnvironmentChannel(image, pixel, 1) * weight;
                result.z += rawEnvironmentChannel(image, pixel, 2) * weight;
            }
        }
        return result;
    };
    auto radicalInverse = [](std::uint32_t bits) {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    };
    constexpr int prefilterSamples = 32;
    for (int mip = 1; mip < mipCount; ++mip) {
        const int nextSize = std::max(1, faceSize >> mip);
        const float roughness = static_cast<float>(mip) / (mipCount - 1);
        const float alpha = std::max(roughness * roughness, 0.001f);
        std::vector<float> next(static_cast<std::size_t>(nextSize) * nextSize * 6 * 4,
                                0.0f);
        for (int face = 0; face < 6; ++face) {
            for (int y = 0; y < nextSize; ++y) {
                for (int x = 0; x < nextSize; ++x) {
                    const float u = 2.0f * (static_cast<float>(x) + 0.5f) / nextSize - 1.0f;
                    const float v = 2.0f * (static_cast<float>(y) + 0.5f) / nextSize - 1.0f;
                    auto normal = directionForFace(face, u, v);
                    const float normalLength = std::sqrt(normal.x * normal.x +
                                                         normal.y * normal.y +
                                                         normal.z * normal.z);
                    normal.x /= normalLength; normal.y /= normalLength; normal.z /= normalLength;
                    const EnvironmentFloat3 up = std::abs(normal.y) < 0.99f
                        ? EnvironmentFloat3{0.0f, 1.0f, 0.0f}
                        : EnvironmentFloat3{1.0f, 0.0f, 0.0f};
                    auto tangent = EnvironmentFloat3{
                        up.y * normal.z - up.z * normal.y,
                        up.z * normal.x - up.x * normal.z,
                        up.x * normal.y - up.y * normal.x};
                    const float tangentLength = std::sqrt(tangent.x * tangent.x +
                                                           tangent.y * tangent.y +
                                                           tangent.z * tangent.z);
                    tangent.x /= tangentLength; tangent.y /= tangentLength; tangent.z /= tangentLength;
                    const auto bitangent = EnvironmentFloat3{
                        normal.y * tangent.z - normal.z * tangent.y,
                        normal.z * tangent.x - normal.x * tangent.z,
                        normal.x * tangent.y - normal.y * tangent.x};
                    EnvironmentFloat3 sum{0.0f, 0.0f, 0.0f};
                    float weightSum = 0.0f;
                    for (int sample = 0; sample < prefilterSamples; ++sample) {
                        const float xi1 = (static_cast<float>(sample) + 0.5f) /
                                          prefilterSamples;
                        const float xi2 = radicalInverse(static_cast<std::uint32_t>(sample));
                        const float phi = 2.0f * std::numbers::pi_v<float> * xi1;
                        const float cosTheta = std::sqrt(
                            (1.0f - xi2) /
                            (1.0f + (alpha * alpha - 1.0f) * xi2));
                        const float sinTheta = std::sqrt(std::max(
                            0.0f, 1.0f - cosTheta * cosTheta));
                        const EnvironmentFloat3 halfVector{
                            tangent.x * (std::cos(phi) * sinTheta) +
                                bitangent.x * (std::sin(phi) * sinTheta) + normal.x * cosTheta,
                            tangent.y * (std::cos(phi) * sinTheta) +
                                bitangent.y * (std::sin(phi) * sinTheta) + normal.y * cosTheta,
                            tangent.z * (std::cos(phi) * sinTheta) +
                                bitangent.z * (std::sin(phi) * sinTheta) + normal.z * cosTheta};
                        const float normalHalf = std::max(
                            normal.x * halfVector.x + normal.y * halfVector.y +
                            normal.z * halfVector.z, 0.0f);
                        const EnvironmentFloat3 light{
                            2.0f * normalHalf * halfVector.x - normal.x,
                            2.0f * normalHalf * halfVector.y - normal.y,
                            2.0f * normalHalf * halfVector.z - normal.z};
                        const float normalLight = std::max(
                            normal.x * light.x + normal.y * light.y + normal.z * light.z,
                            0.0f);
                        if (normalLight <= 0.0f) continue;
                        const auto radiance = sampleEquirectangular(light);
                        sum.x += radiance.x * normalLight;
                        sum.y += radiance.y * normalLight;
                        sum.z += radiance.z * normalLight;
                        weightSum += normalLight;
                    }
                    const std::size_t destination =
                        (static_cast<std::size_t>(face) * nextSize * nextSize +
                         static_cast<std::size_t>(y) * nextSize + x) * 4;
                    const float inverseWeight = weightSum > 1e-5f
                        ? 1.0f / weightSum : 0.0f;
                    next[destination + 0] = sum.x * inverseWeight;
                    next[destination + 1] = sum.y * inverseWeight;
                    next[destination + 2] = sum.z * inverseWeight;
                    next[destination + 3] = 1.0f;
                }
            }
        }
        mipData.push_back(std::move(next));
    }

    Diligent::TextureDesc desc;
    desc.Name = "MeshRenderer/EnvironmentCubemap";
    desc.Type = Diligent::RESOURCE_DIM_TEX_CUBE;
    desc.Width = faceSize;
    desc.Height = faceSize;
    desc.ArraySize = 6;
    desc.MipLevels = mipCount;
    desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    std::array<Diligent::TextureSubResData, 42> faces{};
    int currentSize = faceSize;
    std::size_t subresourceIndex = 0;
    for (int mip = 0; mip < mipCount; ++mip) {
        const auto& pixels = mipData[static_cast<std::size_t>(mip)];
        for (int face = 0; face < 6; ++face) {
            faces[subresourceIndex].pData = pixels.data() +
                static_cast<std::size_t>(face) * currentSize * currentSize * 4;
            faces[subresourceIndex].Stride =
                static_cast<Diligent::Uint64>(currentSize * 4 * sizeof(float));
            ++subresourceIndex;
        }
        currentSize = std::max(1, currentSize / 2);
    }
    Diligent::TextureData initialData;
    initialData.pSubResources = faces.data();
    initialData.NumSubresources = static_cast<Diligent::Uint32>(subresourceIndex);
    device->CreateTexture(desc, &initialData, &texture);
    if (!texture) return false;
    view = texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);

    // Build a low-resolution diffuse irradiance cube from the original HDR
    // image.  This is intentionally kept separate from the specular cube:
    // diffuse IBL needs a cosine convolution, not merely a low mip average.
    constexpr int irradianceSize = 16;
    constexpr int thetaSamples = 4;
    constexpr int phiSamples = 8;
    std::vector<float> irradiance(
        static_cast<std::size_t>(irradianceSize) * irradianceSize * 6 * 4, 0.0f);
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < irradianceSize; ++y) {
            for (int x = 0; x < irradianceSize; ++x) {
                const float u = 2.0f * (static_cast<float>(x) + 0.5f) / irradianceSize - 1.0f;
                const float v = 2.0f * (static_cast<float>(y) + 0.5f) / irradianceSize - 1.0f;
                auto normal = directionForFace(face, u, v);
                const float normalLength = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                normal.x /= normalLength; normal.y /= normalLength; normal.z /= normalLength;
                const EnvironmentFloat3 up = std::abs(normal.y) < 0.99f
                    ? EnvironmentFloat3{0.0f, 1.0f, 0.0f}
                    : EnvironmentFloat3{1.0f, 0.0f, 0.0f};
                auto tangent = EnvironmentFloat3{up.y * normal.z - up.z * normal.y,
                                                  up.z * normal.x - up.x * normal.z,
                                                  up.x * normal.y - up.y * normal.x};
                const float tangentLength = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
                tangent.x /= tangentLength; tangent.y /= tangentLength; tangent.z /= tangentLength;
                const auto bitangent = EnvironmentFloat3{normal.y * tangent.z - normal.z * tangent.y,
                                                          normal.z * tangent.x - normal.x * tangent.z,
                                                          normal.x * tangent.y - normal.y * tangent.x};
                EnvironmentFloat3 sum{0.0f, 0.0f, 0.0f};
                for (int thetaIndex = 0; thetaIndex < thetaSamples; ++thetaIndex) {
                    const float theta = (static_cast<float>(thetaIndex) + 0.5f) *
                        (0.5f * std::numbers::pi_v<float>) / thetaSamples;
                    const float sinTheta = std::sin(theta);
                    const float cosTheta = std::cos(theta);
                    for (int phiIndex = 0; phiIndex < phiSamples; ++phiIndex) {
                        const float phi = (static_cast<float>(phiIndex) + 0.5f) *
                            (2.0f * std::numbers::pi_v<float>) / phiSamples;
                        const EnvironmentFloat3 sampleDirection{
                            tangent.x * (std::cos(phi) * sinTheta) + bitangent.x * (std::sin(phi) * sinTheta) + normal.x * cosTheta,
                            tangent.y * (std::cos(phi) * sinTheta) + bitangent.y * (std::sin(phi) * sinTheta) + normal.y * cosTheta,
                            tangent.z * (std::cos(phi) * sinTheta) + bitangent.z * (std::sin(phi) * sinTheta) + normal.z * cosTheta};
                        const auto sample = sampleEquirectangular(sampleDirection);
                        const float weight = cosTheta * sinTheta;
                        sum.x += sample.x * weight; sum.y += sample.y * weight; sum.z += sample.z * weight;
                    }
                }
                const float normalization = std::numbers::pi_v<float> / (thetaSamples * phiSamples);
                const std::size_t dst = (static_cast<std::size_t>(face) * irradianceSize * irradianceSize +
                                         static_cast<std::size_t>(y) * irradianceSize + x) * 4;
                irradiance[dst + 0] = sum.x * normalization;
                irradiance[dst + 1] = sum.y * normalization;
                irradiance[dst + 2] = sum.z * normalization;
                irradiance[dst + 3] = 1.0f;
            }
        }
    }
    TextureDesc irradianceDesc;
    irradianceDesc.Name = "MeshRenderer/IrradianceCubemap";
    irradianceDesc.Type = RESOURCE_DIM_TEX_CUBE;
    irradianceDesc.Width = irradianceSize;
    irradianceDesc.Height = irradianceSize;
    irradianceDesc.ArraySize = 6;
    irradianceDesc.MipLevels = 1;
    irradianceDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    irradianceDesc.Usage = USAGE_IMMUTABLE;
    irradianceDesc.BindFlags = BIND_SHADER_RESOURCE;
    TextureSubResData irradianceFaces[6] = {};
    for (auto& faceData : irradianceFaces) {
        faceData.pData = irradiance.data() +
            static_cast<std::size_t>(&faceData - irradianceFaces) * irradianceSize * irradianceSize * 4;
        faceData.Stride = static_cast<Diligent::Uint64>(irradianceSize * 4 * sizeof(float));
    }
    TextureData irradianceData;
    irradianceData.pSubResources = irradianceFaces;
    irradianceData.NumSubresources = 6;
    device->CreateTexture(irradianceDesc, &irradianceData, &irradianceTexture);
    if (irradianceTexture) {
        irradianceView = irradianceTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
    return view.RawPtr() != nullptr && irradianceView.RawPtr() != nullptr;
}

bool createBrdfLut(ArtifactCore::GpuContext& context,
                   Diligent::RefCntAutoPtr<Diligent::ITexture>& texture,
                   Diligent::RefCntAutoPtr<Diligent::ITextureView>& view)
{
    auto* device = context.RenderDevice();
    if (!device) return false;
    constexpr int size = 64;
    constexpr int samples = 64;
    std::vector<float> lut(static_cast<std::size_t>(size) * size * 2, 0.0f);
    auto radicalInverse = [](std::uint32_t bits) {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    };
    for (int y = 0; y < size; ++y) {
        const float roughness = (static_cast<float>(y) + 0.5f) / size;
        const float alpha = roughness * roughness;
        for (int x = 0; x < size; ++x) {
            const float nDotV = (static_cast<float>(x) + 0.5f) / size;
            const float viewX = std::sqrt(std::max(0.0f, 1.0f - nDotV * nDotV));
            float scale = 0.0f;
            float bias = 0.0f;
            for (std::uint32_t sample = 0; sample < samples; ++sample) {
                const float xi1 = (static_cast<float>(sample) + 0.5f) / samples;
                const float xi2 = radicalInverse(sample);
                const float phi = 2.0f * std::numbers::pi_v<float> * xi1;
                const float cosTheta = std::sqrt(
                    (1.0f - xi2) / (1.0f + (alpha * alpha - 1.0f) * xi2));
                const float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
                const EnvironmentFloat3 halfVector{sinTheta * std::cos(phi),
                                                    sinTheta * std::sin(phi), cosTheta};
                const float viewDotHalf = std::max(0.0f,
                    viewX * halfVector.x + nDotV * halfVector.z);
                const float lightZ = 2.0f * viewDotHalf * halfVector.z - nDotV;
                const float lightX = 2.0f * viewDotHalf * halfVector.x - viewX;
                const float nDotL = std::max(0.0f, lightZ);
                if (nDotL <= 0.0f) continue;
                const float geometryV = nDotV / std::max(
                    nDotV * (1.0f - roughness * 0.5f) + roughness * 0.5f, 1e-4f);
                const float geometryL = nDotL / std::max(
                    nDotL * (1.0f - roughness * 0.5f) + roughness * 0.5f, 1e-4f);
                const float visibility = geometryV * geometryL;
                const float fresnel = std::pow(1.0f - viewDotHalf, 5.0f);
                scale += visibility * (1.0f - fresnel);
                bias += visibility * fresnel;
            }
            const std::size_t index = (static_cast<std::size_t>(y) * size + x) * 2;
            lut[index + 0] = scale / samples;
            lut[index + 1] = bias / samples;
        }
    }
    Diligent::TextureDesc desc;
    desc.Name = "MeshRenderer/BRDFLUT";
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = size;
    desc.Height = size;
    desc.MipLevels = 1;
    desc.Format = Diligent::TEX_FORMAT_RG32_FLOAT;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData sub;
    sub.pData = lut.data();
    sub.Stride = static_cast<Diligent::Uint64>(size * 2 * sizeof(float));
    Diligent::TextureData data;
    data.pSubResources = &sub;
    data.NumSubresources = 1;
    device->CreateTexture(desc, &data, &texture);
    if (!texture) return false;
    view = texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    return view.RawPtr() != nullptr;
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
    float4 ShadowPosition : TEXCOORD6;
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

cbuffer ShadowParams : register(b3) {
    float4x4 LightViewProjection;
    float4 ShadowSettings;
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
    Out.ShadowPosition = mul(worldPos, LightViewProjection);
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

// Keep the shadow pass deliberately independent from the material shader.  A
// shadow map records only the closest opaque surface in light clip space.
const char* MeshShadowVSSource = R"(
struct VSInput {
    float3 pos : ATTRIB0;
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

cbuffer ShadowConstants : register(b0) {
    float4x4 LightViewProjection;
};

float4 VSMain(VSInput In, uint InstanceID : SV_InstanceID) : SV_Position {
    return mul(mul(float4(In.pos, 1.0), g_Instances[InstanceID].transform),
               LightViewProjection);
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
    float4 ShadowPosition : TEXCOORD6;
};

Texture2D g_BaseColorTexture : register(t0);
Texture2D g_OpacityTexture : register(t1);
Texture2D g_EmissionTexture : register(t2);
Texture2D g_MetallicRoughnessTexture : register(t3);
Texture2D g_NormalTexture : register(t4);
Texture2D g_OcclusionTexture : register(t5);
Texture2D<float> g_ShadowMap : register(t6);
Texture2D g_GoboTexture0 : register(t7);
Texture2D g_GoboTexture1 : register(t8);
Texture2D g_GoboTexture2 : register(t9);
Texture2D g_GoboTexture3 : register(t10);
Texture2D g_GoboTexture4 : register(t11);
Texture2D g_GoboTexture5 : register(t12);
Texture2D g_GoboTexture6 : register(t13);
Texture2D g_GoboTexture7 : register(t14);
TextureCube g_IrradianceMap : register(t15);
TextureCube g_PrefilteredEnvironment : register(t16);
Texture2D g_BrdfLut : register(t17);
SamplerState g_BaseColorSampler : register(s0);
SamplerComparisonState g_ShadowSampler : register(s1);
SamplerState g_GoboSampler : register(s2);

float4 sampleGobo(uint lightIndex, float2 uv) {
    switch (lightIndex) {
        case 0u: return g_GoboTexture0.Sample(g_GoboSampler, uv);
        case 1u: return g_GoboTexture1.Sample(g_GoboSampler, uv);
        case 2u: return g_GoboTexture2.Sample(g_GoboSampler, uv);
        case 3u: return g_GoboTexture3.Sample(g_GoboSampler, uv);
        case 4u: return g_GoboTexture4.Sample(g_GoboSampler, uv);
        case 5u: return g_GoboTexture5.Sample(g_GoboSampler, uv);
        case 6u: return g_GoboTexture6.Sample(g_GoboSampler, uv);
        default: return g_GoboTexture7.Sample(g_GoboSampler, uv);
    }
}

cbuffer MaterialParams : register(b1) {
    float4 EmissionColorStrength;
    float4 PbrFactors;
    float4 PbrTextureFlags;
    float4 PrincipledFactors;
    float4 ClearcoatFactors;
    float4 AlphaSettings;
};

struct SceneLight {
    float4 PositionType;
    float4 DirectionIntensity;
    float4 ColorAttenuationConstant;
    float4 AttenuationSpot;
    float4 AreaSize;
    float4 GoboInfo;
};

cbuffer SceneLighting : register(b2) {
    SceneLight SceneLights[8];
    uint4 SceneLightingMeta;
    float4 SceneCameraPosition;
};

cbuffer ShadowParams : register(b3) {
    float4x4 LightViewProjection;
    float4 ShadowSettings;
};

float sampleShadowVisibility(float2 shadowUV, float depth) {
    if (ShadowSettings.w <= 0.001) {
        return g_ShadowMap.SampleCmpLevelZero(g_ShadowSampler, shadowUV, depth);
    }
    uint shadowWidth = 1;
    uint shadowHeight = 1;
    g_ShadowMap.GetDimensions(shadowWidth, shadowHeight);
    float2 texel = 1.0 / float2(shadowWidth, shadowHeight);
    float radius = min(2.0, ShadowSettings.w);
    float visibility = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            visibility += g_ShadowMap.SampleCmpLevelZero(
                g_ShadowSampler, shadowUV + float2(x, y) * texel * radius, depth);
        }
    }
    return visibility / 9.0;
}

cbuffer EnvironmentLighting : register(b4) {
    float4 EnvironmentSettings;
};

float3 sampleEnvironmentDiffuse(float3 normal) {
    float angle = EnvironmentSettings.z;
    float2 rotation = float2(cos(angle), sin(angle));
    normal = float3(normal.x * rotation.x - normal.z * rotation.y,
                    normal.y,
                    normal.x * rotation.y + normal.z * rotation.x);
    uint width = 1;
    uint height = 1;
    uint mipCount = 1;
    g_IrradianceMap.GetDimensions(0, width, height, mipCount);
    // Diffuse irradiance is generated as a dedicated cosine-convolved cube;
    // do not substitute the specular prefilter mip chain here.
    return g_IrradianceMap.SampleLevel(
        g_BaseColorSampler, normalize(normal), max(0, mipCount - 1)).rgb;
}

float3 sampleEnvironmentSpecular(float3 normal, float3 viewDirection,
                                 float roughness, float3 F0) {
    float3 reflectionDirection = reflect(-normalize(viewDirection), normalize(normal));
    float angle = EnvironmentSettings.z;
    float2 rotation = float2(cos(angle), sin(angle));
    reflectionDirection = float3(
        reflectionDirection.x * rotation.x - reflectionDirection.z * rotation.y,
        reflectionDirection.y,
        reflectionDirection.x * rotation.y + reflectionDirection.z * rotation.x);
    uint width = 1;
    uint height = 1;
    uint mipCount = 1;
    g_PrefilteredEnvironment.GetDimensions(0, width, height, mipCount);
    float lod = saturate(roughness) * max(0.0, float(mipCount) - 1.0);
    float3 prefiltered = g_PrefilteredEnvironment.SampleLevel(
        g_BaseColorSampler, reflectionDirection, lod).rgb;
    float2 brdf = g_BrdfLut.SampleLevel(
        g_BaseColorSampler,
        float2(saturate(dot(normalize(normal), normalize(viewDirection))),
               saturate(roughness)), 0).rg;
    float3 fresnel = F0 + (1.0 - F0) *
        pow(1.0 - saturate(dot(normalize(normal), normalize(viewDirection))), 5.0);
    return prefiltered * (fresnel * brdf.x + brdf.y);
}

float3 sampleEnvironmentTransmission(float3 normal, float3 viewDirection,
                                     float roughness, float ior,
                                     float3 tint) {
    float3 N = normalize(normal);
    float3 V = normalize(viewDirection);
    float eta = 1.0 / max(ior, 1.001);
    float3 refractionDirection = refract(-V, N, eta);
    if (dot(refractionDirection, refractionDirection) <= 1e-5) {
        refractionDirection = reflect(-V, N);
    }
    float angle = EnvironmentSettings.z;
    float2 rotation = float2(cos(angle), sin(angle));
    refractionDirection = float3(
        refractionDirection.x * rotation.x - refractionDirection.z * rotation.y,
        refractionDirection.y,
        refractionDirection.x * rotation.y + refractionDirection.z * rotation.x);
    uint width = 1;
    uint height = 1;
    uint mipCount = 1;
    g_PrefilteredEnvironment.GetDimensions(0, width, height, mipCount);
    float lod = saturate(roughness) * max(0.0, float(mipCount) - 1.0);
    return g_PrefilteredEnvironment.SampleLevel(
        g_BaseColorSampler, normalize(refractionDirection), lod).rgb * tint;
}

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

float3 srgbToLinear(float3 value) {
    return float3(
        value.r <= 0.04045 ? value.r / 12.92
                           : pow((value.r + 0.055) / 1.055, 2.4),
        value.g <= 0.04045 ? value.g / 12.92
                           : pow((value.g + 0.055) / 1.055, 2.4),
        value.b <= 0.04045 ? value.b / 12.92
                           : pow((value.b + 0.055) / 1.055, 2.4));
}

float4 PSMain(PSInput In) : SV_Target {
    float4 baseSample = g_BaseColorTexture.Sample(g_BaseColorSampler, In.UV);
    float4 opacitySample = g_OpacityTexture.Sample(g_BaseColorSampler, In.UV);
    float4 emissionSample = g_EmissionTexture.Sample(g_BaseColorSampler, In.UV);
    float4 metallicRoughnessSample =
        g_MetallicRoughnessTexture.Sample(g_BaseColorSampler, In.UV);
    float3 normalSample = g_NormalTexture.Sample(g_BaseColorSampler, In.UV).xyz;
    float occlusionSample = g_OcclusionTexture.Sample(g_BaseColorSampler, In.UV).r;
    float3 instanceColor = srgbToLinear(saturate(In.Color.rgb));
    float4 baseColor = float4(baseSample.rgb * instanceColor,
                              baseSample.a * In.Color.a);
    float emissionStrength = max(EmissionColorStrength.a, 0.0);
    float3 emissionTint = srgbToLinear(saturate(EmissionColorStrength.rgb));
    float materialAlpha = baseColor.a * opacitySample.a;
    // Masked materials use the serialized material cutoff; 0.5 remains the
    // default for materials created without an explicit value.
    if (PbrTextureFlags.w > 0.5) {
        clip(materialAlpha - AlphaSettings.x);
    }
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
            emissionSample.rgb * emissionTint * emissionStrength;
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

    float4 solidBase = (In.Mode > 7.5 && In.Mode < 8.5)
        ? float4(instanceColor, In.Color.a) : baseColor;
    float shadowVisibility = 1.0;
    if (ShadowSettings.x > 0.5) {
        float3 shadowNdc = In.ShadowPosition.xyz /
            max(In.ShadowPosition.w, 1e-5);
        float2 shadowUV = shadowNdc.xy * float2(0.5, -0.5) + 0.5;
        if (all(shadowUV >= 0.0) && all(shadowUV <= 1.0) &&
            shadowNdc.z >= 0.0 && shadowNdc.z <= 1.0) {
            shadowVisibility = sampleShadowVisibility(
                shadowUV, shadowNdc.z - ShadowSettings.y);
        }
    }
    if (SceneLightingMeta.x > 0 || EnvironmentSettings.x > 0.5) {
        float3 cameraDelta = SceneCameraPosition.xyz - In.WorldPosition;
        float3 viewDirection = cameraDelta *
            rsqrt(max(dot(cameraDelta, cameraDelta), 1e-8));
        float3 directColor = float3(0.0, 0.0, 0.0);
        // Retain the no-light studio rig's base fill when scene lights are
        // present. A light augments the viewport shading; it must not replace
        // the baseline illumination and leave every unlit-facing surface
        // nearly black.
        float3 ambientColor = solidBase.rgb * (0.24 * ao);
        float dielectricF0 = pow((max(PrincipledFactors.y, 1.0) - 1.0) /
                                 (max(PrincipledFactors.y, 1.0) + 1.0), 2.0);
        float3 F0 = lerp(float3(dielectricF0, dielectricF0, dielectricF0),
                         solidBase.rgb, metallic);
        F0 *= lerp(1.0, saturate(PrincipledFactors.x), 1.0 - metallic);
        float transmissionWeight = saturate(PrincipledFactors.z) *
            (1.0 - metallic);
        float transmissionIor = max(PrincipledFactors.y, 1.0);
        float transmissionF0 = pow((transmissionIor - 1.0) /
                                   (transmissionIor + 1.0), 2.0);
        float transmissionFresnel = transmissionF0 +
            (1.0 - transmissionF0) * pow(
                1.0 - saturate(dot(worldNormal, viewDirection)), 5.0);
        float3 transmissionDirection = refract(
            -viewDirection, worldNormal, 1.0 / transmissionIor);
        if (dot(transmissionDirection, transmissionDirection) <= 1e-5) {
            transmissionFresnel = 1.0;
        }
        if (EnvironmentSettings.x > 0.5) {
            float3 indirectDiffuse = sampleEnvironmentDiffuse(worldNormal) *
                solidBase.rgb * (1.0 - metallic) * ao;
            float sheenWeight = saturate(PrincipledFactors.w) *
                (1.0 - metallic) * (1.0 - saturate(PrincipledFactors.z));
            indirectDiffuse += sheenWeight * pow(
                1.0 - saturate(dot(worldNormal, viewDirection)), 5.0) *
                sampleEnvironmentDiffuse(worldNormal) * ao;
            float3 indirectSpecular = sampleEnvironmentSpecular(
                worldNormal, viewDirection, roughness, F0);
            float coatWeight = saturate(ClearcoatFactors.x);
            if (coatWeight > 0.0) {
                float coatRoughness = clamp(ClearcoatFactors.y, 0.04, 1.0);
                float3 coatF0 = float3(0.04, 0.04, 0.04);
                float3 coatReflection = sampleEnvironmentSpecular(
                    worldNormal, viewDirection, coatRoughness, coatF0);
                float coatNdotV = saturate(dot(worldNormal, viewDirection));
                float coatFresnel = 0.04 + (1.0 - 0.04) *
                    pow(1.0 - coatNdotV, 5.0);
                indirectDiffuse *= 1.0 - coatWeight * coatFresnel;
                indirectSpecular = indirectSpecular *
                    (1.0 - coatWeight * coatFresnel) +
                    coatReflection * coatWeight;
            }
            float3 transmittedEnvironment = sampleEnvironmentTransmission(
                worldNormal, viewDirection, roughness,
                max(PrincipledFactors.y, 1.0), solidBase.rgb) * transmissionWeight;
            transmittedEnvironment *= 1.0 - transmissionFresnel;
            float specularOcclusion = saturate(
                pow(saturate(dot(worldNormal, viewDirection)) + ao, 2.0) -
                1.0 + ao);
            indirectSpecular *= specularOcclusion;
            transmittedEnvironment *= ao;
            ambientColor += (indirectDiffuse + indirectSpecular +
                             transmittedEnvironment) *
                max(EnvironmentSettings.y, 0.0);
        }
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
                    if (light.GoboInfo.w > 0.5) {
                        float3 goboAxis = normalize(light.DirectionIntensity.xyz);
                        float3 goboUp = abs(goboAxis.y) > 0.98
                            ? float3(0.0, 0.0, 1.0)
                            : float3(0.0, 1.0, 0.0);
                        float3 goboRight = normalize(cross(goboUp, goboAxis));
                        goboUp = normalize(cross(goboAxis, goboRight));
                        float3 fromLight = In.WorldPosition - light.PositionType.xyz;
                        float goboDepth = dot(fromLight, goboAxis);
                        float goboRadius = max(goboDepth *
                            tan(acos(clamp(outerCos, -0.999, 0.999))), 1e-5);
                        float2 goboUV = float2(dot(fromLight, goboRight),
                                               dot(fromLight, goboUp)) /
                                        (2.0 * goboRadius) + 0.5;
                        float goboSin = sin(light.GoboInfo.y);
                        float goboCos = cos(light.GoboInfo.y);
                        float2 centeredUV = goboUV - 0.5;
                        goboUV = float2(centeredUV.x * goboCos - centeredUV.y * goboSin,
                                        centeredUV.x * goboSin + centeredUV.y * goboCos) + 0.5;
                        if (goboDepth > 0.0 && all(goboUV >= 0.0) &&
                            all(goboUV <= 1.0)) {
                            float4 gobo = sampleGobo(lightIndex, goboUV);
                            float3 goboColor = gobo.rgb * gobo.a;
                            if (light.GoboInfo.z > 0.5) {
                                goboColor = 1.0 - goboColor;
                            }
                            radiance *= lerp(float3(1.0, 1.0, 1.0), goboColor,
                                             saturate(light.GoboInfo.x));
                        } else {
                            attenuation = 0.0;
                        }
                    }
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
            float coatWeight = saturate(ClearcoatFactors.x);
            float coatFresnel = 0.04 + (1.0 - 0.04) * pow(1.0 - VdotH, 5.0);
            float coatEnergy = 1.0 - coatWeight * coatFresnel;
            float3 diffuse = (1.0 - fresnel) * (1.0 - metallic) *
                             (1.0 - saturate(PrincipledFactors.z)) *
                             solidBase.rgb / 3.14159265 * coatEnergy;
            float sheenWeight = saturate(PrincipledFactors.w) *
                (1.0 - metallic) * (1.0 - saturate(PrincipledFactors.z));
            float sheen = sheenWeight * pow(1.0 - NdotV, 5.0) /
                          3.14159265;
            specular *= coatEnergy;
            float coatRoughness = clamp(ClearcoatFactors.y, 0.04, 1.0);
            float coatAlpha = coatRoughness * coatRoughness;
            float coatAlpha2 = coatAlpha * coatAlpha;
            float coatDenom = NdotH * NdotH * (coatAlpha2 - 1.0) + 1.0;
            float coatDistribution = coatAlpha2 /
                max(3.14159265 * coatDenom * coatDenom, 1e-4);
            float coatSpecular = coatDistribution * geometryV * geometryL /
                max(4.0 * NdotV * NdotL, 1e-4);
            specular += coatSpecular * coatWeight * coatFresnel;
            float lightShadow = (ShadowSettings.x > 0.5 &&
                                 lightIndex == (uint)ShadowSettings.z)
                                    ? shadowVisibility
                                    : 1.0;
            directColor += (diffuse + specular + sheen) * radiance *
                           NdotL * attenuation * lightShadow;
            float backLight = saturate(dot(-worldNormal, lightDirection));
            directColor += solidBase.rgb * transmissionWeight * radiance *
                           backLight * (1.0 - transmissionFresnel) *
                           attenuation * lightShadow;
        }
        float3 emissionColor = emissionSample.rgb * emissionTint *
                               emissionStrength;
        return float4(directColor + ambientColor + emissionColor,
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
    float3 studioEmission = emissionSample.rgb * emissionTint *
                            emissionStrength;
    float3 studioHdr = (solidBase.rgb * studioLight * cavity +
                        rimColor * rim * 0.10 +
                        lerp(float3(1.0, 1.0, 1.0), solidBase.rgb,
                             metallic) * studioSpecular) * ao +
                       studioEmission;
    float3 studioToneMapped = studioHdr / (studioHdr + 1.0);
    float3 litColor = pow(max(studioToneMapped, 0.0), 1.0 / 2.2);
    return float4(litColor, solidBase.a * opacitySample.a);
}
)";

// Minimal mesh-shader path for the packed meshlet buffers.  One dispatched
// group renders one meshlet; the index stream is expanded into local output
// vertices so this path does not depend on a second local-vertex remap buffer.
const char* MeshletMSSource = R"(
struct MeshletGpu {
    uint indexOffset;
    uint indexCount;
    uint vertexOffset;
    uint vertexCount;
    float3 boundsCenter;
    float boundsRadius;
};

StructuredBuffer<MeshletGpu> g_Meshlets : register(t0);
StructuredBuffer<uint> g_MeshletIndices : register(t1);
StructuredBuffer<float3> g_Positions : register(t2);

cbuffer MeshletConstants : register(b0) {
    float4x4 g_View;
    float4x4 g_Projection;
    float4x4 g_Model;
    uint g_MeshletBase;
    uint3 g_MeshletPadding;
};

struct MSVertex {
    float4 position : SV_Position;
};

[outputtopology("triangle")]
[numthreads(32, 1, 1)]
void MSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex,
            out vertices MSVertex vertices[192],
            out indices uint3 triangles[64]) {
    MeshletGpu meshlet = g_Meshlets[g_MeshletBase + groupId.x];
    uint triangleCount = min(meshlet.indexCount / 3u, 64u);
    SetMeshOutputCounts(min(meshlet.indexCount, 192u), triangleCount);

    for (uint i = groupIndex; i < meshlet.indexCount && i < 192u; i += 32u) {
        uint sourceIndex = g_MeshletIndices[meshlet.indexOffset + i];
        float4 worldPosition = mul(float4(g_Positions[sourceIndex], 1.0), g_Model);
        vertices[i].position = mul(mul(worldPosition, g_View), g_Projection);
    }

    for (uint i = groupIndex; i < triangleCount; i += 32u) {
        uint baseIndex = i * 3u;
        triangles[i] = uint3(baseIndex, baseIndex + 1u, baseIndex + 2u);
    }
}
)";

const char* MeshletPSSource = R"(
float4 PSMain() : SV_Target {
    return float4(1.0, 1.0, 1.0, 1.0);
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
        float goboInfo[4] = {};
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
        float principledFactors[4] = {0.5f, 1.5f, 0.0f, 0.0f};
        float clearcoatFactors[4] = {0.0f, 0.2f, 0.0f, 0.0f};
        float alphaSettings[4] = {0.5f, 0.0f, 0.0f, 0.0f};
    };

    struct ShadowParamsConstants {
        float lightViewProjection[16] = {};
        float shadowSettings[4] = {};
    };

    struct EnvironmentConstants {
        float settings[4] = {};
    };

    struct MeshletConstants {
        float view[16] = {};
        float projection[16] = {};
        float model[16] = {};
        std::uint32_t meshletBase = 0;
        std::uint32_t padding[3] = {};
    };

    static_assert(sizeof(SceneLightGpu) == sizeof(float) * 24);
    static_assert(sizeof(SceneLightingConstants) ==
                  sizeof(SceneLightGpu) * MaxSceneLights + sizeof(float) * 8);
    static_assert(sizeof(MaterialConstants) == sizeof(float) * 24);
    static_assert(sizeof(MeshletConstants) == sizeof(float) * 52);

    Diligent::RefCntAutoPtr<Diligent::IPipelineStateCache>    pPSOCache_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pPSO_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> pSRB_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pTransparentPSO_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> pTransparentSRB_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pMeshletPSO_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> pMeshletSRB_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pMeshletBufferSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pMeshletIndexBufferSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pMeshletLodBufferSRV_;
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
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pMeshletBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pMeshletIndexBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pMeshletLodBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pMeshletConstantsBuffer_;
    std::vector<MeshRenderer::MeshletLodGpu>                  meshletLods_;
    size_t                                                     meshletCount_ = 0;
    float meshletView_[16] = {};
    float meshletProjection_[16] = {};
    float meshletModel_[16] = {};
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
    Diligent::RefCntAutoPtr<Diligent::ISampler>               pShadowSampler_;
    Diligent::RefCntAutoPtr<Diligent::ISampler>               pGoboSampler_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pShadowMapSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pShadowFallbackTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pShadowFallbackSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pIrradianceMapSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pPrefilteredEnvironmentSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pBrdfLutSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>                pEnvironmentFallbackTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>                pEnvironmentTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>                pIrradianceTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>                pBrdfLutTexture_;
    std::array<Diligent::RefCntAutoPtr<Diligent::ITexture>, MaxSceneLights>
        pGoboTextures_;
    std::array<Diligent::RefCntAutoPtr<Diligent::ITextureView>, MaxSceneLights>
        pGoboTextureSRVs_;
    std::array<QString, MaxSceneLights> goboTexturePaths_;
    
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
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pShadowParamsBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pEnvironmentBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pShadowConstantsBuffer_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         pShadowPSO_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> pShadowSRB_;
    bool shadowPrepared_ = false;
    
    size_t vertexCount_ = 0;
    size_t indexCount_ = 0;
    size_t maxInstances_ = 0;
    QString baseColorTexturePath_;
    QString opacityTexturePath_;
    QString emissionTexturePath_;
    QString metallicRoughnessTexturePath_;
    QString normalTexturePath_;
    QString occlusionTexturePath_;
    QString environmentMapPath_;
    MaterialConstants materialConstants_;
    SceneLightingConstants sceneLighting_;
    ShadowParamsConstants shadowParams_;
    EnvironmentConstants environmentConstants_;
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
    std::copy(std::begin(identity), std::end(identity), pImpl_->meshletView_);
    std::copy(std::begin(identity), std::end(identity), pImpl_->meshletProjection_);
    std::copy(std::begin(identity), std::end(identity), pImpl_->meshletModel_);
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
    pImpl_->pMeshletBuffer_.Release();
    pImpl_->pMeshletIndexBuffer_.Release();
    pImpl_->pMeshletLodBuffer_.Release();
    pImpl_->pMeshletConstantsBuffer_.Release();
    pImpl_->meshletLods_.clear();
    pImpl_->meshletCount_ = 0;
    pImpl_->pInstanceBuffer_.Release();
    pImpl_->pCompactedInstanceBuffer_.Release();
    pImpl_->pIndirectArgsBuffer_.Release();
    pImpl_->pCullConstantsBuffer_.Release();
    pImpl_->pConstantBuffer_.Release();
    pImpl_->pMaterialBuffer_.Release();
    pImpl_->pSceneLightingBuffer_.Release();
    pImpl_->pShadowParamsBuffer_.Release();
    pImpl_->pEnvironmentBuffer_.Release();
    pImpl_->pEnvironmentFallbackTexture_.Release();
    pImpl_->pEnvironmentTexture_.Release();
    pImpl_->pIrradianceTexture_.Release();
    pImpl_->pBrdfLutTexture_.Release();
    pImpl_->pShadowConstantsBuffer_.Release();
    pImpl_->pShadowPSO_.Release();
    pImpl_->pShadowSRB_.Release();
    // Cached SRBs contain static bindings to the constant buffers above.
    // They must not survive a buffer rebuild.
    pImpl_->pipelineSets_.clear();

    pImpl_->gpuCullReady_ = false;
    pImpl_->gpuCullActive_ = false;
    const auto& deviceInfo = pDevice->GetDeviceInfo();
    const auto& adapterInfo = pDevice->GetAdapterInfo();
    const bool rayTracingSupported =
        deviceInfo.Features.RayTracing != DEVICE_FEATURE_STATE_DISABLED &&
        (adapterInfo.RayTracing.CapFlags &
         RAY_TRACING_CAP_FLAG_STANDALONE_SHADERS) != 0;
    const auto vertexBindFlags = BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE |
        (rayTracingSupported ? BIND_RAY_TRACING : BIND_NONE);
    const auto indexBindFlags = BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE |
        (rayTracingSupported ? BIND_RAY_TRACING : BIND_NONE);
    
    // 1. Position buffer (always needed)
    if (vertexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Position Buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.Size              = sizeof(float) * 3 * vertexCount_;
        BuffDesc.BindFlags         = vertexBindFlags;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pPositionBuffer_);
    }
    
    // 2. Normal buffer
    if (vertexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Normal Buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.Size              = sizeof(float) * 3 * vertexCount_;
        BuffDesc.BindFlags         = vertexBindFlags;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pNormalBuffer_);
    }
    // 3. UV buffer
    if (vertexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh UV Buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.Size              = sizeof(float) * 2 * vertexCount_;
        BuffDesc.BindFlags         = vertexBindFlags;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pUVBuffer_);
    }
    
    // 4. Index buffer (if indexed rendering)
    if (indexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Index Buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.Size              = sizeof(uint32_t) * indexCount_;
        BuffDesc.BindFlags         = indexBindFlags;
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
    {
        BufferDesc BuffDesc;
        BuffDesc.Name = "Meshlet Constants";
        BuffDesc.Usage = USAGE_DYNAMIC;
        BuffDesc.Size = sizeof(Impl::MeshletConstants);
        BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pMeshletConstantsBuffer_);
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
    {
        BufferDesc BuffDesc;
        BuffDesc.Name = "Mesh Shadow Constants CB";
        BuffDesc.Usage = USAGE_DYNAMIC;
        BuffDesc.Size = sizeof(float) * 16;
        BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pShadowConstantsBuffer_);
    }
    {
        BufferDesc BuffDesc;
        BuffDesc.Name = "Mesh Shadow Parameters CB";
        BuffDesc.Usage = USAGE_DYNAMIC;
        BuffDesc.Size = sizeof(Impl::ShadowParamsConstants);
        BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pShadowParamsBuffer_);
    }
    {
        BufferDesc BuffDesc;
        BuffDesc.Name = "Mesh Environment Lighting CB";
        BuffDesc.Usage = USAGE_DYNAMIC;
        BuffDesc.Size = sizeof(Impl::EnvironmentConstants);
        BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pEnvironmentBuffer_);
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
    if (!pImpl_->pShadowSampler_) {
        SamplerDesc samplerDesc;
        samplerDesc.MinFilter = FILTER_TYPE_COMPARISON_LINEAR;
        samplerDesc.MagFilter = FILTER_TYPE_COMPARISON_LINEAR;
        samplerDesc.MipFilter = FILTER_TYPE_COMPARISON_LINEAR;
        samplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = COMPARISON_FUNC_LESS_EQUAL;
        pDevice->CreateSampler(samplerDesc, &pImpl_->pShadowSampler_);
    }
    if (!pImpl_->pGoboSampler_) {
        SamplerDesc samplerDesc;
        samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
        samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
        samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
        samplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = COMPARISON_FUNC_ALWAYS;
        pDevice->CreateSampler(samplerDesc, &pImpl_->pGoboSampler_);
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
    if (!pImpl_->pShadowFallbackTexture_) {
        const float litDepth = 1.0f;
        TextureDesc texDesc;
        texDesc.Name = "MeshRenderer_LitShadowFallback";
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = TEX_FORMAT_R32_FLOAT;
        texDesc.Usage = USAGE_IMMUTABLE;
        texDesc.BindFlags = BIND_SHADER_RESOURCE;
        TextureSubResData subRes;
        subRes.pData = &litDepth;
        subRes.Stride = sizeof(litDepth);
        TextureData initData;
        initData.pSubResources = &subRes;
        initData.NumSubresources = 1;
        pDevice->CreateTexture(texDesc, &initData, &pImpl_->pShadowFallbackTexture_);
        if (pImpl_->pShadowFallbackTexture_) {
            pImpl_->pShadowFallbackSRV_ = pImpl_->pShadowFallbackTexture_
                ->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }
    }
    if (!pImpl_->pShadowMapSRV_) {
        pImpl_->pShadowMapSRV_ = pImpl_->pShadowFallbackSRV_;
    }
    if (!pImpl_->pEnvironmentFallbackTexture_) {
        const float neutralFace[4] = {0.18f, 0.20f, 0.24f, 1.0f};
        TextureDesc envDesc;
        envDesc.Name = "MeshRenderer_NeutralEnvironmentFallback";
        envDesc.Type = RESOURCE_DIM_TEX_CUBE;
        envDesc.Width = 1;
        envDesc.Height = 1;
        envDesc.ArraySize = 6;
        envDesc.MipLevels = 1;
        envDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
        envDesc.Usage = USAGE_IMMUTABLE;
        envDesc.BindFlags = BIND_SHADER_RESOURCE;
        TextureSubResData faces[6] = {};
        for (auto& face : faces) {
            face.pData = neutralFace;
            face.Stride = sizeof(neutralFace);
        }
        TextureData envData;
        envData.pSubResources = faces;
        envData.NumSubresources = 6;
        pDevice->CreateTexture(envDesc, &envData,
                               &pImpl_->pEnvironmentFallbackTexture_);
        if (pImpl_->pEnvironmentFallbackTexture_) {
            auto view = pImpl_->pEnvironmentFallbackTexture_->GetDefaultView(
                TEXTURE_VIEW_SHADER_RESOURCE);
            pImpl_->pIrradianceMapSRV_ = view;
            pImpl_->pPrefilteredEnvironmentSRV_ = view;
            pImpl_->pBrdfLutSRV_ = pImpl_->pLinearWhiteTextureSRV_;
        }
    }

    // Diligent output parameters must be empty. createPSO() is also called
    // when the active render-target format changes, so release the currently
    // selected set before writing the new references.
    pImpl_->pSRB_.Release();
    pImpl_->pTransparentSRB_.Release();
    pImpl_->pMeshletSRB_.Release();
    pImpl_->pPSO_.Release();
    pImpl_->pTransparentPSO_.Release();
    pImpl_->pMeshletPSO_.Release();

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

    // Rasterizer: disable backface cull which hid meshes with mismatched winding (gizmo lines unaffected)
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
    
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
    
    static std::array<ShaderResourceVariableDesc, 20> Vars = {{
        {SHADER_TYPE_VERTEX, "g_Instances", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_BaseColorTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_OpacityTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_EmissionTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_MetallicRoughnessTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_NormalTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_OcclusionTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_GoboTexture0", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_GoboTexture1", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_GoboTexture2", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_GoboTexture3", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_GoboTexture4", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_GoboTexture5", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_GoboTexture6", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_GoboTexture7", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_IrradianceMap", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_PrefilteredEnvironment", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_BrdfLut", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
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

    // Mesh-shader PSO. This is deliberately a separate pipeline: mesh
    // pipelines cannot have a vertex input layout or a vertex shader.
    GraphicsPipelineStateCreateInfo meshPSOInfo;
    meshPSOInfo.PSODesc.Name = "Meshlet LOD Mesh Shader PSO";
    meshPSOInfo.PSODesc.PipelineType = PIPELINE_TYPE_MESH;
    meshPSOInfo.pPSOCache = pImpl_->pPSOCache_.RawPtr();
    meshPSOInfo.GraphicsPipeline.NumRenderTargets = 1;
    meshPSOInfo.GraphicsPipeline.RTVFormats[0] = renderTargetFormat_;
    meshPSOInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
    meshPSOInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    meshPSOInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
    meshPSOInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
    meshPSOInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;
    auto& meshRT = meshPSOInfo.GraphicsPipeline.BlendDesc.RenderTargets[0];
    meshRT.BlendEnable = true;
    meshRT.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    meshRT.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    meshRT.BlendOp = BLEND_OPERATION_ADD;

    RefCntAutoPtr<IShader> meshShader;
    RefCntAutoPtr<IShader> meshPixelShader;
    if (context_.CompileShader(MeshletMSSource, SHADER_TYPE_MESH,
                               "MSMain", &meshShader) &&
        context_.CompileShader(MeshletPSSource, SHADER_TYPE_PIXEL,
                               "PSMain", &meshPixelShader)) {
        meshPSOInfo.pMS = meshShader;
        meshPSOInfo.pPS = meshPixelShader;
        std::array<ShaderResourceVariableDesc, 4> meshVars = {{
            {SHADER_TYPE_MESH, "g_Meshlets", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_MESH, "g_MeshletIndices", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_MESH, "g_Positions", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_MESH, "MeshletConstants", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
        }};
        meshPSOInfo.PSODesc.ResourceLayout.DefaultVariableType =
            SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
        meshPSOInfo.PSODesc.ResourceLayout.Variables = meshVars.data();
        meshPSOInfo.PSODesc.ResourceLayout.NumVariables =
            static_cast<Uint32>(meshVars.size());
        pDevice->CreateGraphicsPipelineState(meshPSOInfo,
                                              &pImpl_->pMeshletPSO_);
        if (pImpl_->pMeshletPSO_) {
            pImpl_->pMeshletPSO_->CreateShaderResourceBinding(
                &pImpl_->pMeshletSRB_, true);
        }
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
        if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "ShadowParams")) {
            var->Set(pImpl_->pShadowParamsBuffer_);
        }
        if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "ShadowParams")) {
            var->Set(pImpl_->pShadowParamsBuffer_);
        }
        if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "EnvironmentLighting")) {
            var->Set(pImpl_->pEnvironmentBuffer_);
        }
        if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_BaseColorSampler")) {
            var->Set(pImpl_->pBaseColorSampler_);
        }
        if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_ShadowSampler")) {
            var->Set(pImpl_->pShadowSampler_);
        }
        if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_GoboSampler")) {
            var->Set(pImpl_->pGoboSampler_);
        }
        pso->CreateShaderResourceBinding(&srb, true);
    };
    initializeBindings(pImpl_->pPSO_, pImpl_->pSRB_);
    initializeBindings(pImpl_->pTransparentPSO_, pImpl_->pTransparentSRB_);

    // Depth-only pipeline used by the composition renderer's shadow-map
    // prepass.  It intentionally has no colour attachment or pixel shader.
    pImpl_->pShadowPSO_.Release();
    pImpl_->pShadowSRB_.Release();
    GraphicsPipelineStateCreateInfo shadowPSOInfo;
    shadowPSOInfo.PSODesc.Name = "Mesh Shadow Depth PSO";
    shadowPSOInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    shadowPSOInfo.pPSOCache = pImpl_->pPSOCache_.RawPtr();
    shadowPSOInfo.GraphicsPipeline.PrimitiveTopology =
        PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    shadowPSOInfo.GraphicsPipeline.NumRenderTargets = 0;
    shadowPSOInfo.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_UNKNOWN;
    shadowPSOInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
    shadowPSOInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
    shadowPSOInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
    shadowPSOInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc =
        COMPARISON_FUNC_LESS;
    std::array<LayoutElement, 1> shadowLayout;
    shadowLayout[0].HLSLSemantic = "ATTRIB";
    shadowLayout[0].InputIndex = 0;
    shadowLayout[0].BufferSlot = 0;
    shadowLayout[0].NumComponents = 3;
    shadowLayout[0].ValueType = VT_FLOAT32;
    shadowPSOInfo.GraphicsPipeline.InputLayout.NumElements = 1;
    shadowPSOInfo.GraphicsPipeline.InputLayout.LayoutElements = shadowLayout.data();
    RefCntAutoPtr<IShader> shadowVS;
    context_.CompileShader(MeshShadowVSSource, SHADER_TYPE_VERTEX, "VSMain",
                           &shadowVS);
    shadowPSOInfo.pVS = shadowVS;
    shadowPSOInfo.PSODesc.ResourceLayout.DefaultVariableType =
        SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    static std::array<ShaderResourceVariableDesc, 1> shadowVars = {{
        {SHADER_TYPE_VERTEX, "g_Instances", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    }};
    shadowPSOInfo.PSODesc.ResourceLayout.Variables = shadowVars.data();
    shadowPSOInfo.PSODesc.ResourceLayout.NumVariables =
        static_cast<Uint32>(shadowVars.size());
    pDevice->CreateGraphicsPipelineState(shadowPSOInfo, &pImpl_->pShadowPSO_);
    if (pImpl_->pShadowPSO_) {
        if (auto* var = pImpl_->pShadowPSO_->GetStaticVariableByName(
                SHADER_TYPE_VERTEX, "ShadowConstants")) {
            var->Set(pImpl_->pShadowConstantsBuffer_);
        }
        pImpl_->pShadowPSO_->CreateShaderResourceBinding(&pImpl_->pShadowSRB_,
                                                          true);
    } else {
        qWarning("[MeshRenderer] shadow depth PSO creation failed");
    }
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

void MeshRenderer::updateMeshletGeometry(
    const MeshletGpu* meshlets, size_t meshletCount,
    const uint32_t* indices, size_t indexCount,
    const MeshletLodGpu* lods, size_t lodCount)
{
    auto* device = context_.RenderDevice();
    if (!device) {
        qWarning("[MeshRenderer] updateMeshletGeometry skipped because device is unavailable");
        return;
    }

    pImpl_->pMeshletBuffer_.Release();
    pImpl_->pMeshletIndexBuffer_.Release();
    pImpl_->pMeshletLodBuffer_.Release();

    const auto createStructured = [device](const char* name, const void* data,
                                            size_t size, size_t stride,
                                            RefCntAutoPtr<IBuffer>& out) {
        if (!data || size == 0 || stride == 0) {
            return;
        }
        BufferDesc desc;
        desc.Name = name;
        desc.Usage = USAGE_DEFAULT;
        desc.Size = static_cast<Uint32>(size);
        desc.BindFlags = BIND_SHADER_RESOURCE;
        desc.Mode = BUFFER_MODE_STRUCTURED;
        desc.ElementByteStride = static_cast<Uint32>(stride);
        BufferData initData;
        initData.pData = data;
        initData.DataSize = size;
        device->CreateBuffer(desc, &initData, &out);
    };

    createStructured("Meshlet Metadata Buffer", meshlets,
                     sizeof(MeshletGpu) * meshletCount, sizeof(MeshletGpu),
                     pImpl_->pMeshletBuffer_);
    createStructured("Meshlet Index Buffer", indices,
                     sizeof(uint32_t) * indexCount, sizeof(uint32_t),
                     pImpl_->pMeshletIndexBuffer_);
    createStructured("Meshlet LOD Buffer", lods,
                     sizeof(MeshletLodGpu) * lodCount, sizeof(MeshletLodGpu),
                     pImpl_->pMeshletLodBuffer_);
    pImpl_->meshletCount_ = meshletCount;
    if (lods && lodCount > 0) {
        pImpl_->meshletLods_.assign(lods, lods + lodCount);
    }
}

void MeshRenderer::prepareMeshShader(IDeviceContext* pContext, size_t lodIndex)
{
    if (!pContext || !pImpl_->pMeshletPSO_ || !pImpl_->pMeshletSRB_ ||
        !pImpl_->pMeshletBuffer_ || !pImpl_->pMeshletIndexBuffer_ ||
        !pImpl_->pPositionBuffer_ || pImpl_->meshletCount_ == 0) {
        return;
    }
    if (lodIndex >= pImpl_->meshletLods_.size()) {
        lodIndex = 0;
    }
    const auto& lod = pImpl_->meshletLods_.empty()
        ? MeshletLodGpu{}
        : pImpl_->meshletLods_[lodIndex];
    auto* meshVar = pImpl_->pMeshletSRB_->GetVariableByName(
        SHADER_TYPE_MESH, "g_Meshlets");
    auto* indexVar = pImpl_->pMeshletSRB_->GetVariableByName(
        SHADER_TYPE_MESH, "g_MeshletIndices");
    auto* positionVar = pImpl_->pMeshletSRB_->GetVariableByName(
        SHADER_TYPE_MESH, "g_Positions");
    auto* constantsVar = pImpl_->pMeshletSRB_->GetVariableByName(
        SHADER_TYPE_MESH, "MeshletConstants");
    if (!meshVar || !indexVar || !positionVar || !constantsVar ||
        !pImpl_->pMeshletConstantsBuffer_) {
        return;
    }
    meshVar->Set(pImpl_->pMeshletBuffer_->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    indexVar->Set(pImpl_->pMeshletIndexBuffer_->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    positionVar->Set(pImpl_->pPositionBuffer_->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    MeshRenderer::Impl::MeshletConstants constants;
    std::memcpy(constants.view, pImpl_->meshletView_, sizeof(constants.view));
    std::memcpy(constants.projection, pImpl_->meshletProjection_, sizeof(constants.projection));
    std::memcpy(constants.model, pImpl_->meshletModel_, sizeof(constants.model));
    constants.meshletBase = lod.meshletOffset;
    pContext->UpdateBuffer(pImpl_->pMeshletConstantsBuffer_, 0,
                           sizeof(constants), &constants,
                           RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    constantsVar->Set(pImpl_->pMeshletConstantsBuffer_);
    pContext->SetPipelineState(pImpl_->pMeshletPSO_);
    pContext->CommitShaderResources(pImpl_->pMeshletSRB_,
                                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    (void)lod;
}

void MeshRenderer::setMeshletMatrices(const float* viewMatrix,
                                      const float* projectionMatrix,
                                      const float* modelMatrix)
{
    if (viewMatrix) {
        std::memcpy(pImpl_->meshletView_, viewMatrix, sizeof(pImpl_->meshletView_));
    }
    if (projectionMatrix) {
        std::memcpy(pImpl_->meshletProjection_, projectionMatrix,
                    sizeof(pImpl_->meshletProjection_));
    }
    if (modelMatrix) {
        std::memcpy(pImpl_->meshletModel_, modelMatrix, sizeof(pImpl_->meshletModel_));
    }
}

void MeshRenderer::drawMeshlets(IDeviceContext* pContext, size_t lodIndex)
{
    if (!pContext || !pImpl_->pMeshletPSO_ || pImpl_->meshletCount_ == 0) {
        return;
    }
    size_t offset = 0;
    size_t count = pImpl_->meshletCount_;
    if (lodIndex < pImpl_->meshletLods_.size()) {
        offset = pImpl_->meshletLods_[lodIndex].meshletOffset;
        count = pImpl_->meshletLods_[lodIndex].meshletCount;
    }
    if (count == 0) {
        return;
    }
    DrawMeshAttribs drawAttrs;
    drawAttrs.ThreadGroupCountX = static_cast<Uint32>(count);
    pContext->DrawMesh(drawAttrs);
}

bool MeshRenderer::meshShaderReady() const noexcept
{
    const auto* device = context_.RenderDevice();
    const bool meshShadersSupported =
        device && device->GetDeviceInfo().Features.MeshShaders !=
                      DEVICE_FEATURE_STATE_DISABLED;
    return meshShadersSupported && pImpl_ && pImpl_->pMeshletPSO_ &&
           pImpl_->pMeshletSRB_ &&
           pImpl_->pMeshletBuffer_ && pImpl_->pMeshletIndexBuffer_ &&
           pImpl_->pPositionBuffer_ && pImpl_->pMeshletConstantsBuffer_ &&
           pImpl_->meshletCount_ > 0 && !pImpl_->meshletLods_.empty();
}

size_t MeshRenderer::chooseMeshletLOD(float projectedRadiusPixels) const noexcept
{
    if (pImpl_ == nullptr || pImpl_->meshletLods_.empty()) {
        return 0;
    }
    size_t selected = 0;
    for (size_t i = 0; i < pImpl_->meshletLods_.size(); ++i) {
        if (projectedRadiusPixels <= pImpl_->meshletLods_[i].switchPixels) {
            selected = i;
        }
    }
    return selected;
}

IBuffer* MeshRenderer::positionBuffer() const noexcept
{
    return pImpl_ ? pImpl_->pPositionBuffer_ : nullptr;
}

IBuffer* MeshRenderer::indexBuffer() const noexcept
{
    return pImpl_ ? pImpl_->pIndexBuffer_ : nullptr;
}

size_t MeshRenderer::vertexCount() const noexcept { return vertexCount_; }
size_t MeshRenderer::indexCount() const noexcept { return indexCount_; }

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
        !pImpl_->pShadowParamsBuffer_ || !pImpl_->pEnvironmentBuffer_ ||
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
    pData = nullptr;
    pContext->MapBuffer(pImpl_->pShadowParamsBuffer_, MAP_WRITE,
                        MAP_FLAG_DISCARD, pData);
    if (!pData) {
        qWarning("[MeshRenderer] prepare skipped because shadow parameter mapping failed");
        return;
    }
    memcpy(pData, &pImpl_->shadowParams_, sizeof(Impl::ShadowParamsConstants));
    pContext->UnmapBuffer(pImpl_->pShadowParamsBuffer_, MAP_WRITE);
    if (frameCostStats_) ++frameCostStats_->bufferUpdates;
    pData = nullptr;
    pContext->MapBuffer(pImpl_->pEnvironmentBuffer_, MAP_WRITE,
                        MAP_FLAG_DISCARD, pData);
    if (!pData) {
        qWarning("[MeshRenderer] prepare skipped because environment mapping failed");
        return;
    }
    memcpy(pData, &pImpl_->environmentConstants_,
           sizeof(Impl::EnvironmentConstants));
    pContext->UnmapBuffer(pImpl_->pEnvironmentBuffer_, MAP_WRITE);
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
        if (auto* envVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_IrradianceMap")) {
            envVar->Set(pImpl_->pIrradianceMapSRV_);
        }
        if (auto* envVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_PrefilteredEnvironment")) {
            envVar->Set(pImpl_->pPrefilteredEnvironmentSRV_);
        }
        if (auto* envVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_BrdfLut")) {
            envVar->Set(pImpl_->pBrdfLutSRV_);
        }
        if (auto* shadowVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")) {
            shadowVar->Set(pImpl_->pShadowMapSRV_ ? pImpl_->pShadowMapSRV_
                                                   : pImpl_->pShadowFallbackSRV_);
        }
        static constexpr std::array<const char*, Impl::MaxSceneLights> goboNames = {
            "g_GoboTexture0", "g_GoboTexture1", "g_GoboTexture2", "g_GoboTexture3",
            "g_GoboTexture4", "g_GoboTexture5", "g_GoboTexture6", "g_GoboTexture7"};
        for (size_t lightIndex = 0; lightIndex < goboNames.size(); ++lightIndex) {
            if (auto* goboVar = activeSRB->GetVariableByName(
                    SHADER_TYPE_PIXEL, goboNames[lightIndex])) {
                goboVar->Set(pImpl_->pGoboTextureSRVs_[lightIndex]
                                 ? pImpl_->pGoboTextureSRVs_[lightIndex]
                                 : pImpl_->pBaseColorTextureSRV_);
            }
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

void MeshRenderer::prepareShadow(IDeviceContext* pContext,
                                 const float* lightViewProjection)
{
    pImpl_->shadowPrepared_ = false;
    if (!pContext || !lightViewProjection || !pImpl_->pShadowPSO_ ||
        !pImpl_->pShadowSRB_ || !pImpl_->pShadowConstantsBuffer_ ||
        !pImpl_->pInstanceBuffer_ || !pImpl_->pPositionBuffer_) {
        return;
    }

    float transposedLightViewProjection[16] = {};
    transpose4x4(lightViewProjection, transposedLightViewProjection);
    void* pData = nullptr;
    pContext->MapBuffer(pImpl_->pShadowConstantsBuffer_, MAP_WRITE,
                        MAP_FLAG_DISCARD, pData);
    if (!pData) {
        qWarning("[MeshRenderer] shadow prepass skipped because constant buffer mapping failed");
        return;
    }
    std::memcpy(pData, transposedLightViewProjection,
                sizeof(transposedLightViewProjection));
    pContext->UnmapBuffer(pImpl_->pShadowConstantsBuffer_, MAP_WRITE);

    if (auto* instances = pImpl_->pShadowSRB_->GetVariableByName(
            SHADER_TYPE_VERTEX, "g_Instances")) {
        instances->Set(pImpl_->pInstanceBuffer_->GetDefaultView(
            BUFFER_VIEW_SHADER_RESOURCE));
    }
    pContext->SetPipelineState(pImpl_->pShadowPSO_);
    pContext->CommitShaderResources(pImpl_->pShadowSRB_,
                                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IBuffer* positionBuffer[] = {pImpl_->pPositionBuffer_};
    pContext->SetVertexBuffers(0, 1, positionBuffer, nullptr,
                               RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                               SET_VERTEX_BUFFERS_FLAG_RESET);
    if (pImpl_->pIndexBuffer_) {
        pContext->SetIndexBuffer(pImpl_->pIndexBuffer_, 0,
                                 RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    pImpl_->shadowPrepared_ = true;
}

void MeshRenderer::drawShadow(IDeviceContext* pContext, size_t instanceCount)
{
    if (!pContext || !pImpl_->shadowPrepared_ || instanceCount == 0) {
        return;
    }
    instanceCount = std::min(instanceCount, maxInstances_);
    if (pImpl_->pIndexBuffer_ && indexCount_ > 0) {
        DrawIndexedAttribs drawAttrs;
        drawAttrs.NumIndices = static_cast<Uint32>(indexCount_);
        drawAttrs.NumInstances = static_cast<Uint32>(instanceCount);
        drawAttrs.IndexType = VT_UINT32;
        drawAttrs.Flags = DRAW_FLAG_NONE;
        pContext->DrawIndexed(drawAttrs);
    } else if (vertexCount_ > 0) {
        DrawAttribs drawAttrs;
        drawAttrs.NumVertices = static_cast<Uint32>(vertexCount_);
        drawAttrs.NumInstances = static_cast<Uint32>(instanceCount);
        drawAttrs.Flags = DRAW_FLAG_NONE;
        pContext->Draw(drawAttrs);
    }
    pImpl_->shadowPrepared_ = false;
}

void MeshRenderer::setViewMatrix(const float* matrix)
{
    prepared_ = false;
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
    prepared_ = false;
    transpose4x4(matrix, constants_.projMatrix);
}

void MeshRenderer::setPreviousViewMatrix(const float* matrix)
{
    prepared_ = false;
    transpose4x4(matrix, constants_.prevViewMatrix);
}

void MeshRenderer::setPreviousProjectionMatrix(const float* matrix)
{
    prepared_ = false;
    transpose4x4(matrix, constants_.prevProjMatrix);
}

void MeshRenderer::setSceneLights(const std::vector<Light>& lights)
{
    prepared_ = false;
    std::fill(std::begin(pImpl_->sceneLighting_.lights),
              std::end(pImpl_->sceneLighting_.lights), Impl::SceneLightGpu{});
    pImpl_->sceneLighting_.lightingMeta[0] = 0;
    for (const auto& light : lights) {
        if (!light.enabled() ||
            pImpl_->sceneLighting_.lightingMeta[0] >= Impl::MaxSceneLights) {
            continue;
        }

        const size_t packedLightIndex = pImpl_->sceneLighting_.lightingMeta[0]++;
        auto& gpuLight = pImpl_->sceneLighting_.lights[packedLightIndex];
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
        const QString goboPath = QString::fromStdString(light.goboTexturePath());
        const bool shouldLoadGobo = light.type() == LightType::Spot &&
            !goboPath.isEmpty();
        const bool goboPathChanged =
            pImpl_->goboTexturePaths_[packedLightIndex] != goboPath;
        const bool goboWasDisabled = !shouldLoadGobo &&
            pImpl_->pGoboTextureSRVs_[packedLightIndex];
        const bool goboRecovered = shouldLoadGobo &&
            !pImpl_->pGoboTextureSRVs_[packedLightIndex] &&
            QFileInfo::exists(goboPath);
        if (goboPathChanged || goboWasDisabled || goboRecovered) {
            pImpl_->goboTexturePaths_[packedLightIndex] = goboPath;
            pImpl_->pGoboTextures_[packedLightIndex].Release();
            pImpl_->pGoboTextureSRVs_[packedLightIndex].Release();
            if (shouldLoadGobo) {
                loadLinearTexture(context_, goboPath,
                                  "MeshRenderer GOBO Texture",
                                  pImpl_->pGoboTextures_[packedLightIndex],
                                  pImpl_->pGoboTextureSRVs_[packedLightIndex]);
            }
        }
        gpuLight.goboInfo[0] = light.goboIntensity();
        gpuLight.goboInfo[1] = light.goboRotation() *
            std::numbers::pi_v<float> / 180.0f;
        gpuLight.goboInfo[2] = light.goboInvert() ? 1.0f : 0.0f;
        gpuLight.goboInfo[3] =
            light.type() == LightType::Spot &&
            !goboPath.isEmpty() && pImpl_->pGoboTextureSRVs_[packedLightIndex]
                ? 1.0f : 0.0f;
    }
}

void MeshRenderer::setShadowMap(ITextureView* shadowMap,
                                const float* lightViewProjection,
                                bool enabled, int sceneLightIndex,
                                float depthBias, float softness)
{
    prepared_ = false;
    pImpl_->pShadowMapSRV_ = shadowMap ? shadowMap : pImpl_->pShadowFallbackSRV_;
    if (lightViewProjection) {
        transpose4x4(lightViewProjection,
                     pImpl_->shadowParams_.lightViewProjection);
    }
    pImpl_->shadowParams_.shadowSettings[0] = enabled && shadowMap ? 1.0f : 0.0f;
    pImpl_->shadowParams_.shadowSettings[1] = std::max(depthBias, 0.0f);
    pImpl_->shadowParams_.shadowSettings[2] =
        static_cast<float>(std::max(sceneLightIndex, 0));
    pImpl_->shadowParams_.shadowSettings[3] =
        std::isfinite(softness) ? std::clamp(softness, 0.0f, 2.0f) : 0.0f;
}

void MeshRenderer::setEnvironmentMaps(ITextureView* irradianceMap,
                                      ITextureView* prefilteredEnvironment,
                                      ITextureView* brdfLut,
                                      float intensity)
{
    prepared_ = false;
    pImpl_->pIrradianceMapSRV_ = irradianceMap
        ? irradianceMap : pImpl_->pIrradianceMapSRV_;
    pImpl_->pPrefilteredEnvironmentSRV_ = prefilteredEnvironment
        ? prefilteredEnvironment : pImpl_->pPrefilteredEnvironmentSRV_;
    pImpl_->pBrdfLutSRV_ = brdfLut
        ? brdfLut : pImpl_->pBrdfLutSRV_;
    const bool enabled = irradianceMap && prefilteredEnvironment && brdfLut;
    pImpl_->environmentConstants_.settings[0] = enabled ? 1.0f : 0.0f;
    pImpl_->environmentConstants_.settings[1] =
        std::isfinite(intensity) ? std::max(0.0f, intensity) : 0.0f;
}

bool MeshRenderer::setEnvironmentMap(const QString& path, float intensity)
{
    prepared_ = false;
    const QString normalizedPath = path.trimmed();
    if (normalizedPath == pImpl_->environmentMapPath_ &&
        pImpl_->environmentConstants_.settings[0] > 0.5f) {
        pImpl_->environmentConstants_.settings[1] =
            std::isfinite(intensity) ? std::max(0.0f, intensity) : 0.0f;
        return true;
    }
    pImpl_->environmentMapPath_ = normalizedPath;
    pImpl_->pEnvironmentTexture_.Release();
    pImpl_->pIrradianceTexture_.Release();
    pImpl_->pBrdfLutTexture_.Release();
    if (normalizedPath.isEmpty()) {
        setEnvironmentMaps(nullptr, nullptr, nullptr, 0.0f);
        return false;
    }
    RefCntAutoPtr<ITextureView> environmentView;
    RefCntAutoPtr<ITextureView> irradianceView;
    if (!createEnvironmentCube(context_, normalizedPath,
                               pImpl_->pEnvironmentTexture_, environmentView,
                               pImpl_->pIrradianceTexture_, irradianceView)) {
        setEnvironmentMaps(nullptr, nullptr, nullptr, 0.0f);
        return false;
    }
    // Bind the generated diffuse irradiance cube and GGX-prefiltered specular
    // cube separately. The BRDF LUT is generated below and replaces the
    // fallback binding when creation succeeds.
    setEnvironmentMaps(irradianceView.RawPtr(), environmentView.RawPtr(),
                       pImpl_->pBrdfLutSRV_.RawPtr(), intensity);
    RefCntAutoPtr<ITextureView> brdfView;
    if (createBrdfLut(context_, pImpl_->pBrdfLutTexture_, brdfView)) {
        setEnvironmentMaps(irradianceView.RawPtr(), environmentView.RawPtr(),
                           brdfView.RawPtr(), intensity);
    }
    return true;
}

void MeshRenderer::shareEnvironmentMapsFrom(const MeshRenderer& source)
{
    if (this == &source) {
        return;
    }
    prepared_ = false;
    pImpl_->environmentMapPath_ = source.pImpl_->environmentMapPath_;
    pImpl_->pEnvironmentTexture_ = source.pImpl_->pEnvironmentTexture_;
    pImpl_->pIrradianceTexture_ = source.pImpl_->pIrradianceTexture_;
    pImpl_->pBrdfLutTexture_ = source.pImpl_->pBrdfLutTexture_;
    pImpl_->pPrefilteredEnvironmentSRV_ = source.pImpl_->pPrefilteredEnvironmentSRV_;
    pImpl_->pIrradianceMapSRV_ = source.pImpl_->pIrradianceMapSRV_;
    pImpl_->pBrdfLutSRV_ = source.pImpl_->pBrdfLutSRV_;
    pImpl_->environmentConstants_ = source.pImpl_->environmentConstants_;
}

bool MeshRenderer::hasEnvironmentMap() const
{
    return pImpl_->environmentConstants_.settings[0] > 0.5f;
}

void MeshRenderer::setEnvironmentRotation(float degrees)
{
    pImpl_->environmentConstants_.settings[2] = std::isfinite(degrees)
        ? degrees * (std::numbers::pi_v<float> / 180.0f) : 0.0f;
    prepared_ = false;
}

Diligent::ITextureView* MeshRenderer::environmentMapView() const
{
    return pImpl_->pPrefilteredEnvironmentSRV_.RawPtr();
}

void MeshRenderer::setTransparentPass(bool transparent)
{
    if (pImpl_->transparentPass_ == transparent) {
        return;
    }
    pImpl_->transparentPass_ = transparent;
    prepared_ = false;
}

void MeshRenderer::setAlphaMasked(bool masked)
{
    prepared_ = false;
    pImpl_->materialConstants_.pbrTextureFlags[3] = masked ? 1.0f : 0.0f;
}

void MeshRenderer::setAlphaCutoff(float cutoff)
{
    prepared_ = false;
    pImpl_->materialConstants_.alphaSettings[0] =
        std::clamp(std::isfinite(cutoff) ? cutoff : 0.5f, 0.0f, 1.0f);
}

void MeshRenderer::setBaseColorTexture(const QString& path)
{
    prepared_ = false;
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

    QVector<QVector<quint8>> mipData = buildRgba8MipChain(
        rgba8, rawImage.width, rawImage.height, true);
    texDesc.MipLevels = static_cast<Uint32>(mipData.size());
    QVector<TextureSubResData> subResources;
    subResources.resize(mipData.size());
    int levelWidth = rawImage.width;
    for (int level = 0; level < mipData.size(); ++level) {
        subResources[level].pData = mipData[level].constData();
        subResources[level].Stride = static_cast<Uint64>(levelWidth * 4);
        levelWidth = std::max(1, levelWidth / 2);
    }

    TextureData initData;
    initData.pSubResources = subResources.constData();
    initData.NumSubresources = static_cast<Uint32>(subResources.size());

    pDevice->CreateTexture(texDesc, &initData, &pImpl_->pBaseColorTexture_);
    if (pImpl_->pBaseColorTexture_) {
        pImpl_->pBaseColorTextureSRV_ =
            pImpl_->pBaseColorTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
}

void MeshRenderer::clearBaseColorTexture()
{
    prepared_ = false;
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
    prepared_ = false;
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

    QVector<QVector<quint8>> mipData = buildRgba8MipChain(
        rgba8, rawImage.width, rawImage.height, true);
    texDesc.MipLevels = static_cast<Uint32>(mipData.size());
    QVector<TextureSubResData> subResources;
    subResources.resize(mipData.size());
    int levelWidth = rawImage.width;
    for (int level = 0; level < mipData.size(); ++level) {
        subResources[level].pData = mipData[level].constData();
        subResources[level].Stride = static_cast<Uint64>(levelWidth * 4);
        levelWidth = std::max(1, levelWidth / 2);
    }

    TextureData initData;
    initData.pSubResources = subResources.constData();
    initData.NumSubresources = static_cast<Uint32>(subResources.size());

    pDevice->CreateTexture(texDesc, &initData, &pImpl_->pEmissionTexture_);
    if (pImpl_->pEmissionTexture_) {
        pImpl_->pEmissionTextureSRV_ =
            pImpl_->pEmissionTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
}

void MeshRenderer::clearEmissionTexture()
{
    prepared_ = false;
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
    prepared_ = false;
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
    prepared_ = false;
    pImpl_->materialConstants_.pbrFactors[0] = std::clamp(metallic, 0.0f, 1.0f);
    pImpl_->materialConstants_.pbrFactors[1] = std::clamp(roughness, 0.0f, 1.0f);
    pImpl_->materialConstants_.pbrFactors[2] = std::max(normalStrength, 0.0f);
    pImpl_->materialConstants_.pbrFactors[3] =
        std::clamp(occlusionStrength, 0.0f, 1.0f);
}

void MeshRenderer::setPrincipledFactors(float specular, float ior,
                                        float transmission, float clearcoat,
                                        float clearcoatRoughness, float sheen)
{
    prepared_ = false;
    pImpl_->materialConstants_.principledFactors[0] =
        std::clamp(specular, 0.0f, 1.0f);
    pImpl_->materialConstants_.principledFactors[1] =
        std::clamp(ior, 1.0f, 3.0f);
    pImpl_->materialConstants_.principledFactors[2] =
        std::clamp(transmission, 0.0f, 1.0f);
    pImpl_->materialConstants_.principledFactors[3] =
        std::clamp(sheen, 0.0f, 1.0f);
    pImpl_->materialConstants_.clearcoatFactors[0] =
        std::clamp(clearcoat, 0.0f, 1.0f);
    pImpl_->materialConstants_.clearcoatFactors[1] =
        std::clamp(clearcoatRoughness, 0.0f, 1.0f);
}

void MeshRenderer::setMetallicRoughnessTexture(const QString& path)
{
    prepared_ = false;
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
    prepared_ = false;
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
                          pImpl_->pNormalTexture_, pImpl_->pNormalTextureSRV_,
                          true)) {
        pImpl_->normalTexturePath_ = newPath;
        pImpl_->materialConstants_.pbrTextureFlags[1] = 1.0f;
    } else {
        pImpl_->pNormalTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
    }
}

void MeshRenderer::setOcclusionTexture(const QString& path)
{
    prepared_ = false;
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
    prepared_ = false;
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
    texDesc.Format = TEX_FORMAT_RGBA8_UNORM;
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

    QVector<QVector<quint8>> mipData = buildRgba8MipChain(
        rgba8, rawImage.width, rawImage.height, false);
    texDesc.MipLevels = static_cast<Uint32>(mipData.size());
    QVector<TextureSubResData> subResources;
    subResources.resize(mipData.size());
    int levelWidth = rawImage.width;
    for (int level = 0; level < mipData.size(); ++level) {
        subResources[level].pData = mipData[level].constData();
        subResources[level].Stride = static_cast<Uint64>(levelWidth * 4);
        levelWidth = std::max(1, levelWidth / 2);
    }

    TextureData initData;
    initData.pSubResources = subResources.constData();
    initData.NumSubresources = static_cast<Uint32>(subResources.size());

    pDevice->CreateTexture(texDesc, &initData, &pImpl_->pOpacityTexture_);
    if (pImpl_->pOpacityTexture_) {
        pImpl_->pOpacityTextureSRV_ =
            pImpl_->pOpacityTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
}

void MeshRenderer::clearOpacityTexture()
{
    prepared_ = false;
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

