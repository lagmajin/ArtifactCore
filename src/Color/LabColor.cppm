module;
#include <QRegularExpression>
#include <QString>

module Color.Lab;

import std;
import Color.Float;
import Utils.String.UniString;

namespace ArtifactCore
{


 class LabColor::Impl
 {
 private:
  float L_;
  float a_;
  float b_;
 public:
  Impl();
  Impl(float L, float a, float b);
  ~Impl();
  float L() const;
  float a() const;
  float b() const;
  void setL(float L);
  void setA(float a);
  void setB(float b);
 };

 LabColor::Impl::Impl() : L_(0.0f), a_(0.0f), b_(0.0f)
 {

 }

 LabColor::Impl::Impl(float L, float a, float b):L_(L), a_(a), b_(b)
 {
 }

 LabColor::Impl::~Impl()
 {

 }

 float LabColor::Impl::L() const
 {
  return L_;
 }

 float LabColor::Impl::a() const
 {
  return a_;
 }

 float LabColor::Impl::b() const
 {
  return b_;
 }

 void LabColor::Impl::setL(float L)
 {
  L_ = L;
 }

 void LabColor::Impl::setA(float a)
 {
  a_ = a;
 }

 void LabColor::Impl::setB(float b)
 {
  b_ = b;
 }

 LabColor::LabColor() :impl_(new Impl())
 {

 }

 LabColor::LabColor(float L, float a, float b) :impl_(new Impl(L,a,b))
 {
 }

 LabColor::LabColor(const LabColor& color) :impl_(new Impl(*color.impl_))
 {
 }

 LabColor::~LabColor()
 {
  delete impl_;
 }

 float LabColor::L() const
 {
  return impl_->L();
 }

 float LabColor::a() const
 {
  return impl_->a();
 }

 float LabColor::b() const
 {
  return impl_->b();
 }

 void LabColor::setL(float L)
 {
  impl_->setL(L);
 }

 void LabColor::setA(float a)
 {
  impl_->setA(a);
 }

 void LabColor::setB(float b)
 {
  impl_->setB(b);
 }

 float LabColor::luminance() const
 {
  return impl_->L();
 }

 float LabColor::deltaE(const LabColor& other) const
 {
  float dL = impl_->L() - other.impl_->L();
  float da = impl_->a() - other.impl_->a();
  float db = impl_->b() - other.impl_->b();
  return std::sqrt(dL * dL + da * da + db * db);
 }

 LabColor& LabColor::operator=(const LabColor& other)
 {
  if (this != &other)
  {
   *impl_ = *other.impl_;
  }
  return *this;
 }

 bool LabColor::operator==(const LabColor& other) const
 {
  //イプシロンつかった比較
  const float epsilon = 1e-6f;

  return std::abs(impl_->L() - other.impl_->L()) < epsilon &&
         std::abs(impl_->a() - other.impl_->a()) < epsilon &&
         std::abs(impl_->b() - other.impl_->b()) < epsilon;
 }

 bool LabColor::operator!=(const LabColor& other) const
 {
  return !(*this == other);
 }

 FloatColor LabColor::toFloatColor() const
 {
  // Lab色空間からXYZ色空間への変換
  float L = impl_->L();
  float a = impl_->a();
  float b = impl_->b();

  // Lab -> XYZ変換
  // 標準的なLab->XYZ変換式
  float fy = (L + 16.0f) / 116.0f;
  float fx = a / 500.0f + fy;
  float fz = fy - b / 200.0f;

  // 逆gamma補正
  auto invGamma = [](float t) -> float {
    const float delta = 6.0f / 29.0f;
    if (t > delta) {
      return t * t * t;
    } else {
      return (t - 4.0f / 29.0f) * 3.0f * delta * delta;
    }
  };

  // D65標準光源の参照白
  const float Xn = 0.95047f;
  const float Yn = 1.00000f;
  const float Zn = 1.08883f;

  float X = Xn * invGamma(fx);
  float Y = Yn * invGamma(fy);
  float Z = Zn * invGamma(fz);

  // XYZ -> RGB変換（sRGB色空間）
  // XYZ to RGB matrix (sRGB)
  float r =  X *  3.2406f + Y * -1.5372f + Z * -0.4986f;
  float g =  X * -0.9689f + Y *  1.8758f + Z *  0.0415f;
  float b_out =  X *  0.0557f + Y * -0.2040f + Z *  1.0570f;

  // sRGB Gamma補正
  auto srgbGamma = [](float value) -> float {
    if (value <= 0.0031308f) {
      return value * 12.92f;
    } else {
      return 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    }
  };

  r = srgbGamma(r);
  g = srgbGamma(g);
  b_out = srgbGamma(b_out);

  // クランプ
  r = std::clamp(r, 0.0f, 1.0f);
  g = std::clamp(g, 0.0f, 1.0f);
  b_out = std::clamp(b_out, 0.0f, 1.0f);

  return FloatColor(r, g, b_out, 1.0f);
 }

 LabColor LabColor::fromFloatColor(const FloatColor& color)
 {
  // RGB -> XYZ変換
  float r = color.r();
  float g = color.g();
  float b = color.b();

  // sRGB逆Gamma補正
  auto invSrgbGamma = [](float value) -> float {
    if (value <= 0.04045f) {
      return value / 12.92f;
    } else {
      return std::pow((value + 0.055f) / 1.055f, 2.4f);
    }
  };

  r = invSrgbGamma(r);
  g = invSrgbGamma(g);
  b = invSrgbGamma(b);

  // RGB -> XYZ (sRGB)
  float X = r * 0.4124f + g * 0.3576f + b * 0.1805f;
  float Y = r * 0.2126f + g * 0.7152f + b * 0.0722f;
  float Z = r * 0.0193f + g * 0.1192f + b * 0.9505f;

  // D65標準光源の参照白
  const float Xn = 0.95047f;
  const float Yn = 1.00000f;
  const float Zn = 1.08883f;

  // 正規化
  X /= Xn;
  Y /= Yn;
  Z /= Zn;

  // XYZ -> Lab変換
  auto labGamma = [](float t) -> float {
    const float delta = 6.0f / 29.0f;
    if (t > delta * delta * delta) {
      return std::cbrt(t);
    } else {
      return t / (3.0f * delta * delta) + 4.0f / 29.0f;
    }
  };

  float fx = labGamma(X);
  float fy = labGamma(Y);
  float fz = labGamma(Z);

  float L = 116.0f * fy - 16.0f;
  float a = 500.0f * (fx - fy);
  float b_lab = 200.0f * (fy - fz);

  return LabColor(L, a, b_lab);
 }

 UniString LabColor::toString() const
 {
  // L,a,b値をCSV形式の文字列に変換
  QString qstr = QString("L:%1,a:%2,b:%3")
    .arg(impl_->L())
    .arg(impl_->a())
    .arg(impl_->b());
  return UniString(qstr);
 }

 LabColor LabColor::fromString(const UniString& str)
 {
  // 文字列からLab値をパース
  // "L:50.5,a:10.2,b:-20.3" 形式を想定
  QString qstr = str.toQString();
  
  float L = 0.0f, a = 0.0f, b = 0.0f;
  
  // 正規表現でパース
  QRegularExpression regex(R"(L:([-+]?\d*\.?\d+),a:([-+]?\d*\.?\d+),b:([-+]?\d*\.?\d+))");
  QRegularExpressionMatch match = regex.match(qstr);
  
  if (match.hasMatch()) {
    L = match.captured(1).toFloat();
    a = match.captured(2).toFloat();
    b = match.captured(3).toFloat();
  }
  
  return LabColor(L, a, b);
 }

};