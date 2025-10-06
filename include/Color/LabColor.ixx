module;
#include "../Define/DllExportMacro.hpp"
export module Color.Lab;

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
  void setL(float L);
  void setA(float a);
  void setB(float b);

  LabColor& operator=(const LabColor& other);
 };







};