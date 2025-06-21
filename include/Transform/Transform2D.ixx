

module;
#include <stdint.h>
#include <QtGui/QtGui>

#include <opencv2/opencv.hpp>

export module Transform2D;


//import Scale2D;

export namespace ArtifactCore {


 using namespace cv;

 class Transform2DPrivate;

 class Transform2D {
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
 public:
  Transform2D();
  ~Transform2D();
  
  float x() const;
  float y() const;
  float scaleX() const;
  float scaleY() const;
  void setX(double x);
  void setY(double y);
  void setTransform2D();
  void setScaleX(double x);
  void setScaleY(double y);
  

  QTransform toQTransform() const;

  float rotation() const;
  Point2f anchor() const;
 };

 







};