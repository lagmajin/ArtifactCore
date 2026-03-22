module;
#include "../Define/DllExportMacro.hpp"
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>


#include <wobjectimpl.h>
export module Color.Float;

import FloatRGBA;

export namespace ArtifactCore {

// class FloatRGBA;

class HSV;
class XYZ;

enum class NamedColor {
  Red,
  Green,
  Blue,
  White,
  Black,
  Yellow,
  Cyan,
  Magenta,
  Orange,
  Transparent,
  // 必要に応じて追加
};

class LIBRARY_DLL_API FloatColor {
private:
  class Impl;
  Impl *impl_;

public:
  FloatColor();
  FloatColor(float r, float g, float b, float a);
  ~FloatColor();
  FloatColor(const FloatColor &other);
  FloatColor(FloatColor &&color) noexcept;

  float red() const;
  float green() const;
  float blue() const;
  float alpha() const;

  float r() const;
  float g() const;
  float b() const;
  float a() const;

  float sumRGB() const;
  float sumRGBA() const;

  float averageRGB() const;
  float averageRGBA() const;

  void setRed(float red);
  void setGreen(float green);
  void setBlue(float blue);
  void setAlpha(float alpha);
  void setColor(float red, float green, float blue);
  void setColor(float red, float green, float blue, float alpha);
  void clamp();

  FloatColor &operator=(const FloatColor &other); // コピー代入演算子
  FloatColor &operator=(FloatColor &&other) noexcept;

  // 算術演算子
  FloatColor operator+(const FloatColor &other) const;
  FloatColor operator-(const FloatColor &other) const;
  FloatColor operator*(float scalar) const;
  FloatColor &operator+=(const FloatColor &other);
  FloatColor &operator-=(const FloatColor &other);
  FloatColor &operator*=(float scalar);
};

}; // namespace ArtifactCore

W_REGISTER_ARGTYPE(ArtifactCore::FloatColor)