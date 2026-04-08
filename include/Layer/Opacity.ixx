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
export module Core.Opacity;




import Utils.String.UniString;

export namespace ArtifactCore {

 class Opacity
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  Opacity();
  Opacity(float value);
  Opacity(Opacity&& other) noexcept;
  ~Opacity();

  Opacity(const Opacity& other);
  Opacity& operator=(const Opacity& other);
  Opacity& operator=(Opacity&& other) noexcept;

  // Getter and setter
  float value() const;
  void setValue(float value);

  // Arithmetic operators
  Opacity operator+(const Opacity& other) const;
  Opacity operator-(const Opacity& other) const;
  Opacity operator*(float factor) const;
  Opacity operator/(float factor) const;
  Opacity& operator+=(const Opacity& other);
  Opacity& operator-=(const Opacity& other);
  Opacity& operator*=(float factor);
  Opacity& operator/=(float factor);

  // Comparison operators with epsilon
  bool operator==(const Opacity& other) const;
  bool operator!=(const Opacity& other) const;
  bool operator<(const Opacity& other) const;
  bool operator<=(const Opacity& other) const;
  bool operator>(const Opacity& other) const;
  bool operator>=(const Opacity& other) const;

  // Utility
  bool isValid() const;
  void clamp();
  UniString toString() const;
  static Opacity fromString(const UniString& str);
 };

};