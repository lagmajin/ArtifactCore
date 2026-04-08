module;
#include <utility>

#include <ranges>
#include <optional>

export module CppLinq;



export namespace ArtifactCore {

 template <std::ranges::input_range R, typename Pred>
 bool Any(const R& range, Pred pred) {
  for (auto&& v : range) {
   if (pred(v)) return true;
  }
  return false;
 }

 // All: ׂď𖞂
 template <std::ranges::input_range R, typename Pred>
 bool All(const R& range, Pred pred) {
  for (auto&& v : range) {
   if (!pred(v)) return false;
  }
  return true;
 }

 // FirstOrDefault: 𖞂ŏ̗vfBȂ std::nullopt
 template <std::ranges::input_range R, typename Pred>
 auto FirstOrDefault(const R& range, Pred pred) -> std::optional<std::ranges::range_value_t<R>> {
  for (auto&& v : range) {
   if (pred(v)) return v;
  }
  return std::nullopt;
 }

 // Where: filter ̃GCAXiranges::views::filter ̂܂܁j
 template <std::ranges::input_range R, typename Pred>
 auto Where(const R& range, Pred pred) {
  return range | std::views::filter(pred);
 }

 // Select: map ̃GCAXiranges::views::transform ̂܂܁j
 template <std::ranges::input_range R, typename Func>
 auto Select(const R& range, Func func) {
  return range | std::views::transform(func);
 }















};
