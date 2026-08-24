module;
#include <utility>

#include <cstdint>
#include <cmath>
#include <limits>
module Frame.Position;

import Frame.Rate;
import Time.Rational;

namespace ArtifactCore {

class FramePosition::Impl {
private:
public:
 explicit Impl(int64_t position = 0);
 ~Impl();
 int64_t frame = 0;
};

FramePosition::Impl::Impl(int64_t position) : frame(position)
{
}

FramePosition::Impl::~Impl()
{
}

FramePosition::FramePosition() : FramePosition(0)
{
}

FramePosition::FramePosition(int framePosition) : impl_(new Impl(framePosition))
{
}

FramePosition::FramePosition(const FramePosition& other) : impl_(new Impl(other.impl_->frame))
{
}

FramePosition::FramePosition(FramePosition&& other) noexcept : impl_(other.impl_)
{
 other.impl_ = nullptr;
}

FramePosition::~FramePosition()
{
 delete impl_;
}

int64_t FramePosition::framePosition() const
{
 return impl_->frame;
}

FramePosition& FramePosition::operator=(const FramePosition& other)
{
 if (this != &other) {
  impl_->frame = other.impl_->frame;
 }
 return *this;
}

FramePosition& FramePosition::operator=(FramePosition&& other) noexcept
{
 if (this != &other) {
  delete impl_;
  impl_ = other.impl_;
  other.impl_ = nullptr;
 }
 return *this;
}

FramePosition& FramePosition::operator=(int64_t frame) noexcept
{
 impl_->frame = frame;
 return *this;
}

bool FramePosition::isValid() const
{
 return impl_ && impl_->frame >= 0;
}

bool FramePosition::isNegative() const
{
 return impl_ && impl_->frame < 0;
}

FramePosition FramePosition::operator+(int64_t frames) const
{
 return FramePosition(impl_->frame + frames);
}

FramePosition FramePosition::operator-(int64_t frames) const
{
 return FramePosition(impl_->frame - frames);
}

FramePosition& FramePosition::operator+=(int64_t frames)
{
 impl_->frame += frames;
 return *this;
}

FramePosition& FramePosition::operator-=(int64_t frames)
{
 impl_->frame -= frames;
 return *this;
}

int64_t FramePosition::operator-(const FramePosition& other) const
{
 return impl_->frame - other.impl_->frame;
}

bool FramePosition::operator==(const FramePosition& other) const
{
 return impl_->frame == other.impl_->frame;
}

bool FramePosition::operator!=(const FramePosition& other) const
{
 return !(*this == other);
}

bool FramePosition::operator<(const FramePosition& other) const
{
 return impl_->frame < other.impl_->frame;
}

bool FramePosition::operator<=(const FramePosition& other) const
{
 return !(other < *this);
}

bool FramePosition::operator>(const FramePosition& other) const
{
 return other < *this;
}

bool FramePosition::operator>=(const FramePosition& other) const
{
 return !(*this < other);
}

double FramePosition::toSeconds(double fps) const
{
 if (!impl_ || fps <= 0.0) {
  return 0.0;
 }
 return static_cast<double>(impl_->frame) / fps;
}

FramePosition FramePosition::fromSeconds(double seconds, double fps)
{
 if (fps <= 0.0) {
  return FramePosition(0);
 }
 return FramePosition(static_cast<int64_t>(std::llround(seconds * fps)));
}

RationalTime FramePosition::toRationalTime(const FrameRate& rate) const
{
 const std::int64_t frame = impl_ ? impl_->frame : 0;
 if (rate.hasExactRational()) {
  // frame / (num/den) == frame * den / num.
  return RationalTime(frame * rate.denominator(), rate.numerator());
 }
 const std::int64_t nominal =
     std::max<std::int64_t>(1, static_cast<std::int64_t>(std::llround(rate.framerate())));
 return RationalTime(frame, nominal);
}

FramePosition FramePosition::fromRationalTime(const RationalTime& rationalTime,
                                              const FrameRate& rate)
{
 if (rate.framerate() <= 0.0f) {
  return FramePosition(0);
 }
 if (rate.hasExactRational()) {
  // value/scale frames at num/den fps: seconds = value/scale, frame = seconds*num/den.
  const double exact = static_cast<double>(rationalTime.value()) *
                       static_cast<double>(rate.numerator()) /
                       static_cast<double>(rationalTime.scale() * rate.denominator());
  return FramePosition(static_cast<int64_t>(std::llround(exact)));
 }
 const std::int64_t nominal =
     std::max<std::int64_t>(1, static_cast<std::int64_t>(std::llround(rate.framerate())));
 return FramePosition(rationalTime.rescaledTo(nominal));
}

FramePosition FramePosition::min()
{
 return FramePosition(std::numeric_limits<int64_t>::min());
}

FramePosition FramePosition::max()
{
 return FramePosition(std::numeric_limits<int64_t>::max());
}

}
