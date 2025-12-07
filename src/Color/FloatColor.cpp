module;
#include <QColor>
#include <qnamespace.h>
module Color.Float;
import FloatRGBA;

import std;



namespace ArtifactCore {

class FloatColor::Impl{
private:

public:
 Impl();
 Impl(float r, float g, float b, float a);
 ~Impl();
 float sumRGB() const;
 float sumRGBA() const;

 float averageRGB() const;
 float averageRGBA() const;
  void clamp();
  float r_ = 0.f;
  float g_ = 0.f;
  float b_ = 0.f;
  float a_ = 1.f;
};

FloatColor::Impl::Impl()
{

}

FloatColor::Impl::Impl(float r, float g, float b, float a):r_(r),g_(g),b_(b),a_(a)
{

}

FloatColor::Impl::~Impl()
{

}

void FloatColor::Impl::clamp()
{

}

float FloatColor::Impl::averageRGB() const
{
 return (r_ + g_ + b_) / 3;
}

float FloatColor::Impl::averageRGBA() const
{
 return (r_ + g_ + b_+ a_) / 4;
}

float FloatColor::Impl::sumRGB() const
{
 return r_ + g_ + b_;
}



FloatColor::FloatColor() : impl_(new Impl) {}

FloatColor::FloatColor(float r, float g, float b, float a):impl_(new Impl(r,g,b,a))
{

}

FloatColor::FloatColor(FloatColor&& other) noexcept : impl_(other.impl_)
{
 //other.impl_ = nullptr;
}

FloatColor::FloatColor(const FloatColor& other) : impl_(other.impl_)
{
 //other.impl_ = nullptr;
}

FloatColor::~FloatColor() { delete impl_; }

void FloatColor::setRed(float red) { impl_->r_ = red; }
void FloatColor::setGreen(float green) { impl_->g_ = green; }
void FloatColor::setBlue(float blue) { impl_->b_ = blue; }
void FloatColor::setAlpha(float alpha) { impl_->a_ = alpha; }
void FloatColor::setColor(float red, float green, float blue) {
 impl_->r_ = red;
 impl_->g_ = green;
 impl_->b_ = blue;
}
void FloatColor::setColor(float red, float green, float blue, float alpha) {
 impl_->r_ = red;
 impl_->g_ = green;
 impl_->b_ = blue;
 impl_->a_ = alpha;
}

float FloatColor::red() const
{
 return impl_->r_;
}
float FloatColor::green() const
{
 return impl_->g_;
}

float FloatColor::blue() const
{
 return impl_->b_;
}

float FloatColor::alpha() const
{
 return impl_->a_;
}

float FloatColor::r() const
{
 return red();
}

float FloatColor::g() const
{
 return green();
}

float FloatColor::b() const
{
 return blue();
}

float FloatColor::a() const
{
 return alpha();
}

FloatColor& FloatColor::operator=(const FloatColor& other)
{

 return *this;
}

FloatColor& FloatColor::operator=(FloatColor&& other) noexcept
{
 return *this;
}

void FloatColor::clamp()
{

}



static bool approximatelyEqual(float a, float b, float epsilon = 1e-5f) {
 return std::fabs(a - b) < epsilon;
}



};

