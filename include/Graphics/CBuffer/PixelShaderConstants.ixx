module;
#include <utility>
#include <DiligentCore/Common/interface/BasicMath.hpp>
export module Graphics.CBuffer.Constants.PixelShader;

export namespace ArtifactCore {
 using namespace Diligent;

 struct CBSolidColor
 {
  float color[4]; // RGBA stored as float32x4
 };

 // Constant buffer for fill color overrides
 struct CBFillRectColor
 {
  float4 color;    // Shared fill color
 };

 struct SpritePixelConstants {
  float4 SolidColor; // RGBA
  float4 ExtraParams; // [invert, opacity, grayscale flag, reserved]
 };

};
