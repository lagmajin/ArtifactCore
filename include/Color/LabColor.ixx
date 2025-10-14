module;
#include "../Define/DllExportMacro.hpp"
export module Color.Lab;

import Color.Float;

export namespace ArtifactCore
{


  class LIBRARY_DLL_API LabColor
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  LabColor();
  LabColor(const LabColor& color);
  ~LabColor();
  float L() const;
  float a() const;
  float b() const;
  void setL(float L);
  void setA(float a);
  void setB(float b);
  float luminance() const;          // L値をそのまま使用
  float deltaE(const LabColor& other) const;
  LabColor& operator=(const LabColor& other);
 };







};