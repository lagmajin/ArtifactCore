module;

#include <glm/glm.hpp>
#include "../Define/DllExportMacro.hpp"


export module Transform.Hlper;

import Transform;

export namespace ArtifactCore
{

LIBRARY_DLL_API glm::f32mat4 calcTransform();

LIBRARY_DLL_API glm::f32mat4 toTransform2D(const StaticTransform2D& transform);








};