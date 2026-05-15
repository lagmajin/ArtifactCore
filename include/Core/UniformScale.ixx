module;
#include <utility>

#include "../Define/DllExportMacro.hpp"
export module Core.UniformScale;

#include <QString>


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
