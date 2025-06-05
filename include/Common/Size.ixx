module;
import std;
export module Size;





export namespace ArtifactCore {

 template<typename T>
 concept HasWidthHeight = requires(T a) {
  { a.width } -> std::convertible_to<int>;
  { a.height } -> std::convertible_to<int>;
 } || requires(T a) {
  { a.width() } -> std::convertible_to<int>;
  { a.height() } -> std::convertible_to<int>;
 };

 // 独自Sizeクラス
 struct Size {
  int width = 0;
  int height = 0;

  Size() = default;

  // 他のサイズ互換コンストラクタ
  template<HasWidthHeight T>
  Size(const T& other) {
   if constexpr (requires { other.width; other.height; }) {
	width = other.width;
	height = other.height;
   }
   else {
	width = other.width();
	height = other.height();
   }
  }

  bool isEmpty() const { return width <= 0 || height <= 0; }

  bool operator==(const Size& other) const {
   return width == other.width && height == other.height;
  }

  bool operator!=(const Size& other) const {
   return !(*this == other);
  }
 };












};