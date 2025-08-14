module;
#include <QSize>

export module Size;


import std;


export namespace ArtifactCore {

 template<typename T>
 concept HasWidthHeightLike =
  // width / height メンバ変数
  requires(const T & a) { { a.width } -> std::convertible_to<int>; { a.height } -> std::convertible_to<int>; } ||
 // width() / height() メンバ関数
  requires(const T & a) { { a.width() } -> std::convertible_to<int>; { a.height() } -> std::convertible_to<int>; } ||
 // w / h メンバ変数 (SDL, 独自GUI等)
  requires(const T & a) { { a.w } -> std::convertible_to<int>; { a.h } -> std::convertible_to<int>; } ||
 // cols / rows (OpenCV Mat 等)
  requires(const T & a) { { a.cols } -> std::convertible_to<int>; { a.rows } -> std::convertible_to<int>; } ||
 // x / y (ImGuiのImVec2等)
  requires(const T & a) { { a.x } -> std::convertible_to<int>; { a.y } -> std::convertible_to<int>; } ||
 // Width / Height (WinRT Size等)
  requires(const T & a) { { a.Width } -> std::convertible_to<int>; { a.Height } -> std::convertible_to<int>; };

 // 独自Sizeクラス
 struct Size_2D {
  int width = 0;
  int height = 0;

  Size_2D() = default;

  // 幅・高さ系を何でも吸収するコンストラクタ
  template<HasWidthHeightLike T>
  Size_2D(const T& other) {
   if constexpr (requires { other.width; other.height; }) {
	width = other.width;
	height = other.height;
   }
   else if constexpr (requires { other.width(); other.height(); }) {
	width = other.width();
	height = other.height();
   }
   else if constexpr (requires { other.w; other.h; }) {
	width = other.w;
	height = other.h;
   }
   else if constexpr (requires { other.cols; other.rows; }) {
	width = other.cols;
	height = other.rows;
   }
   else if constexpr (requires { other.x; other.y; }) {
	width = other.x;
	height = other.y;
   }
   else { // Width / Height
	width = other.Width;
	height = other.Height;
   }
  }

  bool isEmpty() const { return width <= 0 || height <= 0; }

  bool operator==(const Size_2D& other) const {
   return width == other.width && height == other.height;
  }

  bool operator!=(const Size_2D& other) const {
   return !(*this == other);
  }

  int area() const noexcept { return width * height; }
  float aspectRatio() const noexcept {
   return height != 0 ? static_cast<float>(width) / height : 0.0f;
  }
 };

 };






