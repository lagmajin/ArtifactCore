module;
#include <DiligentCore/Common/interface/BasicMath.hpp>
export module Graphics.CBuffer.Constants.PixelShader;

export namespace ArtifactCore {
 using namespace Diligent;

 struct CBSolidColor
 {
  float color[4]; // RGBAなど。float32x4に対応
 };

 // 定数バッファ用（オプション、全体色指定用）
 struct CBFillRectColor
 {
  float4 color;    // 矩形の共通色
 };


 struct SpritePixelConstants {
  float4 SolidColor; // RGBA
  float4 ExtraParams; // [不透明度, 彩度, 反転フラグ, 予備]
 };



};