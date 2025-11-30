module ;
#include "../Define/DllExportMacro.hpp"
export module Utils.NameGenerator;

import std;

export namespace ArtifactCore
{

 class LIBRARY_DLL_API PatternNameGenerator
 {
 private:
  class Impl;
  Impl* impl_;

  std::string pattern_;
  int width_ = 0;

  std::string makeCandidate(const std::string& base, int n) const;

 public:
  PatternNameGenerator(const std::string& pattern, int zeroPad = 0);
  ~PatternNameGenerator();
  std::string Generate(const std::string& baseName);

  void Release(const std::string& name);
 };
















};