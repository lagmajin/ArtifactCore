module;
#include <stdint.h>
#include<QtGui/QTransform>

module Transform2D;

//import Transform:Scale2D;



namespace ArtifactCore {


 struct Transform2D::Impl {
  double x = 0;
  double y = 0;
  double scaleX = 1.0;
  double scaleY = 1.0;
  double rotation = 0; // radians or degrees?
  QPointF anchor = QPointF(0.5, 0.5); // normalized anchor?

  QTransform toQTransform() const {
   QTransform t;
   t.translate(x, y);
   t.translate(anchor.x(), anchor.y()); // anchor
   t.rotate(rotation); // QTransform uses degrees
   t.scale(scaleX, scaleY);
   t.translate(-anchor.x(), -anchor.y()); // anchor back
   return t;
  }
 };




 Transform2D::Transform2D()
  : impl_(std::make_unique<Impl>()) {
 }
 Transform2D::~Transform2D() = default;

 float Transform2D::scaleX() const {
  return static_cast<float>(impl_->scaleX);
 }

 float Transform2D::scaleY() const {
  return static_cast<float>(impl_->scaleY);
 }

 void Transform2D::setX(double x) { impl_->x = x; }
 void Transform2D::setY(double y) { impl_->y = y; }


 QTransform Transform2D::toQTransform() const {
  return impl_->toQTransform();
 }
};