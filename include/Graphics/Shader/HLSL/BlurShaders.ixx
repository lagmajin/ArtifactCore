module;
#include <utility>
#include <QString>
#include <QByteArray>
#include "..\..\..\Define\DllExportMacro.hpp"

export module Graphics.Shader.HLSL.Blur;

export namespace ArtifactCore {

// ─────────────────────────────────────────────────────────
// Blur Shaders (HLSL)
//
// Separable Gaussian Blur:
//   Pass 1: Horizontal blur
//   Pass 2: Vertical blur
//
// Features:
//   - Edge clamp (BORDER_REPLICATE equivalent)
//   - Premultiplied alpha support
//   - Configurable sigma via constant buffer
// ─────────────────────────────────────────────────────────

LIBRARY_DLL_API const QByteArray g_blurHorizontalVS = R"(
struct VSInput {
    float2 pos     : ATTRIB0;
    float2 texCoord: TEXCOORD0;
};

struct PSInput {
    float4 pos     : SV_POSITION;
    float2 texCoord: TEXCOORD0;
};

PSInput main(VSInput input) {
    PSInput output;
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.texCoord = input.texCoord;
    return output;
}
)";

LIBRARY_DLL_API const QByteArray g_blurHorizontalPS = R"(
Texture2D    g_texture  : register(t0);
SamplerState g_sampler  : register(s0);

cbuffer BlurCB : register(b0) {
    float2 texelSize;  // (1.0/width, 1.0/height)
    float  sigma;
    float  padding;
};

static const int MAX_KERNEL_RADIUS = 32;

float4 main(PSInput input) : SV_TARGET {
    float2 uv = input.texCoord;
    float2 direction = float2(1.0, 0.0); // Horizontal

    int radius = int(ceil(sigma * 3.0));
    radius = min(radius, MAX_KERNEL_RADIUS);

    float2 offset = texelSize * direction;
    float totalWeight = 0.0;
    float4 result = float4(0.0, 0.0, 0.0, 0.0);

    for (int i = -radius; i <= radius; ++i) {
        float weight = exp(-0.5 * (i * i) / (sigma * sigma));
        float2 sampleUV = uv + offset * i;

        // Edge clamp
        sampleUV = clamp(sampleUV, float2(0.0, 0.0), float2(1.0, 1.0));

        float4 sample = g_texture.Sample(g_sampler, sampleUV);
        result += sample * weight;
        totalWeight += weight;
    }

    return result / totalWeight;
}
)";

LIBRARY_DLL_API const QByteArray g_blurVerticalPS = R"(
Texture2D    g_texture  : register(t0);
SamplerState g_sampler  : register(s0);

cbuffer BlurCB : register(b0) {
    float2 texelSize;  // (1.0/width, 1.0/height)
    float  sigma;
    float  padding;
};

static const int MAX_KERNEL_RADIUS = 32;

float4 main(PSInput input) : SV_TARGET {
    float2 uv = input.texCoord;
    float2 direction = float2(0.0, 1.0); // Vertical

    int radius = int(ceil(sigma * 3.0));
    radius = min(radius, MAX_KERNEL_RADIUS);

    float2 offset = texelSize * direction;
    float totalWeight = 0.0;
    float4 result = float4(0.0, 0.0, 0.0, 0.0);

    for (int i = -radius; i <= radius; ++i) {
        float weight = exp(-0.5 * (i * i) / (sigma * sigma));
        float2 sampleUV = uv + offset * i;

        // Edge clamp
        sampleUV = clamp(sampleUV, float2(0.0, 0.0), float2(1.0, 1.0));

        float4 sample = g_texture.Sample(g_sampler, sampleUV);
        result += sample * weight;
        totalWeight += weight;
    }

    return result / totalWeight;
}
)";

// Edge-preserving blur shader (simplified bilateral)
LIBRARY_DLL_API const QByteArray g_blurEdgePreservingPS = R"(
Texture2D    g_texture  : register(t0);
SamplerState g_sampler  : register(s0);

cbuffer BlurCB : register(b0) {
    float2 texelSize;
    float  sigma;
    float  edgeThreshold;
};

static const int MAX_KERNEL_RADIUS = 16;

float4 main(PSInput input) : SV_TARGET {
    float2 uv = input.texCoord;
    int radius = int(ceil(sigma * 2.0));
    radius = min(radius, MAX_KERNEL_RADIUS);

    float4 center = g_texture.Sample(g_sampler, uv);
    float4 result = float4(0.0, 0.0, 0.0, 0.0);
    float totalWeight = 0.0;

    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            float2 sampleUV = uv + texelSize * float2(x, y);
            sampleUV = clamp(sampleUV, float2(0.0, 0.0), float2(1.0, 1.0));

            float4 sample = g_texture.Sample(g_sampler, sampleUV);

            // Spatial weight (Gaussian)
            float dist = length(float2(x, y));
            float spatialWeight = exp(-0.5 * (dist * dist) / (sigma * sigma));

            // Range weight (edge-preserving)
            float colorDist = length(sample.rgb - center.rgb);
            float rangeWeight = exp(-0.5 * (colorDist * colorDist) / (edgeThreshold * edgeThreshold));

            float weight = spatialWeight * rangeWeight;
            result += sample * weight;
            totalWeight += weight;
        }
    }

    return result / totalWeight;
}
)";

} // namespace ArtifactCore
