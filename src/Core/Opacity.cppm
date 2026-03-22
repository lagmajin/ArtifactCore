module;
#include <algorithm>
#include <QString>


module Core.Opacity;

import Utils.String.UniString;

namespace ArtifactCore
{
 class Opacity::Impl
 {
 private:

 public:
  float opacity_ = 1.0f; // Default to fully opaque
  Impl() = default;
  Impl(const Impl& other) = default;
  Impl& operator=(const Impl& other) = default;
 };

 // Helper function for floating point comparison
 bool approximatelyEqual(float a, float b, float epsilon = 1e-6f) {
  return std::abs(a - b) <= epsilon;
 }

 Opacity::Opacity():impl_(new Impl())
 {

 }

 Opacity::Opacity(float value) : impl_(new Impl())
 {
  setValue(value);
 }

 Opacity::Opacity(const Opacity& other) :impl_(new Impl(*other.impl_))
 {

 }

 Opacity::Opacity(Opacity&& other) noexcept : impl_(other.impl_)
 {
  other.impl_ = nullptr;
 }

 Opacity::~Opacity()
 {
  delete impl_;
 }

 Opacity& Opacity::operator=(const Opacity& other)
 {
  if (this != &other) {
    *impl_ = *other.impl_;
  }
  return *this;
 }

 Opacity& Opacity::operator=(Opacity&& other) noexcept
 {
  if (this != &other) {
    delete impl_;
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }
  return *this;
 }

 float Opacity::value() const
 {
  return impl_->opacity_;
 }

 void Opacity::setValue(float value)
 {
  impl_->opacity_ = std::clamp(value, 0.0f, 1.0f);
 }

 Opacity Opacity::operator+(const Opacity& other) const
 {
  return Opacity(value() + other.value());
 }

 Opacity Opacity::operator-(const Opacity& other) const
 {
  return Opacity(value() - other.value());
 }

 Opacity Opacity::operator*(float factor) const
 {
  return Opacity(value() * factor);
 }

 Opacity Opacity::operator/(float factor) const
 {
  return Opacity(value() / factor);
 }

 Opacity& Opacity::operator+=(const Opacity& other)
 {
  setValue(value() + other.value());
  return *this;
 }

 Opacity& Opacity::operator-=(const Opacity& other)
 {
  setValue(value() - other.value());
  return *this;
 }

 Opacity& Opacity::operator*=(float factor)
 {
  setValue(value() * factor);
  return *this;
 }

 Opacity& Opacity::operator/=(float factor)
 {
  setValue(value() / factor);
  return *this;
 }

 bool Opacity::operator==(const Opacity& other) const
 {
  return approximatelyEqual(value(), other.value());
 }

 bool Opacity::operator!=(const Opacity& other) const
 {
  return !(*this == other);
 }

 bool Opacity::operator<(const Opacity& other) const
 {
  return value() < other.value();
 }

 bool Opacity::operator<=(const Opacity& other) const
 {
  return *this < other || *this == other;
 }

 bool Opacity::operator>(const Opacity& other) const
 {
  return value() > other.value();
 }

 bool Opacity::operator>=(const Opacity& other) const
 {
  return *this > other || *this == other;
 }

 bool Opacity::isValid() const
 {
  return value() >= 0.0f && value() <= 1.0f;
 }

 void Opacity::clamp()
 {
  setValue(value()); // setValue already clamps
 }

 UniString Opacity::toString() const
 {
  return UniString(std::to_string(value()));
 }

 Opacity Opacity::fromString(const UniString& str)
 {
  try {
    float val = std::stof(static_cast<std::string>(str));
    return Opacity(val);
  } catch (...) {
    return Opacity(1.0f); // Default on error
  }
 }

};