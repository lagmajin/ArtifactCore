module;
#include <QRegularExpression>
#include <QString>

module Color.XYZ;

import std;
import Color.Float;
import Utils.String.UniString;

namespace ArtifactCore
{

 class XYZColor::Impl
 {
 private:
  float X_;
  float Y_;
  float Z_;
 public:
  Impl();
  Impl(float X, float Y, float Z);
  ~Impl();
  float X() const;
  float Y() const;
  float Z() const;
  void setX(float X);
  void setY(float Y);
  void setZ(float Z);
 };

 XYZColor::Impl::Impl() : X_(0.0f), Y_(0.0f), Z_(0.0f)
 {
 }

 XYZColor::Impl::Impl(float X, float Y, float Z) : X_(X), Y_(Y), Z_(Z)
 {
 }

 XYZColor::Impl::~Impl()
 {
 }

 float XYZColor::Impl::X() const
 {
  return X_;
 }

 float XYZColor::Impl::Y() const
 {
  return Y_;
 }

 float XYZColor::Impl::Z() const
 {
  return Z_;
 }

 void XYZColor::Impl::setX(float X)
 {
  X_ = X;
 }

 void XYZColor::Impl::setY(float Y)
 {
  Y_ = Y;
 }

 void XYZColor::Impl::setZ(float Z)
 {
  Z_ = Z;
 }

 XYZColor::XYZColor() : impl_(new Impl())
 {
 }

 XYZColor::XYZColor(float X, float Y, float Z) : impl_(new Impl(X, Y, Z))
 {
 }

 XYZColor::XYZColor(const XYZColor& other) : impl_(new Impl(*other.impl_))
 {
 }

 XYZColor::~XYZColor()
 {
  delete impl_;
 }

 float XYZColor::X() const
 {
  return impl_->X();
 }

 float XYZColor::Y() const
 {
  return impl_->Y();
 }

 float XYZColor::Z() const
 {
  return impl_->Z();
 }

 void XYZColor::setX(float X)
 {
  impl_->setX(X);
 }

 void XYZColor::setY(float Y)
 {
  impl_->setY(Y);
 }

 void XYZColor::setZ(float Z)
 {
  impl_->setZ(Z);
 }

 void XYZColor::clamp()
 {
  impl_->setX(std::clamp(impl_->X(), 0.0f, 1.0f));
  impl_->setY(std::clamp(impl_->Y(), 0.0f, 1.0f));
  impl_->setZ(std::clamp(impl_->Z(), 0.0f, 1.0f));
 }

 float XYZColor::luminance() const
 {
  return impl_->Y();
 }

 float XYZColor::deltaE(const XYZColor& other) const
 {
  float dX = impl_->X() - other.impl_->X();
  float dY = impl_->Y() - other.impl_->Y();
  float dZ = impl_->Z() - other.impl_->Z();
  return std::sqrt(dX * dX + dY * dY + dZ * dZ);
 }

 XYZColor& XYZColor::operator=(const XYZColor& other)
 {
  if (this != &other)
  {
   *impl_ = *other.impl_;
  }
  return *this;
 }

 bool XYZColor::operator==(const XYZColor& other) const
 {
  const float epsilon = 1e-6f;
  return std::abs(impl_->X() - other.impl_->X()) < epsilon &&
         std::abs(impl_->Y() - other.impl_->Y()) < epsilon &&
         std::abs(impl_->Z() - other.impl_->Z()) < epsilon;
 }

 bool XYZColor::operator!=(const XYZColor& other) const
 {
  return !(*this == other);
 }

 FloatColor XYZColor::toFloatColor() const
 {
  float X = impl_->X();
  float Y = impl_->Y();
  float Z = impl_->Z();

  float r = X *  3.2406f + Y * -1.5372f + Z * -0.4986f;
  float g = X * -0.9689f + Y *  1.8758f + Z *  0.0415f;
  float b = X *  0.0557f + Y * -0.2040f + Z *  1.0570f;

  auto srgbGamma = [](float value) -> float {
    if (value <= 0.0031308f) {
      return value * 12.92f;
    } else {
      return 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    }
  };

  r = srgbGamma(r);
  g = srgbGamma(g);
  b = srgbGamma(b);

  r = std::clamp(r, 0.0f, 1.0f);
  g = std::clamp(g, 0.0f, 1.0f);
  b = std::clamp(b, 0.0f, 1.0f);

  return FloatColor(r, g, b, 1.0f);
 }

 XYZColor XYZColor::fromFloatColor(const FloatColor& color)
 {
  float r = color.r();
  float g = color.g();
  float b = color.b();

  auto invGamma = [](float value) {
    if (value <= 0.04045f) {
      return value / 12.92f;
    } else {
      return std::pow((value + 0.055f) / 1.055f, 2.4f);
    }
  };

  r = invGamma(r);
  g = invGamma(g);
  b = invGamma(b);

  float X = r * 0.4124f + g * 0.3576f + b * 0.1805f;
  float Y = r * 0.2126f + g * 0.7152f + b * 0.0722f;
  float Z = r * 0.0193f + g * 0.1192f + b * 0.9505f;

  return XYZColor(X, Y, Z);
 }

 UniString XYZColor::toString() const
 {
  QString qstr = QString("X:%1,Y:%2,Z:%3")
    .arg(impl_->X())
    .arg(impl_->Y())
    .arg(impl_->Z());
  return UniString(qstr);
 }

 XYZColor XYZColor::fromString(const UniString& str)
 {
  QString qstr = str.toQString();
  float X = 0.0f;
  float Y = 0.0f;
  float Z = 0.0f;

  QRegularExpression regex(R"(X:([-+]?[0-9]*\.?[0-9]+),Y:([-+]?[0-9]*\.?[0-9]+),Z:([-+]?[0-9]*\.?[0-9]+))");
  QRegularExpressionMatch match = regex.match(qstr);

  if (match.hasMatch()) {
    X = match.captured(1).toFloat();
    Y = match.captured(2).toFloat();
    Z = match.captured(3).toFloat();
  }

  return XYZColor(X, Y, Z);
 }

};
