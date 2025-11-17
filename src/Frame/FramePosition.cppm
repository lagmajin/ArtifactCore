module;


module Frame.Position;

import std;
import <cstdint>;

namespace ArtifactCore {

 class FramePosition::Impl {
 private:

 public:
  explicit Impl(int64_t position=0);
  
  ~Impl();
  int64_t frame = 0.0;
 };

 FramePosition::Impl::Impl(int64_t position/*=0*/)
 {

 }

 FramePosition::Impl::~Impl()
 {

 }

 FramePosition::FramePosition(int framePosition/*=0*/):impl_(new Impl(framePosition))
 {

 }

 FramePosition::FramePosition(const FramePosition& other) :impl_(new Impl(other.impl_->frame))
 {

 }

 FramePosition::FramePosition(FramePosition&& other) noexcept:impl_(other.impl_)
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

};