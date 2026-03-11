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

 LIBRARY_DLL_API const QByteArray normalBlendShaderText_New = R"(

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


	

 LIBRARY_DLL_API const std::map<BlendMode, QByteArray> BlendShaders = {
  {BlendMode::Normal, normalBlendShaderText},
  {BlendMode::Multiply, mulBlendShaderText},
  {BlendMode::Screen, screenBlendShaderText},
  {BlendMode::Add, addBlendShaderText },
 	
  // 他のブレンドタイプに対応するシェーダもここに追加
 };









}