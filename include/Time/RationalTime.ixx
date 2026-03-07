module;
#include <QTime>
#include "../Define/DllExportMacro.hpp"
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
export module Time.Rational;





export namespace ArtifactCore {

 class LIBRARY_DLL_API RationalTime {
 private:
  class Impl;
  Impl* impl_;
 public:
  RationalTime();
  RationalTime(int64_t value,int64_t scale);
  RationalTime(const RationalTime& other);
  RationalTime& operator=(const RationalTime& other);
  ~RationalTime();
  int64_t value() const;
  int64_t scale() const;
  double toSeconds() const;
  // w肵XP[ifpsȂǁjɕϊۂvalueԂ
  int64_t rescaledTo(int64_t newScale) const;

  // --- ZqI[o[[h (Ԃ̌vZɕK{) ---
  RationalTime operator+(const RationalTime& other) const;
  RationalTime operator-(const RationalTime& other) const;
  bool operator<(const RationalTime& other) const;
  bool operator>(const RationalTime& other) const;
  bool operator<=(const RationalTime& other) const;
  bool operator>=(const RationalTime& other) const;
  bool operator==(const RationalTime& other) const;
  bool operator!=(const RationalTime& other) const;
  // ldoubleibPʁjɕϊ
  double toDouble() const;
  // --- [eBeB ---
  // b琶 (œK؂scaleݒA: 100000)
  static RationalTime fromSeconds(double seconds);
 };


};