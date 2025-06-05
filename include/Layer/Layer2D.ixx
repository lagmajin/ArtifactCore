module;

//#include "../third_party/Eigen/Core"
//#include "../third_party/Eigen/Dense"

#include <stdint.h>

export module Layer2D;

import ImageF32x4;
import ImageF32x4_RGBA;

export namespace ArtifactCore {

 

 class Layer2DSettingPrivate;

 class Layer2DSetting {
  
 public:

  //QPoint position;
  float opacity = 1.0f;
 };


 class Layer2D {
 private:

 public:
  Layer2D();
  ~Layer2D();
  ImageF32x4_RGBA transformedLayer();
 };




};