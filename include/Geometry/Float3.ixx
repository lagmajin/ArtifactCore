module;
#include <concepts>
#include <cmath>
#include <iostream>

export module Float3;


export namespace ArtifactCore
{

 // ���l�^���������R���Z�v�g
 template<typename T>
 concept Numeric = std::floating_point<T> || std::integral<T>;

 // float3�I�ȃx�N�g���\����
 template<Numeric T>
 struct float3 {
  T x, y, z;

  // �R���X�g���N�^
  constexpr float3(T x_ = 0, T y_ = 0, T z_ = 0) : x(x_), y(y_), z(z_) {}

  // ���Z���Z�q
  constexpr float3 operator+(const float3& rhs) const {
   return { x + rhs.x, y + rhs.y, z + rhs.z };
  }

  // �X�J���[��Z
  constexpr float3 operator*(T scalar) const {
   return { x * scalar, y * scalar, z * scalar };
  }

  // ����
  constexpr T dot(const float3& rhs) const {
   return x * rhs.x + y * rhs.y + z * rhs.z;
  }

  // �傫��
  T length() const {
   return std::sqrt(dot(*this));
  }

  // ���K��
  float3 normalized() const {
   T len = length();
   if (len == 0) return { 0,0,0 };
   return *this * (1 / len);
  }
 };

 // ostream�Ή��i�f�o�b�O�p�j
 template <Numeric T>
 std::ostream& operator<<(std::ostream& os, const float3<T>& v) {
  return os << '(' << v.x << ", " << v.y << ", " << v.z << ')';
 }









};