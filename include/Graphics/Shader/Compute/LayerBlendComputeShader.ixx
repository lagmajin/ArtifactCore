module;
#include <QByteArray>

#include "../../../Define/DllExportMacro.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Graphics.Shader.Compute.HLSL.Blend;

import Layer.Blend;

export namespace ArtifactCore
{

// =====================================================================
// Common Header
// =====================================================================
const char* blendShaderHeader = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

#define CHECK_BOUNDS \
    uint outWidth, outHeight; \
    OutTex.GetDimensions(outWidth, outHeight); \
    if (id.x >= outWidth || id.y >= outHeight) return;

#define LOAD_BLEND_PIXELS \
    CHECK_BOUNDS \
    float4 src = SrcTex[id.xy]; \
    float4 dst = DstTex[id.xy]; \
    float srcA = saturate(src.a * opacity); \
    float3 srcRGB = src.rgb * srcA; \
    float3 srcColor = src.rgb; \
    if (srcA <= 0.0001) { OutTex[id.xy] = dst; return; } \
    if (dst.a <= 0.0001) { OutTex[id.xy] = float4(srcRGB, srcA); return; }
)";

// =====================================================================
// Individual Shaders
// =====================================================================

LIBRARY_DLL_API const QByteArray normalBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = srcRGB + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray addBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = saturate(dst.rgb + srcRGB);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray subtractBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = saturate(dst.rgb - srcRGB);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray mulBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = (srcRGB * dst.rgb) + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray screenBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 screenRes = srcRGB + dst.rgb - (srcRGB * dst.rgb);
    float outA = srcA + dst.a - (srcA * dst.a);
    OutTex[id.xy] = float4(screenRes, outA);
}
)";

