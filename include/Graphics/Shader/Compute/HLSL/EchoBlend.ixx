module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.EchoBlend;

export namespace ArtifactCore::Shaders::EchoBlend
{
inline constexpr const char* EchoBlendSource = R"(
cbuffer EchoParams : register(b0)
{
    int   g_FrameCount;
    float g_InvNormalizer;
    float g_Decay;
    float g_StartIntensity;
};

Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

// Ring entries: up to 64 frames, each as individual texture SRV
// Bound as t1..tN by the C++ wrapper
Texture2D<float4> g_Ring0 : register(t1);
Texture2D<float4> g_Ring1 : register(t2);
Texture2D<float4> g_Ring2 : register(t3);
Texture2D<float4> g_Ring3 : register(t4);
Texture2D<float4> g_Ring4 : register(t5);
Texture2D<float4> g_Ring5 : register(t6);
Texture2D<float4> g_Ring6 : register(t7);
Texture2D<float4> g_Ring7 : register(t8);

float decodeWeight(int i)
{
    if (i == 0) return 1.0f;
    return g_StartIntensity * pow(g_Decay, (float)(i - 1));
}

float4 readRing(int i, uint2 pos)
{
    if (i == 0) return g_Ring0.Load(int3(pos, 0));
    if (i == 1) return g_Ring1.Load(int3(pos, 0));
    if (i == 2) return g_Ring2.Load(int3(pos, 0));
    if (i == 3) return g_Ring3.Load(int3(pos, 0));
    if (i == 4) return g_Ring4.Load(int3(pos, 0));
    if (i == 5) return g_Ring5.Load(int3(pos, 0));
    if (i == 6) return g_Ring6.Load(int3(pos, 0));
    if (i == 7) return g_Ring7.Load(int3(pos, 0));
    return float4(0, 0, 0, 0);
}

[numthreads(16, 16, 1)]
void EchoBlendCS(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    g_OutputTexture.GetDimensions(dims.x, dims.y);
    if (id.x >= dims.x || id.y >= dims.y) return;

    float4 accum = 0;
    for (int i = 0; i < min(g_FrameCount, 8); ++i) {
        float w = decodeWeight(i);
        accum += readRing(i, id.xy) * w;
    }
    accum *= g_InvNormalizer;

    g_OutputTexture[id.xy] = saturate(accum);
}
)";

inline constexpr const char* EchoBlendEntryPoint = "EchoBlendCS";
inline constexpr int ECHO_MAX_RING = 8;
}
