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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <cstdint>
export module Frame.Offset;

import Frame.Rate;
import Time.Rational;
import Time.Code;


export namespace ArtifactCore {

 class FrameOffset
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  FrameOffset();
  FrameOffset(int64_t offset);
  FrameOffset(const FrameOffset& other);
  FrameOffset(FrameOffset&& other) noexcept;
  ~FrameOffset();

  FrameOffset& operator=(const FrameOffset& other);
  FrameOffset& operator=(FrameOffset&& other) noexcept;

  // Getter and setter
  int value() const;
  void setValue(int offset);

  // Arithmetic operators
  FrameOffset operator+(const FrameOffset& other) const;
  FrameOffset operator-(const FrameOffset& other) const;
  FrameOffset operator+(int frames) const;
  FrameOffset operator-(int frames) const;
  FrameOffset operator*(int multiplier) const;
  FrameOffset operator/(int divisor) const;
  FrameOffset& operator+=(const FrameOffset& other);
  FrameOffset& operator-=(const FrameOffset& other);
  FrameOffset& operator+=(int frames);
  FrameOffset& operator-=(int frames);
  FrameOffset& operator*=(int multiplier);
  FrameOffset& operator/=(int divisor);

  // Unary operators
  FrameOffset operator-() const;

  // Comparison operators
  bool operator==(const FrameOffset& other) const;
  bool operator!=(const FrameOffset& other) const;
  bool operator<(const FrameOffset& other) const;
  bool operator<=(const FrameOffset& other) const;
  bool operator>(const FrameOffset& other) const;
  bool operator>=(const FrameOffset& other) const;

  // Utility
  bool isZero() const;
  bool isPositive() const;
  bool isNegative() const;
  FrameOffset abs() const;
  FrameOffset negate() const;
  std::string toString() const;
  static FrameOffset fromString(const std::string& str);

  // Relation with other frame classes
  double toTimeSeconds(const FrameRate& rate) const;
  static FrameOffset fromTimeSeconds(double seconds, const FrameRate& rate);

  // RationalTime Ag
  RationalTime toRationalTime(const FrameRate& rate) const;
  static FrameOffset fromRationalTime(const RationalTime& rt, const FrameRate& rate);

  // TimeCode Ag
  TimeCode applyToTimeCode(const TimeCode& tc) const;
  static FrameOffset between(const TimeCode& from, const TimeCode& to);
 };



};
