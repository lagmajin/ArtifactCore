module;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Utils.NameGenerator;

import Core.ArtifactString;
import Container.NameMap;

namespace ArtifactCore
{
 namespace {
 ZeroString formatPaddedNumber(int value, int width)
 {
  char buffer[32];
  if (width > 0) {
   std::snprintf(buffer, sizeof(buffer), "%0*d", width, value);
  } else {
   std::snprintf(buffer, sizeof(buffer), "%d", value);
  }
  return ZeroString(buffer);
 }
 }

 class PatternNameGenerator::Impl
 {
 public:
  NameMap<std::string, int> counters{
      makeNameMap<std::string, int>(ContainerName{"PatternNameCounters"})};
  NameMap<std::string, bool> usedNames{
      makeNameMap<std::string, bool>(ContainerName{"PatternNameUsedNames"})};
  ZeroString pattern_;
  int width_ = 0;
 };
	
 ZeroString PatternNameGenerator::makeCandidateZero(const String& base, int n) const
 {
  ZeroString s = impl_->pattern_;
  size_t pos = s.find("(\\name)");
  if (pos != static_cast<size_t>(-1))
   s.replace(pos, 7, base.data());
  pos = s.find("***");
 if (pos != static_cast<size_t>(-1))
  {
   const ZeroString replacement = formatPaddedNumber(n, width_);
   s.replace(pos, 3, replacement.data());
  }
  return s;
 }

 String PatternNameGenerator::makeCandidate(const String& base, int n) const
 {
  const ZeroString candidate = makeCandidateZero(base, n);
  return String(candidate.data(), candidate.length());
 }

  PatternNameGenerator::PatternNameGenerator(const String& pattern, int zeroPad /*= 0*/):impl_(new Impl()), width_(zeroPad)
  {
  impl_->pattern_ = ZeroString(toStdString(pattern));

 }

 PatternNameGenerator::~PatternNameGenerator()
 {
  delete impl_;
 }

 String PatternNameGenerator::Generate(const String& baseName)
 {
  const std::string baseStd = toStdString(baseName);
  int n = impl_->counters[baseStd] + 1;
  ZeroString candidate;
  std::string candidateStd;
  do
  {
   candidate = makeCandidateZero(baseName, n);
   candidateStd = std::string(candidate.data(), candidate.length());
   n++;
  } while (impl_->usedNames.contains(candidateStd));
  impl_->counters[baseStd] = n - 1;
  impl_->usedNames.insert(candidateStd, true);
  return String(candidateStd);
 }

 void PatternNameGenerator::Release(const String& name)
 {
  impl_->usedNames.erase(toStdString(name));
 }

};
