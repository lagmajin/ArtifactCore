module;
#include <stdint.h>


#include <cstdint>

#include <QtCore/QScopedPointer>

export module Rotate;

export namespace ArtifactCore {


 class RotatePrivate;

 class Rotate {
 private:
 //QScopedPointer<RotatePrivate> const  pImpl_;
 public:
  Rotate();
  Rotate(const Rotate& other);
  Rotate(Rotate&& other) noexcept;
  ~Rotate();
  float rotate() const;
  void setRotate(float rotate);
  void setFromRandom();

  void swap(Rotate& other) noexcept;

  Rotate& operator=(const Rotate& other);
  Rotate& operator=(Rotate&& other);
 };


};