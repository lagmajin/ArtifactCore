module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.ScopeVectorscope;

export namespace ArtifactCore::Shaders::ScopeVectorscope
{
inline constexpr const char* VectorscopeSource = R"(
cbuffer VectorscopeParams : register(b0)
{
    int g_ScopeSize;
    int g_Step;
    int2 g_Padding;
};

Texture2D<float4> g_InputTexture : register(t0);
RWStructuredBuffer<uint> g_OutputVectorscope : register(u0);

[numthreads(256, 1, 1)]
void VectorscopeCS(uint3 groupId : SV_GroupID, uint3 threadId : SV_GroupThreadID)
{
    uint2 textureSize;
    g_InputTexture.GetDimensions(textureSize.x, textureSize.y);

    const float cx = (float)g_ScopeSize * 0.5f;
    const float cy = (float)g_ScopeSize * 0.5f;
    const float radius = cx * 0.9f;
    const float inv127 = 1.0f / 127.5f;

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

        float cb = -0.1146f * color.r - 0.3854f * color.g + 0.5f * color.b;
        float cr =  0.5f * color.r - 0.4542f * color.g - 0.0458f * color.b;

        int sx2 = (int)(cx + cb * radius * inv127);
        int sy2 = (int)(cy - cr * radius * inv127);

        sx2 = clamp(sx2, 0, g_ScopeSize - 1);
        sy2 = clamp(sy2, 0, g_ScopeSize - 1);

        InterlockedAdd(g_OutputVectorscope[sy2 * g_ScopeSize + sx2], 1);
    }
}
)";

inline constexpr const char* VectorscopeEntryPoint = "VectorscopeCS";
}
