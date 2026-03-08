module;

export module Core.Camera;

import Float3;

export namespace ArtifactCore
{

 struct Camera {
  float3<float> position;
  float3<float> target;
  float3<float> up;

  float fovY;     // vertical field of view in degrees
  float aspect;   // projection aspect ratio
  float nearZ;    // near clipping plane
  float farZ;     // far clipping plane

 public:
  explicit Camera();
  ~Camera();
  
  // accessors to avoid exposing members publicly
  float getFovY() const { return fovY; }
  float getAspect() const { return aspect; }
  float getNearZ() const { return nearZ; }
  float getFarZ() const { return farZ; }
 };

 Camera::Camera()
 {

 }

 Camera::~Camera()
 {

 }










};