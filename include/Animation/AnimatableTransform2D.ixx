module ;
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
export module Animation.Transform2D;

import Animation.Value;

export namespace ArtifactCore {

 class AnimatableTransform2D {
 private:
  class Impl;
  Impl* impl_;

 public:
  AnimatableTransform2D();
  ~AnimatableTransform2D();

  AnimatableTransform2D(const AnimatableTransform2D& other);
  AnimatableTransform2D& operator=(const AnimatableTransform2D& other);
  AnimatableTransform2D(AnimatableTransform2D&& other) noexcept;
  AnimatableTransform2D& operator=(AnimatableTransform2D&& other) noexcept;

  enum class Element {
   Position,
   Rotation,
   Scale,
   Count
  };

  void setPosition(float x, float y);
  void setRotation(float degrees);
  void setScale(float sx, float sy);

  // Getters
  void position(float& x, float& y) const;
  float rotation() const;
  void scale(float& sx, float& sy) const;

  size_t size() const;
 };

};