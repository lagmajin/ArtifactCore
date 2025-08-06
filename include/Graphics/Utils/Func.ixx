
module;

#include <glm/ext/vector_float4_precision.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.inl>
#include <DiligentCore/Common/interface/BasicMath.hpp>

#include "../../Define/DllExportMacro.hpp"
export module Graphics.Func;


export namespace ArtifactCore
{
 LIBRARY_DLL_API glm::mat4 CreateInitialViewMatrix()
 {
  // 1. カメラの位置 (Eye/Camera Position)
  // 例えば、Z軸方向に少し離れた位置から原点を見る
  glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 2.0f);

  // 2. 注視点 (Target/LookAt Position)
  // シーンの中心（原点）を見る
  glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);

  // 3. 上方向 (Up Vector)
  // 通常はワールドのY軸プラス方向
  glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

  // glm::lookAt() を使ってビュー行列を計算
  return glm::lookAt(cameraPos, cameraTarget, cameraUp);
 }

 LIBRARY_DLL_API Diligent::float4x4 GLMMat4ToDiligentFloat4x4(const glm::mat4& glm_mat)
 {
  Diligent::float4x4 diligent_mat;
  for (int i = 0; i < 4; ++i)
  {
   for (int j = 0; j < 4; ++j)
   {
	// GLMはColumn-Majorなので、[列][行]の順でアクセス
	// Diligent::float4x4 も内部的には列優先の場合が多いですが、
	// 安全のため要素ごとにコピー
	diligent_mat.m[i][j] = glm_mat[i][j];
   }
  }
  return diligent_mat;
 }







};