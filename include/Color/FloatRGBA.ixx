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
export module FloatRGBA;





export namespace ArtifactCore {

 class FloatColor;

 //class FloatRGBAPrivate;

 class FloatRGBA {
 private:
  float r_, g_, b_, a_;

 public:
  // RXgN^
  constexpr FloatRGBA() : r_(0), g_(0), b_(0), a_(0) {}
  constexpr FloatRGBA(float r, float g, float b, float a = 1.0f)
   : r_(r), g_(g), b_(b), a_(a) {
  }

  // Rs[E[uRXgN^
  FloatRGBA(const FloatRGBA&) = default;
  FloatRGBA(FloatRGBA&&) = default;

  // ANZbT
  float r() const { return r_; }
  float g() const { return g_; }
  float b() const { return b_; }
  float a() const { return a_; }

  void setRed(float r) { r_ = r; }
  void setGreen(float g) { g_ = g; }
  void setBlue(float b) { b_ = b; }
  void setAlpha(float a) { a_ = a; }  //  fix: Apha  Alpha

  void setRGBA(float r, float g, float b = 0.0f, float a = 0.0f) {
   r_ = r; g_ = g; b_ = b; a_ = a;
  }

  // YZqiv͈̓`FbNj
  float& operator[](int index) {
   if (index < 0 || index > 3) throw std::out_of_range("FloatRGBA index");
   return (&r_)[index];
  }

  const float& operator[](int index) const {
   if (index < 0 || index > 3) throw std::out_of_range("FloatRGBA index");
   return (&r_)[index];
  }

  // ZpZq
  FloatRGBA operator+(const FloatRGBA& rhs) const {
   return FloatRGBA(r_ + rhs.r_, g_ + rhs.g_, b_ + rhs.b_, a_ + rhs.a_);
  }

  FloatRGBA operator*(float scalar) const {
   return FloatRGBA(r_ * scalar, g_ * scalar, b_ * scalar, a_ * scalar);
  }

  // Zq
  FloatRGBA& operator=(const FloatRGBA&) = default;
  FloatRGBA& operator=(FloatRGBA&&) = default;

  // ϊZq
  operator FloatColor() const; // ͕ʓr

  // [eBeB
  void setFromFloatColor(const FloatColor& color);
  void setFromRandom(); // _lŏ

  // Comparison
  bool operator==(const FloatRGBA& other) const {
   return r_ == other.r_ && g_ == other.g_ && b_ == other.b_ && a_ == other.a_;
  }
  bool operator!=(const FloatRGBA& other) const {
   return !(*this == other);
  }

  // Xbv
  void swap(FloatRGBA& other) noexcept {
   std::swap(r_, other.r_);
   std::swap(g_, other.g_);
   std::swap(b_, other.b_);
   std::swap(a_, other.a_);
  }
 };



};