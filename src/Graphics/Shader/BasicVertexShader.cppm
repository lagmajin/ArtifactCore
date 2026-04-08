module;
#include <utility>
#include <QString>
#include <QByteArray>
#include "../../../include/Define/DllExportMacro.hpp"
#define QT_NO_KEYWORDS

module Graphics.Shader.HLSL.Basics.Vertex;


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
cbuffer TransformCB : register(b0)
{
    float2 offset; // x, y 位置
    float2 scale;  // 幅・高さ
    float2 screenSize;
};

struct VSInput
{
    float2 Position : ATTRIB0;
    float2 TexCoord : ATTRIB1;
    float4 Color    : ATTRIB2;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR0;
};

PSInput main(VSInput Input)
{
    PSInput Out;

    // Input.Position は 0..1 のローカル座標
    float2 pos = offset + Input.Position.xy * scale;
    float2 ndc = pos / screenSize * 2.0f - float2(1.0f, 1.0f);
    ndc.y = -ndc.y; // Y軸反転

    Out.Position = float4(ndc, 0.0f, 1.0f);
    Out.TexCoord = Input.TexCoord;
    Out.Color    = Input.Color;
    return Out;
}
)";

 LIBRARY_DLL_API const QByteArray drawOutlineRectVSSource=R"(
cbuffer TransformCB : register(b0)
{
    float2 offset;     // 矩形の位置オフセット
    float2 scale;      // 幅・高さ
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
    float2 uv  : TEXCOORD0;
};

PSInput main(VSInput input)
{
    PSInput output;
    // offset + pos * scale でワールド座標に変換
    float2 worldPos = offset + input.pos * scale;
    float2 ndc = worldPos / screenSize * 2.0f - float2(1.0f, 1.0f);
    ndc.y = -ndc.y;
    output.pos = float4(ndc, 0, 1);
    output.color = input.color;
    output.uv = input.pos;
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
    float2 uv  : TEXCOORD0;
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

    // Composition -> View: viewPos = input.pos * scale + offset
    float2 pos = input.pos * scale + offset;
    float2 ndc = pos / screenSize * 2.0f - float2(1.0f, 1.0f);
    ndc.y = -ndc.y; // Y軸反転

    output.pos = float4(ndc, 0.0f, 1.0f);
    output.uv = input.pos;
    return output;
}
)";

// Pass-through vertex shader for batch rendering (NDC coords pre-computed on CPU)
LIBRARY_DLL_API const QByteArray drawBatchSolidRectVSSource = R"(
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
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}
)";
LIBRARY_DLL_API const QByteArray drawSolidRectTransformVSSource = R"(
cbuffer TransformCB : register(b0)
{
    float4 row0;
    float4 row1;
    float4 row2;
    float4 row3;
};

struct VSInput
{
    float2 pos : ATTRIB0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

PSInput main(VSInput input)
{
    PSInput output;
    float4 localPos = float4(input.pos.xy, 0.0f, 1.0f);
    output.pos = float4(
        dot(localPos, row0),
        dot(localPos, row1),
        dot(localPos, row2),
        dot(localPos, row3));
    output.uv = input.pos.xy;
    return output;
}
)";
LIBRARY_DLL_API const QByteArray drawSpriteTransformVSSource = R"(
cbuffer TransformCB : register(b0)
{
    float4 row0;
    float4 row1;
    float4 row2;
    float4 row3;
};

struct VSInput
{
    float2 pos : ATTRIB0;
    float2 texCoord : ATTRIB1;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
};

PSInput main(VSInput input)
{
    PSInput output;
    float4 localPos = float4(input.pos.xy, 0.0f, 1.0f);
    output.pos = float4(
        dot(localPos, row0),
        dot(localPos, row1),
        dot(localPos, row2),
        dot(localPos, row3));
    output.texCoord = input.texCoord;
    output.color = float4(1.0, 1.0, 1.0, 1.0);
    return output;
}
)";









}
