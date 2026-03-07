module;
#include <numeric>
#include <memory>
#include <QString>

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
module Core.AspectRatio;





namespace ArtifactCore {

 class AspectRatio::Impl {
 public:
  Impl() = default;
  Impl(int w, int h) : w_(w), h_(h) {}
  int w_ = 1;
  int h_ = 1;
 };

 AspectRatio::AspectRatio() : impl_(std::make_unique<Impl>()) {}

 AspectRatio::AspectRatio(int width, int height) : impl_(std::make_unique<Impl>(width, height)) {
  simplify();
 }

 AspectRatio::~AspectRatio() = default;

 AspectRatio::AspectRatio(const AspectRatio& other) 
  : impl_(std::make_unique<Impl>(*other.impl_)) {}

 AspectRatio& AspectRatio::operator=(const AspectRatio& other) {
  if (this != &other) {
   impl_ = std::make_unique<Impl>(*other.impl_);
  }
  return *this;
 }

 AspectRatio::AspectRatio(AspectRatio&& other) noexcept = default;
 AspectRatio& AspectRatio::operator=(AspectRatio&& other) noexcept = default;

 void AspectRatio::simplify() {
  if (!impl_ || impl_->w_ == 0 || impl_->h_ == 0) return;
  int common = std::gcd(impl_->w_, impl_->h_);
  if (common > 0) {
   impl_->w_ /= common;
   impl_->h_ /= common;
  }
 }

 UniString AspectRatio::toString() const {
  if (!impl_) return "1:1";
  return QString("%1:%2").arg(impl_->w_).arg(impl_->h_);
 }

 double AspectRatio::ratio() const {
  if (!impl_ || impl_->h_ == 0) return 1.0;
  return static_cast<double>(impl_->w_) / impl_->h_;
 }

 void AspectRatio::setFromString(const UniString& str) {
  // TODO: Implementation
 }

}