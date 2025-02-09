module;

//#include "../third_party/Eigen/Core"
//#include "../third_party/Eigen/Dense"

#include <stdint.h>

export module Layer2D;

export namespace ArtifactCore {

 

 class Layer2DSettingPrivate;

 class Layer2DSetting {
  
 public:

  //QPoint position;
  float opacity = 1.0f;
 };


 class Layer2D {
 public:
  Layer2D();
  ~Layer2D();
 };




};