LIBRARY_DLL_API const QByteArray overlayBlendShaderText = QByteArray(blendShaderHeader) + R"(
float3 Overlay(float3 base, float3 blend)
{
    float3 r;
    r.r = (base.r < 0.5) ? (2.0 * base.r * blend.r) : (1.0 - 2.0 * (1.0 - base.r) * (1.0 - blend.r));
    r.g = (base.g < 0.5) ? (2.0 * base.g * blend.g) : (1.0 - 2.0 * (1.0 - base.g) * (1.0 - blend.g));
    r.b = (base.b < 0.5) ? (2.0 * base.b * blend.b) : (1.0 - 2.0 * (1.0 - base.b) * (1.0 - blend.b));
    return r;
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = Overlay(dst.rgb, srcRGB) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray darkenBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = min(srcRGB, dst.rgb) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray lightenBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = max(srcRGB, dst.rgb) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray colorDodgeBlendShaderText = QByteArray(blendShaderHeader) + R"(
float3 ColorDodge(float3 base, float3 blend)
{
    float3 r;
    r.r = (blend.r < 1.0) ? saturate(base.r / (1.0 - blend.r)) : 1.0;
    r.g = (blend.g < 1.0) ? saturate(base.g / (1.0 - blend.g)) : 1.0;
    r.b = (blend.b < 1.0) ? saturate(base.b / (1.0 - blend.b)) : 1.0;
    return r;
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = ColorDodge(dst.rgb, srcRGB) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray colorBurnBlendShaderText = QByteArray(blendShaderHeader) + R"(
float3 ColorBurn(float3 base, float3 blend)
{
    float3 r;
    r.r = (blend.r > 0.0) ? saturate(1.0 - (1.0 - base.r) / blend.r) : 0.0;
    r.g = (blend.g > 0.0) ? saturate(1.0 - (1.0 - base.g) / blend.g) : 0.0;
    r.b = (blend.b > 0.0) ? saturate(1.0 - (1.0 - base.b) / blend.b) : 0.0;
    return r;
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = ColorBurn(dst.rgb, srcRGB) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray hardLightBlendShaderText = QByteArray(blendShaderHeader) + R"(
float3 HardLight(float3 base, float3 blend)
{
    float3 r;
    r.r = (blend.r < 0.5) ? (2.0 * base.r * blend.r) : (1.0 - 2.0 * (1.0 - base.r) * (1.0 - blend.r));
    r.g = (blend.g < 0.5) ? (2.0 * base.g * blend.g) : (1.0 - 2.0 * (1.0 - base.g) * (1.0 - blend.g));
    r.b = (blend.b < 0.5) ? (2.0 * base.b * blend.b) : (1.0 - 2.0 * (1.0 - base.b) * (1.0 - blend.b));
    return r;
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = HardLight(dst.rgb, srcRGB) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray softLightBlendShaderText = QByteArray(blendShaderHeader) + R"(
float SoftLightChannel(float base, float blend)
{
    return (blend < 0.5)
        ? base - (1.0 - 2.0 * blend) * base * (1.0 - base)
        : base + (2.0 * blend - 1.0) * (sqrt(base) - base);
}

float3 SoftLight(float3 base, float3 blend)
{
    return float3(SoftLightChannel(base.r, blend.r), SoftLightChannel(base.g, blend.g), SoftLightChannel(base.b, blend.b));
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = SoftLight(dst.rgb, srcRGB) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray differenceBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = abs(dst.rgb - srcRGB) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray exclusionBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = (srcRGB + dst.rgb - 2.0 * srcRGB * dst.rgb) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

// HSL Helper functions
const char* hslHelpers = R"(
float3 RgbToHsl(float3 c)
{
    float mx = max(max(c.r, c.g), c.b);
    float mn = min(min(c.r, c.g), c.b);
    float d = mx - mn;
    float h = 0.0, s = 0.0, l = (mx + mn) * 0.5;
    if (d > 1e-5) {
        s = (l < 0.5) ? (d / (mx + mn)) : (d / (2.0 - mx - mn));
        if (mx == c.r) h = fmod(((c.g - c.b) / d) + (c.g < c.b ? 6.0 : 0.0), 6.0);
        else if (mx == c.g) h = ((c.b - c.r) / d) + 2.0;
        else h = ((c.r - c.g) / d) + 4.0;
        h /= 6.0;
    }
    return float3(h, s, l);
}

float HueToRgb(float p, float q, float t)
{
    t = fmod(t, 1.0);
    if (t < 0.0) t += 1.0;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 0.5) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}

float3 HslToRgb(float3 hsl)
{
    if (hsl.y < 1e-5) return float3(hsl.z, hsl.z, hsl.z);
    float q = (hsl.z < 0.5) ? (hsl.z * (1.0 + hsl.y)) : (hsl.z + hsl.y - hsl.z * hsl.y);
    float p = 2.0 * hsl.z - q;
    return float3(HueToRgb(p, q, hsl.x + 1.0/3.0), HueToRgb(p, q, hsl.x), HueToRgb(p, q, hsl.x - 1.0/3.0));
}
)";

LIBRARY_DLL_API const QByteArray hueBlendShaderText = QByteArray(blendShaderHeader) + hslHelpers + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 baseHsl = RgbToHsl(dst.rgb);
    float3 blendHsl = RgbToHsl(srcRGB);
    float3 blended = saturate(HslToRgb(float3(blendHsl.x, baseHsl.y, baseHsl.z))) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray saturationBlendShaderText = QByteArray(blendShaderHeader) + hslHelpers + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 baseHsl = RgbToHsl(dst.rgb);
    float3 blendHsl = RgbToHsl(srcRGB);
    float3 blended = saturate(HslToRgb(float3(baseHsl.x, blendHsl.y, baseHsl.z))) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray colorBlendShaderText = QByteArray(blendShaderHeader) + hslHelpers + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 baseHsl = RgbToHsl(dst.rgb);
    float3 blendHsl = RgbToHsl(srcRGB);
    float3 blended = saturate(HslToRgb(float3(blendHsl.x, blendHsl.y, baseHsl.z))) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray luminosityBlendShaderText = QByteArray(blendShaderHeader) + hslHelpers + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 baseHsl = RgbToHsl(dst.rgb);
    float3 blendHsl = RgbToHsl(srcRGB);
    float3 blended = saturate(HslToRgb(float3(baseHsl.x, baseHsl.y, blendHsl.z))) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray linearBurnBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = saturate(srcRGB + dst.rgb - 1.0) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray divideBlendShaderText = QByteArray(blendShaderHeader) + R"(
float3 Divide(float3 base, float3 blend)
{
    float3 r;
    r.r = saturate(base.r / max(blend.r, 1e-6));
    r.g = saturate(base.g / max(blend.g, 1e-6));
    r.b = saturate(base.b / max(blend.b, 1e-6));
    return r;
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = Divide(dst.rgb, srcRGB) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray pinLightBlendShaderText = QByteArray(blendShaderHeader) + R"(
float3 PinLight(float3 base, float3 blend)
{
    float3 r;
    r.r = (blend.r < 0.5) ? min(base.r, 2.0 * blend.r) : max(base.r, 2.0 * (blend.r - 0.5));
    r.g = (blend.g < 0.5) ? min(base.g, 2.0 * blend.g) : max(base.g, 2.0 * (blend.g - 0.5));
    r.b = (blend.b < 0.5) ? min(base.b, 2.0 * blend.b) : max(base.b, 2.0 * (blend.b - 0.5));
    return r;
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = PinLight(dst.rgb, srcRGB) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray vividLightBlendShaderText = QByteArray(blendShaderHeader) + R"(
float3 VividLight(float3 base, float3 blend)
{
    float3 r;
    r.r = (blend.r < 0.5)
        ? (blend.r < 1e-6 ? 0.0 : saturate(1.0 - (1.0 - base.r) / (2.0 * blend.r)))
        : (blend.r > 0.999 ? 1.0 : saturate(base.r / (2.0 * (1.0 - blend.r))));
    r.g = (blend.g < 0.5)
        ? (blend.g < 1e-6 ? 0.0 : saturate(1.0 - (1.0 - base.g) / (2.0 * blend.g)))
        : (blend.g > 0.999 ? 1.0 : saturate(base.g / (2.0 * (1.0 - blend.g))));
    r.b = (blend.b < 0.5)
        ? (blend.b < 1e-6 ? 0.0 : saturate(1.0 - (1.0 - base.b) / (2.0 * blend.b)))
        : (blend.b > 0.999 ? 1.0 : saturate(base.b / (2.0 * (1.0 - blend.b))));
    return r;
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = VividLight(dst.rgb, srcRGB) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray linearLightBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = saturate(dst.rgb + 2.0 * srcRGB - 1.0) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

LIBRARY_DLL_API const QByteArray hardMixBlendShaderText = QByteArray(blendShaderHeader) + R"(
[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    LOAD_BLEND_PIXELS
    float3 blended = step(1.0 - srcRGB, dst.rgb) * srcA + dst.rgb * (1.0 - srcA);
    float outA = srcA + dst.a * (1.0 - srcA);
    OutTex[id.xy] = float4(blended, outA);
}
)";

 LIBRARY_DLL_API const std::map<BlendMode, QByteArray> BlendShaders = {
  {BlendMode::Normal,      normalBlendShaderText},
  {BlendMode::Add,         addBlendShaderText},
  {BlendMode::Subtract,    subtractBlendShaderText},
  {BlendMode::Multiply,    mulBlendShaderText},
  {BlendMode::Screen,      screenBlendShaderText},
  {BlendMode::Overlay,     overlayBlendShaderText},
  {BlendMode::Darken,      darkenBlendShaderText},
  {BlendMode::Lighten,     lightenBlendShaderText},
  {BlendMode::ColorDodge,  colorDodgeBlendShaderText},
  {BlendMode::ColorBurn,   colorBurnBlendShaderText},
  {BlendMode::HardLight,   hardLightBlendShaderText},
  {BlendMode::SoftLight,   softLightBlendShaderText},
  {BlendMode::Difference,  differenceBlendShaderText},
  {BlendMode::Exclusion,   exclusionBlendShaderText},
  {BlendMode::Hue,         hueBlendShaderText},
  {BlendMode::Saturation,  saturationBlendShaderText},
  {BlendMode::Color,       colorBlendShaderText},
  {BlendMode::Luminosity,  luminosityBlendShaderText},
  {BlendMode::LinearBurn,  linearBurnBlendShaderText},
  {BlendMode::Divide,      divideBlendShaderText},
  {BlendMode::PinLight,    pinLightBlendShaderText},
  {BlendMode::VividLight,  vividLightBlendShaderText},
  {BlendMode::LinearLight, linearLightBlendShaderText},
  {BlendMode::HardMix,     hardMixBlendShaderText},
 };

}
