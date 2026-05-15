module;
#include <QString>
#include <utility>

#include "../Define/DllExportMacro.hpp"
export module CompressQuality;

export namespace ArtifactCore
{

 class LIBRARY_DLL_API CompressQuality
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  CompressQuality();
  ~CompressQuality();
  QString toQString() const;

  
 };








};
