module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.EdgeEcho;

export namespace ArtifactCore::Shaders::EdgeEcho
{
// ---- Pass 1: Sobel edge detection ----
inline constexpr const char* SobelSource = R"(
cbuffer SobelParams : register(b0)
{
    float g_EdgeThreshold;
    float3 g_Padding;
};

Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float>  g_OutputEdges : register(u0);

[numthreads(16, 16, 1)]
void SobelCS(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    g_OutputEdges.GetDimensions(dims.x, dims.y);
    if (id.x >= dims.x || id.y >= dims.y) return;

    // Skip border pixels
    if (id.x < 1 || id.x >= dims.x - 1 || id.y < 1 || id.y >= dims.y - 1) {
        g_OutputEdges[id.xy] = 0;
        return;
    }

    const int3 off[9] = {
        int3(-1, -1, 0), int3(0, -1, 0), int3(1, -1, 0),
        int3(-1,  0, 0), int3(0,  0, 0), int3(1,  0, 0),
        int3(-1,  1, 0), int3(0,  1, 0), int3(1,  1, 0),
    };

    float lum[9];
    for (int i = 0; i < 9; ++i) {
        float4 c = g_InputTexture.Load(int3(id.xy, 0) + off[i]);
        lum[i] = dot(c.rgb, float3(0.299f, 0.587f, 0.114f));
    }

    // Sobel X: [-1 0 +1; -2 0 +2; -1 0 +1]
    float gx = -lum[0] + lum[2] - 2.0f * lum[3] + 2.0f * lum[5] - lum[6] + lum[8];
    // Sobel Y: [-1 -2 -1; 0 0 0; +1 +2 +1]
    float gy = -lum[0] - 2.0f * lum[1] - lum[2] + lum[6] + 2.0f * lum[7] + lum[8];

    float mag = sqrt(gx * gx + gy * gy);
    float val = mag > g_EdgeThreshold
        ? clamp((mag - g_EdgeThreshold) / max(1.0f - g_EdgeThreshold, 0.001f), 0.0f, 1.0f)
        : 0.0f;

    g_OutputEdges[id.xy] = val;
}
)";

inline constexpr const char* SobelEntryPoint = "SobelCS";

// ---- Pass 2: Temporal wave warp + history combine ----
inline constexpr const char* WarpSource = R"(
cbuffer WarpParams : register(b0)
{
    float g_Decay;
    float g_WaveAmp;
    float g_WaveFreq;
    float g_Time;
};

Texture2D<float>  g_CurrentEdges  : register(t0);
Texture2D<float4> g_History       : register(t1);
RWTexture2D<float4> g_OutputHistory : register(u0);

float2 bilinearSample(Texture2D<float4> tex, float2 uv, uint2 texSize)
{
    float2 f = uv * float2(texSize);
    int2 i = (int2)f;
    float2 d = f - float2(i);
    i = clamp(i, int2(0, 0), int2(texSize) - 2);

    float4 c00 = tex.Load(int3(i.x,     i.y,     0));
    float4 c10 = tex.Load(int3(i.x + 1, i.y,     0));
    float4 c01 = tex.Load(int3(i.x,     i.y + 1, 0));
    float4 c11 = tex.Load(int3(i.x + 1, i.y + 1, 0));
    float4 c0 = lerp(c00, c10, d.x);
    float4 c1 = lerp(c01, c11, d.x);
    return lerp(c0, c1, d.y).rg;
}

[numthreads(16, 16, 1)]
void WarpCS(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    g_OutputHistory.GetDimensions(dims.x, dims.y);
    if (id.x >= dims.x || id.y >= dims.y) return;

    float edge = g_CurrentEdges[id.xy];
    uint2 texSize;
    g_History.GetDimensions(texSize.x, texSize.y);

    // Sinusoidal vertical warp
    float nx = (float)id.x;
    float ny = (float)id.y + g_WaveAmp * sin(6.2831853f * g_WaveFreq * (nx / (float)dims.x) + g_Time);
    float2 uv = float2(nx / (float)texSize.x, ny / (float)texSize.y);

    float2 prev = bilinearSample(g_History, uv, texSize);
    float prevVal = max(prev.x, prev.y) * g_Decay;

    float nextVal = max(edge, prevVal);
    g_OutputHistory[id.xy] = float4(nextVal, nextVal, nextVal, 1.0f);
}
)";

inline constexpr const char* WarpEntryPoint = "WarpCS";

// ---- Pass 3: Composite edge onto original ----
inline constexpr const char* CompositeSource = R"(
cbuffer CompositeParams : register(b0)
{
    float4 g_EchoColor;
};

Texture2D<float4> g_InputTexture  : register(t0);
Texture2D<float4> g_History       : register(t1);
RWTexture2D<float4> g_OutputTexture : register(u0);

[numthreads(16, 16, 1)]
void CompositeCS(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    g_OutputTexture.GetDimensions(dims.x, dims.y);
    if (id.x >= dims.x || id.y >= dims.y) return;

    float4 orig  = g_InputTexture.Load(int3(id.xy, 0));
    float4 hist  = g_History.Load(int3(id.xy, 0));
    float strength = max(hist.r, max(hist.g, hist.b));

    float3 result = (strength > 0.001f)
        ? orig.rgb + g_EchoColor.rgb * strength
        : orig.rgb;

    g_OutputTexture[id.xy] = float4(clamp(result, 0.0f, 1.0f), orig.a);
}
)";

inline constexpr const char* CompositeEntryPoint = "CompositeCS";
}
