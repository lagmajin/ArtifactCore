module;

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../../../ArtifactWidgets/include/Define/DllExportMacro.hpp"
module Transform.Hlper;


namespace ArtifactCore
{

 LIBRARY_DLL_API glm::f32mat4 calcTransform()
 {
  glm::f32mat4 result(1.0f);


  return result;
 }

 LIBRARY_DLL_API glm::f32mat4 toTransform2D(const StaticTransform2D& t)
 {
  glm::f32mat4 result(1.0f);
  result = glm::translate(result, glm::vec3(-t.anchorPointX(), -t.anchorPointY(), 0.0f));


  return result;
 }

};