module;
#include <QByteArray>


#include "../../../Define/DllExportMacro.hpp"
export module Graphics.Shader.Compute.HLSL.Blend;

import Layer.Blend;

import std;


export namespace ArtifactCore
{

 LIBRARY_DLL_API const QByteArray normalBlendShaderText = R"(

Texture2D<float4> LayerA : register(t0);
Texture2D<float4> LayerB : register(t1);
RWTexture2D<float4> OutImage : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 a = LayerA[id.xy];
    float4 b = LayerB[id.xy];

    // 標準的なアルファブレンド (A over B)
    float alpha = a.a;
    float4 result = a * alpha + b * (1.0 - alpha);

    OutImage[id.xy] = result;
}

)";

 LIBRARY_DLL_API const QByteArray addBlendShaderText = R"(

Texture2D<float4> LayerA : register(t0);
Texture2D<float4> LayerB : register(t1);
RWTexture2D<float4> OutImage : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 a = LayerA[id.xy];
    float4 b = LayerB[id.xy];

    // 加算合成
    float4 result = a + b;

    OutImage[id.xy] = saturate(result); // 0〜1にクランプ
}

)";

 LIBRARY_DLL_API const QByteArray mulBlendShaderText = R"(

Texture2D<float4> LayerA : register(t0);
Texture2D<float4> LayerB : register(t1);
RWTexture2D<float4> OutImage : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 a = LayerA[id.xy];
    float4 b = LayerB[id.xy];

    // 乗算合成
    float4 result = a * b;

    OutImage[id.xy] = result;
}

)";

 LIBRARY_DLL_API const QByteArray screenBlendShaderText = R"(

Texture2D<float4> LayerA : register(t0);
Texture2D<float4> LayerB : register(t1);
RWTexture2D<float4> OutImage : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    float4 a = LayerA[id.xy];
    float4 b = LayerB[id.xy];

    // スクリーン合成
    float4 result = 1.0 - (1.0 - a) * (1.0 - b);

    OutImage[id.xy] = result;
}

)";



 const std::map<LAYER_BLEND_TYPE, QByteArray> BlendShaders = {
  {LAYER_BLEND_TYPE::BLEND_NORMAL, normalBlendShaderText},
  {LAYER_BLEND_TYPE::BLEND_MULTIPLY, mulBlendShaderText},
  {LAYER_BLEND_TYPE::BLEND_SCREEN, screenBlendShaderText},
  // 他のブレンドタイプに対応するシェーダもここに追加
 };









}