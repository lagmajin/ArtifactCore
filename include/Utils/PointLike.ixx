module;
#include <concepts>
export module Utils.Point.Like;

export namespace ArtifactCore
{
 template <typename T>
 concept HasMethods = requires(T s) { s.width(); s.height(); };

 // ®”Œ^‚ÉŒÀ’è‚µ‚½ SizeLike
 template <typename T>
 concept IntSizeLike = HasMethods<T> &&
  std::integral<decltype(std::declval<T>().width())>;

 // •‚“®¬”“_Œ^‚ÉŒÀ’è‚µ‚½ SizeLike
 template <typename T>
 concept FloatSizeLike = HasMethods<T> &&
  std::floating_point<decltype(std::declval<T>().width())>;

 // ‘S‚Ä‚ğ‹–—e‚·‚éê‡
 template <typename T>
 concept SizeLike = IntSizeLike<T> || FloatSizeLike<T>;
	
	
	
	
	
};