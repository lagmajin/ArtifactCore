module;
#include <utility>

module Core.Point2D;

namespace ArtifactCore
{


 class Point2DF::Impl
 {
 private:
 public:
  float x_ = 0.0f;
  float y_ = 0.0f;
 };

 class Point2DI::Impl
 {
 public:
  int32_t x_ = 0;
  int32_t y_ = 0;
 };

 Point2DF::Point2DF() : impl_(new Impl()) {}

 Point2DF::~Point2DF()
 {
  delete impl_;
  impl_ = nullptr;
 }

 float Point2DF::getX() const
 {
  return impl_ ? impl_->x_ : 0.0f;
 }

 float Point2DF::getY() const
 {
  return impl_ ? impl_->y_ : 0.0f;
 }

 Point2DI::Point2DI() : impl_(new Impl()) {}

 Point2DI::~Point2DI()
 {
  delete impl_;
  impl_ = nullptr;
 }

 int32_t Point2DI::getX() const
 {
  return impl_ ? impl_->x_ : 0;
 }

 int32_t Point2DI::getY() const
 {
  return impl_ ? impl_->y_ : 0;
 }

};
