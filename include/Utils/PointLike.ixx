module;
#include <concepts>
#include <utility>
export module Utils.Point.Like;

export namespace ArtifactCore
{
 template <typename T>
 concept HasMethods = requires(T s) { s.width(); s.height(); };

 // Integer-like size concept
 template <typename T>
 concept IntSizeLike = HasMethods<T> &&
  std::integral<decltype(std::declval<T>().width())>;

 // Floating-point size concept
 template <typename T>
 concept FloatSizeLike = HasMethods<T> &&
  std::floating_point<decltype(std::declval<T>().width())>;

 // Either integer or floating-point size
 template <typename T>
 concept SizeLike = IntSizeLike<T> || FloatSizeLike<T>;
};
