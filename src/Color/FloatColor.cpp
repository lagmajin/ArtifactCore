module;

#include <qnamespace.h>
module Color.Float;
import FloatRGBA;

import std;



namespace ArtifactCore {

class FloatColor::Impl{
private:

public:
 Impl();
 ~Impl();
 float r = 0.f, g = 0.f, b = 0.f, a = 1.f;
};

FloatColor::Impl::Impl()
{

}

FloatColor::Impl::~Impl()
{

}

FloatColor::FloatColor() : impl_(new Impl) {}

FloatColor::FloatColor(FloatColor&& other) noexcept : impl_(other.impl_)
{
 //other.impl_ = nullptr;
}

FloatColor::FloatColor(const FloatColor& other) : impl_(other.impl_)
{
 //other.impl_ = nullptr;
}

FloatColor::~FloatColor() { delete impl_; }

void FloatColor::setRed(float red) { impl_->r = red; }
void FloatColor::setGreen(float green) { impl_->g = green; }
void FloatColor::setBlue(float blue) { impl_->b = blue; }
void FloatColor::setAlpha(float alpha) { impl_->a = alpha; }
void FloatColor::setColor(float red, float green, float blue) {
 impl_->r = red;
 impl_->g = green;
 impl_->b = blue;
}
void FloatColor::setColor(float red, float green, float blue, float alpha) {
 impl_->r = red;
 impl_->g = green;
 impl_->b = blue;
 impl_->a = alpha;
}

float FloatColor::red() const
{
 return impl_->r;
}
float FloatColor::green() const
{
 return impl_->g;
}

float FloatColor::blue() const
{
 return impl_->b;
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

FloatColor& FloatColor::operator=(const FloatColor& other)
{

 return *this;
}

FloatColor& FloatColor::operator=(FloatColor&& other) noexcept
{
 return *this;
}

static bool approximatelyEqual(float a, float b, float epsilon = 1e-5f) {
 return std::fabs(a - b) < epsilon;
}



};

