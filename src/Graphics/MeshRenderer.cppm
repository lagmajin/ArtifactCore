module;
#include <utility>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <exception>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>
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
#include <QVector3D>
#include <QDebug>
#include <QFileInfo>
#include <QVector>

module Graphics.MeshRenderer;

import Math.Vec;
import Core.Parallel;

import Frame.Debug;
import Graphics.ParticleData;
import Graphics.Compute;
import IO.ImageImporter;
import Image.Raw;
import Artifact.ShaderNode.Core;

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
    if (renormalizeNormal) {
        auto& baseLevel = mipData.back();
        for (int pixel = 0; pixel < width * height; ++pixel) {
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
    const bool alphaWeighted = srgb && !renormalizeNormal;

    while (mipWidth > 1 || mipHeight > 1) {
        const int nextWidth = std::max(1, mipWidth / 2);
        const int nextHeight = std::max(1, mipHeight / 2);
        const auto& previous = mipData.back();
        QVector<quint8> next(nextWidth * nextHeight * 4, 0);
        for (int y = 0; y < nextHeight; ++y) {
            for (int x = 0; x < nextWidth; ++x) {
                float sum[4] = {};
                float totalWeight = 0.0f;
                const float sourceX0 =
                    static_cast<float>(x) * mipWidth / nextWidth;
                const float sourceX1 =
                    static_cast<float>(x + 1) * mipWidth / nextWidth;
                const float sourceY0 =
                    static_cast<float>(y) * mipHeight / nextHeight;
                const float sourceY1 =
                    static_cast<float>(y + 1) * mipHeight / nextHeight;
                for (int py = static_cast<int>(std::floor(sourceY0));
                     py < static_cast<int>(std::ceil(sourceY1)); ++py) {
                    const float weightY = std::max(0.0f,
                        std::min(sourceY1, static_cast<float>(py + 1)) -
                        std::max(sourceY0, static_cast<float>(py)));
                    for (int px = static_cast<int>(std::floor(sourceX0));
                         px < static_cast<int>(std::ceil(sourceX1)); ++px) {
                        const float weightX = std::max(0.0f,
                            std::min(sourceX1, static_cast<float>(px + 1)) -
                            std::max(sourceX0, static_cast<float>(px)));
                        const float weight = weightX * weightY;
                        const int source = (py * mipWidth + px) * 4;
                        const float alpha = previous[source + 3] / 255.0f;
                        if (renormalizeNormal) {
                            const float previousLength =
                                previous[source + 3] / 255.0f;
                            for (int channel = 0; channel < 3; ++channel) {
                                const float component =
                                    previous[source + channel] / 127.5f - 1.0f;
                                sum[channel] +=
                                    (component * previousLength + 1.0f) *
                                    (0.5f * weight);
                            }
                        } else {
                            for (int channel = 0; channel < 3; ++channel) {
                                const float value =
                                    previous[source + channel] / 255.0f;
                                sum[channel] +=
                                    (srgb ? decodeSrgb(value) : value) *
                                    (alphaWeighted ? alpha : 1.0f) * weight;
                            }
                        }
                        sum[3] += alpha * weight;
                        totalWeight += weight;
                    }
                }
                const int destination = (y * nextWidth + x) * 4;
                if (renormalizeNormal) {
                    float nx = sum[0] / totalWeight * 2.0f - 1.0f;
                    float ny = sum[1] / totalWeight * 2.0f - 1.0f;
                    float nz = sum[2] / totalWeight * 2.0f - 1.0f;
                    const float length = std::sqrt(std::max(
                        nx * nx + ny * ny + nz * nz, 1e-8f));
                    next[destination + 3] = static_cast<quint8>(
                        std::lround(std::clamp(length, 0.0f, 1.0f) * 255.0f));
                    next[destination + 0] = static_cast<quint8>(std::clamp((nx / length + 1.0f) * 127.5f, 0.0f, 255.0f));
                    next[destination + 1] = static_cast<quint8>(std::clamp((ny / length + 1.0f) * 127.5f, 0.0f, 255.0f));
                    next[destination + 2] = static_cast<quint8>(std::clamp((nz / length + 1.0f) * 127.5f, 0.0f, 255.0f));
                } else {
                    for (int channel = 0; channel < 3; ++channel) {
                        const float value = alphaWeighted
                            ? (sum[3] > 1.0e-8f ? sum[channel] / sum[3] : 0.0f)
                            : sum[channel] / totalWeight;
                        const float encoded = srgb ? encodeSrgb(value) : value;
                        next[destination + channel] = static_cast<quint8>(std::clamp(encoded * 255.0f, 0.0f, 255.0f));
                    }
                }
                if (!renormalizeNormal) {
                    next[destination + 3] = static_cast<quint8>(
                        std::clamp(sum[3] / totalWeight * 255.0f, 0.0f, 255.0f));
                }
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
    initData.pSubResources = subResources.data();
    initData.NumSubresources = static_cast<Diligent::Uint32>(subResources.size());
    device->CreateTexture(texDesc, &initData, &texture);
    if (!texture) {
        return false;
    }
    textureSRV = texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    return textureSRV.RawPtr() != nullptr;
}

bool loadSrgbTexture(ArtifactCore::GpuContext& context, const QString& path,
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
        qWarning() << "[MeshRenderer] Failed to open sRGB texture:" << path;
        return false;
    }
    const ArtifactCore::RawImage rawImage = importer.readImage();
    if (!rawImage.isValid() || rawImage.width <= 0 || rawImage.height <= 0) {
        qWarning() << "[MeshRenderer] Failed to read sRGB texture:" << path;
        return false;
    }
    const bool floatSource = rawImage.pixelType == QStringLiteral("float");
    if (rawImage.isLogEncoded) {
        qWarning() << "[MeshRenderer] Log-encoded base color textures are unsupported:" << path;
        return false;
    }
    const QString sourceColorSpace = rawImage.colorSpace.trimmed().toLower();
    const QString sourceTransfer = rawImage.transferFunction.trimmed().toLower();
    const QString sourceColorDescription =
        sourceColorSpace + QLatin1Char(' ') + sourceTransfer;
    const bool explicitlyLinear =
        ((sourceTransfer.contains(QStringLiteral("linear")) &&
          !sourceTransfer.contains(QStringLiteral("nonlinear"))) ||
         (sourceColorSpace.contains(QStringLiteral("linear")) &&
          !sourceColorSpace.contains(QStringLiteral("nonlinear"))));
    const bool explicitlySrgb = !explicitlyLinear &&
        (sourceTransfer.contains(QStringLiteral("srgb")) ||
         sourceColorSpace.contains(QStringLiteral("srgb")));
    const bool unsupportedPrimaries =
        sourceColorDescription.contains(QStringLiteral("aces")) ||
        sourceColorDescription.contains(QStringLiteral("2020")) ||
        sourceColorDescription.contains(QStringLiteral("p3")) ||
        sourceColorDescription.contains(QStringLiteral("xyz")) ||
        sourceColorDescription.contains(QStringLiteral("adobe")) ||
        sourceColorDescription.contains(QStringLiteral("prophoto")) ||
        sourceColorDescription.contains(QStringLiteral("wide gamut"));
    const bool floatLinearSrgb = floatSource && !explicitlySrgb &&
        !unsupportedPrimaries &&
        (sourceColorDescription.trimmed().isEmpty() || explicitlyLinear);
    QVector<quint8> rgba8 = expandTextureToRgba8(rawImage, false);
    if (rgba8.isEmpty()) {
        qWarning() << "[MeshRenderer] Unsupported sRGB texture:" << path
                   << rawImage.pixelType;
        return false;
    }
    if (floatLinearSrgb) {
        const auto encodeSrgb = [](float value) {
            value = std::clamp(value, 0.0f, 1.0f);
            return value <= 0.0031308f ? value * 12.92f
                : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
        };
        for (qsizetype pixel = 0; pixel < rgba8.size() / 4; ++pixel) {
            for (int channel = 0; channel < 3; ++channel) {
                const qsizetype index = pixel * 4 + channel;
                const float linear = rgba8[index] / 255.0f;
                rgba8[index] = static_cast<quint8>(std::lround(
                    encodeSrgb(linear) * 255.0f));
            }
        }
    }
    const auto mipData = buildRgba8MipChain(
        rgba8, rawImage.width, rawImage.height, true);
    Diligent::TextureDesc desc;
    desc.Name = debugName;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Diligent::Uint32>(rawImage.width);
    desc.Height = static_cast<Diligent::Uint32>(rawImage.height);
    desc.MipLevels = static_cast<Diligent::Uint32>(mipData.size());
    desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    QVector<Diligent::TextureSubResData> subResources;
    subResources.resize(mipData.size());
    int levelWidth = rawImage.width;
    for (int level = 0; level < mipData.size(); ++level) {
        subResources[level].pData = mipData[level].constData();
        subResources[level].Stride =
            static_cast<Diligent::Uint64>(levelWidth * 4);
        levelWidth = std::max(1, levelWidth / 2);
    }
    Diligent::TextureData initialData;
    initialData.pSubResources = subResources.data();
    initialData.NumSubresources =
        static_cast<Diligent::Uint32>(subResources.size());
    device->CreateTexture(desc, &initialData, &texture);
    if (!texture) {
        return false;
    }
    textureSRV = texture->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    return textureSRV.RawPtr() != nullptr;
}

bool loadEmissionTexture(ArtifactCore::GpuContext& context, const QString& path,
                         const char* debugName,
                         Diligent::RefCntAutoPtr<Diligent::ITexture>& texture,
                         Diligent::RefCntAutoPtr<Diligent::ITextureView>& textureSRV)
{
    auto* device = context.RenderDevice();
    if (!device || path.isEmpty()) return false;
    ArtifactCore::ImageImporter importer;
    if (!importer.open(path)) return false;
    const ArtifactCore::RawImage image = importer.readImage();
    const qsizetype pixelCount = static_cast<qsizetype>(image.width) * image.height;
    const int pixelSize = image.getPixelTypeSizeInBytes();
    if (!image.isValid() || pixelCount <= 0 || image.channels <= 0 || pixelSize <= 0)
        return false;
    const qsizetype requiredBytes = pixelCount * image.channels * pixelSize;
    if (image.data.size() < requiredBytes) return false;

    const bool floatSource = image.pixelType == QStringLiteral("float");
    if (image.isLogEncoded) return false;
    const bool floatSrgb = floatSource &&
        (image.colorSpace.contains(QStringLiteral("srgb"), Qt::CaseInsensitive) ||
         image.transferFunction.contains(QStringLiteral("srgb"), Qt::CaseInsensitive));
    const auto srgbToLinear = [](float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        return value <= 0.04045f ? value / 12.92f
                                 : std::pow((value + 0.055f) / 1.055f, 2.4f);
    };
    QVector<QVector<float>> mips;
    QVector<float> base(pixelCount * 4, 1.0f);
    const quint8* bytes = image.data.constData();
    const qsizetype stride = static_cast<qsizetype>(image.channels) * pixelSize;
    for (qsizetype pixel = 0; pixel < pixelCount; ++pixel) {
        for (int channel = 0; channel < 4; ++channel) {
            float value = channel == 3 ? 1.0f : 0.0f;
            if (channel < image.channels) {
                const quint8* src = bytes + pixel * stride + channel * pixelSize;
                if (image.pixelType == QStringLiteral("float")) {
                    std::memcpy(&value, src, sizeof(value));
                    value = std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
                } else if (image.pixelType == QStringLiteral("uint16")) {
                    quint16 sample = 0;
                    std::memcpy(&sample, src, sizeof(sample));
                    value = sample / 65535.0f;
                } else if (image.pixelType == QStringLiteral("uint8")) {
                    value = *src / 255.0f;
                } else {
                    return false;
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
    int width = image.width;
    int height = image.height;
    while (width > 1 || height > 1) {
        const int nextWidth = std::max(1, width / 2);
        const int nextHeight = std::max(1, height / 2);
        const auto& previous = mips.back();
        QVector<float> next(static_cast<qsizetype>(nextWidth) * nextHeight * 4, 0.0f);
        for (int y = 0; y < nextHeight; ++y) for (int x = 0; x < nextWidth; ++x) {
            float sum[4]{};
            int samples = 0;
            for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
                const int sx = std::min(x * 2 + dx, width - 1);
                const int sy = std::min(y * 2 + dy, height - 1);
                const qsizetype offset = (static_cast<qsizetype>(sy) * width + sx) * 4;
                for (int c = 0; c < 4; ++c) sum[c] += previous[offset + c];
                ++samples;
            }
            const qsizetype out = (static_cast<qsizetype>(y) * nextWidth + x) * 4;
            for (int c = 0; c < 4; ++c) next[out + c] = sum[c] / samples;
        }
        mips.push_back(std::move(next));
        width = nextWidth;
        height = nextHeight;
    }

    Diligent::TextureDesc desc;
    desc.Name = debugName;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Diligent::Uint32>(image.width);
    desc.Height = static_cast<Diligent::Uint32>(image.height);
    desc.MipLevels = static_cast<Diligent::Uint32>(mips.size());
    desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    QVector<Diligent::TextureSubResData> subresources(mips.size());
    width = image.width;
    for (int mip = 0; mip < mips.size(); ++mip) {
        subresources[mip].pData = mips[mip].constData();
        subresources[mip].Stride = static_cast<Diligent::Uint64>(width) * 4u * sizeof(float);
        width = std::max(1, width / 2);
    }
    Diligent::TextureData initialData;
    initialData.pSubResources = subresources.data();
    initialData.NumSubresources = static_cast<Diligent::Uint32>(subresources.size());
    device->CreateTexture(desc, &initialData, &texture);
    if (!texture) return false;
    textureSRV = texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    return textureSRV.RawPtr() != nullptr;
}

bool loadOpacityTexture(ArtifactCore::GpuContext& context, const QString& path,
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
        qWarning() << "[MeshRenderer] Failed to open opacity texture:" << path;
        return false;
    }
    const ArtifactCore::RawImage rawImage = importer.readImage();
    if (!rawImage.isValid() || rawImage.width <= 0 || rawImage.height <= 0) {
        qWarning() << "[MeshRenderer] Failed to read opacity texture:" << path;
        return false;
    }
    const QVector<quint8> rgba8 = expandTextureToRgba8(rawImage, true);
    if (rgba8.isEmpty()) {
        qWarning() << "[MeshRenderer] Unsupported opacity texture:" << path
                   << rawImage.pixelType;
        return false;
    }
    const auto mipData = buildRgba8MipChain(
        rgba8, rawImage.width, rawImage.height, false);
    Diligent::TextureDesc desc;
    desc.Name = debugName;
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = static_cast<Diligent::Uint32>(rawImage.width);
    desc.Height = static_cast<Diligent::Uint32>(rawImage.height);
    desc.MipLevels = static_cast<Diligent::Uint32>(mipData.size());
    desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    QVector<Diligent::TextureSubResData> subResources;
    subResources.resize(mipData.size());
    int levelWidth = rawImage.width;
    for (int level = 0; level < mipData.size(); ++level) {
        subResources[level].pData = mipData[level].constData();
        subResources[level].Stride =
            static_cast<Diligent::Uint64>(levelWidth * 4);
        levelWidth = std::max(1, levelWidth / 2);
    }
    Diligent::TextureData initialData;
    initialData.pSubResources = subResources.data();
    initialData.NumSubresources =
        static_cast<Diligent::Uint32>(subResources.size());
    device->CreateTexture(desc, &initialData, &texture);
    if (!texture) {
        return false;
    }
    textureSRV = texture->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
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
                           Diligent::RefCntAutoPtr<Diligent::ITextureView>& irradianceView,
                           Diligent::RefCntAutoPtr<Diligent::ITexture>& sheenTexture,
                           Diligent::RefCntAutoPtr<Diligent::ITextureView>& sheenView)
{
    auto* device = context.RenderDevice();
    ArtifactCore::ImageImporter importer;
    if (!device || !importer.open(path)) return false;
    const auto image = importer.readImage();
    if (!image.isValid() || image.width < 2 || image.height < 2 || image.channels < 3)
        return false;

    // Decode format checks, bounds checks, and channel conversion once. The
    // filtered mip generation samples the source many times, so repeating
    // rawEnvironmentChannel() for each bilinear tap dominated HDR load time.
    const auto sourcePixelCount = static_cast<std::size_t>(image.width) *
                                  static_cast<std::size_t>(image.height);
    std::vector<std::array<float, 4>> sourcePixels(sourcePixelCount);
    for (std::size_t pixel = 0; pixel < sourcePixelCount; ++pixel) {
        const int pixelIndex = static_cast<int>(pixel);
        sourcePixels[pixel][0] = rawEnvironmentChannel(image, pixelIndex, 0);
        sourcePixels[pixel][1] = rawEnvironmentChannel(image, pixelIndex, 1);
        sourcePixels[pixel][2] = rawEnvironmentChannel(image, pixelIndex, 2);
        sourcePixels[pixel][3] = image.channels > 3
            ? rawEnvironmentChannel(image, pixelIndex, 3) : 1.0f;
    }

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
    auto sampleEquirectangularRgba = [&](const EnvironmentFloat3& direction) {
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
        std::array<float, 4> result{};
        for (int dy = 0; dy < 2; ++dy) {
            for (int dx = 0; dx < 2; ++dx) {
                const int pixel = clampY(y0 + dy) * image.width + wrapX(x0 + dx);
                const float weight = (dx == 0 ? 1.0f - tx : tx) *
                                     (dy == 0 ? 1.0f - ty : ty);
                const auto& source = sourcePixels[static_cast<std::size_t>(pixel)];
                for (int channel = 0; channel < 4; ++channel) {
                    result[channel] += source[channel] * weight;
                }
            }
        }
        return result;
    };
    auto sampleEquirectangular = [&](const EnvironmentFloat3& direction) {
        const auto rgba = sampleEquirectangularRgba(direction);
        return EnvironmentFloat3{rgba[0], rgba[1], rgba[2]};
    };
    constexpr std::size_t baseFacePixelCount =
        static_cast<std::size_t>(faceSize) * faceSize;
    constexpr std::size_t basePixelCount = baseFacePixelCount * 6;
    Parallel::ForSize(0, basePixelCount, basePixelCount, 512,
        [&](std::size_t pixelIndex) {
            const int face = static_cast<int>(pixelIndex / baseFacePixelCount);
            const auto localPixel = pixelIndex % baseFacePixelCount;
            const int y = static_cast<int>(localPixel / faceSize);
            const int x = static_cast<int>(localPixel % faceSize);
            const float u = 2.0f * (static_cast<float>(x) + 0.5f) / faceSize - 1.0f;
            const float v = 2.0f * (static_cast<float>(y) + 0.5f) / faceSize - 1.0f;
            auto direction = directionForFace(face, u, v);
            const float length = std::sqrt(direction.x * direction.x +
                                           direction.y * direction.y +
                                           direction.z * direction.z);
            direction.x /= length; direction.y /= length; direction.z /= length;
            const std::size_t destination =
                (static_cast<std::size_t>(face) * faceSize * faceSize +
                 static_cast<std::size_t>(y) * faceSize + x) * 4;
            const auto rgba = sampleEquirectangularRgba(direction);
            std::copy(rgba.begin(), rgba.end(), cube.begin() + destination);
        });

    constexpr int mipCount = 7;
    std::vector<std::vector<float>> mipData;
    mipData.push_back(std::move(cube));
    std::vector<std::vector<float>> sheenMipData;
    sheenMipData.push_back(mipData.front());
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
        std::vector<float> nextSheen(next.size(), 0.0f);
        const auto mipFacePixelCount = static_cast<std::size_t>(nextSize) * nextSize;
        const auto mipPixelCount = mipFacePixelCount * 6;
        Parallel::ForSize(0, mipPixelCount,
            mipPixelCount * prefilterSamples, 128,
            [&](std::size_t pixelIndex) {
                const int face = static_cast<int>(pixelIndex / mipFacePixelCount);
                const auto localPixel = pixelIndex % mipFacePixelCount;
                const int y = static_cast<int>(localPixel / nextSize);
                const int x = static_cast<int>(localPixel % nextSize);
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
                EnvironmentFloat3 sheenSum{0.0f, 0.0f, 0.0f};
                float weightSum = 0.0f;
                float sheenWeightSum = 0.0f;
                for (int sample = 0; sample < prefilterSamples; ++sample) {
                    const float xi1 = (static_cast<float>(sample) + 0.5f) /
                                      prefilterSamples;
                    const float xi2 = radicalInverse(static_cast<std::uint32_t>(sample));
                    const float phi = 2.0f * std::numbers::pi_v<float> * xi1;
                    const float sheenAlphaG = std::max(roughness * roughness, 1.0e-4f);
                    const float sheenSin2Theta = std::pow(
                        xi2, (2.0f * sheenAlphaG) / (2.0f * sheenAlphaG + 1.0f));
                    const float sheenSinTheta = std::sqrt(sheenSin2Theta);
                    const float sheenCosTheta = std::sqrt(
                        std::max(0.0f, 1.0f - sheenSin2Theta));
                    const EnvironmentFloat3 sheenHalfVector{
                        tangent.x * (std::cos(phi) * sheenSinTheta) +
                            bitangent.x * (std::sin(phi) * sheenSinTheta) +
                            normal.x * sheenCosTheta,
                        tangent.y * (std::cos(phi) * sheenSinTheta) +
                            bitangent.y * (std::sin(phi) * sheenSinTheta) +
                            normal.y * sheenCosTheta,
                        tangent.z * (std::cos(phi) * sheenSinTheta) +
                            bitangent.z * (std::sin(phi) * sheenSinTheta) +
                            normal.z * sheenCosTheta};
                    const float sheenNormalHalf = std::max(
                        normal.x * sheenHalfVector.x +
                            normal.y * sheenHalfVector.y +
                            normal.z * sheenHalfVector.z,
                        0.0f);
                    const EnvironmentFloat3 sheenLight{
                        2.0f * sheenNormalHalf * sheenHalfVector.x - normal.x,
                        2.0f * sheenNormalHalf * sheenHalfVector.y - normal.y,
                        2.0f * sheenNormalHalf * sheenHalfVector.z - normal.z};
                    const float sheenNormalLight = std::max(
                        normal.x * sheenLight.x + normal.y * sheenLight.y +
                            normal.z * sheenLight.z,
                        0.0f);
                    if (sheenNormalLight > 0.0f) {
                        const auto sheenRadiance = sampleEquirectangular(sheenLight);
                        sheenSum.x += sheenRadiance.x * sheenNormalLight;
                        sheenSum.y += sheenRadiance.y * sheenNormalLight;
                        sheenSum.z += sheenRadiance.z * sheenNormalLight;
                        sheenWeightSum += sheenNormalLight;
                    }
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
                const float inverseSheenWeight = sheenWeightSum > 1.0e-5f
                    ? 1.0f / sheenWeightSum : 0.0f;
                nextSheen[destination + 0] = sheenSum.x * inverseSheenWeight;
                nextSheen[destination + 1] = sheenSum.y * inverseSheenWeight;
                nextSheen[destination + 2] = sheenSum.z * inverseSheenWeight;
                nextSheen[destination + 3] = 1.0f;
            });
        mipData.push_back(std::move(next));
        sheenMipData.push_back(std::move(nextSheen));
    }

    auto createPrefilteredCube = [&](const char* name,
                                 const std::vector<std::vector<float>>& levels,
                                 Diligent::RefCntAutoPtr<Diligent::ITexture>& outTexture,
                                 Diligent::RefCntAutoPtr<Diligent::ITextureView>& outView) {
        Diligent::TextureDesc desc;
        desc.Name = name;
        desc.Type = Diligent::RESOURCE_DIM_TEX_CUBE;
        desc.Width = faceSize;
        desc.Height = faceSize;
        desc.ArraySize = 6;
        desc.MipLevels = mipCount;
        desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
        desc.Usage = Diligent::USAGE_IMMUTABLE;
        desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
        std::array<Diligent::TextureSubResData, 42> faces{};
        std::size_t subresourceIndex = 0;
        for (int face = 0; face < 6; ++face) {
            int currentSize = faceSize;
            for (int mip = 0; mip < mipCount; ++mip) {
            const auto& pixels = levels[static_cast<std::size_t>(mip)];
            faces[subresourceIndex].pData = pixels.data() +
                static_cast<std::size_t>(face) * currentSize * currentSize * 4;
            faces[subresourceIndex].Stride =
                static_cast<Diligent::Uint64>(currentSize * 4 * sizeof(float));
            ++subresourceIndex;
            currentSize = std::max(1, currentSize / 2);
            }
        }
        Diligent::TextureData initialData;
        initialData.pSubResources = faces.data();
        initialData.NumSubresources = static_cast<Diligent::Uint32>(subresourceIndex);
        device->CreateTexture(desc, &initialData, &outTexture);
        if (!outTexture) return false;
        outView = outTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
        return outView.RawPtr() != nullptr;
    };
    if (!createPrefilteredCube("MeshRenderer/EnvironmentCubemap", mipData,
                           texture, view) ||
        !createPrefilteredCube("MeshRenderer/CharlieSheenEnvironmentCubemap",
                           sheenMipData, sheenTexture, sheenView)) {
        return false;
    }

    // Build a low-resolution diffuse irradiance cube from the original HDR
    // image.  This is intentionally kept separate from the specular cube:
    // diffuse IBL needs a cosine convolution, not merely a low mip average.
    constexpr int irradianceSize = 16;
    constexpr int thetaSamples = 4;
    constexpr int phiSamples = 8;
    std::vector<float> irradiance(
        static_cast<std::size_t>(irradianceSize) * irradianceSize * 6 * 4, 0.0f);
    constexpr std::size_t irradianceFacePixelCount =
        static_cast<std::size_t>(irradianceSize) * irradianceSize;
    constexpr std::size_t irradiancePixelCount = irradianceFacePixelCount * 6;
    Parallel::ForSize(0, irradiancePixelCount,
        irradiancePixelCount * thetaSamples * phiSamples, 128,
        [&](std::size_t pixelIndex) {
            const int face = static_cast<int>(pixelIndex / irradianceFacePixelCount);
            const auto localPixel = pixelIndex % irradianceFacePixelCount;
            const int y = static_cast<int>(localPixel / irradianceSize);
            const int x = static_cast<int>(localPixel % irradianceSize);
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
        });
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
    return view.RawPtr() != nullptr && irradianceView.RawPtr() != nullptr &&
           sheenView.RawPtr() != nullptr;
}

bool createBrdfLut(ArtifactCore::GpuContext& context,
                   Diligent::RefCntAutoPtr<Diligent::ITexture>& texture,
                   Diligent::RefCntAutoPtr<Diligent::ITextureView>& view)
{
    auto* device = context.RenderDevice();
    if (!device) return false;
    constexpr int size = 64;
    constexpr int lutWidth = size * 2;
    constexpr int samples = 128;
    std::vector<float> lut(static_cast<std::size_t>(lutWidth) * size * 4, 0.0f);
    auto radicalInverse = [](std::uint32_t bits) {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    };
    auto sheenLambda = [](float cosine, float alphaG) {
        const float x = std::clamp(cosine, 0.0f, 1.0f);
        const float oneMinusAlphaSquared = (1.0f - alphaG) * (1.0f - alphaG);
        const float a = std::lerp(21.5473f, 25.3245f, oneMinusAlphaSquared);
        const float b = std::lerp(3.82987f, 3.32435f, oneMinusAlphaSquared);
        const float c = std::lerp(0.19823f, 0.16801f, oneMinusAlphaSquared);
        const float d = std::lerp(-1.97760f, -1.27393f, oneMinusAlphaSquared);
        const float e = std::lerp(-4.32054f, -4.85967f, oneMinusAlphaSquared);
        const auto l = [=](float value) {
            return a / (1.0f + b * std::pow(std::max(value, 1.0e-5f), c)) +
                   d * value + e;
        };
        return x < 0.5f ? std::exp(l(x))
                        : std::exp(2.0f * l(0.5f) - l(1.0f - x));
    };
    for (int y = 0; y < size; ++y) {
        const float roughness = (static_cast<float>(y) + 0.5f) / size;
        const float alpha = roughness * roughness;
        std::array<float, size> singleScatterAlbedo{};
        for (int x = 0; x < size; ++x) {
            const float nDotV = (static_cast<float>(x) + 0.5f) / size;
            const float viewX = std::sqrt(std::max(0.0f, 1.0f - nDotV * nDotV));
            float scale = 0.0f;
            float bias = 0.0f;
            float sheenDirectionalAlbedo = 0.0f;
            float diffuseDirectionalAlbedo = 0.0f;
            const float sheenAlphaG = std::max(alpha, 1.0e-4f);
            const float sheenInverseRoughness = 1.0f / sheenAlphaG;
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
                const float nDotL = std::max(0.0f, lightZ);
                if (nDotL > 0.0f) {
                    // The half-vector is sampled from the GGX NDF. Convert its
                    // PDF to the reflected-light PDF when integrating the BRDF;
                    // omitting VdotH / (NdotH * NdotV) biases the split-sum LUT
                    // toward an overly bright, view-insensitive environment.
                    const float geometryK = (roughness + 1.0f) *
                        (roughness + 1.0f) * 0.125f;
                    const float geometryV = nDotV / std::max(
                        nDotV * (1.0f - geometryK) + geometryK, 1e-4f);
                    const float geometryL = nDotL / std::max(
                        nDotL * (1.0f - geometryK) + geometryK, 1e-4f);
                    const float visibility = geometryV * geometryL * viewDotHalf /
                        std::max(halfVector.z * nDotV, 1e-4f);
                    const float fresnel = std::pow(1.0f - viewDotHalf, 5.0f);
                    scale += visibility * (1.0f - fresnel);
                    bias += visibility * fresnel;
                }

                const float diskRadius = std::sqrt(xi1);
                const float sheenLightX = diskRadius * std::cos(phi);
                const float sheenLightZ = std::sqrt(std::max(0.0f, 1.0f - xi1));
                const float sheenViewDotLight = std::max(
                    nDotV * sheenLightZ + viewX * sheenLightX, 0.0f);
                const float halfLength = std::sqrt(std::max(
                    1.0e-8f, 2.0f + 2.0f * sheenViewDotLight));
                const float nDotH = std::clamp(
                    (sheenLightZ + nDotV) / halfLength, 0.0f, 1.0f);
                const float sheenSin2H = std::max(1.0f - nDotH * nDotH, 1.0e-6f);
                const float sheenDistribution =
                    (2.0f + sheenInverseRoughness) *
                    std::pow(sheenSin2H, 0.5f * sheenInverseRoughness) /
                    (2.0f * std::numbers::pi_v<float>);
                const float sheenVisibility = 1.0f /
                    ((1.0f + sheenLambda(nDotV, sheenAlphaG) +
                      sheenLambda(sheenLightZ, sheenAlphaG)) *
                     (4.0f * std::max(nDotV, 1.0e-4f) *
                      std::max(sheenLightZ, 1.0e-4f)));
                sheenDirectionalAlbedo += sheenDistribution * sheenVisibility *
                                          std::numbers::pi_v<float>;

                const float diffuseXi1 =
                    (static_cast<float>(sample) + 0.5f) / samples;
                const float diffuseXi2 = radicalInverse(sample);
                const float diffuseSinTheta = std::sqrt(diffuseXi1);
                const float diffusePhi = 2.0f * std::numbers::pi_v<float> *
                                         diffuseXi2;
                const EnvironmentFloat3 diffuseLight{
                    diffuseSinTheta * std::cos(diffusePhi),
                    diffuseSinTheta * std::sin(diffusePhi),
                    std::sqrt(std::max(0.0f, 1.0f - diffuseXi1))};
                const EnvironmentFloat3 diffuseView{viewX, 0.0f, nDotV};
                const EnvironmentFloat3 diffuseHalf{
                    diffuseLight.x + diffuseView.x,
                    diffuseLight.y + diffuseView.y,
                    diffuseLight.z + diffuseView.z};
                const float inverseHalfLength = 1.0f / std::sqrt(std::max(
                    diffuseHalf.x * diffuseHalf.x +
                    diffuseHalf.y * diffuseHalf.y +
                    diffuseHalf.z * diffuseHalf.z, 1.0e-8f));
                const float lightDotHalf = std::clamp(
                    (diffuseLight.x * diffuseHalf.x +
                     diffuseLight.y * diffuseHalf.y +
                     diffuseLight.z * diffuseHalf.z) * inverseHalfLength,
                    0.0f, 1.0f);
                const float diffuseF90 = 0.5f + 2.0f * roughness *
                                         lightDotHalf * lightDotHalf;
                const float diffuseLightScatter = 1.0f + (diffuseF90 - 1.0f) *
                    std::pow(1.0f - diffuseLight.z, 5.0f);
                const float diffuseViewScatter = 1.0f + (diffuseF90 - 1.0f) *
                    std::pow(1.0f - nDotV, 5.0f);
                diffuseDirectionalAlbedo +=
                    diffuseLightScatter * diffuseViewScatter;
            }
            const std::size_t index =
                (static_cast<std::size_t>(y) * lutWidth + x) * 4;
            lut[index + 0] = scale / samples;
            lut[index + 1] = bias / samples;
            singleScatterAlbedo[static_cast<std::size_t>(x)] =
                lut[index + 0] + lut[index + 1];
            lut[index + 2] = std::clamp(
                sheenDirectionalAlbedo / samples, 0.0f, 1.0f);
            const std::size_t diffuseIndex =
                (static_cast<std::size_t>(y) * lutWidth + size + x) * 4;
            lut[diffuseIndex + 0] = diffuseDirectionalAlbedo / samples;
            lut[diffuseIndex + 3] = 1.0f;
        }
        float averageSingleScatterAlbedo = 0.0f;
        for (int x = 0; x < size; ++x) {
            const float nDotV = (static_cast<float>(x) + 0.5f) / size;
            averageSingleScatterAlbedo +=
                2.0f * singleScatterAlbedo[static_cast<std::size_t>(x)] *
                nDotV / size;
        }
        for (int x = 0; x < size; ++x) {
            const std::size_t index =
                (static_cast<std::size_t>(y) * lutWidth + x) * 4;
            lut[index + 3] = std::clamp(averageSingleScatterAlbedo, 0.0f, 1.0f);
        }
    }
    Diligent::TextureDesc desc;
    desc.Name = "MeshRenderer/BRDFLUT";
    desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    desc.Width = lutWidth;
    desc.Height = size;
    desc.MipLevels = 1;
    desc.Format = Diligent::TEX_FORMAT_RGBA32_FLOAT;
    desc.Usage = Diligent::USAGE_IMMUTABLE;
    desc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    Diligent::TextureSubResData sub;
    sub.pData = lut.data();
    sub.Stride = static_cast<Diligent::Uint64>(lutWidth * 4 * sizeof(float));
    Diligent::TextureData data;
    data.pSubResources = &sub;
    data.NumSubresources = 1;
    device->CreateTexture(desc, &data, &texture);
    if (!texture) return false;
    view = texture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    return view.RawPtr() != nullptr;
}

float clampFinite(float value, float minimum, float maximum,
                  float fallback) noexcept {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum)
                                : fallback;
}
}

const char* MeshVSSource = R"(
struct VSInput {
    float3 pos    : ATTRIB0;
    float3 normal : ATTRIB1;
    float2 uv     : ATTRIB2;
    float4 color  : ATTRIB3;
    float2 uv1    : ATTRIB4;
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
    float4 VertexColor : TEXCOORD7;
    float4 ShadowPosition : TEXCOORD6;
    float2 UV1 : TEXCOORD8;
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
    
    // Normals must use the inverse-transpose of the instance transform.
    // Applying the position matrix directly is only correct for rigid or
    // uniform-scale transforms and skews PBR lobes under non-uniform scale.
    float3x3 transformMatrix = (float3x3)inst.transform;
    float3 cofactor0 = cross(transformMatrix[1], transformMatrix[2]);
    float3 cofactor1 = cross(transformMatrix[2], transformMatrix[0]);
    float3 cofactor2 = cross(transformMatrix[0], transformMatrix[1]);
    float determinant = dot(transformMatrix[0], cofactor0);
    float3x3 normalMatrix = float3x3(cofactor0, cofactor1, cofactor2);
    if (abs(determinant) > 1.0e-8) {
        normalMatrix *= rcp(determinant);
    } else {
        normalMatrix = transformMatrix;
    }
    Out.WorldNormal = mul(In.normal, normalMatrix);
    Out.Normal = mul(Out.WorldNormal, (float3x3)ViewMatrix);
    
    Out.UV = In.uv;
    Out.UV1 = In.uv1;
    Out.Mode = inst.timeOffset;
    Out.Color = inst.color * inst.weight; // weight acts as alpha multiplier
    Out.VertexColor = In.color;
    return Out;
}
)";

// Shadow depth remains independent from surface lighting, while sharing the
// material alpha contract so masked cutouts do not cast solid rectangular
// shadows.
const char* MeshShadowVSSource = R"(
struct VSInput {
    float3 pos : ATTRIB0;
    float2 uv : ATTRIB2;
    float4 color : ATTRIB3;
    float2 uv1 : ATTRIB4;
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

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float instanceAlpha : TEXCOORD1;
    float2 uv1 : TEXCOORD2;
};

VSOutput VSMain(VSInput In, uint InstanceID : SV_InstanceID) {
    InstanceData instance = g_Instances[InstanceID];
    VSOutput Out;
    Out.position = mul(mul(float4(In.pos, 1.0), instance.transform),
                       LightViewProjection);
    Out.uv = In.uv;
    Out.uv1 = In.uv1;
    Out.color = In.color;
    Out.instanceAlpha = instance.color.a;
    return Out;
}
)";

