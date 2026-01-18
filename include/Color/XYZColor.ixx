module;
#include "../Define/DllExportMacro.hpp"
export module Color.XYZ;

import std;
import Color.Float;
import Utils.String.UniString;

export namespace ArtifactCore
{

 class LIBRARY_DLL_API XYZColor
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  XYZColor();
  XYZColor(float X, float Y, float Z);
  XYZColor(const XYZColor& other);
  ~XYZColor();

  float X() const;
  float Y() const;
  float Z() const;
  void setX(float X);
  void setY(float Y);
  void setZ(float Z);
  void clamp();

  float luminance() const;
  float deltaE(const XYZColor& other) const;

  XYZColor& operator=(const XYZColor& other);

  bool operator==(const XYZColor& other) const;
  bool operator!=(const XYZColor& other) const;

  FloatColor toFloatColor() const;
  static XYZColor fromFloatColor(const FloatColor& color);

  UniString toString() const;
  static XYZColor fromString(const UniString& str);
 };


};
