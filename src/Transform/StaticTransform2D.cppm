module ;
#include <stdint.h>
#include<QtGui/QTransform>

module Transform._2D;


import std;
//import Transform:Scale2D;



namespace ArtifactCore {


 struct StaticTransform2D::Impl {
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


 StaticTransform2D::StaticTransform2D()
  : impl_(new Impl())
 {

 }

 StaticTransform2D::StaticTransform2D(const StaticTransform2D& other) : impl_(new Impl())
 {

 }

 StaticTransform2D::~StaticTransform2D()
 {
  delete impl_;
 }

 float StaticTransform2D::scaleX() const {
  return impl_->scaleX;
 }

 float StaticTransform2D::scaleY() const {
  return impl_->scaleY;
 }

 void StaticTransform2D::setX(float x)
 {
	 impl_->x = x;
 }


 void StaticTransform2D::setY(float y)
 {
  impl_->y = y;
 }

 void StaticTransform2D::setScaleX(float x)
 {

 }

 void StaticTransform2D::setScaleY(float y)
 {

 }

 float StaticTransform2D::x() const
 {
  return impl_->x;
 }

 float StaticTransform2D::y() const
 {
  return impl_->y;
 }

 float StaticTransform2D::rotation() const
 {
  return 0;
 }

 void StaticTransform2D::setInitialScaleX(float x)
 {

 }

 void StaticTransform2D::setInitialScaleY(float y)
 {

 }

 void StaticTransform2D::setInitialScale(float x, float y)
 {

 }

 void StaticTransform2D::setAnchorPointX(float x)
 {

 }

 void StaticTransform2D::setAnchorPointY(float y)
 {

 }

 float StaticTransform2D::anchorPointX() const
 {
  return 0.0f;
 }

 float StaticTransform2D::anchorPointY() const
 {
  return 0.0f;
 }

};