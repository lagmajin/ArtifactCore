module;
#include <algorithm>
export module Opacity;






export namespace ArtifactCore
{
 class Opacity {
  float value; // 0.0f (透明) 〜 1.0f (不透明)
 public:
  constexpr Opacity() noexcept : value(1.0f) {} // デフォルトは不透明
  constexpr explicit Opacity(float v) noexcept : value(std::clamp(v, 0.0f, 1.0f)) {}

  // 値取得
  constexpr float get() const noexcept { return value; }

  // 値セット（0~1に制限）
  constexpr void set(float v) noexcept { value = std::clamp(v, 0.0f, 1.0f); }

  // 不透明度を0〜255の整数で取得
  constexpr int toByte() const noexcept { return static_cast<int>(value * 255.0f + 0.5f); }

  // 乗算ブレンド的にopacityを掛ける（例: 親レイヤーのopacityと掛け合わせ）
  constexpr void multiply(float factor) noexcept { set(value * factor); }

  // 演算子オーバーロード例
  constexpr Opacity operator*(const Opacity& other) const noexcept {
   return Opacity(value * other.value);
  }
 };













};