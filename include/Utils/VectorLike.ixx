module;
#include <utility>
#include <concepts>

#include "../Define/DllExportMacro.hpp"
export module Vector.Like;

export namespace ArtifactCore {

 template <typename T>
 concept VectorLike = requires(T v) {
  { v.x } -> std::convertible_to<float>;
  { v.y } -> std::convertible_to<float>;
 };



};
