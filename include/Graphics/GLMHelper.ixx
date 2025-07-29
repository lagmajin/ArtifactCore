module;
#include <glm/glm.hpp>
#include <QString>
#include <QStringList>
#include "../Define/DllExportMacro.hpp"
export module Graphics.Helpler.GLM;

export namespace ArtifactCore
{

 LIBRARY_DLL_API QString glmMat4ToStringOneLine(const glm::mat4& mat) {
  QStringList elements;
  for (int row = 0; row < 4; ++row)
  {
   for (int col = 0; col < 4; ++col)
   {
	elements << QString::number(mat[col][row], 'f', 6); // 小数点以下6桁固定
   }
  }
  return elements.join(", ");
 }





};