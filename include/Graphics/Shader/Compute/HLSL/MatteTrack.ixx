module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.MatteTrack;

export namespace ArtifactCore::Shaders::MatteTrack
{

// Constants
// matteMode: 0=Alpha, 1=Luminance, 2=AlphaInverted, 3=LuminanceInverted
// stackMode: 0=Add, 1=Common(min), 2=Subtract
inline constexpr const char* MatteTrackSource = R"(
cbuffer MatteTrackParams : register(b0)
{
    uint  g_MatteCount;           // 1-3
    uint  g_MatteMode0;           // matte source 0 mode
    uint  g_MatteMode1;           // matte source 1 mode
    uint  g_MatteMode2;           // matte source 2 mode
    uint  g_StackMode;            // combine mode across sources
    uint  g_LumaMode;             // 0=Rec.601, 1=Rec.709 (for luminance extraction)
    float g_Opacity;              // master matte opacity [0,1]
    float _pad0;
};

Texture2D<float4>  g_LayerTex    : register(t0);
Texture2D<float4>  g_MatteSrc0   : register(t1);
Texture2D<float4>  g_MatteSrc1   : register(t2);
Texture2D<float4>  g_MatteSrc2   : register(t3);
RWTexture2D<float4> g_OutTex     : register(u0);

// Luminance coefficients
static const float3 kLumaRec601 = float3(0.299f, 0.587f, 0.114f);
static const float3 kLumaRec709 = float3(0.2126f, 0.7152f, 0.0722f);

float extractMask(float4 color, uint mode, float3 lumaCoeffs)
{
    // mode: 0=Alpha, 1=Luminance, 2=AlphaInverted, 3=LuminanceInverted
    float mask;
    if (mode == 0 || mode == 2) {
        mask = color.a;
    } else {
        mask = dot(color.rgb, lumaCoeffs);
    }
    if (mode == 2 || mode == 3) {
        mask = 1.0f - mask;
    }
    return saturate(mask);
}

float combineMasks(float a, float b, uint mode)
{
    // mode: 0=Add, 1=Common(min), 2=Subtract
    if (mode == 0) return saturate(a + b);
    if (mode == 1) return min(a, b);
    return saturate(a - b); // Subtract
}

[numthreads(16, 16, 1)]
void MatteTrackCS(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    g_OutTex.GetDimensions(dims.x, dims.y);
    if (id.x >= dims.x || id.y >= dims.y) return;

    float3 lumaCoeffs = (g_LumaMode == 1) ? kLumaRec709 : kLumaRec601;

    // Start with full-opacity mask; combine each source
    float combinedMask = 1.0f;

    [branch] if (g_MatteCount > 0) {
        float4 matteColor = g_MatteSrc0.Load(int3(id.xy, 0));
        combinedMask = extractMask(matteColor, g_MatteMode0, lumaCoeffs);
    }

    [branch] if (g_MatteCount > 1) {
        float4 matteColor = g_MatteSrc1.Load(int3(id.xy, 0));
        float mask = extractMask(matteColor, g_MatteMode1, lumaCoeffs);
        combinedMask = combineMasks(combinedMask, mask, g_StackMode);
    }

    [branch] if (g_MatteCount > 2) {
        float4 matteColor = g_MatteSrc2.Load(int3(id.xy, 0));
        float mask = extractMask(matteColor, g_MatteMode2, lumaCoeffs);
        combinedMask = combineMasks(combinedMask, mask, g_StackMode);
    }

    // Apply opacity scalar
    combinedMask *= g_Opacity;

    // Apply mask to layer (uniform premultiplied-alpha multiply)
    float4 layerColor = g_LayerTex.Load(int3(id.xy, 0));
    g_OutTex[id.xy] = float4(layerColor.rgb * combinedMask, layerColor.a * combinedMask);
}
)";

inline constexpr const char* MatteTrackEntryPoint = "MatteTrackCS";

} // namespace ArtifactCore::Shaders::MatteTrack