const char* MeshShadowPSSource = R"(
struct PSInput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float instanceAlpha : TEXCOORD1;
    float2 uv1 : TEXCOORD2;
};

Texture2D g_BaseColorTexture : register(t0);
Texture2D g_OpacityTexture : register(t1);
SamplerState g_ShadowMaskSampler : register(s0);

cbuffer MaterialParams : register(b1) {
    float4 MaterialBaseColorFactor;
    float4 EmissionColorStrength;
    float4 PbrFactors;
    float4 PbrTextureFlags;
    float4 PrincipledFactors;
    float4 SpecularTintFactors;
    float4 ThinFilmFactors;
    float4 ClearcoatFactors;
    float4 AlphaSettings;
    float4 UvTransformA;
    float4 UvTransformB;
    float4 ClearcoatNormalUvTransform;
    float4 EnvFactors;
    float4 SheenFactors;
    float4 AnisotropyFactors;
    float4 SubsurfaceFactors;
    float4 SubsurfaceRadius;
    float4 SubsurfaceScatteringFactors;
    float4 ClearcoatTintFactors;
    float4 SurfaceTextureUvTransform;
    float4 NormalTextureUvOffset;
    float4 OcclusionTextureUvTransform;
    float4 EmissionTextureUvTransform;
    float4 OpacityTextureUvTransform;
    float4 BaseColorTextureUvTransform;
    float4 OpacityTextureUvOffset;
    float4 EmissionTextureUvRotation;
    float4 TextureUvSetSelectors;
};

float2 transformShadowUv(float2 uv) {
    float2 scaled = uv * UvTransformA.zw;
    float angle = UvTransformB.x;
    float2 rotated = scaled;
    if (abs(angle) > 1e-5) {
        float2 s = float2(cos(angle), sin(angle));
        float2 centered = scaled - 0.5;
        rotated = float2(centered.x * s.x - centered.y * s.y,
                         centered.x * s.y + centered.y * s.x) + 0.5;
    }
    return rotated + UvTransformA.xy;
}

float2 transformBaseColorUv(float2 uv) {
    const float scale = clamp(UvTransformB.w, 0.01, 10.0);
    const float angle = BaseColorTextureUvTransform.z;
    float2 centered = (uv - 0.5) * scale;
    if (abs(angle) > 1e-5) {
        const float sine = sin(angle);
        const float cosine = cos(angle);
        centered = float2(centered.x * cosine - centered.y * sine,
                          centered.x * sine + centered.y * cosine);
    }
    return centered + 0.5 + BaseColorTextureUvTransform.xy;
}

float2 transformOpacityTextureUv(float2 uv) {
    const float scale = clamp(OpacityTextureUvTransform.x, 0.01, 10.0);
    float2 centered = (uv - 0.5) * scale;
    const float angle = OpacityTextureUvTransform.w;
    if (abs(angle) > 1.0e-5) {
        const float sine = sin(angle);
        const float cosine = cos(angle);
        centered = float2(centered.x * cosine - centered.y * sine,
                          centered.x * sine + centered.y * cosine);
    }
    return centered + 0.5 + OpacityTextureUvOffset.xy;
}

