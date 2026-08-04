module;
#include <cstdint>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
export module Particle;

export namespace ArtifactCore
{
 struct float2 {
  float x = 0.0f, y = 0.0f;

  constexpr float2() = default;
  constexpr float2(float v) : x(v), y(v) {}
  constexpr float2(float x_, float y_) : x(x_), y(y_) {}

  float2 operator+(const float2& o) const { return {x + o.x, y + o.y}; }
  float2 operator-(const float2& o) const { return {x - o.x, y - o.y}; }
  float2 operator-() const { return {-x, -y}; }
  float2 operator*(float s) const { return {x * s, y * s}; }
  float2 operator*(const float2& o) const { return {x * o.x, y * o.y}; }
  float2 operator/(float s) const { return {x / s, y / s}; }
  float2 operator/(const float2& o) const { return {x / o.x, y / o.y}; }
  float2& operator+=(const float2& o) { x += o.x; y += o.y; return *this; }
  float2& operator-=(const float2& o) { x -= o.x; y -= o.y; return *this; }
  float2& operator*=(float s) { x *= s; y *= s; return *this; }
  float2& operator*=(const float2& o) { x *= o.x; y *= o.y; return *this; }
  float2& operator/=(float s) { x /= s; y /= s; return *this; }
  float2& operator/=(const float2& o) { x /= o.x; y /= o.y; return *this; }
  bool operator==(const float2& o) const { return x == o.x && y == o.y; }
  bool operator!=(const float2& o) const { return !(*this == o); }
  static float2 lerp(const float2& a, const float2& b, float t) {
   const float u = std::clamp(t, 0.0f, 1.0f);
   return {a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u};
  }
 };

 struct float3 {
  float x = 0.0f, y = 0.0f, z = 0.0f;

  constexpr float3() = default;
  constexpr float3(float v) : x(v), y(v), z(v) {}
  constexpr float3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

  float3 operator+(const float3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  float3 operator-(const float3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  float3 operator-() const { return {-x, -y, -z}; }
  float3 operator*(float s) const { return {x * s, y * s, z * s}; }
  float3 operator*(const float3& o) const { return {x * o.x, y * o.y, z * o.z}; }
  float3 operator/(float s) const { return {x / s, y / s, z / s}; }
  float3 operator/(const float3& o) const { return {x / o.x, y / o.y, z / o.z}; }
  float3& operator+=(const float3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  float3& operator-=(const float3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
  float3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
  float3& operator*=(const float3& o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
  float3& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }
  float3& operator/=(const float3& o) { x /= o.x; y /= o.y; z /= o.z; return *this; }
  bool operator==(const float3& o) const { return x == o.x && y == o.y && z == o.z; }
  bool operator!=(const float3& o) const { return !(*this == o); }
  static float3 lerp(const float3& a, const float3& b, float t) {
   const float u = std::clamp(t, 0.0f, 1.0f);
   return {a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u};
  }
 };

 struct float4 {
  float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

  constexpr float4() = default;
  constexpr float4(float v) : x(v), y(v), z(v), w(v) {}
  constexpr float4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

  float4 operator+(const float4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
  float4 operator-(const float4& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
  float4 operator-() const { return {-x, -y, -z, -w}; }
  float4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
  float4 operator*(const float4& o) const { return {x * o.x, y * o.y, z * o.z, w * o.w}; }
  float4 operator/(float s) const { return {x / s, y / s, z / s, w / s}; }
  float4 operator/(const float4& o) const { return {x / o.x, y / o.y, z / o.z, w / o.w}; }
  float4& operator+=(const float4& o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
  float4& operator-=(const float4& o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
  float4& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
  float4& operator*=(const float4& o) { x *= o.x; y *= o.y; z *= o.z; w *= o.w; return *this; }
  float4& operator/=(float s) { x /= s; y /= s; z /= s; w /= s; return *this; }
  float4& operator/=(const float4& o) { x /= o.x; y /= o.y; z /= o.z; w /= o.w; return *this; }
  bool operator==(const float4& o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
  bool operator!=(const float4& o) const { return !(*this == o); }
  static float4 lerp(const float4& a, const float4& b, float t) {
   const float u = std::clamp(t, 0.0f, 1.0f);
   return {a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u, a.w + (b.w - a.w) * u};
  }
 };
	
 struct Particle {
  std::uint64_t id = 0;
  std::uint64_t emitterToken = 0;
  std::uint32_t seed = 0;
  std::uint32_t flags = 0;
  float age = 0.0f;
  float lifetime = 0.0f;

  float3 position{ 0.0f, 0.0f, 0.0f };
  float3 prevPosition{ 0.0f, 0.0f, 0.0f };
  float3 velocity{ 0.0f, 0.0f, 0.0f };
  float3 acceleration{ 0.0f, 0.0f, 0.0f };
  float3 rotation{ 0.0f, 0.0f, 0.0f };
  float3 angularVelocity{ 0.0f, 0.0f, 0.0f };

  float2 scale{ 1.0f, 1.0f };
  float size = 1.0f;
  float mass = 1.0f;
  float drag = 0.0f;
  float opacity = 1.0f;

  float4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
  float4 custom0{ 0.0f, 0.0f, 0.0f, 0.0f };
  float4 custom1{ 0.0f, 0.0f, 0.0f, 0.0f };

  // Rendering properties
  int textureIndex = -1;
  int blendMode = 0; // 0: Alpha, 1: Additive, 2: Screen, 3: Multiply

  // Sub-emitter tracking
  float lastSubEmitAge = 0.0f;
 };


};
