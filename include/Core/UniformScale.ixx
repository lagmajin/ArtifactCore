module;
#include <QString>

#include "../Define/DllExportMacro.hpp"
export module Core.UniformScale;


export namespace ArtifactCore
{

 class LIBRARY_DLL_API UniformScale {
 private:
  class Impl;
  Impl* impl_;
 public:
  UniformScale();
  ~UniformScale();
  float scale() const;
 };







};