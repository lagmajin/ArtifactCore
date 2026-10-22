module;
#include <utility>
#include <algorithm>
#include <cmath>
module Color.Float;

import Color.TransferFunction;

namespace ArtifactCore {

FloatColor::FloatColor() = default;
FloatColor::FloatColor(float r, float g, float b, float a) : r_(r), g_(g), b_(b), a_(a) {}
FloatColor::FloatColor(FloatColor&& other) noexcept : r_(other.r_), g_(other.g_), b_(other.b_), a_(other.a_) {}
FloatColor::FloatColor(const FloatColor& other) = default;
FloatColor::~FloatColor() = default;

void FloatColor::setRed(float red)   { r_ = red; }
void FloatColor::setGreen(float green) { g_ = green; }
void FloatColor::setBlue(float blue)  { b_ = blue; }
void FloatColor::setAlpha(float alpha) { a_ = alpha; }
void FloatColor::setColor(float red, float green, float blue) { r_ = red; g_ = green; b_ = blue; }
void FloatColor::setColor(float red, float green, float blue, float alpha) { r_ = red; g_ = green; b_ = blue; a_ = alpha; }

FloatColor FloatColor::toLinear() const {
  return FloatColor(ColorTransferFunction::srgbToLinear(r_),
                    ColorTransferFunction::srgbToLinear(g_),
                    ColorTransferFunction::srgbToLinear(b_), a_);
}

FloatColor FloatColor::fromLinear() const {
  return FloatColor(ColorTransferFunction::linearToSRGB(r_),
                    ColorTransferFunction::linearToSRGB(g_),
                    ColorTransferFunction::linearToSRGB(b_), a_);
}

float FloatColor::red() const   { return r_; }
float FloatColor::green() const { return g_; }
float FloatColor::blue() const  { return b_; }
float FloatColor::alpha() const { return a_; }
float FloatColor::r() const { return r_; }
float FloatColor::g() const { return g_; }
float FloatColor::b() const { return b_; }
float FloatColor::a() const { return a_; }

FloatColor& FloatColor::operator=(const FloatColor&) = default;
FloatColor& FloatColor::operator=(FloatColor&&) noexcept = default;

FloatColor FloatColor::operator+(const FloatColor& other) const {
  return FloatColor(r_ + other.r_, g_ + other.g_, b_ + other.b_, a_ + other.a_);
}
FloatColor FloatColor::operator-(const FloatColor& other) const {
  return FloatColor(r_ - other.r_, g_ - other.g_, b_ - other.b_, a_ - other.a_);
}
FloatColor FloatColor::operator*(float scalar) const {
  return FloatColor(r_ * scalar, g_ * scalar, b_ * scalar, a_ * scalar);
}
FloatColor FloatColor::operator*(const FloatColor& other) const {
  return FloatColor(r_ * other.r_, g_ * other.g_, b_ * other.b_, a_ * other.a_);
}
FloatColor FloatColor::operator/(float scalar) const {
  return FloatColor(r_ / scalar, g_ / scalar, b_ / scalar, a_ / scalar);
}
FloatColor FloatColor::operator/(const FloatColor& other) const {
  return FloatColor(r_ / other.r_, g_ / other.g_, b_ / other.b_, a_ / other.a_);
}
FloatColor& FloatColor::operator+=(const FloatColor& other) {
  r_ += other.r_; g_ += other.g_; b_ += other.b_; a_ += other.a_; return *this;
}
FloatColor& FloatColor::operator-=(const FloatColor& other) {
  r_ -= other.r_; g_ -= other.g_; b_ -= other.b_; a_ -= other.a_; return *this;
}
FloatColor& FloatColor::operator*=(float scalar) {
  r_ *= scalar; g_ *= scalar; b_ *= scalar; a_ *= scalar; return *this;
}
FloatColor& FloatColor::operator*=(const FloatColor& other) {
  r_ *= other.r_; g_ *= other.g_; b_ *= other.b_; a_ *= other.a_; return *this;
}
FloatColor& FloatColor::operator/=(float scalar) {
  r_ /= scalar; g_ /= scalar; b_ /= scalar; a_ /= scalar; return *this;
}
FloatColor& FloatColor::operator/=(const FloatColor& other) {
  r_ /= other.r_; g_ /= other.g_; b_ /= other.b_; a_ /= other.a_; return *this;
}
FloatColor FloatColor::operator-() const {
  return FloatColor(-r_, -g_, -b_, -a_);
}

bool FloatColor::operator==(const FloatColor& other) const {
  return r_ == other.r_ && g_ == other.g_ && b_ == other.b_ && a_ == other.a_;
}

bool FloatColor::operator!=(const FloatColor& other) const {
  return !(*this == other);
}

FloatColor FloatColor::lerp(const FloatColor& a, const FloatColor& b, float t) {
  const float u = std::clamp(t, 0.0f, 1.0f);
  return FloatColor(
      a.r_ + (b.r_ - a.r_) * u,
      a.g_ + (b.g_ - a.g_) * u,
      a.b_ + (b.b_ - a.b_) * u,
      a.a_ + (b.a_ - a.a_) * u);
}

bool FloatColor::approximatelyEqual(const FloatColor& other, float epsilon) const {
  return std::fabs(r_ - other.r_) < epsilon &&
         std::fabs(g_ - other.g_) < epsilon &&
         std::fabs(b_ - other.b_) < epsilon &&
         std::fabs(a_ - other.a_) < epsilon;
}

void FloatColor::clamp() {
  r_ = std::clamp(r_, 0.0f, 1.0f);
  g_ = std::clamp(g_, 0.0f, 1.0f);
  b_ = std::clamp(b_, 0.0f, 1.0f);
  a_ = std::clamp(a_, 0.0f, 1.0f);
}

float FloatColor::averageRGB() const  { return (r_ + g_ + b_) / 3.f; }
float FloatColor::sumRGB() const     { return r_ + g_ + b_; }
float FloatColor::sumRGBA() const    { return r_ + g_ + b_ + a_; }
float FloatColor::averageRGBA() const { return (r_ + g_ + b_ + a_) / 4.f; }

FloatColor operator*(float scalar, const FloatColor& color) {
  return FloatColor(scalar * color.r(), scalar * color.g(), scalar * color.b(), scalar * color.a());
}

} // namespace ArtifactCore