void PSMain(PSInput In) {
    if (PbrTextureFlags.w > 0.5) {
        float2 baseColorMeshUv = BaseColorTextureUvTransform.w > 0.5
            ? In.uv1 : In.uv;
        float2 uv = transformBaseColorUv(transformShadowUv(baseColorMeshUv));
        float2 opacityMeshUv = TextureUvSetSelectors.w > 0.5
            ? In.uv1 : In.uv;
        float2 opacityUv = transformOpacityTextureUv(
            transformShadowUv(opacityMeshUv));
        float baseAlpha = g_BaseColorTexture.Sample(
            g_ShadowMaskSampler, uv).a;
        float opacityAlpha = g_OpacityTexture.Sample(
            g_ShadowMaskSampler, opacityUv).a;
        float alpha = baseAlpha * opacityAlpha * In.color.a *
                      In.instanceAlpha * MaterialBaseColorFactor.a;
        clip(alpha - AlphaSettings.x);
    }
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
    float4 VertexColor : TEXCOORD7;
    float4 ShadowPosition : TEXCOORD6;
    float2 UV1 : TEXCOORD8;
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
TextureCube g_SheenPrefilteredEnvironment : register(t18);
Texture2D g_ClearcoatNormalTexture : register(t19);
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
    float4 MaterialBaseColorFactor;
    float4 EmissionColorStrength;
    float4 PbrFactors;
    float4 PbrTextureFlags;
    float4 PrincipledFactors;
    float4 SpecularTintFactors;
    float4 ThinFilmFactors;
    float4 ClearcoatFactors;
    float4 AlphaSettings;
    float4 UvTransformA;
    float4 UvTransformB;
    float4 ClearcoatNormalUvTransform;
    float4 EnvFactors;
    float4 SheenFactors;
    float4 AnisotropyFactors;
    float4 SubsurfaceFactors;
    float4 SubsurfaceRadius;
    float4 SubsurfaceScatteringFactors;
    float4 ClearcoatTintFactors;
    float4 SurfaceTextureUvTransform;
    float4 NormalTextureUvOffset;
    float4 OcclusionTextureUvTransform;
    float4 EmissionTextureUvTransform;
    float4 OpacityTextureUvTransform;
    float4 BaseColorTextureUvTransform;
    float4 OpacityTextureUvOffset;
    float4 EmissionTextureUvRotation;
    float4 TextureUvSetSelectors;
};

float2 transformMeshUv(float2 uv) {
    float2 scaled = uv * UvTransformA.zw;
    float angle = UvTransformB.x;
    float2 rotated = scaled;
    if (abs(angle) > 1e-5) {
        float2 s = float2(cos(angle), sin(angle));
        float2 centered = scaled - 0.5;
        rotated = float2(centered.x * s.x - centered.y * s.y,
                         centered.x * s.y + centered.y * s.x) + 0.5;
    }
    return rotated + UvTransformA.xy;
}

float2 applyNormalTextureUvOffset(float2 uv) {
    return uv + NormalTextureUvOffset.xy;
}

float2 rotateNormalMapUv(float2 uv) {
    const float angle = UvTransformB.y;
    const float scale = UvTransformB.z;
    if (abs(angle) <= 1e-5 && abs(scale - 1.0) <= 1e-5) {
        return applyNormalTextureUvOffset(uv);
    }
    const float sine = sin(angle);
    const float cosine = cos(angle);
    const float2 center = 0.5 + UvTransformA.xy;
    const float2 centered = (uv - center) * scale;
    return applyNormalTextureUvOffset(
        float2(centered.x * cosine - centered.y * sine,
               centered.x * sine + centered.y * cosine) + center);
}

float2 transformClearcoatNormalUv(float2 uv) {
    const float angle = ClearcoatNormalUvTransform.x;
    const float scale = ClearcoatNormalUvTransform.y;
    if (abs(angle) <= 1e-5 && abs(scale - 1.0) <= 1e-5) {
        return uv + ClearcoatNormalUvTransform.zw;
    }
    const float sine = sin(angle);
    const float cosine = cos(angle);
    const float2 center = 0.5 + UvTransformA.xy;
    const float2 centered = (uv - center) * scale;
    return float2(centered.x * cosine - centered.y * sine,
                  centered.x * sine + centered.y * cosine) + center +
           ClearcoatNormalUvTransform.zw;
}

float2 transformBaseColorUv(float2 uv) {
    const float scale = clamp(UvTransformB.w, 0.01, 10.0);
    const float angle = BaseColorTextureUvTransform.z;
    float2 centered = (uv - 0.5) * scale;
    if (abs(angle) > 1e-5) {
        const float sine = sin(angle);
        const float cosine = cos(angle);
        centered = float2(centered.x * cosine - centered.y * sine,
                          centered.x * sine + centered.y * cosine);
    }
    return centered + 0.5 + BaseColorTextureUvTransform.xy;
}

float2 transformSurfaceTextureUv(float2 uv) {
    const float scale = clamp(SurfaceTextureUvTransform.x, 0.01, 10.0);
    float2 centered = (uv - 0.5) * scale;
    const float angle = SurfaceTextureUvTransform.w;
    if (abs(angle) > 1.0e-5) {
        const float sine = sin(angle);
        const float cosine = cos(angle);
        centered = float2(centered.x * cosine - centered.y * sine,
                          centered.x * sine + centered.y * cosine);
    }
    return centered + 0.5 + SurfaceTextureUvTransform.yz;
}

float2 transformOcclusionTextureUv(float2 uv) {
    const float scale = clamp(OcclusionTextureUvTransform.x, 0.01, 10.0);
    float2 centered = (uv - 0.5) * scale;
    const float angle = OcclusionTextureUvTransform.w;
    if (abs(angle) > 1.0e-5) {
        const float sine = sin(angle);
        const float cosine = cos(angle);
        centered = float2(centered.x * cosine - centered.y * sine,
                          centered.x * sine + centered.y * cosine);
    }
    return centered + 0.5 + OcclusionTextureUvTransform.yz;
}

float2 transformEmissionTextureUv(float2 uv) {
    const float scale = clamp(EmissionTextureUvTransform.x, 0.01, 10.0);
    float2 centered = (uv - 0.5) * scale;
    const float angle = EmissionTextureUvRotation.x;
    if (abs(angle) > 1.0e-5) {
        const float sine = sin(angle);
        const float cosine = cos(angle);
        centered = float2(centered.x * cosine - centered.y * sine,
                          centered.x * sine + centered.y * cosine);
    }
    return centered + 0.5 + EmissionTextureUvTransform.yz;
}

float2 transformOpacityTextureUv(float2 uv) {
    const float scale = clamp(OpacityTextureUvTransform.x, 0.01, 10.0);
    float2 centered = (uv - 0.5) * scale;
    const float angle = OpacityTextureUvTransform.w;
    if (abs(angle) > 1.0e-5) {
        const float sine = sin(angle);
        const float cosine = cos(angle);
        centered = float2(centered.x * cosine - centered.y * sine,
                          centered.x * sine + centered.y * cosine);
    }
    return centered + 0.5 + OpacityTextureUvOffset.xy;
}

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

float4 sampleBrdfLut(float2 uv) {
    const float halfTexel = 0.5 / 128.0;
    const float specularU = clamp(
        saturate(uv.x) * 0.5, halfTexel, 0.5 - halfTexel);
    return g_BrdfLut.SampleLevel(
        g_BaseColorSampler,
        float2(specularU, saturate(uv.y)), 0);
}

float sampleDiffuseDirectionalAlbedo(float NdotV, float roughness) {
    const float halfTexel = 0.5 / 128.0;
    const float diffuseU = clamp(
        0.5 + saturate(NdotV) * 0.5,
        0.5 + halfTexel, 1.0 - halfTexel);
    return max(g_BrdfLut.SampleLevel(
        g_BaseColorSampler,
        float2(diffuseU, saturate(roughness)), 0).r,
        0.0);
}

float3 sampleEnvironmentSpecular(float3 normal, float3 viewDirection,
                                 float roughness, float3 F0, float3 tangent,
                                 float3 bitangent, float anisotropy) {
    float3 N = normalize(normal);
    float3 V = normalize(viewDirection);
    float3 anisotropyDirection = anisotropy >= 0.0 ? bitangent : tangent;
    float3 anisotropicTangent = cross(anisotropyDirection, V);
    float3 bentNormal = N;
    if (dot(anisotropicTangent, anisotropicTangent) > 1.0e-6) {
        float3 anisotropicNormal = normalize(
            cross(anisotropicTangent, anisotropyDirection));
        bentNormal = normalize(lerp(N, anisotropicNormal, anisotropy));
    }
    float3 reflectionDirection = reflect(-V, bentNormal);
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
    float2 brdf = sampleBrdfLut(
        float2(saturate(dot(N, V)), saturate(roughness))).rg;
    // The LUT integrates the Schlick split-sum terms A and B separately;
    // apply the same F0 * A + B expression used by the RT shader.
    return prefiltered * (F0 * brdf.x + brdf.y);
}

float sampleSheenDirectionalAlbedo(float NdotV, float roughness) {
    return saturate(sampleBrdfLut(
        float2(saturate(NdotV), saturate(roughness))).b);
}

float3 evaluateEmissionLayerEnergy(float3 baseNormal, float3 coatNormal,
                                   float3 viewDirection, float metallic) {
    float coatWeight = saturate(ClearcoatFactors.x);
    float sheenWeight = saturate(PrincipledFactors.w) *
        (1.0 - metallic) * (1.0 - saturate(PrincipledFactors.z));
    if (coatWeight <= 0.0 && sheenWeight <= 0.0) {
        return float3(1.0, 1.0, 1.0);
    }
    float3 layerEnergy = float3(1.0, 1.0, 1.0);
    if (coatWeight > 0.0) {
        float coatIor = max(ClearcoatFactors.z, 1.0);
        float coatIorRatio = (coatIor - 1.0) / (coatIor + 1.0);
        float coatF0 = coatIorRatio * coatIorRatio;
        float coatNdotV = saturate(dot(coatNormal, viewDirection));
        float coatFresnel = coatF0 + (1.0 - coatF0) *
            pow(1.0 - coatNdotV, 5.0);
        float3 coatTint = srgbToLinear(saturate(ClearcoatTintFactors.rgb));
        float coatViewCosInternal = sqrt(saturate(
            1.0 - (1.0 - coatNdotV * coatNdotV) / (coatIor * coatIor)));
        float3 coatTintPath = pow(max(coatTint, 1.0e-4),
            rcp(max(coatViewCosInternal, 1.0e-3)));
        layerEnergy *= (1.0 - coatWeight * coatFresnel) *
            lerp(float3(1.0, 1.0, 1.0), coatTintPath, coatWeight);
    }
    if (sheenWeight > 0.0) {
        float3 sheenTint = srgbToLinear(saturate(SheenFactors.yzw));
        float sheenAlbedo = sampleSheenDirectionalAlbedo(
            saturate(dot(baseNormal, viewDirection)), SheenFactors.x);
        float sheenEnergy = saturate(1.0 -
            max(sheenTint.r, max(sheenTint.g, sheenTint.b)) *
            sheenWeight * sheenAlbedo);
        layerEnergy *= sheenEnergy;
    }
    return layerEnergy;
}

float3 evaluateMultipleScatterCompensation(float NdotV, float NdotL,
                                           float roughness, float3 F0) {
    float4 viewLut = sampleBrdfLut(float2(saturate(NdotV), saturate(roughness)));
    float4 lightLut = sampleBrdfLut(float2(saturate(NdotL), saturate(roughness)));
    float Eavg = saturate(viewLut.a);
    float EssV = saturate(viewLut.r + viewLut.g);
    float EssL = saturate(lightLut.r + lightLut.g);
    float3 Fss = F0 + (1.0 - F0) * (1.0 / 21.0);
    float3 Fms = Fss * Eavg /
        max(1.0 - Fss * (1.0 - Eavg), 1e-4);
    return Fms * ((1.0 - EssV) * (1.0 - EssL)) /
        max(3.14159265 * (1.0 - Eavg), 1e-4);
}

float3 evaluateMultipleScatterIbl(float NdotV, float roughness, float3 F0,
                                  float3 diffuseIrradiance) {
    float4 viewLut = sampleBrdfLut(float2(saturate(NdotV), saturate(roughness)));
    float Eavg = saturate(viewLut.a);
    float EssV = saturate(viewLut.r + viewLut.g);
    float3 Fss = F0 + (1.0 - F0) * (1.0 / 21.0);
    float3 Fms = Fss * Eavg /
        max(1.0 - Fss * (1.0 - Eavg), 1e-4);
    // Approximate the incoming-direction Ess term by its cosine-weighted
    // average. The irradiance cube already contains the cosine convolution.
    return diffuseIrradiance * Fms * (1.0 - EssV) / 3.14159265;
}

