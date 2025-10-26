module;
#include <QString>
#include <QByteArray>
#include "../../../include/Define/DllExportMacro.hpp"
module Graphics.Shader.HLSL.Basics.Vertex;



namespace ArtifactCore
{
 LIBRARY_DLL_API const QByteArray lineShaderVSText = R"(
cbuffer CameraMatrices : register(b0)
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
}

)";

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

    float4 pos = float4(Input.Position.xy, 0.0f, 1.0f); // 2D位置を4Dに拡張

    float4 worldPos = mul(ModelMatrix, pos);
    float4 viewPos  = mul(ViewMatrix, worldPos);
    Out.Position    = mul(ProjectionMatrix, viewPos);

    Out.TexCoord = Input.TexCoord;
    return Out;
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
};

PSInput main(VSInput input)
{
    PSInput output;
    float2 position = input.pos * scale + offset;
    output.pos = float4(position, 0.0f, 1.0f);
    return output;
}


)";









}