module;

module Math.RotationTurns;

import std;

namespace ArtifactCore
{

 class RotationTurns::Impl
 {
 private:

 public:
  Impl();
  ~Impl();
  int rotations = 0;
  double degrees = 0.0;

  void normalize();
 };

 RotationTurns::Impl::Impl()
 {

 }

 RotationTurns::Impl::~Impl()
 {

 }

 void RotationTurns::Impl::normalize()
 {
  double whole = std::floor(degrees / 360.0);
  rotations += static_cast<int>(whole);
  degrees -= whole * 360.0;
  if (degrees < 0) {
   degrees += 360.0;
   rotations -= 1;
  }
 }

 RotationTurns::RotationTurns() : impl_(new Impl())
 {

 }

 RotationTurns::~RotationTurns()
 {
  delete impl_;
 }

}