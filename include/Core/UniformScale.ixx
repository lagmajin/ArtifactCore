module;
#include <utility>

#include "../Define/DllExportMacro.hpp"
#include <QString>
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
