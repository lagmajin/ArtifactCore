module;
#include <utility>
#include <cmath>
#include <cstdint>

module Time.Rational;

namespace ArtifactCore {

namespace {
std::uint64_t unsignedMagnitude(std::int64_t value) {
 return value < 0
  ? static_cast<std::uint64_t>(-(value + 1)) + 1u
  : static_cast<std::uint64_t>(value);
}

struct ReducedTime {
 bool negative = false;
 std::uint64_t numerator = 0;
 std::uint64_t denominator = 1;
};

ReducedTime reduceTime(std::int64_t value, std::int64_t scale) {
 std::uint64_t numerator = unsignedMagnitude(value);
 std::uint64_t denominator = unsignedMagnitude(scale);
 std::uint64_t a = numerator;
 std::uint64_t b = denominator;
 while (b != 0) {
  const std::uint64_t remainder = a % b;
  a = b;
  b = remainder;
 }
 if (a > 1) {
  numerator /= a;
  denominator /= a;
 }
 return {value < 0, numerator, denominator};
}

int comparePositiveFractions(std::uint64_t lhsNumerator,
                             std::uint64_t lhsDenominator,
                             std::uint64_t rhsNumerator,
                             std::uint64_t rhsDenominator) {
 bool reverse = false;
 for (;;) {
  const std::uint64_t lhsQuotient = lhsNumerator / lhsDenominator;
  const std::uint64_t rhsQuotient = rhsNumerator / rhsDenominator;
  if (lhsQuotient != rhsQuotient) {
   const bool less = reverse ? lhsQuotient > rhsQuotient
                             : lhsQuotient < rhsQuotient;
   return less ? -1 : 1;
  }
  const std::uint64_t lhsRemainder = lhsNumerator % lhsDenominator;
  const std::uint64_t rhsRemainder = rhsNumerator % rhsDenominator;
  if (lhsRemainder == 0 || rhsRemainder == 0) {
   if (lhsRemainder == rhsRemainder) return 0;
   const bool less = reverse ? lhsRemainder != 0 : lhsRemainder == 0;
   return less ? -1 : 1;
  }
  lhsNumerator = lhsDenominator;
  lhsDenominator = lhsRemainder;
  rhsNumerator = rhsDenominator;
  rhsDenominator = rhsRemainder;
  reverse = !reverse;
 }
}
} // namespace

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

int64_t RationalTime::toFrameCount(int64_t fps) const
{
 if (fps <= 0) {
  return 0;
 }
 return rescaledTo(fps);
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
 const ReducedTime lhs = reduceTime(impl_->value_, impl_->scale_);
 const ReducedTime rhs = reduceTime(other.impl_->value_, other.impl_->scale_);
 if (lhs.negative != rhs.negative) return lhs.negative;
 const int comparison = comparePositiveFractions(
  lhs.numerator, lhs.denominator, rhs.numerator, rhs.denominator);
 return lhs.negative ? comparison > 0 : comparison < 0;
}

bool RationalTime::operator>(const RationalTime& other) const { return other < *this; }
bool RationalTime::operator<=(const RationalTime& other) const { return *this < other || *this == other; }
bool RationalTime::operator>=(const RationalTime& other) const { return *this > other || *this == other; }

bool RationalTime::operator==(const RationalTime& other) const
{
 if (impl_->scale_ == other.impl_->scale_) {
  return impl_->value_ == other.impl_->value_;
 }
 const ReducedTime lhs = reduceTime(impl_->value_, impl_->scale_);
 const ReducedTime rhs = reduceTime(other.impl_->value_, other.impl_->scale_);
 return lhs.negative == rhs.negative && lhs.numerator == rhs.numerator &&
        lhs.denominator == rhs.denominator;
}

bool RationalTime::operator!=(const RationalTime& other) const { return !(*this == other); }

RationalTime RationalTime::fromSeconds(double seconds)
{
 constexpr int64_t defaultScale = 10000000;
 const int64_t value = static_cast<int64_t>(std::round(seconds * defaultScale));
 return RationalTime(value, defaultScale);
}

RationalTime RationalTime::fromFrameCount(int64_t frames, int64_t fps)
{
 if (fps <= 0) {
  fps = 1;
 }
 return RationalTime(frames, fps);
}

}
