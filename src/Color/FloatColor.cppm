module;
#include <QColor>
#include <qnamespace.h>
module Color.Float;

// import FloatRGBA;
import std;

namespace ArtifactCore {

class FloatColor::Impl {
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

FloatColor::Impl::Impl() {}

FloatColor::Impl::Impl(float r, float g, float b, float a)
    : r_(r), g_(g), b_(b), a_(a) {}

FloatColor::Impl::~Impl() {}

void FloatColor::Impl::clamp() {}

float FloatColor::Impl::averageRGB() const { return (r_ + g_ + b_) / 3; }

float FloatColor::Impl::averageRGBA() const { return (r_ + g_ + b_ + a_) / 4; }

float FloatColor::Impl::sumRGB() const { return r_ + g_ + b_; }

float FloatColor::Impl::sumRGBA() const { return r_ + g_ + b_ + a_; }

FloatColor::FloatColor() : impl_(new Impl) {}

FloatColor::FloatColor(float r, float g, float b, float a)
    : impl_(new Impl(r, g, b, a)) {}

FloatColor::FloatColor(FloatColor &&other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
}

FloatColor::FloatColor(const FloatColor &other) : impl_(new Impl()) {
  if (other.impl_) {
    impl_->r_ = other.impl_->r_;
    impl_->g_ = other.impl_->g_;
    impl_->b_ = other.impl_->b_;
    impl_->a_ = other.impl_->a_;
  }
}

FloatColor::~FloatColor() { delete impl_; }

void FloatColor::setRed(float red) {
  if (!impl_)
    impl_ = new Impl();
  impl_->r_ = red;
}
void FloatColor::setGreen(float green) {
  if (!impl_)
    impl_ = new Impl();
  impl_->g_ = green;
}
void FloatColor::setBlue(float blue) {
  if (!impl_)
    impl_ = new Impl();
  impl_->b_ = blue;
}
void FloatColor::setAlpha(float alpha) {
  if (!impl_)
    impl_ = new Impl();
  impl_->a_ = alpha;
}
void FloatColor::setColor(float red, float green, float blue) {
  if (!impl_)
    impl_ = new Impl();
  impl_->r_ = red;
  impl_->g_ = green;
  impl_->b_ = blue;
}
void FloatColor::setColor(float red, float green, float blue, float alpha) {
  if (!impl_)
    impl_ = new Impl();
  impl_->r_ = red;
  impl_->g_ = green;
  impl_->b_ = blue;
  impl_->a_ = alpha;
}

float FloatColor::red() const {
  if (!impl_)
    return 0.0f;
  return impl_->r_;
}
float FloatColor::green() const {
  if (!impl_)
    return 0.0f;
  return impl_->g_;
}

float FloatColor::blue() const {
  if (!impl_)
    return 0.0f;
  return impl_->b_;
}

float FloatColor::alpha() const {
  if (!impl_)
    return 1.0f;
  return impl_->a_;
}

float FloatColor::r() const { return red(); }

float FloatColor::g() const { return green(); }

float FloatColor::b() const { return blue(); }

float FloatColor::a() const { return alpha(); }

FloatColor &FloatColor::operator=(const FloatColor &other) {
  if (this == &other) {
    return *this;
  }
  if (!impl_) {
    impl_ = new Impl();
  }
  if (other.impl_) {
    impl_->r_ = other.impl_->r_;
    impl_->g_ = other.impl_->g_;
    impl_->b_ = other.impl_->b_;
    impl_->a_ = other.impl_->a_;
  } else {
    impl_->r_ = 0.0f;
    impl_->g_ = 0.0f;
    impl_->b_ = 0.0f;
    impl_->a_ = 1.0f;
  }
  return *this;
}

FloatColor &FloatColor::operator=(FloatColor &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  delete impl_;
  impl_ = other.impl_;
  other.impl_ = nullptr;
  return *this;
}

// 算術演算子
FloatColor FloatColor::operator+(const FloatColor &other) const {
  if (!impl_ || !other.impl_) {
    return FloatColor();
  }
  return FloatColor(impl_->r_ + other.impl_->r_, impl_->g_ + other.impl_->g_,
                    impl_->b_ + other.impl_->b_, impl_->a_ + other.impl_->a_);
}

FloatColor FloatColor::operator-(const FloatColor &other) const {
  if (!impl_ || !other.impl_) {
    return FloatColor();
  }
  return FloatColor(impl_->r_ - other.impl_->r_, impl_->g_ - other.impl_->g_,
                    impl_->b_ - other.impl_->b_, impl_->a_ - other.impl_->a_);
}

FloatColor FloatColor::operator*(float scalar) const {
  if (!impl_) {
    return FloatColor();
  }
  return FloatColor(impl_->r_ * scalar, impl_->g_ * scalar, impl_->b_ * scalar,
                    impl_->a_ * scalar);
}

FloatColor &FloatColor::operator+=(const FloatColor &other) {
  if (!impl_) {
    impl_ = new Impl();
  }
  if (other.impl_) {
    impl_->r_ += other.impl_->r_;
    impl_->g_ += other.impl_->g_;
    impl_->b_ += other.impl_->b_;
    impl_->a_ += other.impl_->a_;
  }
  return *this;
}

FloatColor &FloatColor::operator-=(const FloatColor &other) {
  if (!impl_) {
    impl_ = new Impl();
  }
  if (other.impl_) {
    impl_->r_ -= other.impl_->r_;
    impl_->g_ -= other.impl_->g_;
    impl_->b_ -= other.impl_->b_;
    impl_->a_ -= other.impl_->a_;
  }
  return *this;
}

FloatColor &FloatColor::operator*=(float scalar) {
  if (!impl_) {
    impl_ = new Impl();
  }
  impl_->r_ *= scalar;
  impl_->g_ *= scalar;
  impl_->b_ *= scalar;
  impl_->a_ *= scalar;
  return *this;
}

void FloatColor::clamp() {
  if (!impl_) {
    return;
  }
  impl_->r_ = std::clamp(impl_->r_, 0.0f, 1.0f);
  impl_->g_ = std::clamp(impl_->g_, 0.0f, 1.0f);
  impl_->b_ = std::clamp(impl_->b_, 0.0f, 1.0f);
  impl_->a_ = std::clamp(impl_->a_, 0.0f, 1.0f);
}

float FloatColor::averageRGB() const {
  if (!impl_) {
    return 0.0f;
  }
  return impl_->averageRGB();
}

float FloatColor::sumRGB() const {
  if (!impl_) {
    return 0.0f;
  }
  return impl_->sumRGB();
}

float FloatColor::sumRGBA() const {
  if (!impl_) {
    return 0.0f;
  }
  return impl_->sumRGBA();
}

float FloatColor::averageRGBA() const {
  if (!impl_) {
    return 0.0f;
  }
  return impl_->averageRGBA();
}

static bool approximatelyEqual(float a, float b, float epsilon = 1e-5f) {
  return std::fabs(a - b) < epsilon;
}

} // namespace ArtifactCore

// W_REGISTER_ARGTYPE(ArtifactCore::FloatColor)
