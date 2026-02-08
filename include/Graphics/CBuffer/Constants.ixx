module;
#include <DiligentCore/Common/interface/BasicMath.hpp>
export module Graphics.CBuffer.Constants.VertexShader;

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


#pragma pack(push,1)
 struct CBSolidTransform { float2 offset; float2 scale; };
#pragma pack(pop)

#pragma pack(push,1)
 struct CBSolidTransform2D
 {
  float2 offset;      // 左上原点の位置（ピクセル単位）
  float2 scale;       // 幅・高さ（ピクセル単位）
  float2 screenSize;  // スワップチェーン幅・高さ
 };
#pragma pack(pop)


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


#pragma pack(push,1)
 struct DrawSpriteConstants
 {
  float4x4 ProjectionMatrix;
  float4x4 ViewMatrix;

 };


};