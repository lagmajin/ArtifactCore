module;
#include <glm/glm.hpp>
#include "../Define/DllExportMacro.hpp"
export module Color.Float;

import std;
import FloatRGBA;

export namespace ArtifactCore {

 class FloatRGBA;

 class HSV;
 class XYZ;


 class LIBRARY_DLL_API FloatColor {
 private:
  class Impl;
  Impl* impl_;
 public:
  FloatColor();
  ~FloatColor();
  FloatColor(FloatColor&& color) noexcept;
  float red() const;
  float green() const;
  float blue() const;
  void setRed(float red);
  void setGreen(float green);
  void setBlue(float blue);
  void setAlpha(float alpha);
  void setColor(float red, float green, float blue);
  void setColor(float red, float green, float blue, float alpha);

 };




};