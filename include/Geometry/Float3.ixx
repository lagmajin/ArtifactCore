module;
#include <concepts>
#include <cmath>
#include <iostream>

export module Float3;


export namespace ArtifactCore
{

 // 数値型だけ許すコンセプト
 template<typename T>
 concept Numeric = std::floating_point<T> || std::integral<T>;

 // float3的なベクトル構造体
 template<Numeric T>
 struct float3 {
  T x, y, z;

  // コンストラクタ
  constexpr float3(T x_ = 0, T y_ = 0, T z_ = 0) : x(x_), y(y_), z(z_) {}

  // 加算演算子
  constexpr float3 operator+(const float3& rhs) const {
   return { x + rhs.x, y + rhs.y, z + rhs.z };
  }

  // スカラー乗算
  constexpr float3 operator*(T scalar) const {
   return { x * scalar, y * scalar, z * scalar };
  }

  // 内積
  constexpr T dot(const float3& rhs) const {
   return x * rhs.x + y * rhs.y + z * rhs.z;
  }

  // 大きさ
  T length() const {
   return std::sqrt(dot(*this));
  }

  // 正規化
  float3 normalized() const {
   T len = length();
   if (len == 0) return { 0,0,0 };
   return *this * (1 / len);
  }
 };

 // ostream対応（デバッグ用）
 template <Numeric T>
 std::ostream& operator<<(std::ostream& os, const float3<T>& v) {
  return os << '(' << v.x << ", " << v.y << ", " << v.z << ')';
 }









};