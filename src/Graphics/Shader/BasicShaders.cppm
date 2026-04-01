module ;
#include <QByteArray>


//#include <../ArtifactWidgets/include/Define/DllExportMacro.hpp>

#include "../../../include/Define/DllExportMacro.hpp"
module Graphics.Shader.HLSL.Basics.Pixel;

namespace ArtifactCore {

 LIBRARY_DLL_API const QByteArray g_qsBasicSprite2DImagePS = R"(
struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR0;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 sampled = g_texture.Sample(g_sampler, input.TexCoord);
    
    // Anti-aliasing at the quad edges
    float dx = min(input.TexCoord.x, 1.0 - input.TexCoord.x);
    float dy = min(input.TexCoord.y, 1.0 - input.TexCoord.y);
    float d = min(dx, dy);
    
    float2 fw = fwidth(input.TexCoord);
    float edgeWidth = max(fw.x, fw.y);
    
    float alphaMultiplier = (edgeWidth > 0.0001) ? smoothstep(0.0, edgeWidth * 1.0, d) : 1.0;

    return sampled * input.Color * float4(1.0, 1.0, 1.0, alphaMultiplier);
}
)";



 LIBRARY_DLL_API const QByteArray   g_qsSolidColorPS2 = R"(

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    //float2 TexCoord : TEXCOORD0; // 使わないが、VSからの出力に合わせて定義
};
//Texture2D g_texture : register(t0);
//SamplerState g_sampler : register(s0);
float4 main(PS_INPUT input) : SV_TARGET
{
    
    return float4(1.0f, 0.0f, 0.0f, 1.0f); // RGBA (Red, Green, Blue, Alpha)
}
)";

 LIBRARY_DLL_API const QByteArray drawOutlineRectPSSource = R"(
struct PSInput
{
    float4 pos : SV_POSITION;
    float4 color : COLOR0;
    float2 uv  : TEXCOORD0;
};

cbuffer OutlineParams : register(b1)
{
    float outlineThickness; // アウトラインの太さ (0.0-1.0 のUV空間)
};

float4 main(PSInput input) : SV_TARGET
{
    // UV座標の端からの距離を計算
    float dx = min(input.uv.x, 1.0 - input.uv.x);
    float dy = min(input.uv.y, 1.0 - input.uv.y);
    float distToEdge = min(dx, dy);
    
    // エッジ幅を計算（アンチエイリアス用）
    float2 fw = fwidth(input.uv);
    float edgeWidth = max(fw.x, fw.y) * 2.0;
    
    // アウトライン内かどうかを判定
    float innerEdge = outlineThickness - edgeWidth;
    float outerEdge = outlineThickness + edgeWidth;
    
    float outlineAlpha = 1.0 - smoothstep(innerEdge, outerEdge, distToEdge);
    
    // アウトラインの外側は透明に
    if (distToEdge > outlineThickness + edgeWidth * 2.0) {
        discard;
    }
    
    float4 result = input.color;
    result.a *= outlineAlpha;
    return result;
}
)";

 LIBRARY_DLL_API const QByteArray g_qsSolidColorPSSource = R"(

cbuffer ColorBuffer : register(b0)
{
    float4 uColor; // 外部から指定するRGBAカラー
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 uv       : TEXCOORD0; 
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // Local UV is 0.0 to 1.0 from VSInput pos in drawSolidRectTransformVSSource.
    // However, drawSolidRectVSSource does not output uv. 
    // To be safe and compatible with both, we check if fwidth works well.
    // If uv is 0, derivatives are 0, alpha is 1.0.
    float dx = min(input.uv.x, 1.0 - input.uv.x);
    float dy = min(input.uv.y, 1.0 - input.uv.y);
    float d = min(dx, dy);
    
    float2 fw = fwidth(input.uv);
    float edgeWidth = max(fw.x, fw.y);
    
    if (d < edgeWidth) {
        float alpha = d / edgeWidth;
        return float4(uColor.rgb, uColor.a * alpha);
    }
    
    return uColor;
}
)";

// Batch pixel shader: uses vertex color directly (no ColorBuffer)
LIBRARY_DLL_API const QByteArray g_qsBatchSolidColorPSSource = R"(
struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return input.Color;
}
)";

// Batch pixel shader: uses vertex color directly (no ColorBuffer)
LIBRARY_DLL_API const QByteArray g_qsBatchSolidColorPSSource = R"(
struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return input.Color;
}
)";




};
