module;
#include <utility>

module Core.Position2D;

namespace ArtifactCore
{
 class Position2D::Impl
 {
 public:
  int x_ = 0;
  int y_ = 0;
  Impl() = default;
  Impl(const Impl& other) = default;
  Impl& operator=(const Impl& other) = default;
 };

 Position2D::Position2D() : impl_(new Impl()) {}
 Position2D::~Position2D() { delete impl_; }

 Position2D::Position2D(const Position2D& other) : impl_(new Impl(*other.impl_)) {}
 Position2D& Position2D::operator=(const Position2D& other) {
  if (this != &other) {
   *impl_ = *other.impl_;
  }
  return *this;
 }

 Position2D::Position2D(Position2D&& other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
 }
 Position2D& Position2D::operator=(Position2D&& other) noexcept {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 void Position2D::Set(int x, int y) {
  impl_->x_ = x;
  impl_->y_ = y;
 }

 void Position2D::Get(int& x, int& y) const {
  x = impl_->x_;
  y = impl_->y_;
 }

 class Position2DF::Impl
 {
 public:
  float x_ = 0.0f;
  float y_ = 0.0f;
  Impl() = default;
  Impl(const Impl& other) = default;
  Impl& operator=(const Impl& other) = default;
 };

 Position2DF::Position2DF() : impl_(new Impl()) {}
 Position2DF::~Position2DF() { delete impl_; }

 Position2DF::Position2DF(const Position2DF& other) : impl_(new Impl(*other.impl_)) {}
 Position2DF& Position2DF::operator=(const Position2DF& other) {
  if (this != &other) {
   *impl_ = *other.impl_;
  }
  return *this;
 }

 Position2DF::Position2DF(Position2DF&& other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
 }
 Position2DF& Position2DF::operator=(Position2DF&& other) noexcept {
  if (this != &other) {
   delete impl_;
   impl_ = other.impl_;
   other.impl_ = nullptr;
  }
  return *this;
 }

 void Position2DF::Set(float x, float y) {
  impl_->x_ = x;
  impl_->y_ = y;
 }

 void Position2DF::Get(float& x, float& y) const {
  x = impl_->x_;
  y = impl_->y_;
 }

};
