module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
#include <string>
export module Utils.NameGenerator;

import Core.ArtifactString;

export namespace ArtifactCore
{

 class LIBRARY_DLL_API PatternNameGenerator
 {
 private:
  class Impl;
  Impl* impl_;
  int width_ = 0;

  ZeroString makeCandidateZero(const std::string& base, int n) const;
  std::string makeCandidate(const std::string& base, int n) const;

 public:
  PatternNameGenerator(const std::string& pattern, int zeroPad = 0);
  ~PatternNameGenerator();

  PatternNameGenerator(const PatternNameGenerator&) = delete;
  PatternNameGenerator& operator=(const PatternNameGenerator&) = delete;

  std::string Generate(const std::string& baseName);

  void Release(const std::string& name);
 };
















};
