module;
#include <stdint.h>
#include <QtGui/QtGui>

#include <opencv2/opencv.hpp>
#include "../Define/DllExportMacro.hpp"

export module Transform._2D;



namespace cv {};//dummy

export namespace ArtifactCore {


 using namespace cv;



  class LIBRARY_DLL_API Transform2D {
 private:
  struct Impl;
  Impl* impl_;
  //std::unique_ptr<Impl> impl_2;
 public:
  Transform2D();
  Transform2D(const Transform2D& other);
  ~Transform2D();
  
  float x() const;
  float y() const;
  float scaleX() const;
  float scaleY() const;
  void setX(float x);

  void setY(float y);
  float anchorPointX() const;
  float anchorPointY() const;

  void setScaleX(float x);
  void setScaleY(float y);

  void setAnchorPointX(float x);
  void setAnchorPointY(float y);

  void setInitialScaleX(float x);
  void setInitialScaleY(float y);
  void setInitialScale(float x,float y);
  void setTransform2D();
  QTransform toQTransform() const;

  float rotation() const;
  //Point2f anchor() const;
 };

 







};