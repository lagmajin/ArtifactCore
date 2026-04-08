module;
#include <utility>

export module Transform._3D;




export namespace ArtifactCore {

 class StaticTransform3D {
 private:
  class Impl;
  Impl* impl_;
 public:
  StaticTransform3D();
  StaticTransform3D(const StaticTransform3D& other);
  StaticTransform3D(StaticTransform3D&& other) noexcept;
  ~StaticTransform3D();

  StaticTransform3D& operator=(const StaticTransform3D& other);
  StaticTransform3D& operator=(StaticTransform3D&& other) noexcept;
 };






};
