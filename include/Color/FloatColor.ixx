module;
#include <tuple>
#include <vector>

#include <memory>


export module FloatColor;

import FloatRGBA;

export namespace ArtifactCore {

 class FloatRGBA;

 class HSV;
 class XYZ;

 class FloatColorPrivate;

 class FloatColor {
 private:
  FloatColorPrivate* const	pColor_;
 public:
  void setRed(float red);
  void setGreen(float green);
  void setBlue(float blue);
  void setAlpha(float alpha);
  void setColor(float red, float green, float blue);
  void setColor(float red, float green, float blue, float alpha);
 };




};