#include <stdint.h>
#include<QtGui/QTransform>

import Transform2D;

import Scale2D;



namespace ArtifactCore {


 class Transform2DPrivate {
 private:
  
  Scale2D scale2d_;

 public:
  Transform2DPrivate();
  ~Transform2DPrivate();
  //AnchorPoint2D anchorPoint2D() const;
  Scale2D scale() const;
  void setScale(const Scale2D& scale);

  void setFromRandom();
  QTransform toQTransform() const;
 };

 Transform2DPrivate::Transform2DPrivate()
 {

 }


 Transform2DPrivate::~Transform2DPrivate()
 {

 }



 Scale2D Transform2DPrivate::scale() const
 {

  return scale2d_;
 }

 void Transform2DPrivate::setScale(const Scale2D& scale)
 {

 }

 void Transform2DPrivate::setFromRandom()
 {

 }

 QTransform Transform2DPrivate::toQTransform() const
 {

  return QTransform();
 }

 Transform2D::Transform2D()
 {

 }

 Transform2D::~Transform2D()
 {

 }
 void Transform2D::setX(double x)
 {

 }

 void Transform2D::setY(double y)
 {

 }

 QTransform Transform2D::toQTransform() const
 {
  QTransform transform;

  

  return transform;

 }

};