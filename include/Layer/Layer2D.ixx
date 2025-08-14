module;

//#include "../third_party/Eigen/Core"
//#include "../third_party/Eigen/Dense"

#include <stdint.h>

export module Layer2D;

import std;

import ImageF32x4;
import Image;

import Transform;

export namespace ArtifactCore {

 

 class Layer2DSettingPrivate;

 class Layer2DSetting {
  
 public:

  //QPoint position;
  float opacity = 1.0f;
 };


 class Layer2D {
 private:
  struct Impl;                
  std::unique_ptr<Impl> impl_;
 public:
  Layer2D();
  ~Layer2D();
  Transform2D transform2D() const;
  ImageF32x4_RGBA transformedLayer();
 };




};