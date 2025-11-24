module ;
#include <QByteArray>


//#include <../ArtifactWidgets/include/Define/DllExportMacro.hpp>

module Graphics.Shader.HLSL.Basics.Pixel;

#include "../../../include/Define/DllExportMacro.hpp"
namespace ArtifactCore {

 LIBRARY_DLL_API const QByteArray g_qsBasicSprite2DImagePS = R"(
struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = g_texture.Sample(g_sampler, input.TexCoord);
    return color;
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
};

float4 main(PSInput input) : SV_TARGET
{
    return input.color;
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
    //float2 TexCoord : TEXCOORD0; // VSに合わせて存在させてるだけ
};

float4 main(PS_INPUT input) : SV_TARGET
{
    return uColor;
}
)";





};