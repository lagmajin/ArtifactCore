module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.ScopeParade;

export namespace ArtifactCore::Shaders::ScopeParade
{
inline constexpr const char* ParadeSource = R"(
cbuffer ParadeParams : register(b0)
{
    int g_OutputWidth;
    int g_OutputHeight;
    int g_Step;
    int g_Padding;
};

Texture2D<float4> g_InputTexture : register(t0);
RWStructuredBuffer<uint> g_OutputParade : register(u0);

[numthreads(256, 1, 1)]
void ParadeCS(uint3 groupId : SV_GroupID, uint3 threadId : SV_GroupThreadID)
{
    uint2 textureSize;
    g_InputTexture.GetDimensions(textureSize.x, textureSize.y);

    uint samplesX = (textureSize.x + g_Step - 1) / g_Step;
    uint samplesY = (textureSize.y + g_Step - 1) / g_Step;
    uint totalSampled = samplesX * samplesY;
    uint groupOffset = groupId.x * 256 * 16;
    int perPane = g_OutputWidth * g_OutputHeight;

    for (uint i = 0; i < 16; ++i) {
        uint sampleIndex = groupOffset + threadId.x + i * 256;
        if (sampleIndex >= totalSampled) continue;

        uint sy = sampleIndex / samplesX;
        uint sx = sampleIndex % samplesX;
        uint2 pos = uint2(sx * g_Step, sy * g_Step);
        if (pos.x >= textureSize.x || pos.y >= textureSize.y) continue;

        float4 color = g_InputTexture.Load(int3(pos, 0));

        int ox = (sx * g_Step * g_OutputWidth) / (int)textureSize.x;
        int oyR = (int)(color.r * (float)(g_OutputHeight - 1));
        int oyG = (int)(color.g * (float)(g_OutputHeight - 1));
        int oyB = (int)(color.b * (float)(g_OutputHeight - 1));

        ox = clamp(ox, 0, g_OutputWidth - 1);
        oyR = clamp(oyR, 0, g_OutputHeight - 1);
        oyG = clamp(oyG, 0, g_OutputHeight - 1);
        oyB = clamp(oyB, 0, g_OutputHeight - 1);

        InterlockedAdd(g_OutputParade[oyR * g_OutputWidth + ox + 0 * perPane], 1);
        InterlockedAdd(g_OutputParade[oyG * g_OutputWidth + ox + 1 * perPane], 1);
        InterlockedAdd(g_OutputParade[oyB * g_OutputWidth + ox + 2 * perPane], 1);
    }
}
)";

inline constexpr const char* ParadeEntryPoint = "ParadeCS";
}
