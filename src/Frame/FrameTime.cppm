module;
#include <utility>
#include <cmath>
#include <cstdint>

module Frame.Time;

namespace ArtifactCore {

 class FrameTime::Impl {
 public:
  explicit Impl(int value) : frame_(value) {}
  int frame_ = 0;
 };

 FrameTime::FrameTime(int frame) : impl_(new Impl(frame)) {}

 FrameTime::FrameTime(const FrameTime& other)
  : impl_(new Impl(other.frame())) {}

 FrameTime::FrameTime(FrameTime&& other) noexcept
  : impl_(other.impl_) {
  other.impl_ = nullptr;
 }

 FrameTime::~FrameTime() {
  delete impl_;
  impl_ = nullptr;
 }

 FrameTime& FrameTime::operator=(const FrameTime& other) {
  if (this != &other) setFrame(other.frame());
  return *this;
 }

 FrameTime& FrameTime::operator=(FrameTime&& other) noexcept {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 int FrameTime::frame() const {
  return impl_ ? impl_->frame_ : 0;
 }

 void FrameTime::setFrame(int f) {
  if (impl_) impl_->frame_ = f;
 }

 RationalTime FrameTime::toTime(double fps) const {
  if (!(fps > 0.0) || !std::isfinite(fps)) return RationalTime();
  const auto roundedFps = static_cast<int64_t>(std::llround(fps));
  return RationalTime(frame(), roundedFps > 0 ? roundedFps : 1);
 }

 FrameTime FrameTime::operator+(int delta) const { return FrameTime(frame() + delta); }
 FrameTime FrameTime::operator-(int delta) const { return FrameTime(frame() - delta); }
 FrameTime& FrameTime::operator+=(int delta) { setFrame(frame() + delta); return *this; }
 FrameTime& FrameTime::operator-=(int delta) { setFrame(frame() - delta); return *this; }
 bool FrameTime::operator==(const FrameTime& other) const { return frame() == other.frame(); }
 bool FrameTime::operator<(const FrameTime& other) const { return frame() < other.frame(); }

};
