module;
#include <utility>
#include <cmath>
#include <algorithm>

export module Graphics.Vector3D;

export namespace ArtifactCore
{

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    // Constructors
    constexpr Vec3() = default;
    constexpr Vec3(float v) : x(v), y(v), z(v) {}
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    // Element access
    float& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
    const float& operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }

    // Arithmetic operators
    Vec3 operator-() const { return { -x, -y, -z }; }
    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    Vec3& operator*=(float t) { x *= t; y *= t; z *= t; return *this; }
    Vec3& operator/=(float t) { return *this *= 1.0f / t; }

    Vec3 operator+(const Vec3& v) const { return Vec3(*this) += v; }
    Vec3 operator-(const Vec3& v) const { return Vec3(*this) -= v; }
    Vec3 operator*(const Vec3& v) const { return { x * v.x, y * v.y, z * v.z }; }
    Vec3 operator*(float t) const { return Vec3(*this) *= t; }
    Vec3 operator/(float t) const { return Vec3(*this) /= t; }

    // Utility
    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec3 normalized() const { return *this / length(); }

    bool operator==(const Vec3& v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const Vec3& v) const { return !(*this == v); }

    static Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
        const float u = std::clamp(t, 0.0f, 1.0f);
        return {a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u};
    }

    static Vec3 zero() { return { 0.0f, 0.0f, 0.0f }; }
    static Vec3 one() { return { 1.0f, 1.0f, 1.0f }; }

    // Dot and cross products
    static float dot(const Vec3& u, const Vec3& v)
    {
        return u.x * v.x + u.y * v.y + u.z * v.z;
    }
    static Vec3 cross(const Vec3& u, const Vec3& v)
    {
        return {
            u.y * v.z - u.z * v.y,
            u.z * v.x - u.x * v.z,
            u.x * v.y - u.y * v.x
        };
    }
};

// Free operators for scalar * Vec3
inline Vec3 operator*(float t, const Vec3& v) { return v * t; }

// Point3 for ray tracing clarity (same as Vec3 but semantically different)
using Point3 = Vec3;
using Color = Vec3;

} // namespace ArtifactCore

import Math.Quaternion;

export namespace ArtifactCore
{

using Vec3f = Vec3;
using Vec3Point = Point3;

} // namespace ArtifactCore
