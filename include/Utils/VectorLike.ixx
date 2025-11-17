module;
#include <QVector2D>
#include <QVector3D>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <opencv2/core.hpp>
export module Vector.Like;

import std;

export namespace ArtifactCore {

 template <typename T>
 concept VectorLike = requires(T v) {
  { v.x } -> std::convertible_to<float>;
  { v.y } -> std::convertible_to<float>;
 };




};