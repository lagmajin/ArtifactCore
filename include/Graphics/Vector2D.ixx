module;
#include <algorithm>
#include <utility>
export module Graphics.Vector2D;

export namespace ArtifactCore
{
 struct Vex2
 {
  float x;
  float y;

  constexpr Vex2() = default;
  constexpr Vex2(float v) : x(v), y(v) {}
  constexpr Vex2(float x_, float y_) : x(x_), y(y_) {}

  Vex2 operator+(const Vex2& v) const { return {x + v.x, y + v.y}; }
  Vex2 operator-(const Vex2& v) const { return {x - v.x, y - v.y}; }
  Vex2 operator-() const { return {-x, -y}; }
  Vex2 operator*(float s) const { return {x * s, y * s}; }
  Vex2 operator*(const Vex2& v) const { return {x * v.x, y * v.y}; }
  Vex2 operator/(float s) const { return {x / s, y / s}; }
  Vex2 operator/(const Vex2& v) const { return {x / v.x, y / v.y}; }

  Vex2& operator+=(const Vex2& v) { x += v.x; y += v.y; return *this; }
  Vex2& operator-=(const Vex2& v) { x -= v.x; y -= v.y; return *this; }
  Vex2& operator*=(float s) { x *= s; y *= s; return *this; }
  Vex2& operator*=(const Vex2& v) { x *= v.x; y *= v.y; return *this; }
  Vex2& operator/=(float s) { x /= s; y /= s; return *this; }
  Vex2& operator/=(const Vex2& v) { x /= v.x; y /= v.y; return *this; }

  bool operator==(const Vex2& v) const { return x == v.x && y == v.y; }
  bool operator!=(const Vex2& v) const { return !(*this == v); }

  static Vex2 lerp(const Vex2& a, const Vex2& b, float t) {
   const float u = std::clamp(t, 0.0f, 1.0f);
   return {a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u};
  }
 };

 inline Vex2 operator*(float s, const Vex2& v) { return v * s; }

};
