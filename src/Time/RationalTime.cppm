module;
#include <utility>

module Time.Rational;

import std;

namespace ArtifactCore {

static int64_t gcd(int64_t a, int64_t b) {
 a = std::abs(a);
 b = std::abs(b);
 while (b != 0) {
  const int64_t t = b;
  b = a % b;
  a = t;
 }
 return a;
}

static int64_t lcm(int64_t a, int64_t b) {
 if (a == 0 || b == 0) return 0;
 return std::abs(a) / gcd(a, b) * std::abs(b);
}

class RationalTime::Impl {
private:
public:
 Impl();
 Impl(int64_t value, int64_t scale);
 ~Impl();
 int64_t value_ = 0;
 int64_t scale_ = 1;
};

RationalTime::Impl::Impl()
{
}

RationalTime::Impl::Impl(int64_t value, int64_t scale) : value_(value), scale_(scale > 0 ? scale : 1)
{
}

RationalTime::Impl::~Impl()
{
}

RationalTime::RationalTime() : impl_(new Impl())
{
}

RationalTime::RationalTime(int64_t value, int64_t scale) : impl_(new Impl(value, scale))
{
}

RationalTime::RationalTime(const RationalTime& other) : impl_(new Impl(other.impl_->value_, other.impl_->scale_))
{
}

RationalTime::~RationalTime()
{
 delete impl_;
}

int64_t RationalTime::value() const { return impl_->value_; }
int64_t RationalTime::scale() const { return impl_->scale_; }

RationalTime& RationalTime::operator=(const RationalTime& other)
{
 if (this != &other) {
  impl_->value_ = other.impl_->value_;
  impl_->scale_ = other.impl_->scale_;
 }
 return *this;
}

double RationalTime::toSeconds() const
{
 if (impl_->scale_ == 0) return 0.0;
 return static_cast<double>(impl_->value_) / static_cast<double>(impl_->scale_);
}

double RationalTime::toDouble() const
{
 return toSeconds();
}

int64_t RationalTime::rescaledTo(int64_t newScale) const
{
 if (impl_->scale_ == 0 || newScale == 0) return 0;
 return (impl_->value_ * newScale) / impl_->scale_;
}

RationalTime RationalTime::operator+(const RationalTime& other) const
{
 if (impl_->scale_ == other.impl_->scale_) {
  return RationalTime(impl_->value_ + other.impl_->value_, impl_->scale_);
 }
 const int64_t commonScale = lcm(impl_->scale_, other.impl_->scale_);
 const int64_t thisValue = impl_->value_ * (commonScale / impl_->scale_);
 const int64_t otherValue = other.impl_->value_ * (commonScale / other.impl_->scale_);
 return RationalTime(thisValue + otherValue, commonScale);
}

RationalTime RationalTime::operator-(const RationalTime& other) const
{
 if (impl_->scale_ == other.impl_->scale_) {
  return RationalTime(impl_->value_ - other.impl_->value_, impl_->scale_);
 }
 const int64_t commonScale = lcm(impl_->scale_, other.impl_->scale_);
 const int64_t thisValue = impl_->value_ * (commonScale / impl_->scale_);
 const int64_t otherValue = other.impl_->value_ * (commonScale / other.impl_->scale_);
 return RationalTime(thisValue - otherValue, commonScale);
}

bool RationalTime::operator<(const RationalTime& other) const
{
 if (impl_->scale_ == other.impl_->scale_) {
  return impl_->value_ < other.impl_->value_;
 }
 return impl_->value_ * other.impl_->scale_ < other.impl_->value_ * impl_->scale_;
}

bool RationalTime::operator>(const RationalTime& other) const { return other < *this; }
bool RationalTime::operator<=(const RationalTime& other) const { return *this < other || *this == other; }
bool RationalTime::operator>=(const RationalTime& other) const { return *this > other || *this == other; }

bool RationalTime::operator==(const RationalTime& other) const
{
 if (impl_->scale_ == other.impl_->scale_) {
  return impl_->value_ == other.impl_->value_;
 }
 return impl_->value_ * other.impl_->scale_ == other.impl_->value_ * impl_->scale_;
}

bool RationalTime::operator!=(const RationalTime& other) const { return !(*this == other); }

RationalTime RationalTime::fromSeconds(double seconds)
{
 constexpr int64_t defaultScale = 10000000;
 const int64_t value = static_cast<int64_t>(std::round(seconds * defaultScale));
 return RationalTime(value, defaultScale);
}

}
