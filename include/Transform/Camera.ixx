module;

export module Camera;

import Float3;

export namespace ArtifactCore
{

 class Camera {
  float3<float> position;
  float3<float> target;
  float3<float> up;

  float fovY;     // ��������FOV�i�x�j
  float aspect;   // �A�X�y�N�g��
  float nearZ;    // �j�A�N���b�v
  float farZ;     // �t�@�[�N���b�v

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