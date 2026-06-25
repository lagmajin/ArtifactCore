module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.LUT3D;

export namespace ArtifactCore::Shaders::LUT3D
{
inline constexpr const char* LUT3DSource = R"(
Texture2D<float4> g_InputTexture : register(t0);
Texture3D<float4> g_LUT : register(t1);
RWTexture2D<float4> g_OutputTexture : register(u0);

[numthreads(16, 16, 1)]
void LUT3DCS(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    g_OutputTexture.GetDimensions(dims.x, dims.y);
    if (id.x >= dims.x || id.y >= dims.y) return;

    float4 color = g_InputTexture.Load(int3(id.xy, 0));

    uint width, height, depth;
    g_LUT.GetDimensions(width, height, depth);
    float scale = (float)depth - 1.0f;

    float3 f = color.rgb * scale;
    int3 i = (int3)f;
    float3 d = f - (float3)i;
    i = clamp(i, int3(0, 0, 0), int3((int)depth - 2, (int)depth - 2, (int)depth - 2));

    auto load = [&](int x, int y, int z) -> float4 {
        return g_LUT.Load(int4(x, y, z, 0));
    };

    float4 c000 = load(i.x,     i.y,     i.z);
    float4 c100 = load(i.x + 1, i.y,     i.z);
    float4 c010 = load(i.x,     i.y + 1, i.z);
    float4 c110 = load(i.x + 1, i.y + 1, i.z);
    float4 c001 = load(i.x,     i.y,     i.z + 1);
    float4 c101 = load(i.x + 1, i.y,     i.z + 1);
    float4 c011 = load(i.x,     i.y + 1, i.z + 1);
    float4 c111 = load(i.x + 1, i.y + 1, i.z + 1);

    float4 c00 = lerp(c000, c100, d.x);
    float4 c10 = lerp(c010, c110, d.x);
    float4 c01 = lerp(c001, c101, d.x);
    float4 c11 = lerp(c011, c111, d.x);
    float4 c0  = lerp(c00, c10, d.y);
    float4 c1  = lerp(c01, c11, d.y);
    float4 result = lerp(c0, c1, d.z);

    g_OutputTexture[id.xy] = float4(result.rgb, color.a);
}
)";

inline constexpr const char* LUT3DEntryPoint = "LUT3DCS";
}
