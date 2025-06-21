module;
#include <concepts>
#include <cmath>
#include <iostream>

export module Float3;


export namespace ArtifactCore
{

 template <typename T>
 concept Float = std::floating_point<T>;

 template <Float T>
 struct float3 {
  T x, y, z;

  constexpr float3(T x_ = 0, T y_ = 0, T z_ = 0)
   : x(x_), y(y_), z(z_) {
  }

  // ‰ÁZ
  constexpr float3 operator+(const float3& rhs) const {
   return { x + rhs.x, y + rhs.y, z + rhs.z };
  }

  // ƒXƒJƒ‰[æZ
  constexpr float3 operator*(T s) const {
   return { x * s, y * s, z * s };
  }

  // “àÏ
  constexpr T dot(const float3& rhs) const {
   return x * rhs.x + y * rhs.y + z * rhs.z;
  }

  // ‘å‚«‚³
  T length() const {
   return std::sqrt(dot(*this));
  }

  // ³‹K‰»
  float3 normalized() const {
   T len = length();
   if (len == 0) return { 0, 0, 0 };
   return *this * (1 / len);
  }
 };

 // ostream ‘Î‰
 template <Float T>
 std::ostream& operator<<(std::ostream& os, const float3<T>& v) {
  return os << '(' << v.x << ", " << v.y << ", " << v.z << ')';
 }








};