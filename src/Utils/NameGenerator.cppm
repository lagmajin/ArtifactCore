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
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
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





namespace ArtifactCore
{
 class PatternNameGenerator::Impl
 {
 public:
  std::unordered_map<std::string, int> counters;
  std::unordered_set<std::string> usedNames;
  std::string pattern_;
  int width_ = 0;
 };
	
 std::string PatternNameGenerator::makeCandidate(const std::string& base, int n) const
 {
  std::ostringstream oss;
  std::string s = pattern_;
  size_t pos = s.find("(\\name)");
  if (pos != std::string::npos)
   s.replace(pos, 7, base);
  pos = s.find("***");
  if (pos != std::string::npos)
  {
   oss.str("");
   if (width_ > 0)
	oss << std::setw(width_) << std::setfill('0') << n;
   else
	oss << n;
   s.replace(pos, 3, oss.str());
  }
  return s;
 }

 PatternNameGenerator::PatternNameGenerator(const std::string& pattern, int zeroPad /*= 0*/):impl_(new Impl()),  pattern_(pattern), width_(zeroPad)
 {

 }

 PatternNameGenerator::~PatternNameGenerator()
 {
  delete impl_;
 }

 std::string PatternNameGenerator::Generate(const std::string& baseName)
 {
  int n = impl_->counters[baseName] + 1;
  std::string candidate;
  do
  {
   candidate = makeCandidate(baseName, n);
   n++;
  } while (impl_->usedNames.count(candidate) > 0);
  impl_->counters[baseName] = n - 1;
  impl_->usedNames.insert(candidate);
  return candidate;
 }

 void PatternNameGenerator::Release(const std::string& name)
 {
  impl_->usedNames.erase(name);
 }

};
