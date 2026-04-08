module;
#include <utility>
#include <concepts>
export module Numeric.Like;





export namespace ArtifactCore {

 template <typename T>
 concept NumericLike =
  requires(T a, T b) {
   { a + b } -> std::convertible_to<T>;
   { a - b } -> std::convertible_to<T>;
   { a* b } -> std::convertible_to<T>;
   { a / b } -> std::convertible_to<T>;
   { a = b };
   { static_cast<float>(a) }; // 暗黙または明示的変換ができること
 };

 template <typename T>
 concept FloatingLike = std::floating_point<T>;




};
