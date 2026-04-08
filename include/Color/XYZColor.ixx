module;
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
export module Color.XYZ;




import Color.Float;
import Utils.String.UniString;

export namespace ArtifactCore
{

 class LIBRARY_DLL_API XYZColor
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  XYZColor();
  XYZColor(float X, float Y, float Z);
  XYZColor(const XYZColor& other);
  ~XYZColor();

  float X() const;
  float Y() const;
  float Z() const;
  void setX(float X);
  void setY(float Y);
  void setZ(float Z);
  void clamp();

  float luminance() const;
  float deltaE(const XYZColor& other) const;

  XYZColor& operator=(const XYZColor& other);

  bool operator==(const XYZColor& other) const;
  bool operator!=(const XYZColor& other) const;

  FloatColor toFloatColor() const;
  static XYZColor fromFloatColor(const FloatColor& color);

  UniString toString() const;
  static XYZColor fromString(const UniString& str);
 };


};
