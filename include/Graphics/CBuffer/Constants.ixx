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

 struct CBSolidColor
 {
  float color[4]; // RGBAなど。float32x4に対応
 };
#pragma pack(push,1)
 struct CBSolidTransform { float2 offset; float2 scale; };
	
 struct Vertex
 {
  float2 position; // 頂点の2D位置 (x, y)
  float2 texCoord; // テクスチャ座標 (u, v)
 };
#pragma pack(push,1)
 struct LineVertex
 {
  float2 position; // 画面またはワールド空間のXY座標
  float4 color;
	 
 };
	
#pragma pack(push,1)
 struct RectVertex
 {
  float2 position; // XY座標（スクリーン座標 or NDC）
  float4 color;    // RGBA
 };

 // 定数バッファ用（オプション、全体色指定用）
 struct CBFillRectColor
 {
  float4 color;    // 矩形の共通色
 };

	


};