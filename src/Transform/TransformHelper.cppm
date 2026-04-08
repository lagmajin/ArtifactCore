module;
#include <utility>

#include <QtGui/QMatrix4x4>
#include <QtGui/QTransform>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include "../../include/Define/DllExportMacro.hpp"


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

 LIBRARY_DLL_API QTransform makeLayerTransform2D(double positionX, double positionY,
                                                 double rotationDegrees,
                                                 double scaleX, double scaleY,
                                                 double anchorX, double anchorY)
 {
  QTransform result;
  // AE-like transform order: Translate(Pos) * Rotate(Rot) * Scale(Scale) * Translate(-Anchor)
  result.translate(positionX, positionY);
  result.rotate(rotationDegrees);
  result.scale(scaleX, scaleY);
  result.translate(-anchorX, -anchorY);
  return result;
 }

 LIBRARY_DLL_API QTransform combineLayerTransform2D(const QTransform& local,
                                                    const QTransform& parent)
 {
  // Parent transform is applied in the child's space.
  return local * parent;
 }

 LIBRARY_DLL_API QMatrix4x4 makeLayerTransform3D(double positionX, double positionY,
                                                double positionZ,
                                                double rotationZDegrees,
                                                double scaleX, double scaleY,
                                                double scaleZ,
                                                double anchorX, double anchorY,
                                                double anchorZ)
 {
  QMatrix4x4 result;
  result.translate(positionX, positionY, positionZ);
  result.rotate(rotationZDegrees, 0.0f, 0.0f, 1.0f);
  result.scale(scaleX, scaleY, scaleZ);
  result.translate(-anchorX, -anchorY, -anchorZ);
  return result;
 }

 LIBRARY_DLL_API QMatrix4x4 combineLayerTransform3D(const QMatrix4x4& local,
                                                    const QMatrix4x4& parent)
 {
  return parent * local;
 }

};
