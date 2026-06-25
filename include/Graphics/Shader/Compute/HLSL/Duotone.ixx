module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.Duotone;

export namespace ArtifactCore::Shaders::Duotone
{
inline constexpr const char* DuotoneSource = R"(
cbuffer DuotoneParams : register(b0)
{
    float4 g_ShadowColor;
    float4 g_HighlightColor;
    float  g_Blend;
    float3 g_Padding;
};

Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

[numthreads(16, 16, 1)]
void DuotoneCS(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    g_OutputTexture.GetDimensions(dims.x, dims.y);
    if (id.x >= dims.x || id.y >= dims.y) return;

    float4 color = g_InputTexture.Load(int3(id.xy, 0));
    float luma = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
    luma = clamp(luma, 0.0f, 1.0f);

    float3 mapped = lerp(g_ShadowColor.rgb, g_HighlightColor.rgb, luma);
    float3 result = lerp(color.rgb, mapped, g_Blend);

    g_OutputTexture[id.xy] = float4(result, color.a);
}
)";

inline constexpr const char* DuotoneEntryPoint = "DuotoneCS";
}
