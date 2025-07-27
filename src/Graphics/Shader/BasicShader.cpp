module;
#include <QByteArray>

//#include <../ArtifactWidgets/include/Define/DllExportMacro.hpp>

module Graphics.Shader.Basics;


namespace ArtifactCore {

 const QByteArray g_qsBasic2DImagePS = R"(
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

const QByteArray g_qsBasic2DVS= R"(
cbuffer Constants : register(b0)
{
    float4x4 ModelMatrix;      // オブジェクトの移動、回転、スケール
    float4x4 ProjectionMatrix; // ワールド空間からクリップ空間への変換
};

struct VSInput
{
    float2 Position : ATTRIB0;
float2 TexCoord  : ATTRIB1;
};

struct PSInput
{
    float4 Position : SV_POSITION;
float2 TexCoord  : TEXCOORD0;
};

PSInput main(VSInput Input)
{
    PSInput Out;

    float4 pos = float4(Input.Position.xy, 0.0f, 1.0f); // 2D位置を4Dに拡張

    // モデル行列を適用してワールド空間での位置を計算
    float4 worldPos = mul(ModelMatrix, pos);

    // プロジェクション行列を適用してクリップ空間での位置を計算
    Out.Position = mul(ProjectionMatrix, worldPos);
Out.TexCoord = Input.TexCoord;
    return Out;
}
)";


const QByteArray g_qsSolidColorPS = R"(

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0; // 使わないが、VSからの出力に合わせて定義
};
//Texture2D g_texture : register(t0);
//SamplerState g_sampler : register(s0);
float4 main(PS_INPUT input) : SV_TARGET
{
    
    return float4(1.0f, 0.0f, 0.0f, 1.0f); // RGBA (Red, Green, Blue, Alpha)
}
)";







};