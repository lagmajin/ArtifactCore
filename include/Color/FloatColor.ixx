module;
#include <glm/glm.hpp>
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

#include <wobjectimpl.h>
export module Color.Float;




import FloatRGBA;

export namespace ArtifactCore {

 //class FloatRGBA;

 class HSV;
 class XYZ;

 enum class NamedColor
 {
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
  Impl* impl_;
 public:
  FloatColor();
  FloatColor(float r,float g,float b,float a);
  ~FloatColor();
  FloatColor(const FloatColor& other);
  FloatColor(FloatColor&& color) noexcept;

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

  FloatColor& operator=(const FloatColor& other); // コピー代入演算子
  FloatColor& operator=(FloatColor&& other) noexcept;
 };


};

W_REGISTER_ARGTYPE(ArtifactCore::FloatColor)