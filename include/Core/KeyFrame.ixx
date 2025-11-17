module;

#include <QString>

#include "../Define/DllExportMacro.hpp"
export module Core.KeyFrame;

import std;

import Frame.Position;

export namespace ArtifactCore {

 struct KeyFrame {
  FramePosition frame;  // int frame番号やフレーム単位の小数も扱える
  std::any value;       // または std::variant<float, Vec2, ...>
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