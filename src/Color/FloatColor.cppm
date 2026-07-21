module;
#include <utility>
#include <algorithm>
#include <cmath>
module Color.Float;

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
FloatColor& FloatColor::operator+=(const FloatColor& other) {
  r_ += other.r_; g_ += other.g_; b_ += other.b_; a_ += other.a_; return *this;
}
FloatColor& FloatColor::operator-=(const FloatColor& other) {
  r_ -= other.r_; g_ -= other.g_; b_ -= other.b_; a_ -= other.a_; return *this;
}
FloatColor& FloatColor::operator*=(float scalar) {
  r_ *= scalar; g_ *= scalar; b_ *= scalar; a_ *= scalar; return *this;
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

static bool approximatelyEqual(float a, float b, float epsilon = 1e-5f) {
  return std::fabs(a - b) < epsilon;
}

} // namespace ArtifactCore