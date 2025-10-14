
module;
#include "../Define/DllExportMacro.hpp"
#include <QString>
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