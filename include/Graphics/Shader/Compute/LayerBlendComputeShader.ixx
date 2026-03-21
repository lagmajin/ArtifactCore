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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
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

// BlendParams は Graphics.LayerBlendPipeline で定義

// =====================================================================
// 共通: 前景(src) + 背景(dst) を読み、ブレンド結果に opacity を適用
// Texture2D<float4> SrcTex : register(t0)  (前景 / LayerA)
// Texture2D<float4> DstTex : register(t1)  (背景 / LayerB)
// RWTexture2D<float4> OutTex : register(u0)
// ConstantBuffer<BlendParams> : register(b0)
// =====================================================================

LIBRARY_DLL_API const QByteArray normalBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = src.rgb * src.a + dst.rgb * (1.0 - src.a);
    float outA = src.a + dst.a * (1.0 - src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray addBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = saturate(dst.rgb + src.rgb);
    float outA = saturate(dst.a + src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray subtractBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = saturate(dst.rgb - src.rgb);
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray mulBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = src.rgb * dst.rgb;
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray screenBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = 1.0 - (1.0 - dst.rgb) * (1.0 - src.rgb);
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray overlayBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

float3 Overlay(float3 base, float3 blend)
{
    float3 r;
    r = (base < 0.5) ? (2.0 * base * blend) : (1.0 - 2.0 * (1.0 - base) * (1.0 - blend));
    return r;
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = Overlay(dst.rgb, src.rgb);
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray darkenBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = min(src.rgb, dst.rgb);
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray lightenBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = max(src.rgb, dst.rgb);
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray colorDodgeBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

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
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = ColorDodge(dst.rgb, src.rgb);
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray colorBurnBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

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
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = ColorBurn(dst.rgb, src.rgb);
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray hardLightBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

float3 HardLight(float3 base, float3 blend)
{
    float3 r;
    r = (blend < 0.5) ? (2.0 * base * blend) : (1.0 - 2.0 * (1.0 - base) * (1.0 - blend));
    return r;
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = HardLight(dst.rgb, src.rgb);
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray softLightBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

float SoftLightChannel(float base, float blend)
{
    return (blend < 0.5)
        ? base - (1.0 - 2.0 * blend) * base * (1.0 - base)
        : base + (2.0 * blend - 1.0) * (sqrt(base) - base);
}

float3 SoftLight(float3 base, float3 blend)
{
    return float3(
        SoftLightChannel(base.r, blend.r),
        SoftLightChannel(base.g, blend.g),
        SoftLightChannel(base.b, blend.b)
    );
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = SoftLight(dst.rgb, src.rgb);
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray differenceBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = abs(dst.rgb - src.rgb);
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray exclusionBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 blended = saturate(src.rgb + dst.rgb - 2.0 * src.rgb * dst.rgb);
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray hueBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

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
    return float3(
        HueToRgb(p, q, hsl.x + 1.0/3.0),
        HueToRgb(p, q, hsl.x),
        HueToRgb(p, q, hsl.x - 1.0/3.0)
    );
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 baseHsl = RgbToHsl(dst.rgb);
    float3 blendHsl = RgbToHsl(src.rgb);
    float3 blended = saturate(HslToRgb(float3(blendHsl.x, baseHsl.y, baseHsl.z)));
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray saturationBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

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
    return float3(
        HueToRgb(p, q, hsl.x + 1.0/3.0),
        HueToRgb(p, q, hsl.x),
        HueToRgb(p, q, hsl.x - 1.0/3.0)
    );
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 baseHsl = RgbToHsl(dst.rgb);
    float3 blendHsl = RgbToHsl(src.rgb);
    float3 blended = saturate(HslToRgb(float3(baseHsl.x, blendHsl.y, baseHsl.z)));
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray colorBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

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
    return float3(
        HueToRgb(p, q, hsl.x + 1.0/3.0),
        HueToRgb(p, q, hsl.x),
        HueToRgb(p, q, hsl.x - 1.0/3.0)
    );
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 baseHsl = RgbToHsl(dst.rgb);
    float3 blendHsl = RgbToHsl(src.rgb);
    float3 blended = saturate(HslToRgb(float3(blendHsl.x, blendHsl.y, baseHsl.z)));
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
}
)";

LIBRARY_DLL_API const QByteArray luminosityBlendShaderText = R"(
Texture2D<float4> SrcTex : register(t0);
Texture2D<float4> DstTex : register(t1);
RWTexture2D<float4> OutTex : register(u0);

cbuffer BlendParams : register(b0)
{
    float opacity;
    uint blendMode;
    float2 _pad;
};

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
    return float3(
        HueToRgb(p, q, hsl.x + 1.0/3.0),
        HueToRgb(p, q, hsl.x),
        HueToRgb(p, q, hsl.x - 1.0/3.0)
    );
}

[numthreads(8,8,1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 src = SrcTex[id.xy];
    float4 dst = DstTex[id.xy];
    float3 baseHsl = RgbToHsl(dst.rgb);
    float3 blendHsl = RgbToHsl(src.rgb);
    float3 blended = saturate(HslToRgb(float3(baseHsl.x, baseHsl.y, blendHsl.z)));
    float outA = max(dst.a, src.a);
    float3 result = lerp(dst.rgb, blended, opacity);
    OutTex[id.xy] = float4(result, outA);
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
 };

}
