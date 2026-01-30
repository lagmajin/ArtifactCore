module;

module Time.Rational;

import std;

namespace ArtifactCore {

 // 最大公約数を求める（通分・約分用）
 static int64_t gcd(int64_t a, int64_t b) {
  a = std::abs(a);
  b = std::abs(b);
  while (b != 0) {
   int64_t t = b;
   b = a % b;
   a = t;
  }
  return a;
 }

 // 最小公倍数を求める
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
  int64_t scale_ = 1; // デフォルトは1（0除算防止）
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

 RationalTime::RationalTime():impl_(new Impl())
 {

 }

 RationalTime::RationalTime(int64_t value, int64_t scale) :impl_(new Impl(value,scale))
 {

 }

 RationalTime::RationalTime(const RationalTime& other) : impl_(new Impl(other.impl_->value_, other.impl_->scale_))
 {
 }

 RationalTime::~RationalTime()
 {
  delete impl_;
  
 }

 int64_t RationalTime::value() const
 {
  return impl_->value_;
 }

 int64_t RationalTime::scale() const
 {
  return impl_->scale_;
 }
	
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

 int64_t RationalTime::rescaledTo(int64_t newScale) const
 {
  if (impl_->scale_ == 0 || newScale == 0) return 0;
  // value / scale = newValue / newScale  =>  newValue = value * newScale / scale
  return (impl_->value_ * newScale) / impl_->scale_;
 }

 RationalTime RationalTime::operator+(const RationalTime& other) const
 {
  if (impl_->scale_ == other.impl_->scale_) {
   // 同じスケールなら単純に加算
   return RationalTime(impl_->value_ + other.impl_->value_, impl_->scale_);
  }
  // 異なるスケールは通分して計算
  int64_t commonScale = lcm(impl_->scale_, other.impl_->scale_);
  int64_t thisValue = impl_->value_ * (commonScale / impl_->scale_);
  int64_t otherValue = other.impl_->value_ * (commonScale / other.impl_->scale_);
  return RationalTime(thisValue + otherValue, commonScale);
 }

 RationalTime RationalTime::operator-(const RationalTime& other) const
 {
  if (impl_->scale_ == other.impl_->scale_) {
   return RationalTime(impl_->value_ - other.impl_->value_, impl_->scale_);
  }
  int64_t commonScale = lcm(impl_->scale_, other.impl_->scale_);
  int64_t thisValue = impl_->value_ * (commonScale / impl_->scale_);
  int64_t otherValue = other.impl_->value_ * (commonScale / other.impl_->scale_);
  return RationalTime(thisValue - otherValue, commonScale);
 }

 bool RationalTime::operator<(const RationalTime& other) const
 {
  if (impl_->scale_ == other.impl_->scale_) {
   return impl_->value_ < other.impl_->value_;
  }
  // クロス乗算で比較: a/b < c/d  <=>  a*d < c*b
  return impl_->value_ * other.impl_->scale_ < other.impl_->value_ * impl_->scale_;
 }

 bool RationalTime::operator>(const RationalTime& other) const
 {
  return other < *this;
 }

 bool RationalTime::operator==(const RationalTime& other) const
 {
  if (impl_->scale_ == other.impl_->scale_) {
   return impl_->value_ == other.impl_->value_;
  }
  // クロス乗算で比較
  return impl_->value_ * other.impl_->scale_ == other.impl_->value_ * impl_->scale_;
 }

 bool RationalTime::operator!=(const RationalTime& other) const
 {
  return !(*this == other);
 }

 RationalTime RationalTime::fromSeconds(double seconds)
 {
  // 高精度スケール（1/10,000,000秒 = 100ナノ秒単位、映像編集に十分）
  constexpr int64_t defaultScale = 10000000;
  int64_t value = static_cast<int64_t>(std::round(seconds * defaultScale));
  return RationalTime(value, defaultScale);
 }

};