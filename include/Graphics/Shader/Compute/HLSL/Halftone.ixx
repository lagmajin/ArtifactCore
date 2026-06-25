module;
#include <utility>

export module Graphics.Shader.Compute.HLSL.Halftone;

export namespace ArtifactCore::Shaders::Halftone
{
inline constexpr const char* HalftoneSource = R"(
cbuffer HalftoneParams : register(b0)
{
    float g_DotSize;
    float g_AngleRad;
    float g_Contrast;
    int   g_ColorMode;  // 0=mono, 1=color, 2=CMYK
    float g_EllipseAspect;
    float g_CMYK_Angles[4];
    int   g_DotShape;   // 0=Circle, 1=Ellipse, 2=Diamond, 3=Line, 4=Cross
    int2  g_Padding;
};

Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

float dotShapeFactor(float dx, float dy, float half, int shape, float aspect)
{
    switch (shape) {
    case 0: { // Circle
        float dist = sqrt(dx * dx + dy * dy);
        return 1.0f - dist / half;
    }
    case 1: { // Ellipse
        float ax = dx / aspect;
        float dist = sqrt(ax * ax + dy * dy);
        return 1.0f - dist / half;
    }
    case 2: { // Diamond
        float dist = abs(dx) + abs(dy);
        return 1.0f - dist / (half * 1.41421356f);
    }
    case 3: { // Line (vertical screen)
        return 1.0f - abs(dx) / half;
    }
    case 4: { // Cross
        float hx = 1.0f - abs(dx) / half;
        float hy = 1.0f - abs(dy) / half;
        return max(hx, hy);
    }
    }
    return 1.0f - sqrt(dx * dx + dy * dy) / half;
}

float channelCoverage(float rx, float ry, float dotSize, float halfDot,
                      float intensity, int shape, float aspect)
{
    float cellX = floor(rx / dotSize);
    float cellY = floor(ry / dotSize);
    float cx = (cellX + 0.5f) * dotSize;
    float cy = (cellY + 0.5f) * dotSize;
    float dx = rx - cx;
    float dy = ry - cy;

    float raw = dotShapeFactor(dx, dy, dotSize, shape, aspect);
    float threshold = 1.0f - intensity;
    float edge = raw - threshold;
    if (edge >= 0.5f) return 0.0f;
    if (edge <= -0.5f) return 1.0f;
    return 0.5f - edge;
}

[numthreads(16, 16, 1)]
void HalftoneCS(uint3 id : SV_DispatchThreadID)
{
    uint2 dims;
    g_OutputTexture.GetDimensions(dims.x, dims.y);
    if (id.x >= dims.x || id.y >= dims.y) return;

    float4 color = g_InputTexture.Load(int3(id.xy, 0));
    float px = (float)id.x + 0.5f;
    float py = (float)id.y + 0.5f;
    float cosA = cos(g_AngleRad);
    float sinA = sin(g_AngleRad);
    float rx = px * cosA - py * sinA;
    float ry = px * sinA + py * cosA;
    float halfDot = g_DotSize * 0.5f;

    if (g_ColorMode == 0) { // Monochrome
        float lum = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
        float intensity = clamp(lum * g_Contrast, 0.0f, 1.0f);
        float ink = 1.0f - channelCoverage(rx, ry, g_DotSize, halfDot,
                                           intensity, g_DotShape, g_EllipseAspect);
        g_OutputTexture[id.xy] = float4(ink, ink, ink, color.a);
        return;
    }

    if (g_ColorMode == 1) { // Color
        float r = clamp(color.r * g_Contrast, 0.0f, 1.0f);
        float g = clamp(color.g * g_Contrast, 0.0f, 1.0f);
        float b = clamp(color.b * g_Contrast, 0.0f, 1.0f);
        float cr = 1.0f - channelCoverage(rx, ry, g_DotSize, halfDot, r, g_DotShape, g_EllipseAspect);
        float cg = 1.0f - channelCoverage(rx, ry, g_DotSize, halfDot, g, g_DotShape, g_EllipseAspect);
        float cb = 1.0f - channelCoverage(rx, ry, g_DotSize, halfDot, b, g_DotShape, g_EllipseAspect);
        g_OutputTexture[id.xy] = float4(cr, cg, cb, color.a);
        return;
    }

    // CMYK mode
    float k = 1.0f - max(color.r, max(color.g, color.b));
    float c = k < 1.0f ? (1.0f - color.r - k) / (1.0f - k) : 0.0f;
    float m = k < 1.0f ? (1.0f - color.g - k) / (1.0f - k) : 0.0f;
    float y = k < 1.0f ? (1.0f - color.b - k) / (1.0f - k) : 0.0f;
    float cmyk[4] = {c, m, y, k};

    float3 result = 1.0f;
    for (int ch = 0; ch < 4; ++ch) {
        float aRad = g_CMYK_Angles[ch] * 3.14159265f / 180.0f;
        float cA = cos(aRad), sA = sin(aRad);
        float rrx = px * cA - py * sA;
        float rry = px * sA + py * cA;
        float intensity = clamp(cmyk[ch] * g_Contrast, 0.0f, 1.0f);
        float ink = channelCoverage(rrx, rry, g_DotSize, halfDot,
                                    intensity, g_DotShape, g_EllipseAspect);
        result *= ink;
    }
    g_OutputTexture[id.xy] = float4(result, color.a);
}
)";

inline constexpr const char* HalftoneEntryPoint = "HalftoneCS";
}
