module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.ColorCurves;

export namespace ArtifactCore::Shaders::ColorCurves
{
inline constexpr const char* ColorCurvesSource = R"(
cbuffer CurveParams : register(b0)
{
    int g_MasterOnly;
    int3 g_Padding;
};

Texture2D<float4> g_InputTexture : register(t0);
StructuredBuffer<float> g_LUTs : register(t1);
RWTexture2D<float4> g_OutputTexture : register(u0);

[numthreads(16, 16, 1)]
void ColorCurvesCS(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    g_OutputTexture.GetDimensions(dims.x, dims.y);
    if (id.x >= dims.x || id.y >= dims.y) return;

    float4 color = g_InputTexture.Load(int3(id.xy, 0));

    if (g_MasterOnly) {
        color.r = g_LUTs[(uint)(color.r * 255.0f)];
        color.g = g_LUTs[(uint)(color.g * 255.0f)];
        color.b = g_LUTs[(uint)(color.b * 255.0f)];
    } else {
        uint rIdx = (uint)(color.r * 255.0f);
        uint gIdx = (uint)(color.g * 255.0f);
        uint bIdx = (uint)(color.b * 255.0f);

        float masterR = g_LUTs[rIdx];
        float masterG = g_LUTs[gIdx];
        float masterB = g_LUTs[bIdx];

        uint rMapped = (uint)(masterR * 255.0f);
        uint gMapped = (uint)(masterG * 255.0f);
        uint bMapped = (uint)(masterB * 255.0f);

        color.r = g_LUTs[256u + rMapped];
        color.g = g_LUTs[512u + gMapped];
        color.b = g_LUTs[768u + bMapped];
    }

    g_OutputTexture[id.xy] = color;
}
)";

inline constexpr const char* ColorCurvesEntryPoint = "ColorCurvesCS";
inline constexpr int LUT_OFFSET_MASTER = 0;
inline constexpr int LUT_OFFSET_RED = 256;
inline constexpr int LUT_OFFSET_GREEN = 512;
inline constexpr int LUT_OFFSET_BLUE = 768;
inline constexpr int LUT_TOTAL_FLOATS = 1024;
}
