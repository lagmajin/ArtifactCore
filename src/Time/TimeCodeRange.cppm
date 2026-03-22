// ReSharper disable CppMemberFunctionMayBeStatic
module;

module TimeCodeRange;

import Time.Rational;

namespace ArtifactCore
{
 class TimeCodeRange::Impl {
 private:

 public:
  Impl();
  Impl(int startFrame, int stopFrame, double fps);
  ~Impl();
  int startFrame_ = 0;
  int stopFrame_ = 0;
  double fps_ = 30.0;
 };

 TimeCodeRange::Impl::Impl()
 {
 }

 TimeCodeRange::Impl::Impl(int startFrame, int stopFrame, double fps)
  : startFrame_(startFrame), stopFrame_(stopFrame), fps_(fps > 0.0 ? fps : 30.0)
 {
 }

 TimeCodeRange::Impl::~Impl()
 {
 }

 TimeCodeRange::TimeCodeRange() : impl_(new Impl())
 {
 }

 TimeCodeRange::TimeCodeRange(const TimeCode& start, const TimeCode& stop)
  : impl_(new Impl(start.frame(), stop.frame(), start.fps()))
 {
 }

 TimeCodeRange::TimeCodeRange(const TimeCodeRange& other)
  : impl_(new Impl(other.impl_->startFrame_, other.impl_->stopFrame_, other.impl_->fps_))
 {
 }

 TimeCodeRange::TimeCodeRange(TimeCodeRange&& other) noexcept
  : impl_(other.impl_)
 {
  other.impl_ = nullptr;
 }

 TimeCodeRange::~TimeCodeRange()
 {
  if (impl_) {
   delete impl_;
   impl_ = nullptr;
  }
 }

 TimeCodeRange& TimeCodeRange::operator=(const TimeCodeRange& other)
 {
  if (this != &other) {
   impl_->startFrame_ = other.impl_->startFrame_;
   impl_->stopFrame_ = other.impl_->stopFrame_;
   impl_->fps_ = other.impl_->fps_;
  }
  return *this;
 }

 TimeCodeRange& TimeCodeRange::operator=(TimeCodeRange&& other) noexcept
 {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 void TimeCodeRange::setStartTimeCode(const TimeCode& timecode)
 {
  impl_->startFrame_ = timecode.frame();
  impl_->fps_ = timecode.fps();
 }

 void TimeCodeRange::setStopTimeCode(const TimeCode& timecode)
 {
  impl_->stopFrame_ = timecode.frame();
 }

 void TimeCodeRange::setByFrameRange(int startFrame, int stopFrame, double fps)
 {
  impl_->startFrame_ = startFrame;
  impl_->stopFrame_ = stopFrame;
  impl_->fps_ = fps > 0.0 ? fps : 30.0;
 }

 TimeCode TimeCodeRange::startTimeCode() const
 {
  return TimeCode(impl_->startFrame_, impl_->fps_);
 }

 TimeCode TimeCodeRange::stopTimeCode() const
 {
  return TimeCode(impl_->stopFrame_, impl_->fps_);
 }

 double TimeCodeRange::fps() const
 {
  return impl_->fps_;
 }

 int TimeCodeRange::durationFrames() const
 {
  return impl_->stopFrame_ - impl_->startFrame_;
 }

 double TimeCodeRange::durationSeconds() const
 {
  if (impl_->fps_ <= 0.0) return 0.0;
  return static_cast<double>(durationFrames()) / impl_->fps_;
 }

 RationalTime TimeCodeRange::durationRational() const
 {
  int64_t scale = static_cast<int64_t>(impl_->fps_);
  if (scale <= 0) scale = 30;
  return RationalTime(static_cast<int64_t>(durationFrames()), scale);
 }

 bool TimeCodeRange::containsFrame(int frame) const
 {
  return frame >= impl_->startFrame_ && frame < impl_->stopFrame_;
 }

 bool TimeCodeRange::contains(const TimeCode& tc) const
 {
  return containsFrame(tc.frame());
 }

 bool TimeCodeRange::overlaps(const TimeCodeRange& other) const
 {
  return !(impl_->stopFrame_ <= other.impl_->startFrame_ ||
           other.impl_->stopFrame_ <= impl_->startFrame_);
 }

 bool TimeCodeRange::isValid() const
 {
  return impl_->startFrame_ <= impl_->stopFrame_ && impl_->fps_ > 0.0;
 }

 void TimeCodeRange::offset(int frames)
 {
  impl_->startFrame_ += frames;
  impl_->stopFrame_ += frames;
 }

 void TimeCodeRange::trimStart(int frames)
 {
  impl_->startFrame_ += frames;
  if (impl_->startFrame_ > impl_->stopFrame_) {
   impl_->startFrame_ = impl_->stopFrame_;
  }
 }

 void TimeCodeRange::trimEnd(int frames)
 {
  impl_->stopFrame_ += frames;
  if (impl_->stopFrame_ < impl_->startFrame_) {
   impl_->stopFrame_ = impl_->startFrame_;
  }
 }

 TimeCodeRange TimeCodeRange::intersection(const TimeCodeRange& other) const
 {
  int newStart = (impl_->startFrame_ > other.impl_->startFrame_)
                  ? impl_->startFrame_ : other.impl_->startFrame_;
  int newStop = (impl_->stopFrame_ < other.impl_->stopFrame_)
                 ? impl_->stopFrame_ : other.impl_->stopFrame_;

  TimeCodeRange result;
  if (newStart < newStop) {
   result.setByFrameRange(newStart, newStop, impl_->fps_);
  } else {
   result.setByFrameRange(0, 0, impl_->fps_);
  }
  return result;
 }

 TimeCodeRange TimeCodeRange::united(const TimeCodeRange& other) const
 {
  int newStart = (impl_->startFrame_ < other.impl_->startFrame_)
                  ? impl_->startFrame_ : other.impl_->startFrame_;
  int newStop = (impl_->stopFrame_ > other.impl_->stopFrame_)
                 ? impl_->stopFrame_ : other.impl_->stopFrame_;

  TimeCodeRange result;
  result.setByFrameRange(newStart, newStop, impl_->fps_);
  return result;
 }

 bool TimeCodeRange::operator==(const TimeCodeRange& other) const
 {
  return impl_->startFrame_ == other.impl_->startFrame_ &&
         impl_->stopFrame_ == other.impl_->stopFrame_ &&
         impl_->fps_ == other.impl_->fps_;
 }

 bool TimeCodeRange::operator!=(const TimeCodeRange& other) const
 {
  return !(*this == other);
 }

};
