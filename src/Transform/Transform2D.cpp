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
  : impl_(new Impl())
 {

 }

 Transform2D::Transform2D(const Transform2D& other) : impl_(new Impl())
 {

 }

 Transform2D::~Transform2D()
 {
  delete impl_;
 }

 float Transform2D::scaleX() const {
  return impl_->scaleX;
 }

 float Transform2D::scaleY() const {
  return impl_->scaleY;
 }

 void Transform2D::setX(double x) { impl_->x = x; }
 void Transform2D::setY(double y) { impl_->y = y; }


 QTransform Transform2D::toQTransform() const {
  return impl_->toQTransform();
 }

 void Transform2D::setScaleX(double x)
 {

 }

 void Transform2D::setScaleY(double y)
 {

 }

 float Transform2D::x() const
 {
  return impl_->x;
 }

 float Transform2D::y() const
 {
  return impl_->y;
 }

 float Transform2D::rotation() const
 {
  return 0;
 }

};