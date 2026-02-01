module;


module  Frame.Offset;

import Frame.Rate;
import Time.Rational;
import Time.Code;

import <cstdint>;

namespace ArtifactCore
{

 class FrameOffset::Impl
 {
 private:
  int64_t offset_ = 0;
 public:
  Impl(int64_t offset = 0) : offset_(offset) {}
  ~Impl() {}

  int64_t value() const { return offset_; }
  int64_t offsetValue() const { return offset_; }
  void setValue(int64_t offset) { offset_ = offset; }
 };

 FrameOffset::FrameOffset() : impl_(new Impl())
 {

 }

 FrameOffset::FrameOffset(int64_t offset) : impl_(new Impl(offset))
 {
 }

 FrameOffset::~FrameOffset()
 {
  if (impl_) 
  {
   delete impl_;
   impl_ = nullptr;
  }
 }

 FrameOffset& FrameOffset::operator=(const FrameOffset& other)
 {
  if (this != &other) {
   impl_->setValue(other.value());
  }
  return *this;
 }

 FrameOffset& FrameOffset::operator=(FrameOffset&& other) noexcept
 {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 int FrameOffset::value() const
 {
  return impl_->value();
 }

 void FrameOffset::setValue(int offset)
 {
  impl_->setValue(offset);
 }

 FrameOffset FrameOffset::operator+(const FrameOffset& other) const
 {
  return FrameOffset(value() + other.value());
 }

 FrameOffset FrameOffset::operator-(const FrameOffset& other) const
 {
  return FrameOffset(value() - other.value());
 }

 FrameOffset FrameOffset::operator+(int frames) const
 {
  return FrameOffset(value() + frames);
 }

 FrameOffset FrameOffset::operator-(int frames) const
 {
  return FrameOffset(value() - frames);
 }

 FrameOffset FrameOffset::operator*(int multiplier) const
 {
  return FrameOffset(value() * multiplier);
 }

 FrameOffset FrameOffset::operator/(int divisor) const
 {
  if (divisor == 0) return FrameOffset(0);
  return FrameOffset(value() / divisor);
 }

 FrameOffset& FrameOffset::operator+=(const FrameOffset& other)
 {
  setValue(value() + other.value());
  return *this;
 }

 FrameOffset& FrameOffset::operator-=(const FrameOffset& other)
 {
  setValue(value() - other.value());
  return *this;
 }

 FrameOffset& FrameOffset::operator+=(int frames)
 {
  setValue(value() + frames);
  return *this;
 }

 FrameOffset& FrameOffset::operator-=(int frames)
 {
  setValue(value() - frames);
  return *this;
 }

 FrameOffset& FrameOffset::operator*=(int multiplier)
 {
  setValue(value() * multiplier);
  return *this;
 }

 FrameOffset& FrameOffset::operator/=(int divisor)
 {
  if (divisor != 0) {
   setValue(value() / divisor);
  }
  return *this;
 }

 FrameOffset FrameOffset::operator-() const
 {
  return FrameOffset(-value());
 }

 bool FrameOffset::operator==(const FrameOffset& other) const
 {
  return value() == other.value();
 }

 bool FrameOffset::operator!=(const FrameOffset& other) const
 {
  return !(*this == other);
 }

 bool FrameOffset::operator<(const FrameOffset& other) const
 {
  return value() < other.value();
 }

 bool FrameOffset::operator<=(const FrameOffset& other) const
 {
  return value() <= other.value();
 }

 bool FrameOffset::operator>(const FrameOffset& other) const
 {
  return value() > other.value();
 }

 bool FrameOffset::operator>=(const FrameOffset& other) const
 {
  return value() >= other.value();
 }

 bool FrameOffset::isZero() const
 {
  return value() == 0;
 }

 bool FrameOffset::isPositive() const
 {
  return value() > 0;
 }

 bool FrameOffset::isNegative() const
 {
  return value() < 0;
 }

 FrameOffset FrameOffset::abs() const
 {
  return FrameOffset(std::abs(value()));
 }

 FrameOffset FrameOffset::negate() const
 {
  return FrameOffset(-value());
 }

 std::string FrameOffset::toString() const
 {
  return std::to_string(value());
 }

 FrameOffset FrameOffset::fromString(const std::string& str)
 {
  try {
    int val = std::stoi(str);
    return FrameOffset(val);
  } catch (...) {
    return FrameOffset(0);
  }
 }

 double FrameOffset::toTimeSeconds(const FrameRate& rate) const
 {
  return static_cast<double>(value()) / rate.framerate();
 }

 FrameOffset FrameOffset::fromTimeSeconds(double seconds, const FrameRate& rate)
 {
  int frames = static_cast<int>(seconds * rate.framerate());
  return FrameOffset(frames);
 }

 RationalTime FrameOffset::toRationalTime(const FrameRate& rate) const
 {
  int64_t scale = static_cast<int64_t>(rate.framerate());
  if (scale <= 0) scale = 30;
  return RationalTime(static_cast<int64_t>(value()), scale);
 }

 FrameOffset FrameOffset::fromRationalTime(const RationalTime& rt, const FrameRate& rate)
 {
  int64_t fps = static_cast<int64_t>(rate.framerate());
  if (fps <= 0) fps = 30;
  int64_t frames = rt.rescaledTo(fps);
  return FrameOffset(static_cast<int>(frames));
 }

 TimeCode FrameOffset::applyToTimeCode(const TimeCode& tc) const
 {
  int newFrame = tc.frame() + value();
  if (newFrame < 0) newFrame = 0;
  return TimeCode(newFrame, tc.fps());
 }

 FrameOffset FrameOffset::between(const TimeCode& from, const TimeCode& to)
 {
  return FrameOffset(to.frame() - from.frame());
 }

};
