module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.Histogram;

export namespace ArtifactCore::Shaders::Histogram
{
inline constexpr const char* HistogramSource = R"(
cbuffer HistogramParams : register(b0)
{
    uint2 g_RegionOffset;
    uint2 g_RegionSize;
};

Texture2D<float4> g_InputTexture : register(t0);
RWStructuredBuffer<uint> g_OutputHistogram : register(u0);
RWStructuredBuffer<uint> g_OutputHistogramRGB : register(u1);
RWStructuredBuffer<uint> g_OutputStatistics : register(u2);

groupshared uint gs_Histogram[256];
groupshared uint gs_HistogramR[256];
groupshared uint gs_HistogramG[256];
groupshared uint gs_HistogramB[256];

float luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

bool inRegion(uint2 p)
{
    uint2 begin = g_RegionOffset;
    uint2 end = g_RegionOffset + g_RegionSize;
    return all(p >= begin) && all(p < end);
}

[numthreads(256, 1, 1)]
void HistogramLumaCS(uint3 groupId : SV_GroupID, uint3 threadId : SV_GroupThreadID)
{
    if (threadId.x < 256) {
        gs_Histogram[threadId.x] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint2 textureSize;
    g_InputTexture.GetDimensions(textureSize.x, textureSize.y);
    uint totalPixels = textureSize.x * textureSize.y;
    uint groupOffset = groupId.x * 256 * 16;

    for (uint i = 0; i < 16; ++i) {
        uint pixelIndex = groupOffset + threadId.x + i * 256;
        if (pixelIndex >= totalPixels) continue;
        uint2 pos = uint2(pixelIndex % textureSize.x, pixelIndex / textureSize.x);
        if (g_RegionSize.x != 0u && g_RegionSize.y != 0u && !inRegion(pos)) continue;
        float4 color = g_InputTexture.Load(int3(pos, 0));
        uint bin = (uint)clamp(luminance(color.rgb) * 255.0f, 0.0f, 255.0f);
        InterlockedAdd(gs_Histogram[bin], 1);
    }

    GroupMemoryBarrierWithGroupSync();
    if (threadId.x < 256) {
        InterlockedAdd(g_OutputHistogram[threadId.x], gs_Histogram[threadId.x]);
    }
}

[numthreads(256, 1, 1)]
void HistogramRGBCS(uint3 groupId : SV_GroupID, uint3 threadId : SV_GroupThreadID)
{
    if (threadId.x < 256) {
        gs_HistogramR[threadId.x] = 0;
        gs_HistogramG[threadId.x] = 0;
        gs_HistogramB[threadId.x] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    uint2 textureSize;
    g_InputTexture.GetDimensions(textureSize.x, textureSize.y);
    uint totalPixels = textureSize.x * textureSize.y;
    uint groupOffset = groupId.x * 256 * 8;

    for (uint i = 0; i < 8; ++i) {
        uint pixelIndex = groupOffset + threadId.x + i * 256;
        if (pixelIndex >= totalPixels) continue;
        uint2 pos = uint2(pixelIndex % textureSize.x, pixelIndex / textureSize.x);
        if (g_RegionSize.x != 0u && g_RegionSize.y != 0u && !inRegion(pos)) continue;
        float4 color = g_InputTexture.Load(int3(pos, 0));
        InterlockedAdd(gs_HistogramR[(uint)clamp(color.r * 255.0f, 0.0f, 255.0f)], 1);
        InterlockedAdd(gs_HistogramG[(uint)clamp(color.g * 255.0f, 0.0f, 255.0f)], 1);
        InterlockedAdd(gs_HistogramB[(uint)clamp(color.b * 255.0f, 0.0f, 255.0f)], 1);
    }

    GroupMemoryBarrierWithGroupSync();
    if (threadId.x < 256) {
        InterlockedAdd(g_OutputHistogramRGB[threadId.x + 0 * 256], gs_HistogramR[threadId.x]);
        InterlockedAdd(g_OutputHistogramRGB[threadId.x + 1 * 256], gs_HistogramG[threadId.x]);
        InterlockedAdd(g_OutputHistogramRGB[threadId.x + 2 * 256], gs_HistogramB[threadId.x]);
    }
}

[numthreads(16, 16, 1)]
void StatisticsCS(uint3 id : SV_DispatchThreadID)
{
    uint2 textureSize;
    g_InputTexture.GetDimensions(textureSize.x, textureSize.y);
    if (id.x >= textureSize.x || id.y >= textureSize.y) {
        return;
    }

    if (g_RegionSize.x != 0u && g_RegionSize.y != 0u && !inRegion(id.xy)) {
        return;
    }

    float4 c = g_InputTexture.Load(int3(id.xy, 0));
    float l = luminance(c.rgb);

    uint bin = (uint)clamp(l * 255.0f, 0.0f, 255.0f);
    InterlockedMin(g_OutputStatistics[0], bin);
    InterlockedMax(g_OutputStatistics[1], bin);
    InterlockedAdd(g_OutputStatistics[2], 1u);
    InterlockedAdd(g_OutputStatistics[3], bin);
    InterlockedAdd(g_OutputStatistics[4], bin * bin);
}
)";

inline constexpr const char* HistogramLumaEntryPoint = "HistogramLumaCS";
inline constexpr const char* HistogramRGBEntryPoint = "HistogramRGBCS";
inline constexpr const char* StatisticsEntryPoint = "StatisticsCS";
}
