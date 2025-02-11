import Rotate;

namespace ArtifactCore {

 class RotatePrivate {
 private:

 public:
  RotatePrivate();
  ~RotatePrivate();
 };

 RotatePrivate::RotatePrivate()
 {

 }

 RotatePrivate::~RotatePrivate()
 {

 }

 Rotate::Rotate() :pImpl_(new RotatePrivate)
 {

 }

 Rotate::Rotate(const Rotate& other) :pImpl_(new RotatePrivate)
 {

 }

 Rotate::Rotate(Rotate&& other) noexcept :pImpl_(new RotatePrivate)
 {

 }

 Rotate::~Rotate()
 {

 }

 void Rotate::setRotate(float rotate)
 {

 }

 void Rotate::setFromRandom()
 {

 }

 void Rotate::swap(Rotate& other) noexcept
 {

 }

};