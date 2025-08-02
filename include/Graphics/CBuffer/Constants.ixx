module;
#include <DiligentCore/Common/interface/BasicMath.hpp>
export module Graphics.CBuffer.Constants;

export namespace ArtifactCore
{
 using namespace Diligent;

 struct ViewMatrix
 {
	 
 };

 struct Constants
 {
  float4x4 ModelMatrix;
  float4x4 ViewMatrix;
  float4x4 ProjectionMatrix;
 };



 struct Vertex
 {
  float2 position; // 頂点の2D位置 (x, y)
  float2 texCoord; // テクスチャ座標 (u, v)
 };

 struct LineVertex
 {
  float2 position; // 画面またはワールド空間のXY座標
  float4 color;
	 
 };


};