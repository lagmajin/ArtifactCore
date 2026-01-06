module;

#include <QString>
#include <DiligentCore/Common/interface/BasicMath.hpp>

#include "../Define/DllExportMacro.hpp"
export module Core.KeyFrame;

import std;

import Frame.Position;
import Math.Interpolate;

export namespace ArtifactCore {
	
 using namespace Diligent;
	
 using AnimValue = std::variant<
  float,
  uint32_t,
  float2,
  float4
 >;
	
	
 struct KeyFrame {
  FramePosition frame;  // int frame番号やフレーム単位の小数も扱える
  std::any value;// または std::variant<float, Vec2, ...>
  InterpolationType interpolation;
  float cp1_x, cp1_y;
  float cp2_x, cp2_y;
  //Diligent::float2
 };




 /*
 //keyframe class
 class  LIBRARY_DLL_API KeyFrame
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  KeyFrame();
  KeyFrame(const KeyFrame& frame);
  ~KeyFrame();
  float time() const;
  void setTime(float t);

  void setValue(const std::any& v);

 };
 */









};