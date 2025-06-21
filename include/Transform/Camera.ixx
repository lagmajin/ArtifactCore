module;

export module Camera;

import Float3;

export namespace ArtifactCore
{

 class Camera {
  float3<float> position;
  float3<float> target;
  float3<float> up;

  float fovY;     // 垂直方向FOV（度）
  float aspect;   // アスペクト比
  float nearZ;    // ニアクリップ
  float farZ;     // ファークリップ

 public:
  explicit Camera();
  ~Camera();


 };

 Camera::Camera()
 {

 }

 Camera::~Camera()
 {

 }










};