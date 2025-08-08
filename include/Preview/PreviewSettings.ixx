module;
#include "../Define/DllExportMacro.hpp"
export module Preview.Settings;

export namespace ArtifactCore
{
 
 class LIBRARY_DLL_API PreviewSettings
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  PreviewSettings();
  ~PreviewSettings();
 };










}