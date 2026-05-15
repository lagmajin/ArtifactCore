module;
#include <utility>

#include <QtGui/QMatrix4x4>
#include <QtGui/QTransform>
#include <glm/glm.hpp>
#include "../Define/DllExportMacro.hpp"


export module Transform.Hlper;

import Transform;

export namespace ArtifactCore
{

LIBRARY_DLL_API glm::f32mat4 calcTransform();

LIBRARY_DLL_API glm::f32mat4 toTransform2D(const StaticTransform2D& transform);

LIBRARY_DLL_API QTransform makeLayerTransform2D(double positionX, double positionY,
                                                double rotationDegrees,
                                                double scaleX, double scaleY,
                                                double anchorX, double anchorY);

LIBRARY_DLL_API QTransform combineLayerTransform2D(const QTransform& local,
                                                   const QTransform& parent);

LIBRARY_DLL_API QMatrix4x4 makeLayerTransform3D(double positionX, double positionY,
                                               double positionZ,
                                               double rotationZDegrees,
                                               double scaleX, double scaleY,
                                               double scaleZ,
                                               double anchorX, double anchorY,
                                               double anchorZ);

LIBRARY_DLL_API QMatrix4x4 combineLayerTransform3D(const QMatrix4x4& local,
                                                   const QMatrix4x4& parent);








};
