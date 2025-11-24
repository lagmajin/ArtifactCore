module ;
#define QT_NO_KEYWORDS
#include <QString>
#include <QByteArray>

module Graphics.Shader.HLSL.Basics.Vertex;

#include "../../../include/Define/DllExportMacro.hpp"

namespace ArtifactCore
{
 LIBRARY_DLL_API const QByteArray lineShaderVSText = R"(cbuffer CameraMatrices : register(b0)
{
    float4x4 WorldMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjMatrix;
    float LineThickness;
};

struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(float4(input.position, 1.0), WorldMatrix);
    float4 viewPos = mul(worldPos, ViewMatrix);
    output.position = mul(viewPos, ProjMatrix);
    return output;
})";

 const QByteArray g_qsBasic2DVS = R"(
cbuffer Constants : register(b0)
{
    float4x4 ModelMatrix;      // オブジェクトの移動、回転、スケール
    float4x4 ViewMatrix;
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

    float4 pos = float4(Input.Position.xy, 0.0f, 1.0f);
    float4 worldPos = mul(ModelMatrix, pos);
    float4 viewPos  = mul(ViewMatrix, worldPos);
    Out.Position    = mul(ProjectionMatrix, viewPos);

    Out.TexCoord = Input.TexCoord;
    return Out;
})";

 LIBRARY_DLL_API const QByteArray drawOutlineRectVSSource=R"(
cbuffer TransformCB : register(b0)
{
    float2 offset;     // 矩形の位置オフセット（必要なら）
    float2 screenSize; // 画面サイズ
};

struct VSInput
{
    float2 pos : ATTRIB0;
    float4 color : ATTRIB1;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float4 color : COLOR0;
   
};

PSInput main(VSInput input)
{
    PSInput output;
    float2 ndc = input.pos / screenSize * 2.0f - float2(1.0f,1.0f);
    ndc.y = -ndc.y; // Y軸反転
    output.pos = float4(ndc, 0, 1);
    output.color = input.color;
    return output;
}


  )";



 LIBRARY_DLL_API const QByteArray drawSolidRectVSSource= R"(
struct VSInput
{
    float2 pos : ATTRIB0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
};

cbuffer TransformCB : register(b0)
{
    float2 offset; // x, y 位置
    float2 scale;  // 幅・高さ
    float2 screenSize;
};

PSInput main(VSInput input)
{
    PSInput output;

    // input.pos はピクセル単位
    float2 pos = input.pos + offset;       // 矩形左上 + ピクセル座標
    float2 ndc = pos / screenSize * 2.0f - float2(1.0f, 1.0f);
    ndc.y = -ndc.y; // Y軸反転

    output.pos = float4(ndc, 0.0f, 1.0f);
    return output;
}


)";









}