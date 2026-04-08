module;
#include "../Define/DllExportMacro.hpp"
#include <memory>
#include <utility>

export module Color.Float;

import FloatRGBA;

export namespace ArtifactCore {

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

  FloatColor &operator=(const FloatColor &other);
  FloatColor &operator=(FloatColor &&other) noexcept;

  FloatColor operator+(const FloatColor &other) const;
  FloatColor operator-(const FloatColor &other) const;
  FloatColor operator*(float scalar) const;
  FloatColor &operator+=(const FloatColor &other);
  FloatColor &operator-=(const FloatColor &other);
  FloatColor &operator*=(float scalar);
};

}; // namespace ArtifactCore
