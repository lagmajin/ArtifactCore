module;

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
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
  std::unordered_map<std::string, int> counters;
  std::unordered_set<std::string> usedNames;
  ZeroString pattern_;
  int width_ = 0;
 };
	
 ZeroString PatternNameGenerator::makeCandidateZero(const std::string& base, int n) const
 {
  ZeroString s = impl_->pattern_;
  size_t pos = s.find("(\\name)");
  if (pos != static_cast<size_t>(-1))
   s.replace(pos, 7, base.c_str());
  pos = s.find("***");
 if (pos != static_cast<size_t>(-1))
  {
   const ZeroString replacement = formatPaddedNumber(n, width_);
   s.replace(pos, 3, replacement.data());
  }
  return s;
 }

 std::string PatternNameGenerator::makeCandidate(const std::string& base, int n) const
 {
  const ZeroString candidate = makeCandidateZero(base, n);
  return std::string(candidate.data(), candidate.length());
 }

  PatternNameGenerator::PatternNameGenerator(const std::string& pattern, int zeroPad /*= 0*/):impl_(new Impl()), width_(zeroPad)
  {
  impl_->pattern_ = ZeroString(pattern);

 }

 PatternNameGenerator::~PatternNameGenerator()
 {
  delete impl_;
 }

 std::string PatternNameGenerator::Generate(const std::string& baseName)
 {
  int n = impl_->counters[baseName] + 1;
  ZeroString candidate;
  std::string candidateStd;
  do
  {
   candidate = makeCandidateZero(baseName, n);
   candidateStd = std::string(candidate.data(), candidate.length());
   n++;
  } while (impl_->usedNames.count(candidateStd) > 0);
  impl_->counters[baseName] = n - 1;
  impl_->usedNames.insert(candidateStd);
  return candidateStd;
 }

 void PatternNameGenerator::Release(const std::string& name)
 {
  impl_->usedNames.erase(name);
 }

};
