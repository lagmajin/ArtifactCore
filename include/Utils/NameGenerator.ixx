module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
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

  ZeroString makeCandidateZero(const String& base, int n) const;
  String makeCandidate(const String& base, int n) const;

 public:
  PatternNameGenerator(const String& pattern, int zeroPad = 0);
  ~PatternNameGenerator();

  PatternNameGenerator(const PatternNameGenerator&) = delete;
  PatternNameGenerator& operator=(const PatternNameGenerator&) = delete;

  String Generate(const String& baseName);

  void Release(const String& name);
 };
















};