float3 sampleEnvironmentSheen(float3 normal, float3 viewDirection,
                              float roughness) {
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
    g_SheenPrefilteredEnvironment.GetDimensions(0, width, height, mipCount);
    float lod = saturate(roughness) * max(0.0, float(mipCount) - 1.0);
    float3 prefiltered = g_SheenPrefilteredEnvironment.SampleLevel(
        g_BaseColorSampler, reflectionDirection, lod).rgb;
    float sheenAlbedo = sampleSheenDirectionalAlbedo(
        dot(normalize(normal), normalize(viewDirection)), roughness);
    return prefiltered * sheenAlbedo;
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

float specularOcclusion(float NdotV, float visibility, float roughness) {
    // Lagarde and de Rousiers' roughness-aware approximation from
    // "Moving Frostbite to PBR". Rough specular lobes span a wider cone, so
    // they should lose less reflected environment energy to the same scalar AO.
    const float exponent = exp2(-16.0 * saturate(roughness) - 1.0);
    return saturate(pow(saturate(NdotV) + saturate(visibility), exponent) -
                    1.0 + saturate(visibility));
}

float thinFilmChannelReflectance(float cosIncident, float filmIor,
                                 float substrateIor, float thicknessNm,
                                 float wavelengthNm) {
    const float n0 = 1.0;
    const float n1 = max(filmIor, 1.0001);
    const float n2 = max(substrateIor, 1.0001);
    const float c0 = saturate(cosIncident);
    const float s0 = sqrt(max(1.0 - c0 * c0, 0.0));
    const float s1 = min(s0 / n1, 0.9999);
    const float s2 = min(s0 / n2, 0.9999);
    const float c1 = sqrt(max(1.0 - s1 * s1, 0.0));
    const float c2 = sqrt(max(1.0 - s2 * s2, 0.0));
    const float rs01 = (n0 * c0 - n1 * c1) / max(n0 * c0 + n1 * c1, 1.0e-6);
    const float rp01 = (n1 * c0 - n0 * c1) / max(n1 * c0 + n0 * c1, 1.0e-6);
    const float rs12 = (n1 * c1 - n2 * c2) / max(n1 * c1 + n2 * c2, 1.0e-6);
    const float rp12 = (n2 * c1 - n1 * c2) / max(n2 * c1 + n1 * c2, 1.0e-6);
    const float phase = 12.5663706144 * n1 * max(thicknessNm, 0.0) * c1 /
                        max(wavelengthNm, 1.0);
    const float interference = cos(phase);
    const float rsProduct = rs01 * rs12;
    const float rpProduct = rp01 * rp12;
    const float rs = (rs01 * rs01 + rs12 * rs12 +
                      2.0 * rsProduct * interference) /
                     max(1.0 + rsProduct * rsProduct +
                         2.0 * rsProduct * interference, 1.0e-6);
    const float rp = (rp01 * rp01 + rp12 * rp12 +
                      2.0 * rpProduct * interference) /
                     max(1.0 + rpProduct * rpProduct +
                         2.0 * rpProduct * interference, 1.0e-6);
    return saturate(0.5 * (rs + rp));
}

float3 thinFilmReflectance(float cosIncident, float filmIor,
                           float substrateIor, float thicknessNm) {
    return float3(
        thinFilmChannelReflectance(cosIncident, filmIor, substrateIor,
                                   thicknessNm, 610.0),
        thinFilmChannelReflectance(cosIncident, filmIor, substrateIor,
                                   thicknessNm, 550.0),
        thinFilmChannelReflectance(cosIncident, filmIor, substrateIor,
                                   thicknessNm, 460.0));
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
    float3 tangentCandidate =
        (dpdx * duvdy.y - dpdy * duvdx.y) * inverseDeterminant;
    float3 bitangentCandidate =
        (-dpdx * duvdy.x + dpdy * duvdx.x) * inverseDeterminant;
    tangentCandidate -= N * dot(tangentCandidate, N);
    if (dot(tangentCandidate, tangentCandidate) <= 1.0e-12) {
        return N;
    }
    float3 T = normalize(tangentCandidate);
    float handedness = dot(cross(N, T), bitangentCandidate) < 0.0
        ? -1.0 : 1.0;
    float3 B = normalize(cross(N, T)) * handedness;
    float3 tangentNormal = normalSample * 2.0 - 1.0;
    tangentNormal.xy *= strength;
    tangentNormal = normalize(tangentNormal);
    return normalize(tangentNormal.x * T + tangentNormal.y * B +
                     tangentNormal.z * N);
}

float anisotropicGGXDistribution(float NoH, float ToH, float BoH,
                                 float alphaT, float alphaB) {
    float3 scaledHalf = float3(ToH / alphaT, BoH / alphaB, NoH);
    float denominator = dot(scaledHalf, scaledHalf);
    return rcp(3.14159265 * alphaT * alphaB * denominator * denominator);
}

float anisotropicGGXVisibility(float NoV, float NoL, float ToV, float BoV,
                               float ToL, float BoL, float alphaT,
                               float alphaB) {
    float lambdaV = NoL * length(float3(alphaT * ToV, alphaB * BoV, NoV));
    float lambdaL = NoV * length(float3(alphaT * ToL, alphaB * BoL, NoL));
    return 0.5 / max(lambdaV + lambdaL, 1.0e-6);
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

float3 sceneLinearToMeshOutput(float3 value) {
    value = max(value, 0.0);
#if ARTIFACT_MESH_OUTPUT_HDR
    return value;
#else
    float3 displayLinear = value / (1.0 + value);
#if ARTIFACT_MESH_OUTPUT_SRGB
    return displayLinear;
#else
    return pow(displayLinear, 1.0 / 2.2);
#endif
#endif
}

float sheenLambda(float cosine, float alphaG) {
    float x = saturate(cosine);
    float oneMinusAlphaSquared = (1.0 - alphaG) * (1.0 - alphaG);
    float a = lerp(21.5473, 25.3245, oneMinusAlphaSquared);
    float b = lerp(3.82987, 3.32435, oneMinusAlphaSquared);
    float c = lerp(0.19823, 0.16801, oneMinusAlphaSquared);
    float d = lerp(-1.97760, -1.27393, oneMinusAlphaSquared);
    float e = lerp(-4.32054, -4.85967, oneMinusAlphaSquared);
    float lHalf = a / (1.0 + b * pow(0.5, c)) + d * 0.5 + e;
    float l = a / (1.0 + b * pow(max(x, 1.0e-5), c)) + d * x + e;
    float lComplement = a / (1.0 + b * pow(max(1.0 - x, 1.0e-5), c)) +
                       d * (1.0 - x) + e;
    return x < 0.5 ? exp(l) : exp(2.0 * lHalf - lComplement);
}

float sheenDistribution(float roughness, float NdotH) {
    float alphaG = max(roughness * roughness, 1.0e-4);
    float invR = rcp(alphaG);
    float sin2H = max(1.0 - NdotH * NdotH, 1.0e-6);
    return (2.0 + invR) * pow(sin2H, 0.5 * invR) /
           (2.0 * 3.14159265);
}

float sheenVisibility(float roughness, float NdotV, float NdotL) {
    float alphaG = max(roughness * roughness, 1.0e-4);
    return rcp((1.0 + sheenLambda(NdotV, alphaG) +
                sheenLambda(NdotL, alphaG)) *
               (4.0 * max(NdotV, 1.0e-4) * max(NdotL, 1.0e-4)));
}

// __ARTIFACT_MATERIAL_GRAPH_HELPERS__

float4 PSMain(PSInput In) : SV_Target {
    float2 meshUv = transformMeshUv(In.UV);
    float2 baseColorMeshUv = transformMeshUv(
        BaseColorTextureUvTransform.w > 0.5 ? In.UV1 : In.UV);
    float2 baseColorUv = transformBaseColorUv(baseColorMeshUv);
    float2 surfaceMeshUv = transformMeshUv(
        TextureUvSetSelectors.x > 0.5 ? In.UV1 : In.UV);
    float2 surfaceTextureUv = transformSurfaceTextureUv(surfaceMeshUv);
    float2 normalMeshUv = transformMeshUv(
        NormalTextureUvOffset.z > 0.5 ? In.UV1 : In.UV);
    float2 normalMapUv = rotateNormalMapUv(normalMeshUv);
    float2 clearcoatNormalMeshUv = transformMeshUv(
        NormalTextureUvOffset.w > 0.5 ? In.UV1 : In.UV);
    float2 clearcoatNormalUv = transformClearcoatNormalUv(
        clearcoatNormalMeshUv);
    float4 baseSample = g_BaseColorTexture.Sample(g_BaseColorSampler, baseColorUv);
    float2 opacityMeshUv = transformMeshUv(
        TextureUvSetSelectors.w > 0.5 ? In.UV1 : In.UV);
    float2 opacityTextureUv = transformOpacityTextureUv(opacityMeshUv);
    float4 opacitySample = g_OpacityTexture.Sample(
        g_BaseColorSampler, opacityTextureUv);
    float2 emissionMeshUv = transformMeshUv(
        TextureUvSetSelectors.z > 0.5 ? In.UV1 : In.UV);
    float2 emissionTextureUv = transformEmissionTextureUv(emissionMeshUv);
    float4 emissionSample = g_EmissionTexture.Sample(
        g_BaseColorSampler, emissionTextureUv);
    float4 metallicRoughnessSample =
        g_MetallicRoughnessTexture.Sample(g_BaseColorSampler,
                                          surfaceTextureUv);
    float4 normalTextureSample =
        g_NormalTexture.Sample(g_BaseColorSampler, normalMapUv);
    float3 normalSample = normalTextureSample.xyz;
    float normalMapAverageLength = saturate(normalTextureSample.a);
    float4 clearcoatNormalTextureSample = normalTextureSample;
    float clearcoatNormalMapAverageLength = normalMapAverageLength;
    if (ClearcoatFactors.w > 0.5) {
        clearcoatNormalTextureSample =
            g_ClearcoatNormalTexture.Sample(g_BaseColorSampler,
                                            clearcoatNormalUv);
        clearcoatNormalMapAverageLength =
            saturate(clearcoatNormalTextureSample.a);
    }
    float2 occlusionMeshUv = transformMeshUv(
        TextureUvSetSelectors.y > 0.5 ? In.UV1 : In.UV);
    float2 occlusionTextureUv = transformOcclusionTextureUv(occlusionMeshUv);
    float occlusionSample = g_OcclusionTexture.Sample(
        g_BaseColorSampler, occlusionTextureUv).r;
    // __ARTIFACT_MATERIAL_GRAPH_SAMPLES__
    float3 instanceColor = srgbToLinear(saturate(In.Color.rgb));
    float3 vertexColor = srgbToLinear(saturate(In.VertexColor.rgb));
    float3 materialBaseColor =
        srgbToLinear(saturate(MaterialBaseColorFactor.rgb));
    float vertexAlpha = saturate(In.VertexColor.a);
    float4 baseColor = float4(baseSample.rgb * instanceColor * vertexColor *
                                  materialBaseColor,
                              baseSample.a * In.Color.a * vertexAlpha *
                                  MaterialBaseColorFactor.a);
    float emissionStrength = max(EmissionColorStrength.a, 0.0);
    float3 emissionTint = srgbToLinear(saturate(EmissionColorStrength.rgb));
    float materialAlpha = baseColor.a * opacitySample.a;
    float metallic = saturate(PbrFactors.x *
        lerp(1.0, metallicRoughnessSample.b, PbrTextureFlags.x));
    float roughness = clamp(PbrFactors.y *
        lerp(1.0, metallicRoughnessSample.g, PbrTextureFlags.x), 0.04, 1.0);
    float ao = lerp(1.0,
                    lerp(1.0, occlusionSample, saturate(PbrFactors.w)),
                    PbrTextureFlags.z);
    float3 geometricWorldNormal = normalize(In.WorldNormal);
    float3 worldNormal = computeNormalMapped(
        In.WorldPosition, In.WorldNormal, normalMapUv, normalSample,
        PbrFactors.z, PbrTextureFlags.y);
    float3 viewNormal = computeNormalMapped(
        In.ViewPosition, In.Normal, normalMapUv, normalSample,
        PbrFactors.z, PbrTextureFlags.y);
    float3 dpdx = ddx(In.WorldPosition);
    float3 dpdy = ddy(In.WorldPosition);
    // Match the RT tangent frame: anisotropy follows the final normal-map UV
    // orientation, including its material-specific rotation and scale.
    float2 duvdx = ddx(normalMapUv);
    float2 duvdy = ddy(normalMapUv);
    float uvDeterminant = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
    float3 tangentCandidate = abs(uvDeterminant) > 1.0e-6
        ? (dpdx * duvdy.y - dpdy * duvdx.y) / uvDeterminant
        : cross(abs(worldNormal.y) < 0.9 ? float3(0.0, 1.0, 0.0)
                                         : float3(1.0, 0.0, 0.0), worldNormal);
    float3 worldTangent = normalize(tangentCandidate);
    worldTangent = normalize(worldTangent - worldNormal * dot(worldTangent, worldNormal));
    float3 worldBitangent = normalize(cross(worldNormal, worldTangent)) *
        (uvDeterminant < 0.0 ? -1.0 : 1.0);
    // __ARTIFACT_MATERIAL_GRAPH_OVERRIDE__
    if (PbrTextureFlags.y > 0.5) {
        float normalVariance = max(1.0 - normalMapAverageLength, 0.0) /
                              max(normalMapAverageLength, 1e-4);
        normalVariance *= PbrFactors.z * PbrFactors.z;
        float alpha = roughness * roughness;
        float alphaSquared = alpha * alpha;
        float filteredAlphaSquared = saturate(
            alphaSquared + min(2.0 * normalVariance, 0.2));
        roughness = sqrt(sqrt(filteredAlphaSquared));
    }
    float3 coatNormal = normalize(lerp(
        geometricWorldNormal, worldNormal,
        saturate(ClearcoatTintFactors.w)));
    if (ClearcoatFactors.w > 0.5) {
        coatNormal = computeNormalMapped(
            In.WorldPosition, In.WorldNormal, clearcoatNormalUv,
            clearcoatNormalTextureSample.xyz,
            ClearcoatTintFactors.w, ClearcoatTintFactors.w);
    } else {
        clearcoatNormalMapAverageLength = normalMapAverageLength;
    }
    float clearcoatRoughness = clamp(ClearcoatFactors.y, 0.04, 1.0);
    if ((ClearcoatFactors.w > 0.5 || PbrTextureFlags.y > 0.5) &&
        ClearcoatTintFactors.w > 0.0) {
        float coatNormalVariance =
            max(1.0 - clearcoatNormalMapAverageLength, 0.0) /
            max(clearcoatNormalMapAverageLength, 1e-4);
        coatNormalVariance *= ClearcoatTintFactors.w *
                              ClearcoatTintFactors.w;
        float coatAlpha = clearcoatRoughness * clearcoatRoughness;
        float filteredCoatAlphaSquared = saturate(
            coatAlpha * coatAlpha + min(2.0 * coatNormalVariance, 0.2));
        clearcoatRoughness = sqrt(sqrt(filteredCoatAlphaSquared));
    }
    float anisotropyRotationSin;
    float anisotropyRotationCos;
    sincos(AnisotropyFactors.y, anisotropyRotationSin,
          anisotropyRotationCos);
    float3 unrotatedTangent = worldTangent;
    float3 unrotatedBitangent = worldBitangent;
    worldTangent = unrotatedTangent * anisotropyRotationCos +
                   unrotatedBitangent * anisotropyRotationSin;
    worldBitangent = -unrotatedTangent * anisotropyRotationSin +
                     unrotatedBitangent * anisotropyRotationCos;
    // Clip the final alpha after an optional material-graph override. Surviving
    // Masked samples are opaque even when their source alpha was fractional.
    if (PbrTextureFlags.w > 0.5) {
        clip(materialAlpha - AlphaSettings.x);
    }
    float outputAlpha = PbrTextureFlags.w > 0.5 ? 1.0 : materialAlpha;
    if (In.Mode > 1.5 && In.Mode < 2.5) {
        float3 normalColor = viewNormal * 0.5 + 0.5;
        return float4(normalColor, outputAlpha);
    }
    if (In.Mode > 0.5 && In.Mode < 1.5) {
        return float4(baseColor.rgb, outputAlpha);
    }
    if (In.Mode > 3.5 && In.Mode < 4.5) {
        float3 emissionColor =
            emissionSample.rgb * emissionTint * emissionStrength;
        return float4(sceneLinearToMeshOutput(emissionColor), outputAlpha);
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
    // Position AOV (mode 9): raw world-space position, no encoding.
    // UV AOV (mode 10): raw vertex UV (material uv transform not applied).
    if (In.Mode > 8.5 && In.Mode < 9.5) {
        return float4(In.WorldPosition, 1.0);
    }
    if (In.Mode > 9.5 && In.Mode < 10.5) {
        return float4(In.UV, 0.0, 1.0);
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
                                 (max(PrincipledFactors.y, 1.0) + 1.0), 2.0) *
                             (saturate(PrincipledFactors.x) / 0.5);
        float3 dielectricSpecularF0 = dielectricF0 *
            srgbToLinear(saturate(SpecularTintFactors.rgb));
        float3 F0 = lerp(dielectricSpecularF0, solidBase.rgb, metallic);
        const float specularScale = saturate(PrincipledFactors.x) / 0.5;
        if (ThinFilmFactors.x > 0.0) {
            float3 filmF0 = thinFilmReflectance(
                1.0, ThinFilmFactors.y, PrincipledFactors.y,
                ThinFilmFactors.x) * specularScale *
                srgbToLinear(saturate(SpecularTintFactors.rgb));
            F0 = lerp(filmF0, solidBase.rgb, metallic);
        }
        float transmissionWeight = saturate(PrincipledFactors.z) *
            (1.0 - metallic);
        float transmissionIor = max(PrincipledFactors.y, 1.0);
        float transmissionF0 = pow((transmissionIor - 1.0) /
                                   (transmissionIor + 1.0), 2.0);
        float transmissionFresnel = transmissionF0 +
            (1.0 - transmissionF0) * pow(
                1.0 - saturate(dot(worldNormal, viewDirection)), 5.0);
        float3 transmissionFresnelColor = float3(
            transmissionFresnel, transmissionFresnel, transmissionFresnel);
        if (ThinFilmFactors.x > 0.0) {
            transmissionFresnelColor = thinFilmReflectance(
                saturate(dot(worldNormal, viewDirection)), ThinFilmFactors.y,
                transmissionIor, ThinFilmFactors.x) * specularScale *
                srgbToLinear(saturate(SpecularTintFactors.rgb));
        }
        float3 transmissionDirection = refract(
            -viewDirection, worldNormal, 1.0 / transmissionIor);
        if (dot(transmissionDirection, transmissionDirection) <= 1e-5) {
            transmissionFresnel = 1.0;
            transmissionFresnelColor = float3(1.0, 1.0, 1.0);
        }
        if (EnvironmentSettings.x > 0.5) {
            float sheenWeight = saturate(PrincipledFactors.w) *
                (1.0 - metallic) * (1.0 - saturate(PrincipledFactors.z));
            float3 sheenTint = srgbToLinear(saturate(SheenFactors.yzw));
            float sheenNdotV = saturate(dot(worldNormal, viewDirection));
            float3 environmentF0 = F0;
            if (ThinFilmFactors.x > 0.0) {
                float3 filmViewFresnel = thinFilmReflectance(
                    sheenNdotV, ThinFilmFactors.y, PrincipledFactors.y,
                    ThinFilmFactors.x) * specularScale *
                    srgbToLinear(saturate(SpecularTintFactors.rgb));
                environmentF0 = lerp(filmViewFresnel, solidBase.rgb, metallic);
            }
            float sheenViewAlbedo = sampleSheenDirectionalAlbedo(
                sheenNdotV, SheenFactors.x);
            float sheenBaseEnergy = saturate(
                1.0 - max(sheenTint.r, max(sheenTint.g, sheenTint.b)) *
                    sheenWeight * sheenViewAlbedo);
            float3 diffuseEnergy = (1.0 - environmentF0) * (1.0 - metallic) *
                (1.0 - transmissionWeight);
            float3 diffuseIrradiance = sampleEnvironmentDiffuse(worldNormal);
            float diffuseDirectionalAlbedo = sampleDiffuseDirectionalAlbedo(
                sheenNdotV, roughness);
            diffuseDirectionalAlbedo = lerp(
                diffuseDirectionalAlbedo, 1.0,
                saturate(SubsurfaceFactors.x));
            float3 diffuseColor = lerp(
                solidBase.rgb,
                srgbToLinear(saturate(SubsurfaceFactors.yzw)) *
                    max(SubsurfaceRadius.rgb, 0.0),
                saturate(SubsurfaceFactors.x));
            float subsurfaceIorRatio = (max(SubsurfaceRadius.w, 1.0) - 1.0) /
                                       (max(SubsurfaceRadius.w, 1.0) + 1.0);
            float subsurfaceF0 = subsurfaceIorRatio * subsurfaceIorRatio;
            float3 subsurfaceViewFresnel = subsurfaceF0 +
                (1.0 - subsurfaceF0) * pow(1.0 - sheenNdotV, 5.0);
            float3 subsurfaceDiffuseEnergy =
                (1.0 - subsurfaceViewFresnel) * (1.0 - metallic) *
                (1.0 - transmissionWeight);
            diffuseEnergy = lerp(diffuseEnergy, subsurfaceDiffuseEnergy,
                                 saturate(SubsurfaceFactors.x));
            float3 indirectDiffuse = diffuseIrradiance *
                (diffuseDirectionalAlbedo * diffuseColor * diffuseEnergy *
                 ao * sheenBaseEnergy);
            float3 indirectSpecular = sampleEnvironmentSpecular(
                worldNormal, viewDirection, roughness, environmentF0,
                worldTangent, worldBitangent, AnisotropyFactors.x);
            if (transmissionWeight <= 1e-4) {
                indirectSpecular += evaluateMultipleScatterIbl(
                    sheenNdotV, roughness, environmentF0, diffuseIrradiance);
            }
            indirectSpecular *= sheenBaseEnergy;
            float3 sheenEnvironment = sampleEnvironmentSheen(
                worldNormal, viewDirection, SheenFactors.x) *
                (sheenWeight * sheenTint * specularOcclusion(
                    dot(worldNormal, viewDirection), ao, SheenFactors.x));
            float coatWeight = saturate(ClearcoatFactors.x);
            float3 coatEnvironmentEnergy = float3(1.0, 1.0, 1.0);
            if (coatWeight > 0.0) {
                float coatIor = max(ClearcoatFactors.z, 1.0);
                float coatIorRatio = (coatIor - 1.0) / (coatIor + 1.0);
                float coatF0Value = coatIorRatio * coatIorRatio;
                float coatNdotV = saturate(dot(coatNormal, viewDirection));
                float3 coatTint = srgbToLinear(
                    saturate(ClearcoatTintFactors.rgb));
                float coatViewCosInternal = sqrt(saturate(
                    1.0 - (1.0 - coatNdotV * coatNdotV) /
                        (coatIor * coatIor)));
                float coatViewPathLength = rcp(max(coatViewCosInternal, 1.0e-3));
                float3 coatTintPath = pow(max(coatTint, 1.0e-4),
                                          coatViewPathLength);
                float3 coatF0 = float3(coatF0Value, coatF0Value,
                                       coatF0Value);
                float3 coatReflection = sampleEnvironmentSpecular(
                    coatNormal, viewDirection, clearcoatRoughness, coatF0,
                    worldTangent, worldBitangent, 0.0);
                float coatFresnel = coatF0Value + (1.0 - coatF0Value) *
                    pow(1.0 - coatNdotV, 5.0);
                coatReflection *= coatTintPath;
                coatEnvironmentEnergy = (1.0 - coatWeight * coatFresnel) *
                    lerp(float3(1.0, 1.0, 1.0), coatTintPath, coatWeight);
                indirectDiffuse *= coatEnvironmentEnergy;
                indirectSpecular = indirectSpecular * coatEnvironmentEnergy +
                    coatReflection * coatWeight;
            }
            float3 transmittedEnvironment = sampleEnvironmentTransmission(
                worldNormal, viewDirection, roughness,
                max(PrincipledFactors.y, 1.0), solidBase.rgb) * transmissionWeight;
            transmittedEnvironment *= 1.0 - saturate(transmissionFresnelColor);
            indirectSpecular *= specularOcclusion(
                dot(worldNormal, viewDirection), ao, roughness);
            transmittedEnvironment *= ao;
            ambientColor += (indirectDiffuse + indirectSpecular +
                             sheenEnvironment * coatEnvironmentEnergy +
                             transmittedEnvironment * coatEnvironmentEnergy) *
                max(EnvironmentSettings.y, 0.0) * max(EnvFactors.x, 0.0);
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
                float backLight = saturate(dot(-worldNormal, lightDirection));
                if (transmissionWeight > 0.0 && backLight > 0.0) {
                    float3 transmissionCoatEnergy = float3(1.0, 1.0, 1.0);
                    float coatWeight = saturate(ClearcoatFactors.x);
                    if (coatWeight > 0.0) {
                        float coatIor = max(ClearcoatFactors.z, 1.0);
                        float coatIorRatio = (coatIor - 1.0) /
                                             (coatIor + 1.0);
                        float coatF0 = coatIorRatio * coatIorRatio;
                        float coatNdotV = saturate(
                            dot(coatNormal, viewDirection));
                        float coatFresnel = coatF0 + (1.0 - coatF0) *
                            pow(1.0 - coatNdotV, 5.0);
                        float3 coatTint = srgbToLinear(
                            saturate(ClearcoatTintFactors.rgb));
                        float coatViewCosInternal = sqrt(saturate(
                            1.0 - (1.0 - coatNdotV * coatNdotV) /
                                (coatIor * coatIor)));
                        float3 coatTintPath = pow(max(coatTint, 1.0e-4),
                            rcp(max(coatViewCosInternal, 1.0e-3)));
                        transmissionCoatEnergy =
                            (1.0 - coatWeight * coatFresnel) *
                            lerp(float3(1.0, 1.0, 1.0), coatTintPath,
                                 coatWeight);
                    }
                    float lightShadow = (ShadowSettings.x > 0.5 &&
                        lightIndex == (uint)ShadowSettings.z)
                            ? shadowVisibility : 1.0;
                    directColor += solidBase.rgb * transmissionWeight *
                        radiance * backLight *
                        (1.0 - saturate(transmissionFresnelColor)) *
                        transmissionCoatEnergy * attenuation * lightShadow;
                }
                continue;
            }
            float3 halfVector = normalize(viewDirection + lightDirection);
            float NdotV = max(saturate(dot(worldNormal, viewDirection)), 1e-4);
            float NdotH = max(saturate(dot(worldNormal, halfVector)), 1e-4);
            float VdotH = max(saturate(dot(viewDirection, halfVector)), 1e-4);
            float anisotropy = clamp(AnisotropyFactors.x, -1.0, 1.0);
            float alpha = roughness * roughness;
            float alphaT = max(alpha * (1.0 + anisotropy), 1.0e-3);
            float alphaB = max(alpha * (1.0 - anisotropy), 1.0e-3);
            float distribution = anisotropicGGXDistribution(
                NdotH, dot(halfVector, worldTangent),
                dot(halfVector, worldBitangent), alphaT, alphaB);
            float visibility = anisotropicGGXVisibility(
                NdotV, NdotL, dot(viewDirection, worldTangent),
                dot(viewDirection, worldBitangent),
                dot(lightDirection, worldTangent),
                dot(lightDirection, worldBitangent), alphaT, alphaB);
            float3 fresnel = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
            if (ThinFilmFactors.x > 0.0) {
                float3 filmFresnel = thinFilmReflectance(
                    VdotH, ThinFilmFactors.y, PrincipledFactors.y,
                    ThinFilmFactors.x) * specularScale *
                    srgbToLinear(saturate(SpecularTintFactors.rgb));
                fresnel = lerp(filmFresnel, solidBase.rgb, metallic);
            }
            float3 specular = distribution * visibility * fresnel;
            if (transmissionWeight <= 1e-4) {
                specular += evaluateMultipleScatterCompensation(
                    NdotV, NdotL, roughness, F0);
            }
            float coatWeight = saturate(ClearcoatFactors.x);
            float coatIor = max(ClearcoatFactors.z, 1.0);
            float coatIorRatio = (coatIor - 1.0) / (coatIor + 1.0);
            float coatF0 = coatIorRatio * coatIorRatio;
            float coatNdotV = max(saturate(dot(coatNormal, viewDirection)),
                                  1.0e-4);
            float coatNdotL = max(saturate(dot(coatNormal, lightDirection)),
                                  1.0e-4);
            float3 coatHalfVector = normalize(viewDirection + lightDirection);
            float coatNdotH = max(saturate(dot(coatNormal, coatHalfVector)),
                                  1.0e-4);
            float3 coatTint = srgbToLinear(
                saturate(ClearcoatTintFactors.rgb));
            float coatViewCosInternal = sqrt(saturate(
                1.0 - (1.0 - coatNdotV * coatNdotV) /
                    (coatIor * coatIor)));
            float coatLightCosInternal = sqrt(saturate(
                1.0 - (1.0 - coatNdotL * coatNdotL) /
                    (coatIor * coatIor)));
            float coatPathLength = 0.5 * (
                rcp(max(coatViewCosInternal, 1.0e-3)) +
                rcp(max(coatLightCosInternal, 1.0e-3)));
            float3 coatTintPath = pow(max(coatTint, 1.0e-4), coatPathLength);
            float coatFresnel = coatF0 + (1.0 - coatF0) *
                pow(1.0 - VdotH, 5.0);
            float3 coatEnergy = (1.0 - coatWeight * coatFresnel) *
                lerp(float3(1.0, 1.0, 1.0), coatTintPath, coatWeight);
            float diffuseF90 = 0.5 + 2.0 * roughness * VdotH * VdotH;
            float diffuseLightScatter = 1.0 + (diffuseF90 - 1.0) *
                pow(1.0 - NdotL, 5.0);
            float diffuseViewScatter = 1.0 + (diffuseF90 - 1.0) *
                pow(1.0 - NdotV, 5.0);
            float3 burleyDiffuse = solidBase.rgb * (1.0 - fresnel) *
                (1.0 - metallic) * (1.0 - saturate(PrincipledFactors.z)) *
                (diffuseLightScatter * diffuseViewScatter / 3.14159265) *
                coatEnergy;
            float diffuseSigma = saturate(SubsurfaceScatteringFactors.y);
            if (diffuseSigma > 1.0e-5) {
                // Preserve the Burley base lobe while applying the Oren-Nayar
                // single-scatter response and albedo-dependent energy return.
                float diffuseOrenA = rcp(3.14159265 + diffuseSigma *
                                         (1.57079633 - 0.66666667));
                float diffuseOrenB = diffuseSigma * diffuseOrenA;
                float diffuseOrenT = dot(lightDirection, viewDirection) -
                                     NdotL * NdotV;
                if (diffuseOrenT > 0.0) {
                    diffuseOrenT /= max(max(NdotL, NdotV), 1.0e-4);
                }
                float diffuseNdotL = saturate(NdotL);
                float diffuseNdotV = saturate(NdotV);
                float diffuseSinL = sqrt(max(
                    1.0 - diffuseNdotL * diffuseNdotL, 0.0));
                float diffuseSinV = sqrt(max(
                    1.0 - diffuseNdotV * diffuseNdotV, 0.0));
                float diffuseGL;
                if (diffuseNdotL < 1.0e-6) {
                    diffuseGL = 0.90412966 - diffuseNdotL;
                } else {
                    float diffuseThetaL = acos(diffuseNdotL);
                    diffuseGL = diffuseSinL * (diffuseThetaL - 0.66666667 -
                        diffuseSinL * diffuseNdotL) +
                        0.66666667 * (diffuseSinL / diffuseNdotL) *
                        (1.0 - diffuseSinL * diffuseSinL * diffuseSinL);
                }
                float diffuseGV;
                if (diffuseNdotV < 1.0e-6) {
                    diffuseGV = 0.90412966 - diffuseNdotV;
                } else {
                    float diffuseThetaV = acos(diffuseNdotV);
                    diffuseGV = diffuseSinV * (diffuseThetaV - 0.66666667 -
                        diffuseSinV * diffuseNdotV) +
                        0.66666667 * (diffuseSinV / diffuseNdotV) *
                        (1.0 - diffuseSinV * diffuseSinV * diffuseSinV);
                }
                float diffuseAverageEnergy = diffuseOrenA * 3.14159265 +
                    ((6.28318531 - 5.6) / 3.0) * diffuseOrenB;
                float diffuseEnergyL = diffuseOrenA * 3.14159265 +
                                       diffuseOrenB * diffuseGL;
                float diffuseEnergyV = diffuseOrenA * 3.14159265 +
                                       diffuseOrenB * diffuseGV;
                float3 diffuseAlbedo = saturate(solidBase.rgb);
                float3 diffuseMultiScatter = (diffuseAlbedo * diffuseAlbedo) /
                    (1.0 - diffuseAlbedo * (1.0 - diffuseAverageEnergy));
                diffuseMultiScatter *= (diffuseAverageEnergy /
                    max(1.0 - diffuseAverageEnergy, 1.0e-7)) /
                    3.14159265;
                float3 diffuseOren = diffuseOrenA +
                    diffuseOrenB * diffuseOrenT + diffuseMultiScatter *
                    (1.0 - diffuseEnergyV) * (1.0 - diffuseEnergyL);
                burleyDiffuse *=
                    diffuseOren * 3.14159265;
            }
            float subsurfaceWeight = saturate(SubsurfaceFactors.x);
            float subsurfaceFss90 = roughness * VdotH * VdotH;
            float subsurfaceFss =
                lerp(1.0, subsurfaceFss90, 1.0 - NdotL) *
                lerp(1.0, subsurfaceFss90, 1.0 - NdotV);
            float subsurfaceScatter = 1.25 *
                (subsurfaceFss * (rcp(max(NdotL + NdotV, 1.0e-4)) - 0.5) +
                 0.5);
            float3 subsurfaceColor = srgbToLinear(
                saturate(SubsurfaceFactors.yzw));
            float subsurfaceIorRatio = (max(SubsurfaceRadius.w, 1.0) - 1.0) /
                                       (max(SubsurfaceRadius.w, 1.0) + 1.0);
            float subsurfaceF0 = subsurfaceIorRatio * subsurfaceIorRatio;
            float3 subsurfaceFresnel = subsurfaceF0 +
                (1.0 - subsurfaceF0) * pow(1.0 - VdotH, 5.0);
            float subsurfaceAnisotropy = clamp(
                SubsurfaceScatteringFactors.x, -0.9, 0.9);
            float subsurfaceCosTheta = clamp(
                dot(-lightDirection, viewDirection) /
                    max(length(lightDirection), 1.0e-4),
                -1.0, 1.0);
            float subsurfacePhaseDenom = max(
                1.0 + subsurfaceAnisotropy * subsurfaceAnisotropy -
                    2.0 * subsurfaceAnisotropy * subsurfaceCosTheta,
                0.05);
            float subsurfacePhase = clamp(
                (1.0 - subsurfaceAnisotropy * subsurfaceAnisotropy) /
                    pow(subsurfacePhaseDenom, 1.5), 0.05, 4.0);
            float subsurfaceNeutralPhase = 1.0 /
                pow(max(1.0 + subsurfaceCosTheta * subsurfaceCosTheta, 0.05), 1.5);
            float subsurfacePhaseScale = subsurfacePhase /
                max(subsurfaceNeutralPhase, 1.0e-3);
            float3 subsurfaceDiffuse = subsurfaceColor *
                (1.0 - subsurfaceFresnel) *
                (1.0 - metallic) * (1.0 - saturate(PrincipledFactors.z)) *
                (subsurfaceScatter * subsurfacePhaseScale / 3.14159265) *
                coatEnergy *
                max(SubsurfaceRadius.rgb, 0.0);
            float3 diffuse = lerp(burleyDiffuse, subsurfaceDiffuse,
                                  subsurfaceWeight);
            float sheenWeight = saturate(PrincipledFactors.w) *
                (1.0 - metallic) * (1.0 - saturate(PrincipledFactors.z));
            float3 sheenTint = srgbToLinear(saturate(SheenFactors.yzw));
            float sheenLobe = sheenWeight *
                sheenDistribution(SheenFactors.x, NdotH) *
                sheenVisibility(SheenFactors.x, NdotV, NdotL);
            float3 sheen = sheenTint * sheenLobe;
            float sheenViewAlbedo = sampleSheenDirectionalAlbedo(
                NdotV, SheenFactors.x);
            float sheenLightAlbedo = sampleSheenDirectionalAlbedo(
                NdotL, SheenFactors.x);
            float sheenBaseEnergy = saturate(1.0 - sheenWeight *
                max(sheenTint.r, max(sheenTint.g, sheenTint.b)) *
                max(sheenViewAlbedo, sheenLightAlbedo));
            diffuse *= sheenBaseEnergy;
            specular *= sheenBaseEnergy;
            specular *= coatEnergy;
            sheen *= coatEnergy;
            float coatAlpha = clearcoatRoughness * clearcoatRoughness;
            float coatAlpha2 = coatAlpha * coatAlpha;
            float coatGeometryK = (clearcoatRoughness + 1.0) *
                                  (clearcoatRoughness + 1.0) * 0.125;
            float coatGeometryV = coatNdotV /
                lerp(coatNdotV, 1.0, coatGeometryK);
            float coatGeometryL = coatNdotL /
                lerp(coatNdotL, 1.0, coatGeometryK);
            float coatDenom = coatNdotH * coatNdotH *
                (coatAlpha2 - 1.0) + 1.0;
            float coatDistribution = coatAlpha2 /
                max(3.14159265 * coatDenom * coatDenom, 1e-4);
            float coatSpecular = coatDistribution * coatGeometryV * coatGeometryL /
                max(4.0 * NdotV * NdotL, 1e-4);
            specular += coatSpecular * coatWeight * coatFresnel * coatTintPath;
            float lightShadow = (ShadowSettings.x > 0.5 &&
                                 lightIndex == (uint)ShadowSettings.z)
                                    ? shadowVisibility
                                    : 1.0;
            directColor += (diffuse + specular + sheen) * radiance *
                           NdotL * attenuation * lightShadow;
        }
        float3 emissionColor = emissionSample.rgb * emissionTint *
                               emissionStrength;
        emissionColor *= evaluateEmissionLayerEnergy(
            worldNormal, coatNormal, viewDirection, metallic);
        return float4(sceneLinearToMeshOutput(
                          directColor + ambientColor + emissionColor),
                      outputAlpha);
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
    float3 studioViewDirection = normalize(-In.ViewPosition);
    float3 studioCoatNormal = normalize(lerp(
        normalize(In.Normal), viewNormal,
        saturate(ClearcoatTintFactors.w)));
    studioEmission *= evaluateEmissionLayerEnergy(
        viewNormal, studioCoatNormal, studioViewDirection, metallic);
    float3 studioHdr = (solidBase.rgb * studioLight * cavity +
                        rimColor * rim * 0.10 +
                        lerp(float3(1.0, 1.0, 1.0), solidBase.rgb,
                             metallic) * studioSpecular) * ao +
                       studioEmission;
    return float4(sceneLinearToMeshOutput(studioHdr), outputAlpha);
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

    using MaterialConstants = MeshRenderer::MaterialState;

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
    static_assert(sizeof(MaterialConstants) == sizeof(float) * 108);
    static_assert(sizeof(MeshletConstants) == sizeof(float) * 52);

    Diligent::RefCntAutoPtr<Diligent::IPipelineStateCache>    pPSOCache_;
    // Blender-style material graph splices (compileMaterialGraph output).
    // Empty block = fixed PBR path. Applied on next createPSO().
    std::string materialGraphHelpers_;
    std::string materialGraphBlock_;
    // Last graph JSON handed in (layer-owned). Recompiles only on change.
    std::string materialGraphJson_;
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
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pUV1Buffer_;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                pColorBuffer_;
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
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pWhiteBaseColorTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pWhiteBaseColorTextureSRV_;
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
    Diligent::RefCntAutoPtr<Diligent::ITexture>               pClearcoatNormalTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pClearcoatNormalTextureSRV_;
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
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pSheenPrefilteredEnvironmentSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITextureView>           pBrdfLutSRV_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>                pEnvironmentFallbackTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>                pEnvironmentTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>                pSheenEnvironmentTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>                pIrradianceTexture_;
    Diligent::RefCntAutoPtr<Diligent::ITexture>                pBrdfLutTexture_;
    std::array<Diligent::RefCntAutoPtr<Diligent::ITexture>, MaxSceneLights>
        pGoboTextures_;
    std::array<Diligent::RefCntAutoPtr<Diligent::ITextureView>, MaxSceneLights>
        pGoboTextureSRVs_;
    std::array<Diligent::RefCntAutoPtr<Diligent::ITextureView>, MaxSceneLights>
        pGoboProjectionTextureSRVs_;
    std::array<QString, MaxSceneLights> goboTexturePaths_;
    std::array<std::string, MaxSceneLights> goboProjectionSourceKeys_;
    std::array<std::uint64_t, MaxSceneLights> goboProjectionRevisions_{};
    
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
    // Multi-material index spans and scalar state. Geometry spans stay stable
    // with topology; slot buffers are retained and updated only when their
    // material state changes. Empty means the whole mesh uses the shared state.
    QVector<MeshRenderer::MaterialRange> materialRanges;
    QVector<Diligent::RefCntAutoPtr<Diligent::IBuffer>> materialSlotBuffers;
    struct MaterialSlotTexture {
        QString path;
        Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
        Diligent::RefCntAutoPtr<Diligent::ITextureView> srv;
    };
    struct MaterialSlotTextures {
        MaterialSlotTexture baseColor;
        MaterialSlotTexture metallicRoughness;
        MaterialSlotTexture normal;
        MaterialSlotTexture clearcoatNormal;
        MaterialSlotTexture occlusion;
        MaterialSlotTexture emission;
        MaterialSlotTexture opacity;
    };
    QVector<MaterialSlotTextures> materialSlotTextures;
    QString baseColorTexturePath_;
    QString opacityTexturePath_;
    QString emissionTexturePath_;
    QString metallicRoughnessTexturePath_;
    QString normalTexturePath_;
    QString clearcoatNormalTexturePath_;
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
    pImpl_->pUV1Buffer_.Release();
    pImpl_->pColorBuffer_.Release();
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
    // Only positions are read through an SRV by the mesh-shader path.
    // Diligent rejects BUFFER_MODE_UNDEFINED for an SRV-capable buffer, so
    // keep that buffer structured and do not advertise unused SRVs for the
    // fixed-function normal, UV, and index inputs.
    const auto positionBindFlags = BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE |
        (rayTracingSupported ? BIND_RAY_TRACING : BIND_NONE);
    const auto vertexBindFlags = BIND_VERTEX_BUFFER |
        (rayTracingSupported ? BIND_RAY_TRACING : BIND_NONE);
    const auto indexBindFlags = BIND_INDEX_BUFFER |
        (rayTracingSupported ? BIND_RAY_TRACING : BIND_NONE);
    
    // 1. Position buffer (always needed)
    // position/normal/uv/color are rewritten every frame for animated meshes,
    // so they are dynamic+CPU-writable and uploaded through MapBuffer. The
    // index buffer is never rewritten for an unchanged topology and stays
    // default+CPU_ACCESS_NONE.
    if (vertexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Position Buffer";
        BuffDesc.Usage             = USAGE_DYNAMIC;
        BuffDesc.Size              = sizeof(float) * 3 * vertexCount_;
        BuffDesc.BindFlags         = positionBindFlags;
        BuffDesc.CPUAccessFlags    = CPU_ACCESS_WRITE;
        BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
        BuffDesc.ElementByteStride = sizeof(float) * 3;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pPositionBuffer_);
    }
    
    // 2. Normal buffer
    if (vertexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Normal Buffer";
        BuffDesc.Usage             = USAGE_DYNAMIC;
        BuffDesc.Size              = sizeof(float) * 3 * vertexCount_;
        BuffDesc.BindFlags         = vertexBindFlags;
        BuffDesc.CPUAccessFlags    = CPU_ACCESS_WRITE;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pNormalBuffer_);
    }
    // 3. UV buffer
    if (vertexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh UV Buffer";
        BuffDesc.Usage             = USAGE_DYNAMIC;
        BuffDesc.Size              = sizeof(float) * 2 * vertexCount_;
        BuffDesc.BindFlags         = vertexBindFlags;
        BuffDesc.CPUAccessFlags    = CPU_ACCESS_WRITE;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pUVBuffer_);
        BuffDesc.Name = "Mesh UV1 Buffer";
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pUV1Buffer_);
    }
    // 3b. Vertex color buffer (float4; white = identity when no source colors)
    if (vertexCount_ > 0) {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Mesh Vertex Color Buffer";
        BuffDesc.Usage             = USAGE_DYNAMIC;
        BuffDesc.Size              = sizeof(float) * 4 * vertexCount_;
        BuffDesc.BindFlags         = vertexBindFlags;
        BuffDesc.CPUAccessFlags    = CPU_ACCESS_WRITE;
        BuffDesc.Mode              = BUFFER_MODE_UNDEFINED;
        pDevice->CreateBuffer(BuffDesc, nullptr, &pImpl_->pColorBuffer_);
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
        const Uint32 supportedAnisotropy =
            pDevice->GetAdapterInfo().Sampler.MaxAnisotropy;
        if (supportedAnisotropy > 1) {
            samplerDesc.MinFilter = FILTER_TYPE_ANISOTROPIC;
            samplerDesc.MagFilter = FILTER_TYPE_ANISOTROPIC;
            samplerDesc.MipFilter = FILTER_TYPE_ANISOTROPIC;
            samplerDesc.MaxAnisotropy =
                std::min<Uint32>(16, supportedAnisotropy);
        } else {
            samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
            samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
            samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
            samplerDesc.MaxAnisotropy = 1;
        }
        samplerDesc.AddressU = TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = TEXTURE_ADDRESS_WRAP;
        samplerDesc.ComparisonFunc = COMPARISON_FUNC_ALWAYS;
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
            pImpl_->pWhiteBaseColorTexture_ = pImpl_->pBaseColorTexture_;
            pImpl_->pWhiteBaseColorTextureSRV_ = pImpl_->pBaseColorTextureSRV_;
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
    if (!pImpl_->pClearcoatNormalTextureSRV_) {
        pImpl_->pClearcoatNormalTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
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
        TextureDesc texDesc;
        texDesc.Name = "MeshRenderer_LitShadowFallback";
        texDesc.Type = RESOURCE_DIM_TEX_2D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = TEX_FORMAT_D32_FLOAT;
        texDesc.Usage = USAGE_DEFAULT;
        texDesc.BindFlags = BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE;
        pDevice->CreateTexture(texDesc, nullptr, &pImpl_->pShadowFallbackTexture_);
        if (pImpl_->pShadowFallbackTexture_) {
            pImpl_->pShadowFallbackSRV_ = pImpl_->pShadowFallbackTexture_
                ->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
            RefCntAutoPtr<ITextureView> fallbackDSV;
            fallbackDSV = pImpl_->pShadowFallbackTexture_->GetDefaultView(
                TEXTURE_VIEW_DEPTH_STENCIL);
            if (fallbackDSV && context_.DeviceContext()) {
                context_.DeviceContext()->ClearDepthStencil(
                    fallbackDSV, CLEAR_DEPTH_FLAG, 1.0f, 0,
                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            }
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
            pImpl_->pSheenPrefilteredEnvironmentSRV_ = view;
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
    
    // Opaque and Masked ranges write full coverage. Blended ranges use the
    // transparent PSO created from this state below.
    auto& RT0 = PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0];
    RT0.BlendEnable = false;
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
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = 5;
    std::array<LayoutElement, 5> layoutElements;
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
    // Vertex color (float4; white = identity)
    layoutElements[3].HLSLSemantic = "ATTRIB";
    layoutElements[3].InputIndex = 3;
    layoutElements[3].BufferSlot = 3;
    layoutElements[3].NumComponents = 4;
    layoutElements[3].ValueType = VT_FLOAT32;
    layoutElements[3].IsNormalized = false;
    layoutElements[4].HLSLSemantic = "ATTRIB";
    layoutElements[4].InputIndex = 4;
    layoutElements[4].BufferSlot = 4;
    layoutElements[4].NumComponents = 2;
    layoutElements[4].ValueType = VT_FLOAT32;
    layoutElements[4].IsNormalized = false;
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = layoutElements.data();
    
    // Compile Shaders
    RefCntAutoPtr<IShader> vs, ps;
    context_.CompileShader(MeshVSSource, SHADER_TYPE_VERTEX, "VSMain", &vs);
    // Material graph splice: the template carries three markers. Helpers go
    // to global scope (HLSL forbids nested function definitions); the block
    // goes right after the texture samples so graphNormalSample/
    // graphOcclusion can override the sample vars before normal/AO
    // evaluation; the epilogue maps the rest onto PSMain locals.
    std::string meshPSFinal(MeshPSSource);
    {
        constexpr std::string_view kHelpersMarker =
            "// __ARTIFACT_MATERIAL_GRAPH_HELPERS__";
        const size_t pos = meshPSFinal.find(kHelpersMarker);
        if (pos != std::string::npos) {
            meshPSFinal.replace(pos, kHelpersMarker.size(),
                                pImpl_->materialGraphHelpers_);
        }
        constexpr std::string_view kSamplesMarker =
            "// __ARTIFACT_MATERIAL_GRAPH_SAMPLES__";
        std::string samplesReplacement;
        if (!pImpl_->materialGraphBlock_.empty()) {
            samplesReplacement = pImpl_->materialGraphBlock_ +
                "    normalSample = graphNormalSample;\n"
                "    occlusionSample = graphOcclusion;\n";
        }
        const size_t samplesPos = meshPSFinal.find(kSamplesMarker);
        if (samplesPos != std::string::npos) {
            meshPSFinal.replace(samplesPos, kSamplesMarker.size(),
                                samplesReplacement);
        }
        constexpr std::string_view kMarker =
            "// __ARTIFACT_MATERIAL_GRAPH_OVERRIDE__";
        // Epilogue only: the block itself was emitted at the samples
        // marker above (emitting it twice would redefine every variable).
        std::string replacement;
        if (!pImpl_->materialGraphBlock_.empty()) {
            replacement =
                "    baseColor = graphBaseColor;\n"
                "    metallic = saturate(graphMetallic);\n"
                "    roughness = clamp(graphRoughness, 0.04f, 1.0f);\n"
                "    materialAlpha = graphBaseColor.a * graphAlpha;\n"
                "    emissionSample = float4(graphEmission, 1.0f);\n"
                "    emissionTint = float3(1.0f, 1.0f, 1.0f);\n"
                "    emissionStrength = 1.0f;\n"
                "    if (graphUseWorldNormal > 0.5f) {\n"
                "        worldNormal = normalize(graphWorldNormal);\n"
                "        viewNormal = normalize(mul(graphWorldNormal, (float3x3)ViewMatrix));\n"
                "    }\n";
        }
        const size_t bodyPos = meshPSFinal.find(kMarker);
        if (bodyPos != std::string::npos) {
            meshPSFinal.replace(bodyPos, kMarker.size(), replacement);
        }
    }
    const bool hdrMeshTarget = renderTargetFormat_ == TEX_FORMAT_RGBA16_FLOAT ||
                               renderTargetFormat_ == TEX_FORMAT_RGBA32_FLOAT;
    const bool srgbMeshTarget = renderTargetFormat_ == TEX_FORMAT_RGBA8_UNORM_SRGB;
    meshPSFinal.insert(0, hdrMeshTarget
        ? "#define ARTIFACT_MESH_OUTPUT_HDR 1\n#define ARTIFACT_MESH_OUTPUT_SRGB 0\n"
        : srgbMeshTarget
            ? "#define ARTIFACT_MESH_OUTPUT_HDR 0\n#define ARTIFACT_MESH_OUTPUT_SRGB 1\n"
            : "#define ARTIFACT_MESH_OUTPUT_HDR 0\n#define ARTIFACT_MESH_OUTPUT_SRGB 0\n");
    context_.CompileShader(meshPSFinal.c_str(), SHADER_TYPE_PIXEL, "PSMain", &ps);
    
    PSOCreateInfo.pVS = vs;
    PSOCreateInfo.pPS = ps;
    
    // Resource layout
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    
    static std::array<ShaderResourceVariableDesc, 23> Vars = {{
        {SHADER_TYPE_VERTEX, "g_Instances", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "MaterialParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
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
        {SHADER_TYPE_PIXEL, "g_SheenPrefilteredEnvironment", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_BrdfLut", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_ClearcoatNormalTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_BaseColorSampler", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
    }};
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars.data();
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = (Uint32)Vars.size();
    
    pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pImpl_->pPSO_);

    PSOCreateInfo.PSODesc.Name = "Mesh Transparent Instancing PSO";
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
    RT0.BlendEnable = true;
    RT0.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    RT0.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    RT0.BlendOp = BLEND_OPERATION_ADD;
    RT0.SrcBlendAlpha = BLEND_FACTOR_ONE;
    RT0.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    RT0.BlendOpAlpha = BLEND_OPERATION_ADD;
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

    const auto& deviceInfo = pDevice->GetDeviceInfo();
    const bool meshShadersSupported =
        deviceInfo.Features.MeshShaders == DEVICE_FEATURE_STATE_ENABLED;
    if (meshShadersSupported) {
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
            // Mesh shaders are an optional acceleration path. Some D3D12
            // drivers report mesh-shader support but reject this PSO, so do
            // not let that optional failure abort creation of a 3D layer.
            try {
                pDevice->CreateGraphicsPipelineState(meshPSOInfo,
                                                     &pImpl_->pMeshletPSO_);
                if (pImpl_->pMeshletPSO_) {
                    pImpl_->pMeshletPSO_->CreateShaderResourceBinding(
                        &pImpl_->pMeshletSRB_, true);
                }
            } catch (const std::exception& error) {
                pImpl_->pMeshletSRB_.Release();
                pImpl_->pMeshletPSO_.Release();
                qWarning() << "[MeshRenderer] meshlet PSO creation failed;"
                              "using indexed mesh fallback:"
                           << error.what();
            }
        }
    }
    qDebug() << "[MeshRenderer] indexed mesh PSO created successfully";
    
    const auto initializeBindings = [&](IPipelineState* pso,
                                        RefCntAutoPtr<IShaderResourceBinding>& srb) {
        if (!pso) {
            return;
        }
        if (auto* var = pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")) {
            var->Set(pImpl_->pConstantBuffer_);
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
        if (srb) {
            if (auto* var = srb->GetVariableByName(
                    SHADER_TYPE_PIXEL, "MaterialParams")) {
                var->Set(pImpl_->pMaterialBuffer_);
            }
        }
    };
    initializeBindings(pImpl_->pPSO_, pImpl_->pSRB_);
    initializeBindings(pImpl_->pTransparentPSO_, pImpl_->pTransparentSRB_);

    // Depth-only pipeline used by the composition renderer's shadow-map
    // prepass. Its pixel shader only clips masked alpha; it writes no color.
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
    std::array<LayoutElement, 4> shadowLayout;
    shadowLayout[0].HLSLSemantic = "ATTRIB";
    shadowLayout[0].InputIndex = 0;
    shadowLayout[0].BufferSlot = 0;
    shadowLayout[0].NumComponents = 3;
    shadowLayout[0].ValueType = VT_FLOAT32;
    shadowLayout[1].HLSLSemantic = "ATTRIB";
    shadowLayout[1].InputIndex = 2;
    shadowLayout[1].BufferSlot = 2;
    shadowLayout[1].NumComponents = 2;
    shadowLayout[1].ValueType = VT_FLOAT32;
    shadowLayout[2].HLSLSemantic = "ATTRIB";
    shadowLayout[2].InputIndex = 3;
    shadowLayout[2].BufferSlot = 3;
    shadowLayout[2].NumComponents = 4;
    shadowLayout[2].ValueType = VT_FLOAT32;
    shadowLayout[3].HLSLSemantic = "ATTRIB";
    shadowLayout[3].InputIndex = 4;
    shadowLayout[3].BufferSlot = 4;
    shadowLayout[3].NumComponents = 2;
    shadowLayout[3].ValueType = VT_FLOAT32;
    shadowPSOInfo.GraphicsPipeline.InputLayout.NumElements =
        static_cast<Uint32>(shadowLayout.size());
    shadowPSOInfo.GraphicsPipeline.InputLayout.LayoutElements = shadowLayout.data();
    RefCntAutoPtr<IShader> shadowVS;
    RefCntAutoPtr<IShader> shadowPS;
    context_.CompileShader(MeshShadowVSSource, SHADER_TYPE_VERTEX, "VSMain",
                           &shadowVS);
    context_.CompileShader(MeshShadowPSSource, SHADER_TYPE_PIXEL, "PSMain",
                           &shadowPS);
    shadowPSOInfo.pVS = shadowVS;
    shadowPSOInfo.pPS = shadowPS;
    shadowPSOInfo.PSODesc.ResourceLayout.DefaultVariableType =
        SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    static std::array<ShaderResourceVariableDesc, 4> shadowVars = {{
        {SHADER_TYPE_VERTEX, "g_Instances", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "MaterialParams", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_BaseColorTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "g_OpacityTexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}
    }};
    shadowPSOInfo.PSODesc.ResourceLayout.Variables = shadowVars.data();
    shadowPSOInfo.PSODesc.ResourceLayout.NumVariables =
        static_cast<Uint32>(shadowVars.size());
    static ImmutableSamplerDesc shadowMaskSampler = [] {
        ImmutableSamplerDesc desc;
        desc.ShaderStages = SHADER_TYPE_PIXEL;
        desc.TextureName = "g_ShadowMaskSampler";
        desc.Desc.MinFilter = FILTER_TYPE_LINEAR;
        desc.Desc.MagFilter = FILTER_TYPE_LINEAR;
        desc.Desc.MipFilter = FILTER_TYPE_LINEAR;
        desc.Desc.AddressU = TEXTURE_ADDRESS_WRAP;
        desc.Desc.AddressV = TEXTURE_ADDRESS_WRAP;
        desc.Desc.AddressW = TEXTURE_ADDRESS_WRAP;
        return desc;
    }();
    shadowPSOInfo.PSODesc.ResourceLayout.ImmutableSamplers = &shadowMaskSampler;
    shadowPSOInfo.PSODesc.ResourceLayout.NumImmutableSamplers = 1;
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
                                      const uint32_t* indices, const float* colors,
                                      const float* uvs1)
{
    auto pContext = context_.DeviceContext();
    if (!pContext) {
        qWarning("[MeshRenderer] updateMeshGeometry skipped because device context is unavailable");
        return;
    }

    // position/normal/uv/color are USAGE_DYNAMIC + CPU_ACCESS_WRITE, so they are
    // filled through MapBuffer instead of a staging UpdateBuffer copy. MAP_WRITE
    // with MAP_FLAG_DISCARD lets the driver orphan the previous contents, which
    // is the same contract the constant buffers below already rely on.
    const auto uploadFloat3 = [&](IBuffer* buffer, const float* source,
                                  size_t elementCount) {
        if (!buffer || !source) {
            return;
        }
        void* mapped = nullptr;
        pContext->MapBuffer(buffer, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
        if (!mapped) {
            qWarning("[MeshRenderer] vertex buffer mapping failed");
            return;
        }
        std::memcpy(mapped, source, sizeof(float) * 3 * elementCount);
        pContext->UnmapBuffer(buffer, MAP_WRITE);
        if (frameCostStats_) ++frameCostStats_->bufferUpdates;
    };
    const auto uploadFloats = [&](IBuffer* buffer, const float* source,
                                  size_t elementCount, size_t floatsPerElement) {
        if (!buffer || !source) {
            return;
        }
        void* mapped = nullptr;
        pContext->MapBuffer(buffer, MAP_WRITE, MAP_FLAG_DISCARD, mapped);
        if (!mapped) {
            qWarning("[MeshRenderer] vertex buffer mapping failed");
            return;
        }
        std::memcpy(mapped, source, sizeof(float) * floatsPerElement * elementCount);
        pContext->UnmapBuffer(buffer, MAP_WRITE);
        if (frameCostStats_) ++frameCostStats_->bufferUpdates;
    };

    if (positions && pImpl_->pPositionBuffer_) {
        uploadFloat3(pImpl_->pPositionBuffer_, positions, vertexCount_);
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
        uploadFloat3(pImpl_->pNormalBuffer_, normals, vertexCount_);
    }

    if (uvs && pImpl_->pUVBuffer_) {
        uploadFloats(pImpl_->pUVBuffer_, uvs, vertexCount_, 2);
    }
    if (uvs1 && pImpl_->pUV1Buffer_) {
        uploadFloats(pImpl_->pUV1Buffer_, uvs1, vertexCount_, 2);
    }

    if (pImpl_->pColorBuffer_) {
        if (colors) {
            uploadFloats(pImpl_->pColorBuffer_, colors, vertexCount_, 4);
        } else {
            // Keep the vertex-color channel identity (white) when the caller
            // has no source colors so existing assets render unchanged.
            std::vector<float> whiteColors(vertexCount_ * 4u, 1.0f);
            uploadFloats(pImpl_->pColorBuffer_, whiteColors.data(), vertexCount_, 4);
        }
    }

    // The index buffer stays CPU_ACCESS_NONE, so it keeps the UpdateBuffer path.
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
size_t MeshRenderer::maxInstances() const noexcept { return maxInstances_; }

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
        if (auto* coatNormalVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ClearcoatNormalTexture")) {
            coatNormalVar->Set(pImpl_->pClearcoatNormalTextureSRV_);
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
        if (auto* envVar = activeSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SheenPrefilteredEnvironment")) {
            envVar->Set(pImpl_->pSheenPrefilteredEnvironmentSRV_);
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
                goboVar->Set(pImpl_->pGoboProjectionTextureSRVs_[lightIndex]
                                 ? pImpl_->pGoboProjectionTextureSRVs_[lightIndex]
                                 : pImpl_->pGoboTextureSRVs_[lightIndex]
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
        IBuffer* pVBs[] = {pImpl_->pPositionBuffer_, pImpl_->pNormalBuffer_, pImpl_->pUVBuffer_, pImpl_->pColorBuffer_, pImpl_->pUV1Buffer_};
        pContext->SetVertexBuffers(0, 5, pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                  SET_VERTEX_BUFFERS_FLAG_RESET);
    }
    
    // Set index buffer if available
    if (pImpl_->pIndexBuffer_) {
        pContext->SetIndexBuffer(pImpl_->pIndexBuffer_, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    prepared_ = true;
}

void MeshRenderer::setMaterialRanges(const MaterialRange* ranges, size_t count) {
    if (!ranges || count == 0) {
        pImpl_->materialRanges.clear();
        pImpl_->materialSlotBuffers.clear();
        pImpl_->materialSlotTextures.clear();
        return;
    }

    const bool resized = pImpl_->materialRanges.size() != count;
    if (resized) {
        pImpl_->materialRanges.resize(static_cast<qsizetype>(count));
        pImpl_->materialSlotBuffers.resize(static_cast<qsizetype>(count));
        pImpl_->materialSlotTextures.resize(static_cast<qsizetype>(count));
    }

    auto* device = context_.RenderDevice();
    auto* context = context_.DeviceContext();
    const auto pathChanged = [](const QString& current, const QString& incoming) {
        return current != incoming.trimmed();
    };
    const auto resolveTexture = [&](size_t currentIndex, auto& target,
                                    const QString& requestedPath,
                                    ITextureView* fallback,
                                    const QString& primaryPath,
                                    const auto& primaryView,
                                    auto member, auto&& loader) {
        const QString path = requestedPath.trimmed();
        if (target.path == path && target.srv) {
            return;
        }
        target.path = path;
        target.texture = nullptr;
        target.srv = nullptr;
        if (path.isEmpty()) {
            target.srv = fallback;
            return;
        }
        if (path == primaryPath && primaryView) {
            target.srv = primaryView;
            return;
        }
        for (qsizetype cachedIndex = 0;
             cachedIndex < static_cast<qsizetype>(currentIndex); ++cachedIndex) {
            const auto& cachedTextures =
                pImpl_->materialSlotTextures[cachedIndex];
            const auto& cached = cachedTextures.*member;
            if (cached.path == path && cached.srv) {
                target.texture = cached.texture;
                target.srv = cached.srv;
                return;
            }
        }
        if (!loader(path, target.texture, target.srv)) {
            target.texture = nullptr;
            target.srv = fallback;
        }
    };
    for (size_t i = 0; i < count; ++i) {
        auto& current = pImpl_->materialRanges[static_cast<qsizetype>(i)];
        const auto& incoming = ranges[i];
        auto& slotTextures =
            pImpl_->materialSlotTextures[static_cast<qsizetype>(i)];
        const bool missingBuffer =
            !pImpl_->materialSlotBuffers[static_cast<qsizetype>(i)];
        const bool texturePathsChanged = resized ||
            pathChanged(current.baseColorTexturePath,
                        incoming.baseColorTexturePath) ||
            pathChanged(current.metallicRoughnessTexturePath,
                        incoming.metallicRoughnessTexturePath) ||
            pathChanged(current.normalTexturePath, incoming.normalTexturePath) ||
            pathChanged(current.clearcoatNormalTexturePath,
                        incoming.clearcoatNormalTexturePath) ||
            pathChanged(current.occlusionTexturePath,
                        incoming.occlusionTexturePath) ||
            pathChanged(current.emissionTexturePath,
                        incoming.emissionTexturePath) ||
            pathChanged(current.opacityTexturePath,
                        incoming.opacityTexturePath);
        const bool stateChanged = resized ||
            missingBuffer ||
            current.firstIndex != incoming.firstIndex ||
            current.indexCount != incoming.indexCount ||
            current.materialIndex != incoming.materialIndex ||
            current.transparent != incoming.transparent ||
            texturePathsChanged ||
            std::memcmp(&current.material, &incoming.material,
                        sizeof(MaterialState)) != 0;
        if (!stateChanged) {
            continue;
        }

        current = incoming;
        resolveTexture(i, slotTextures.baseColor, incoming.baseColorTexturePath,
                       pImpl_->pWhiteBaseColorTextureSRV_.RawPtr(),
                       pImpl_->baseColorTexturePath_,
                       pImpl_->pBaseColorTextureSRV_,
                       &Impl::MaterialSlotTextures::baseColor,
                       [&](const QString& path, auto& texture, auto& srv) {
                           return loadSrgbTexture(
                               context_, path,
                               "MeshRenderer_MaterialSlotBaseColorTexture",
                               texture, srv);
                       });
        resolveTexture(i, slotTextures.metallicRoughness,
                       incoming.metallicRoughnessTexturePath,
                       pImpl_->pLinearWhiteTextureSRV_.RawPtr(),
                       pImpl_->metallicRoughnessTexturePath_,
                       pImpl_->pMetallicRoughnessTextureSRV_,
                       &Impl::MaterialSlotTextures::metallicRoughness,
                       [&](const QString& path, auto& texture, auto& srv) {
                           return loadLinearTexture(
                               context_, path,
                               "MeshRenderer_MaterialSlotMetallicRoughnessTexture",
                               texture, srv);
                       });
        resolveTexture(i, slotTextures.normal, incoming.normalTexturePath,
                       pImpl_->pLinearWhiteTextureSRV_.RawPtr(),
                       pImpl_->normalTexturePath_, pImpl_->pNormalTextureSRV_,
                       &Impl::MaterialSlotTextures::normal,
                       [&](const QString& path, auto& texture, auto& srv) {
                           return loadLinearTexture(
                               context_, path,
                               "MeshRenderer_MaterialSlotNormalTexture",
                               texture, srv, true);
                       });
        resolveTexture(i, slotTextures.clearcoatNormal,
                       incoming.clearcoatNormalTexturePath,
                       pImpl_->pLinearWhiteTextureSRV_.RawPtr(),
                       pImpl_->clearcoatNormalTexturePath_,
                       pImpl_->pClearcoatNormalTextureSRV_,
                       &Impl::MaterialSlotTextures::clearcoatNormal,
                       [&](const QString& path, auto& texture, auto& srv) {
                           return loadLinearTexture(
                               context_, path,
                               "MeshRenderer_MaterialSlotClearcoatNormalTexture",
                               texture, srv, true);
                       });
        resolveTexture(i, slotTextures.occlusion, incoming.occlusionTexturePath,
                       pImpl_->pLinearWhiteTextureSRV_.RawPtr(),
                       pImpl_->occlusionTexturePath_, pImpl_->pOcclusionTextureSRV_,
                       &Impl::MaterialSlotTextures::occlusion,
                       [&](const QString& path, auto& texture, auto& srv) {
                           return loadLinearTexture(
                               context_, path,
                               "MeshRenderer_MaterialSlotOcclusionTexture",
                               texture, srv);
                       });
        resolveTexture(i, slotTextures.emission, incoming.emissionTexturePath,
                       pImpl_->pWhiteBaseColorTextureSRV_.RawPtr(),
                       pImpl_->emissionTexturePath_, pImpl_->pEmissionTextureSRV_,
                       &Impl::MaterialSlotTextures::emission,
                       [&](const QString& path, auto& texture, auto& srv) {
                           return loadEmissionTexture(
                               context_, path,
                               "MeshRenderer_MaterialSlotEmissionTexture",
                               texture, srv);
                       });
        resolveTexture(i, slotTextures.opacity, incoming.opacityTexturePath,
                       pImpl_->pLinearWhiteTextureSRV_.RawPtr(),
                       pImpl_->opacityTexturePath_, pImpl_->pOpacityTextureSRV_,
                       &Impl::MaterialSlotTextures::opacity,
                       [&](const QString& path, auto& texture, auto& srv) {
                           return loadOpacityTexture(
                               context_, path,
                               "MeshRenderer_MaterialSlotOpacityTexture",
                               texture, srv);
                       });
        MaterialState materialForGpu = incoming.material;
        materialForGpu.clearcoatFactors[3] =
            slotTextures.clearcoatNormal.texture ? 1.0f : 0.0f;
        if (!device) {
            pImpl_->materialSlotBuffers[static_cast<qsizetype>(i)].Release();
            continue;
        }

        auto& slotBuffer =
            pImpl_->materialSlotBuffers[static_cast<qsizetype>(i)];
        if (slotBuffer && context) {
            context->UpdateBuffer(
                slotBuffer, 0, sizeof(MaterialState), &materialForGpu,
                RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            continue;
        }
        slotBuffer.Release();

        BufferDesc desc;
        desc.Name = "Mesh Material Slot CB";
        desc.Usage = USAGE_DEFAULT;
        desc.Size = sizeof(MaterialState);
        desc.BindFlags = BIND_UNIFORM_BUFFER;
        desc.CPUAccessFlags = CPU_ACCESS_NONE;
        BufferData initialData;
        initialData.pData = &materialForGpu;
        initialData.DataSize = sizeof(MaterialState);
        initialData.pContext = context;
        device->CreateBuffer(desc, &initialData, &slotBuffer);
    }
}

void MeshRenderer::clearMaterialRanges() {
    pImpl_->materialRanges.clear();
    pImpl_->materialSlotBuffers.clear();
    pImpl_->materialSlotTextures.clear();
}

void MeshRenderer::drawMaterialSlots(IDeviceContext* pContext, size_t instanceCount) {
    if (pImpl_->materialRanges.empty()) {
        draw(pContext, instanceCount);
        return;
    }
    if (!pContext || !prepared_ || instanceCount == 0) return;

    if (!pImpl_->pIndexBuffer_) {
        draw(pContext, instanceCount);
        return;
    }

    instanceCount = std::min(instanceCount, maxInstances_);
    const auto drawPass = [&](bool transparentPass) {
        const IPipelineState* activePSO = transparentPass && pImpl_->pTransparentPSO_
            ? pImpl_->pTransparentPSO_.RawPtr() : pImpl_->pPSO_.RawPtr();
        IShaderResourceBinding* activeSRB = transparentPass && pImpl_->pTransparentSRB_
            ? pImpl_->pTransparentSRB_.RawPtr() : pImpl_->pSRB_.RawPtr();
        if (!activePSO || !activeSRB) {
            if (!pImpl_->missingPipelineWarningIssued_) {
                qWarning("[MeshRenderer] drawMaterialSlots skipped because PSO/SRB is unavailable");
                pImpl_->missingPipelineWarningIssued_ = true;
            }
            return;
        }
        pContext->SetPipelineState(activePSO);
        auto* materialVariable = activeSRB->GetVariableByName(
            SHADER_TYPE_PIXEL, "MaterialParams");
        auto* baseColorVariable = activeSRB->GetVariableByName(
            SHADER_TYPE_PIXEL, "g_BaseColorTexture");
        auto* metallicRoughnessVariable = activeSRB->GetVariableByName(
            SHADER_TYPE_PIXEL, "g_MetallicRoughnessTexture");
        auto* normalVariable = activeSRB->GetVariableByName(
            SHADER_TYPE_PIXEL, "g_NormalTexture");
        auto* clearcoatNormalVariable = activeSRB->GetVariableByName(
            SHADER_TYPE_PIXEL, "g_ClearcoatNormalTexture");
        auto* occlusionVariable = activeSRB->GetVariableByName(
            SHADER_TYPE_PIXEL, "g_OcclusionTexture");
        auto* emissionVariable = activeSRB->GetVariableByName(
            SHADER_TYPE_PIXEL, "g_EmissionTexture");
        auto* opacityVariable = activeSRB->GetVariableByName(
            SHADER_TYPE_PIXEL, "g_OpacityTexture");
        for (qsizetype rangeIndex = 0;
             rangeIndex < pImpl_->materialRanges.size(); ++rangeIndex) {
            const MaterialRange& range = pImpl_->materialRanges[rangeIndex];
            if (range.indexCount == 0) continue;
            const bool masked = range.material.pbrTextureFlags[3] > 0.5f;
            const bool rangeTransparent = !masked &&
                (pImpl_->transparentPass_ || range.transparent);
            if (rangeTransparent != transparentPass) continue;
            if (materialVariable) {
                const auto& slotBuffer = pImpl_->materialSlotBuffers[rangeIndex];
                materialVariable->Set(slotBuffer ? slotBuffer
                                                  : pImpl_->pMaterialBuffer_);
            }
            if (baseColorVariable) {
                const auto& slotTexture =
                    pImpl_->materialSlotTextures[rangeIndex].baseColor;
                baseColorVariable->Set(slotTexture.srv
                                           ? slotTexture.srv
                                           : pImpl_->pBaseColorTextureSRV_);
            }
            if (metallicRoughnessVariable) {
                const auto& slotTexture =
                    pImpl_->materialSlotTextures[rangeIndex].metallicRoughness;
                metallicRoughnessVariable->Set(slotTexture.srv
                    ? slotTexture.srv : pImpl_->pMetallicRoughnessTextureSRV_);
            }
            if (normalVariable) {
                const auto& slotTexture =
                    pImpl_->materialSlotTextures[rangeIndex].normal;
                normalVariable->Set(slotTexture.srv
                    ? slotTexture.srv : pImpl_->pNormalTextureSRV_);
            }
            if (clearcoatNormalVariable) {
                const auto& slotTexture =
                    pImpl_->materialSlotTextures[rangeIndex].clearcoatNormal;
                clearcoatNormalVariable->Set(slotTexture.srv
                    ? slotTexture.srv : pImpl_->pClearcoatNormalTextureSRV_);
            }
            if (occlusionVariable) {
                const auto& slotTexture =
                    pImpl_->materialSlotTextures[rangeIndex].occlusion;
                occlusionVariable->Set(slotTexture.srv
                    ? slotTexture.srv : pImpl_->pOcclusionTextureSRV_);
            }
            if (emissionVariable) {
                const auto& slotTexture =
                    pImpl_->materialSlotTextures[rangeIndex].emission;
                emissionVariable->Set(slotTexture.srv
                    ? slotTexture.srv : pImpl_->pEmissionTextureSRV_);
            }
            if (opacityVariable) {
                const auto& slotTexture =
                    pImpl_->materialSlotTextures[rangeIndex].opacity;
                opacityVariable->Set(slotTexture.srv
                    ? slotTexture.srv : pImpl_->pOpacityTextureSRV_);
            }
            pContext->CommitShaderResources(
                activeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            if (frameCostStats_) ++frameCostStats_->drawCalls;
            DrawIndexedAttribs drawAttrs;
            drawAttrs.NumIndices = range.indexCount;
            drawAttrs.NumInstances = static_cast<Uint32>(instanceCount);
            drawAttrs.IndexType = VT_UINT32;
            drawAttrs.Flags = DRAW_FLAG_NONE;
            drawAttrs.FirstIndexLocation = range.firstIndex;
            drawAttrs.BaseVertex = 0;
            pContext->DrawIndexed(drawAttrs);
        }
        if (materialVariable) materialVariable->Set(pImpl_->pMaterialBuffer_);
        if (baseColorVariable) baseColorVariable->Set(pImpl_->pBaseColorTextureSRV_);
        if (metallicRoughnessVariable) {
            metallicRoughnessVariable->Set(pImpl_->pMetallicRoughnessTextureSRV_);
        }
        if (normalVariable) normalVariable->Set(pImpl_->pNormalTextureSRV_);
        if (clearcoatNormalVariable) {
            clearcoatNormalVariable->Set(pImpl_->pClearcoatNormalTextureSRV_);
        }
        if (occlusionVariable) occlusionVariable->Set(pImpl_->pOcclusionTextureSRV_);
        if (emissionVariable) emissionVariable->Set(pImpl_->pEmissionTextureSRV_);
        if (opacityVariable) opacityVariable->Set(pImpl_->pOpacityTextureSRV_);
    };
    drawPass(false);
    drawPass(true);
}

void MeshRenderer::drawShadowMaterialSlots(IDeviceContext* pContext, size_t instanceCount) {
    if (pImpl_->materialRanges.empty()) {
        drawShadow(pContext, instanceCount);
        return;
    }
    if (!pContext || !pImpl_->shadowPrepared_ || instanceCount == 0) {
        return;
    }
    if (!pImpl_->pIndexBuffer_) {
        drawShadow(pContext, instanceCount);
        return;
    }

    instanceCount = std::min(instanceCount, maxInstances_);
    auto* materialVariable = pImpl_->pShadowSRB_->GetVariableByName(
        SHADER_TYPE_PIXEL, "MaterialParams");
    auto* baseColorVariable = pImpl_->pShadowSRB_->GetVariableByName(
        SHADER_TYPE_PIXEL, "g_BaseColorTexture");
    auto* opacityVariable = pImpl_->pShadowSRB_->GetVariableByName(
        SHADER_TYPE_PIXEL, "g_OpacityTexture");
    for (qsizetype rangeIndex = 0;
         rangeIndex < pImpl_->materialRanges.size(); ++rangeIndex) {
        const MaterialRange& range = pImpl_->materialRanges[rangeIndex];
        if (range.indexCount == 0) continue;
        if (materialVariable) {
            const auto& slotBuffer = pImpl_->materialSlotBuffers[rangeIndex];
            materialVariable->Set(slotBuffer ? slotBuffer
                                              : pImpl_->pMaterialBuffer_);
        }
        if (baseColorVariable) {
            const auto& slotTexture =
                pImpl_->materialSlotTextures[rangeIndex].baseColor;
            baseColorVariable->Set(slotTexture.srv
                                       ? slotTexture.srv
                                       : pImpl_->pBaseColorTextureSRV_);
        }
        if (opacityVariable) {
            const auto& slotTexture =
                pImpl_->materialSlotTextures[rangeIndex].opacity;
            opacityVariable->Set(slotTexture.srv
                                     ? slotTexture.srv
                                     : pImpl_->pOpacityTextureSRV_);
        }
        pContext->CommitShaderResources(
            pImpl_->pShadowSRB_, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawIndexedAttribs drawAttrs;
        drawAttrs.NumIndices = range.indexCount;
        drawAttrs.NumInstances = static_cast<Uint32>(instanceCount);
        drawAttrs.IndexType = VT_UINT32;
        drawAttrs.Flags = DRAW_FLAG_NONE;
        drawAttrs.FirstIndexLocation = range.firstIndex;
        drawAttrs.BaseVertex = 0;
        pContext->DrawIndexed(drawAttrs);
    }
    pImpl_->shadowPrepared_ = false;
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
    if (auto* material = pImpl_->pShadowSRB_->GetVariableByName(
            SHADER_TYPE_PIXEL, "MaterialParams")) {
        material->Set(pImpl_->pMaterialBuffer_);
    }
    if (auto* baseColor = pImpl_->pShadowSRB_->GetVariableByName(
            SHADER_TYPE_PIXEL, "g_BaseColorTexture")) {
        baseColor->Set(pImpl_->pBaseColorTextureSRV_);
    }
    if (auto* opacity = pImpl_->pShadowSRB_->GetVariableByName(
            SHADER_TYPE_PIXEL, "g_OpacityTexture")) {
        opacity->Set(pImpl_->pOpacityTextureSRV_);
    }
    pContext->SetPipelineState(pImpl_->pShadowPSO_);
    pContext->CommitShaderResources(pImpl_->pShadowSRB_,
                                    RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    IBuffer* vertexBuffers[] = {
        pImpl_->pPositionBuffer_, pImpl_->pNormalBuffer_,
        pImpl_->pUVBuffer_, pImpl_->pColorBuffer_, pImpl_->pUV1Buffer_};
    pContext->SetVertexBuffers(0, 5, vertexBuffers, nullptr,
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
            std::cos(light.spotInnerCutoff().value *
                     std::numbers::pi_v<float> / 180.0f);
        gpuLight.attenuationSpot[3] =
            std::cos(light.spotOuterCutoff().value *
                     std::numbers::pi_v<float> / 180.0f);
        gpuLight.areaSize[0] = light.areaWidth().value;
        gpuLight.areaSize[1] = light.areaHeight().value;
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
        gpuLight.goboInfo[1] = light.goboRotation().value *
            std::numbers::pi_v<float> / 180.0f;
        gpuLight.goboInfo[2] = light.goboInvert() ? 1.0f : 0.0f;
        gpuLight.goboInfo[3] = light.type() == LightType::Spot &&
            (pImpl_->pGoboProjectionTextureSRVs_[packedLightIndex] ||
             (!goboPath.isEmpty() && pImpl_->pGoboTextureSRVs_[packedLightIndex]))
                ? 1.0f : 0.0f;
    }
}

void MeshRenderer::setGoboProjectionTexture(
    std::size_t sceneLightIndex, const GoboProjectionTextureInput& input)
{
    if (sceneLightIndex >= Impl::MaxSceneLights) {
        return;
    }
    if (!input.isValid()) {
        clearGoboProjectionTexture(sceneLightIndex);
        return;
    }
    prepared_ = false;
    pImpl_->pGoboProjectionTextureSRVs_[sceneLightIndex] = input.srv;
    pImpl_->goboProjectionSourceKeys_[sceneLightIndex] = input.sourceKey;
    pImpl_->goboProjectionRevisions_[sceneLightIndex] = input.revision;
    if (sceneLightIndex < pImpl_->sceneLighting_.lightingMeta[0]) {
        const bool isSpot = static_cast<LightType>(
            static_cast<int>(pImpl_->sceneLighting_.lights[sceneLightIndex].positionType[3])) ==
            LightType::Spot;
        pImpl_->sceneLighting_.lights[sceneLightIndex].goboInfo[3] =
            isSpot ? 1.0f : 0.0f;
    }
}

void MeshRenderer::clearGoboProjectionTexture(std::size_t sceneLightIndex)
{
    if (sceneLightIndex >= Impl::MaxSceneLights) {
        return;
    }
    prepared_ = false;
    pImpl_->pGoboProjectionTextureSRVs_[sceneLightIndex].Release();
    pImpl_->goboProjectionSourceKeys_[sceneLightIndex].clear();
    pImpl_->goboProjectionRevisions_[sceneLightIndex] = 0;
    if (sceneLightIndex < pImpl_->sceneLighting_.lightingMeta[0]) {
        const auto& light = pImpl_->sceneLighting_.lights[sceneLightIndex];
        const bool isSpot = static_cast<LightType>(
            static_cast<int>(light.positionType[3])) == LightType::Spot;
        pImpl_->sceneLighting_.lights[sceneLightIndex].goboInfo[3] =
            isSpot && pImpl_->pGoboTextureSRVs_[sceneLightIndex] ? 1.0f : 0.0f;
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
                                      float intensity,
                                      ITextureView* sheenPrefilteredEnvironment)
{
    prepared_ = false;
    pImpl_->pIrradianceMapSRV_ = irradianceMap
        ? irradianceMap : pImpl_->pIrradianceMapSRV_;
    pImpl_->pPrefilteredEnvironmentSRV_ = prefilteredEnvironment
        ? prefilteredEnvironment : pImpl_->pPrefilteredEnvironmentSRV_;
    pImpl_->pSheenPrefilteredEnvironmentSRV_ = sheenPrefilteredEnvironment
        ? sheenPrefilteredEnvironment
        : (prefilteredEnvironment ? prefilteredEnvironment
                                  : pImpl_->pSheenPrefilteredEnvironmentSRV_);
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
    pImpl_->pSheenEnvironmentTexture_.Release();
    pImpl_->pIrradianceTexture_.Release();
    pImpl_->pBrdfLutTexture_.Release();
    if (normalizedPath.isEmpty()) {
        setEnvironmentMaps(nullptr, nullptr, nullptr, 0.0f);
        return false;
    }
    RefCntAutoPtr<ITextureView> environmentView;
    RefCntAutoPtr<ITextureView> sheenEnvironmentView;
    RefCntAutoPtr<ITextureView> irradianceView;
    if (!createEnvironmentCube(context_, normalizedPath,
                               pImpl_->pEnvironmentTexture_, environmentView,
                               pImpl_->pIrradianceTexture_, irradianceView,
                               pImpl_->pSheenEnvironmentTexture_, sheenEnvironmentView)) {
        setEnvironmentMaps(nullptr, nullptr, nullptr, 0.0f);
        return false;
    }
    // Bind the generated diffuse irradiance cube and GGX-prefiltered specular
    // cube separately. The BRDF LUT is generated below and replaces the
    // fallback binding when creation succeeds.
    setEnvironmentMaps(irradianceView.RawPtr(), environmentView.RawPtr(),
                       pImpl_->pBrdfLutSRV_.RawPtr(), intensity,
                       sheenEnvironmentView.RawPtr());
    RefCntAutoPtr<ITextureView> brdfView;
    if (createBrdfLut(context_, pImpl_->pBrdfLutTexture_, brdfView)) {
        setEnvironmentMaps(irradianceView.RawPtr(), environmentView.RawPtr(),
                           brdfView.RawPtr(), intensity,
                           sheenEnvironmentView.RawPtr());
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
    pImpl_->pSheenEnvironmentTexture_ = source.pImpl_->pSheenEnvironmentTexture_;
    pImpl_->pIrradianceTexture_ = source.pImpl_->pIrradianceTexture_;
    pImpl_->pBrdfLutTexture_ = source.pImpl_->pBrdfLutTexture_;
    pImpl_->pPrefilteredEnvironmentSRV_ = source.pImpl_->pPrefilteredEnvironmentSRV_;
    pImpl_->pSheenPrefilteredEnvironmentSRV_ =
        source.pImpl_->pSheenPrefilteredEnvironmentSRV_;
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

Diligent::ITextureView* MeshRenderer::sheenEnvironmentMapView() const
{
    return pImpl_->pSheenPrefilteredEnvironmentSRV_.RawPtr();
}

Diligent::ITextureView* MeshRenderer::irradianceMapView() const
{
    return pImpl_->pIrradianceMapSRV_.RawPtr();
}

Diligent::ITextureView* MeshRenderer::brdfLutView() const
{
    return pImpl_->pBrdfLutSRV_.RawPtr();
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
    initData.pSubResources = subResources.data();
    initData.NumSubresources = static_cast<Uint32>(subResources.size());

    pDevice->CreateTexture(texDesc, &initData, &pImpl_->pBaseColorTexture_);
    if (pImpl_->pBaseColorTexture_) {
        pImpl_->pBaseColorTextureSRV_ =
            pImpl_->pBaseColorTexture_->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }
}

void MeshRenderer::setBaseColorUvSet(int uvSet)
{
    prepared_ = false;
    pImpl_->materialConstants_.baseColorTextureUvTransform[3] =
        uvSet == 1 ? 1.0f : 0.0f;
}

void MeshRenderer::setPbrMapUvSets(int metallicRoughnessUvSet,
                                   int occlusionUvSet,
                                   int emissionUvSet,
                                   int opacityUvSet,
                                   int clearcoatNormalUvSet)
{
    prepared_ = false;
    auto& selectors = pImpl_->materialConstants_.textureUvSetSelectors;
    selectors[0] = metallicRoughnessUvSet == 1 ? 1.0f : 0.0f;
    selectors[1] = occlusionUvSet == 1 ? 1.0f : 0.0f;
    selectors[2] = emissionUvSet == 1 ? 1.0f : 0.0f;
    selectors[3] = opacityUvSet == 1 ? 1.0f : 0.0f;
    pImpl_->materialConstants_.normalTextureUvOffset[3] =
        clearcoatNormalUvSet == 1 ? 1.0f : 0.0f;
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
        pImpl_->pWhiteBaseColorTexture_ = pImpl_->pBaseColorTexture_;
        pImpl_->pWhiteBaseColorTextureSRV_ = pImpl_->pBaseColorTextureSRV_;
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

    if (loadEmissionTexture(context_, newPath, "MeshRenderer_EmissionTexture",
                            pImpl_->pEmissionTexture_, pImpl_->pEmissionTextureSRV_)) {
        pImpl_->emissionTexturePath_ = newPath;
        return;
    }
    qWarning() << "[MeshRenderer] Failed to load emission texture:" << newPath;
    clearEmissionTexture();
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
        std::isfinite(strength) ? std::max(strength, 0.0f) : 0.0f;
}

void MeshRenderer::setPbrFactors(float metallic, float roughness,
                                 float normalStrength,
                                 float occlusionStrength)
{
    prepared_ = false;
    pImpl_->materialConstants_.pbrFactors[0] =
        clampFinite(metallic, 0.0f, 1.0f, 0.0f);
    pImpl_->materialConstants_.pbrFactors[1] =
        clampFinite(roughness, 0.0f, 1.0f, 0.5f);
    pImpl_->materialConstants_.pbrFactors[2] =
        std::isfinite(normalStrength) ? std::max(normalStrength, 0.0f) : 1.0f;
    pImpl_->materialConstants_.pbrFactors[3] =
        clampFinite(occlusionStrength, 0.0f, 1.0f, 1.0f);
}

void MeshRenderer::setDiffuseRoughness(float roughness)
{
    prepared_ = false;
    pImpl_->materialConstants_.subsurfaceScatteringFactors[1] =
        clampFinite(roughness, 0.0f, 1.0f, 0.0f);
}

void MeshRenderer::setPrincipledFactors(float specular, float ior,
                                        float transmission, float clearcoat,
                                        float clearcoatRoughness, float sheen,
                                        float clearcoatIor)
{
    prepared_ = false;
    pImpl_->materialConstants_.principledFactors[0] =
        clampFinite(specular, 0.0f, 1.0f, 0.5f);
    pImpl_->materialConstants_.principledFactors[1] =
        clampFinite(ior, 1.0f, 3.0f, 1.5f);
    pImpl_->materialConstants_.principledFactors[2] =
        clampFinite(transmission, 0.0f, 1.0f, 0.0f);
    pImpl_->materialConstants_.principledFactors[3] =
        clampFinite(sheen, 0.0f, 1.0f, 0.0f);
    pImpl_->materialConstants_.clearcoatFactors[0] =
        clampFinite(clearcoat, 0.0f, 1.0f, 0.0f);
    pImpl_->materialConstants_.clearcoatFactors[1] =
        clampFinite(clearcoatRoughness, 0.0f, 1.0f, 0.03f);
    pImpl_->materialConstants_.clearcoatFactors[2] =
        clampFinite(clearcoatIor, 1.0f, 2.0f, 1.5f);
}

void MeshRenderer::setSpecularTint(const QColor& color)
{
    prepared_ = false;
    const QColor safeTint = color.isValid() ? color : QColor(255, 255, 255);
    auto& tint = pImpl_->materialConstants_.specularTintFactors;
    tint[0] = safeTint.redF();
    tint[1] = safeTint.greenF();
    tint[2] = safeTint.blueF();
}

void MeshRenderer::setThinFilm(float thicknessNanometers, float ior)
{
    prepared_ = false;
    auto& factors = pImpl_->materialConstants_.thinFilmFactors;
    factors[0] = clampFinite(thicknessNanometers, 0.0f, 1000.0f, 0.0f);
    factors[1] = clampFinite(ior, 1.0f, 2.5f, 1.33f);
}

void MeshRenderer::setClearcoatTint(const QColor& color, float normalStrength)
{
    prepared_ = false;
    auto& factors = pImpl_->materialConstants_.clearcoatTintFactors;
    const QColor safeTint = color.isValid() ? color : QColor(255, 255, 255);
    factors[0] = safeTint.redF();
    factors[1] = safeTint.greenF();
    factors[2] = safeTint.blueF();
    factors[3] = clampFinite(normalStrength, 0.0f, 1.0f, 1.0f);
}

void MeshRenderer::setSheenFactors(float roughness, const QColor& tint)
{
    prepared_ = false;
    auto& factors = pImpl_->materialConstants_.sheenFactors;
    factors[0] = clampFinite(roughness, 0.0f, 1.0f, 0.5f);
    const QColor safeTint = tint.isValid() ? tint : QColor(255, 255, 255);
    factors[1] = safeTint.redF();
    factors[2] = safeTint.greenF();
    factors[3] = safeTint.blueF();
}

void MeshRenderer::setSubsurfaceFactors(float weight, const QColor& color,
                                        const QVector3D& radius, float ior,
                                        float anisotropy)
{
    prepared_ = false;
    auto& factors = pImpl_->materialConstants_.subsurfaceFactors;
    factors[0] = clampFinite(weight, 0.0f, 1.0f, 0.0f);
    const QColor safeColor = color.isValid() ? color : QColor(255, 255, 255);
    factors[1] = safeColor.redF();
    factors[2] = safeColor.greenF();
    factors[3] = safeColor.blueF();
    auto& radiusFactors = pImpl_->materialConstants_.subsurfaceRadius;
    radiusFactors[0] = std::isfinite(radius.x()) ? std::clamp(radius.x(), 0.0f, 10.0f) : 1.0f;
    radiusFactors[1] = std::isfinite(radius.y()) ? std::clamp(radius.y(), 0.0f, 10.0f) : 0.2f;
    radiusFactors[2] = std::isfinite(radius.z()) ? std::clamp(radius.z(), 0.0f, 10.0f) : 0.1f;
    radiusFactors[3] = clampFinite(ior, 1.0f, 2.0f, 1.4f);
    pImpl_->materialConstants_.subsurfaceScatteringFactors[0] =
        clampFinite(anisotropy, -0.9f, 0.9f, 0.0f);
}

void MeshRenderer::setAnisotropy(float value, float rotationDegrees)
{
    prepared_ = false;
    pImpl_->materialConstants_.anisotropyFactors[0] =
        std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
    pImpl_->materialConstants_.anisotropyFactors[1] =
        std::isfinite(rotationDegrees)
            ? std::clamp(rotationDegrees, -180.0f, 180.0f) *
                  3.14159265358979323846f / 180.0f
            : 0.0f;
}

void MeshRenderer::setUvTransform(float offsetU, float offsetV, float scaleU,
                                  float scaleV, float rotationDegrees,
                                  float normalRotationDegrees,
                                  float normalScale, float baseColorScale,
                                  float baseColorOffsetU,
                                  float baseColorOffsetV,
                                  float baseColorRotationDegrees)
{
    prepared_ = false;
    pImpl_->materialConstants_.uvTransformA[0] =
        std::isfinite(offsetU) ? std::clamp(offsetU, -10.0f, 10.0f) : 0.0f;
    pImpl_->materialConstants_.uvTransformA[1] =
        std::isfinite(offsetV) ? std::clamp(offsetV, -10.0f, 10.0f) : 0.0f;
    pImpl_->materialConstants_.uvTransformA[2] =
        std::isfinite(scaleU) ? std::clamp(scaleU, 0.01f, 10.0f) : 1.0f;
    pImpl_->materialConstants_.uvTransformA[3] =
        std::isfinite(scaleV) ? std::clamp(scaleV, 0.01f, 10.0f) : 1.0f;
    const float rotation = std::isfinite(rotationDegrees)
        ? std::clamp(rotationDegrees, -360.0f, 360.0f) : 0.0f;
    pImpl_->materialConstants_.uvTransformB[0] =
        rotation * std::numbers::pi_v<float> / 180.0f;
    const float normalRotation = std::isfinite(normalRotationDegrees)
        ? std::clamp(normalRotationDegrees, -360.0f, 360.0f) : 0.0f;
    pImpl_->materialConstants_.uvTransformB[1] =
        normalRotation * std::numbers::pi_v<float> / 180.0f;
    pImpl_->materialConstants_.uvTransformB[2] =
        std::isfinite(normalScale) ? std::clamp(normalScale, 0.01f, 10.0f)
                                   : 1.0f;
    pImpl_->materialConstants_.uvTransformB[3] =
        std::isfinite(baseColorScale)
            ? std::clamp(baseColorScale, 0.01f, 10.0f) : 1.0f;
    pImpl_->materialConstants_.baseColorTextureUvTransform[0] =
        std::isfinite(baseColorOffsetU)
            ? std::clamp(baseColorOffsetU, -10.0f, 10.0f) : 0.0f;
    pImpl_->materialConstants_.baseColorTextureUvTransform[1] =
        std::isfinite(baseColorOffsetV)
            ? std::clamp(baseColorOffsetV, -10.0f, 10.0f) : 0.0f;
    const float baseColorRotation = std::isfinite(baseColorRotationDegrees)
        ? std::clamp(baseColorRotationDegrees, -360.0f, 360.0f) : 0.0f;
    pImpl_->materialConstants_.baseColorTextureUvTransform[2] =
        baseColorRotation * std::numbers::pi_v<float> / 180.0f;
}

void MeshRenderer::setMetallicRoughnessUvScale(float scale,
                                               float offsetU,
                                               float offsetV)
{
    prepared_ = false;
    auto& transform = pImpl_->materialConstants_.surfaceTextureUvTransform;
    transform[0] =
        std::isfinite(scale) ? std::clamp(scale, 0.01f, 10.0f) : 1.0f;
    transform[1] =
        std::isfinite(offsetU) ? std::clamp(offsetU, -10.0f, 10.0f) : 0.0f;
    transform[2] =
        std::isfinite(offsetV) ? std::clamp(offsetV, -10.0f, 10.0f) : 0.0f;
}

void MeshRenderer::setMetallicRoughnessUvRotation(float rotationDegrees)
{
    prepared_ = false;
    const float rotation = std::isfinite(rotationDegrees)
        ? std::clamp(rotationDegrees, -360.0f, 360.0f) : 0.0f;
    pImpl_->materialConstants_.surfaceTextureUvTransform[3] =
        rotation * std::numbers::pi_v<float> / 180.0f;
}

void MeshRenderer::setNormalUvOffset(float offsetU, float offsetV, int uvSet)
{
    prepared_ = false;
    auto& offset = pImpl_->materialConstants_.normalTextureUvOffset;
    offset[0] = std::isfinite(offsetU)
        ? std::clamp(offsetU, -10.0f, 10.0f) : 0.0f;
    offset[1] = std::isfinite(offsetV)
        ? std::clamp(offsetV, -10.0f, 10.0f) : 0.0f;
    offset[2] = uvSet == 1 ? 1.0f : 0.0f;
}

void MeshRenderer::setOcclusionTextureUvScale(float scale,
                                              float offsetU,
                                              float offsetV)
{
    prepared_ = false;
    auto& transform = pImpl_->materialConstants_.occlusionTextureUvTransform;
    transform[0] =
        std::isfinite(scale) ? std::clamp(scale, 0.01f, 10.0f) : 1.0f;
    transform[1] =
        std::isfinite(offsetU) ? std::clamp(offsetU, -10.0f, 10.0f) : 0.0f;
    transform[2] =
        std::isfinite(offsetV) ? std::clamp(offsetV, -10.0f, 10.0f) : 0.0f;
}

void MeshRenderer::setOcclusionTextureUvRotation(float rotationDegrees)
{
    prepared_ = false;
    const float rotation = std::isfinite(rotationDegrees)
        ? std::clamp(rotationDegrees, -360.0f, 360.0f) : 0.0f;
    pImpl_->materialConstants_.occlusionTextureUvTransform[3] =
        rotation * std::numbers::pi_v<float> / 180.0f;
}

void MeshRenderer::setEmissionTextureUvScale(float scale,
                                             float offsetU,
                                             float offsetV)
{
    prepared_ = false;
    auto& transform = pImpl_->materialConstants_.emissionTextureUvTransform;
    transform[0] =
        std::isfinite(scale) ? std::clamp(scale, 0.01f, 10.0f) : 1.0f;
    transform[1] =
        std::isfinite(offsetU) ? std::clamp(offsetU, -10.0f, 10.0f) : 0.0f;
    transform[2] =
        std::isfinite(offsetV) ? std::clamp(offsetV, -10.0f, 10.0f) : 0.0f;
}

void MeshRenderer::setEmissionTextureUvRotation(float rotationDegrees)
{
    prepared_ = false;
    const float rotation = std::isfinite(rotationDegrees)
        ? std::clamp(rotationDegrees, -360.0f, 360.0f) : 0.0f;
    pImpl_->materialConstants_.emissionTextureUvRotation[0] =
        rotation * std::numbers::pi_v<float> / 180.0f;
}

void MeshRenderer::setOpacityTextureUvScale(float scale,
                                            float offsetU,
                                            float offsetV)
{
    prepared_ = false;
    pImpl_->materialConstants_.opacityTextureUvTransform[0] =
        std::isfinite(scale) ? std::clamp(scale, 0.01f, 10.0f) : 1.0f;
    pImpl_->materialConstants_.opacityTextureUvOffset[0] =
        std::isfinite(offsetU) ? std::clamp(offsetU, -10.0f, 10.0f) : 0.0f;
    pImpl_->materialConstants_.opacityTextureUvOffset[1] =
        std::isfinite(offsetV) ? std::clamp(offsetV, -10.0f, 10.0f) : 0.0f;
}

void MeshRenderer::setOpacityTextureUvRotation(float rotationDegrees)
{
    prepared_ = false;
    const float rotation = std::isfinite(rotationDegrees)
        ? std::clamp(rotationDegrees, -360.0f, 360.0f) : 0.0f;
    pImpl_->materialConstants_.opacityTextureUvTransform[3] =
        rotation * std::numbers::pi_v<float> / 180.0f;
}

void MeshRenderer::setClearcoatNormalUvTransform(float rotationDegrees,
                                                float scale,
                                                float offsetU,
                                                float offsetV)
{
    prepared_ = false;
    const float rotation = std::isfinite(rotationDegrees)
        ? std::clamp(rotationDegrees, -360.0f, 360.0f) : 0.0f;
    pImpl_->materialConstants_.clearcoatNormalUvTransform[0] =
        rotation * std::numbers::pi_v<float> / 180.0f;
    pImpl_->materialConstants_.clearcoatNormalUvTransform[1] =
        std::isfinite(scale) ? std::clamp(scale, 0.01f, 10.0f) : 1.0f;
    pImpl_->materialConstants_.clearcoatNormalUvTransform[2] =
        std::isfinite(offsetU) ? std::clamp(offsetU, -10.0f, 10.0f) : 0.0f;
    pImpl_->materialConstants_.clearcoatNormalUvTransform[3] =
        std::isfinite(offsetV) ? std::clamp(offsetV, -10.0f, 10.0f) : 0.0f;
}

void MeshRenderer::setEnvironmentIntensity(float intensity)
{
    prepared_ = false;
    pImpl_->materialConstants_.envFactors[0] =
        std::isfinite(intensity) ? std::clamp(intensity, 0.0f, 4.0f) : 1.0f;
    pImpl_->materialConstants_.envFactors[1] = 0.0f;
    pImpl_->materialConstants_.envFactors[2] = 0.0f;
    pImpl_->materialConstants_.envFactors[3] = 0.0f;
}

// Blender-style material graph: stores compileMaterialGraph() splices and
// rebuilds the PSOs so PSMain evaluates them. Empty block = fixed PBR path.
void MeshRenderer::setMaterialGraph(const std::string& helperHlsl,
                                    const std::string& blockHlsl)
{
    if (pImpl_->materialGraphHelpers_ == helperHlsl &&
        pImpl_->materialGraphBlock_ == blockHlsl) {
        return;
    }
    pImpl_->materialGraphHelpers_ = helperHlsl;
    pImpl_->materialGraphBlock_ = blockHlsl;
    prepared_ = false;
    // Per-format PSO sets below would otherwise restore a stale variant.
    pImpl_->pipelineSets_.clear();
    createPSO();
}

void MeshRenderer::setMaterialGraphBlock(const std::string& blockHlsl)
{
    setMaterialGraph({}, blockHlsl);
}

void MeshRenderer::clearMaterialGraph()
{
    if (pImpl_->materialGraphHelpers_.empty() &&
        pImpl_->materialGraphBlock_.empty()) {
        return;
    }
    pImpl_->materialGraphHelpers_.clear();
    pImpl_->materialGraphBlock_.clear();
    prepared_ = false;
    pImpl_->pipelineSets_.clear();
    createPSO();
}

bool MeshRenderer::hasMaterialGraph() const
{
    return !pImpl_->materialGraphBlock_.empty();
}

// Layer-owned graph JSON. Parses + compiles only when the payload changes;
// per-frame calls with identical JSON are a string compare. Invalid JSON
// keeps the previous material. Magenta fallback compiles like Apply does.
bool MeshRenderer::setMaterialGraphJson(const std::string& json)
{
    if (pImpl_->materialGraphJson_ == json) {
        return !pImpl_->materialGraphBlock_.empty() || json.empty();
    }
    if (json.empty()) {
        pImpl_->materialGraphJson_.clear();
        clearMaterialGraph();
        return true;
    }
    ::Artifact::ShaderNode::NodeGraph graph;
    const ::Artifact::ShaderNode::MaterialGraphLoadResult loaded =
        graph.fromJson(json);
    if (!loaded.ok) {
        return false;
    }
    const ::Artifact::ShaderNode::MaterialGraphResult compiled =
        graph.compileMaterialGraph();
    pImpl_->materialGraphJson_ = json;
    setMaterialGraph(compiled.helperHlsl, compiled.blockHlsl);
    return true;
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

void MeshRenderer::setClearcoatNormalTexture(const QString& path)
{
    prepared_ = false;
    const QString newPath = path.trimmed();
    if (newPath == pImpl_->clearcoatNormalTexturePath_ &&
        (newPath.isEmpty() || pImpl_->materialConstants_.clearcoatFactors[3] > 0.5f)) {
        return;
    }
    pImpl_->pClearcoatNormalTexture_ = nullptr;
    pImpl_->pClearcoatNormalTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
    pImpl_->clearcoatNormalTexturePath_.clear();
    pImpl_->materialConstants_.clearcoatFactors[3] = 0.0f;
    if (newPath.isEmpty()) return;
    if (loadLinearTexture(context_, newPath,
                          "MeshRenderer_ClearcoatNormalTexture",
                          pImpl_->pClearcoatNormalTexture_,
                          pImpl_->pClearcoatNormalTextureSRV_, true)) {
        pImpl_->clearcoatNormalTexturePath_ = newPath;
        pImpl_->materialConstants_.clearcoatFactors[3] = 1.0f;
    } else {
        pImpl_->pClearcoatNormalTextureSRV_ = pImpl_->pLinearWhiteTextureSRV_;
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
    initData.pSubResources = subResources.data();
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

