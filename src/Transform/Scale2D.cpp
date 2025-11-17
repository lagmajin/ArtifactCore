module;



#include <mutex>
#include <shared_mutex>


#include <QtGui/QTransform>

module Transform.Scale2D;

import std;

namespace ArtifactCore {
 
 class Scale2DPrivate {
 private:
  double x_;
  double y_;
  std::shared_mutex mutex_;
 public:
  Scale2DPrivate();
  ~Scale2DPrivate();
  double x() const;
  double y() const;
  void setScale(double x, double y);
  QTransform toQTransform() const;
 };

 Scale2DPrivate::Scale2DPrivate()
 {

 }

 Scale2DPrivate::~Scale2DPrivate()
 {

 }

 void Scale2DPrivate::setScale(double x, double y)
 {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  x_ = x;
  y_ = y;

 }

 QTransform Scale2DPrivate::toQTransform() const
 {
  return QTransform().scale(x_, y_);
 }

 Scale2D::Scale2D()
 {

 }

 Scale2D::Scale2D(double x, double y)
 {

 }

 Scale2D::~Scale2D()
 {

 }

 void Scale2D::setScale(double x, double y)
 {

 }



};