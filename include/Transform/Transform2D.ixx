

module;

#include <QtGui/QtGui>

export module Transform2D;

import Scale2D;

export namespace ArtifactCore {

 class Transform2DPrivate;

 class Transform2D {
 private:

 public:
  Transform2D();
  ~Transform2D();
  
  void setX(double x);
  void setY(double y);
  void setTransform2D();
  void setScaleX(double x);
  void setScaleY(double y);
  QTransform toQTransform() const;


 };

 





};