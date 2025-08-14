module;
#include <stdint.h>
#include<QtGui/QTransform>

module Transform._2D;

//import Transform:Scale2D;



namespace ArtifactCore {


 struct Transform2D::Impl {
  float x = 0;
  float y = 0;
  float initialX = 1.0f;
  float initialY = 1.0f;
  float scaleX = 1.0;
  float scaleY = 1.0;

  float rotation = 0; // radians or degrees?

  float anchorPointX = 0.0f;
  float anchorPointY = 0.0f;

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

 void Transform2D::setX(float x)
 {
	 impl_->x = x;
 }


 void Transform2D::setY(float y)
 {
  impl_->y = y;
 }

 void Transform2D::setScaleX(float x)
 {

 }

 void Transform2D::setScaleY(float y)
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

 void Transform2D::setInitialScaleX(float x)
 {

 }

 void Transform2D::setInitialScaleY(float y)
 {

 }

 void Transform2D::setInitialScale(float x, float y)
 {

 }

 void Transform2D::setAnchorPointX(float x)
 {

 }

 void Transform2D::setAnchorPointY(float y)
 {

 }

 float Transform2D::anchorPointX() const
 {
  return 0.0f;
 }

 float Transform2D::anchorPointY() const
 {
  return 0.0f;
 }

};