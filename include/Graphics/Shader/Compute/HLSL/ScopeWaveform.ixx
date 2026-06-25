module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.ScopeWaveform;

export namespace ArtifactCore::Shaders::ScopeWaveform
{
inline constexpr const char* WaveformSource = R"(
cbuffer WaveformParams : register(b0)
{
    int g_OutputWidth;
    int g_OutputHeight;
    int g_Step;
    int g_Padding;
};

Texture2D<float4> g_InputTexture : register(t0);
RWStructuredBuffer<uint> g_OutputWaveform : register(u0);

[numthreads(256, 1, 1)]
void WaveformCS(uint3 groupId : SV_GroupID, uint3 threadId : SV_GroupThreadID)
{
    uint2 textureSize;
    g_InputTexture.GetDimensions(textureSize.x, textureSize.y);

    uint samplesX = (textureSize.x + g_Step - 1) / g_Step;
    uint samplesY = (textureSize.y + g_Step - 1) / g_Step;
    uint totalSampled = samplesX * samplesY;
    uint groupOffset = groupId.x * 256 * 16;

    for (uint i = 0; i < 16; ++i) {
        uint sampleIndex = groupOffset + threadId.x + i * 256;
        if (sampleIndex >= totalSampled) continue;

        uint sy = sampleIndex / samplesX;
        uint sx = sampleIndex % samplesX;
        uint2 pos = uint2(sx * g_Step, sy * g_Step);
        if (pos.x >= textureSize.x || pos.y >= textureSize.y) continue;

        float4 color = g_InputTexture.Load(int3(pos, 0));
        float luma = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));

        int ox = (sx * g_Step * g_OutputWidth) / (int)textureSize.x;
        int oy = (int)(luma * (float)(g_OutputHeight - 1));

        ox = clamp(ox, 0, g_OutputWidth - 1);
        oy = clamp(oy, 0, g_OutputHeight - 1);

        InterlockedAdd(g_OutputWaveform[oy * g_OutputWidth + ox], 1);
    }
}
)";

inline constexpr const char* WaveformEntryPoint = "WaveformCS";
}
