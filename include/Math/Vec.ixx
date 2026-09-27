module;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>

#include <QMatrix4x4>
#include <QPointF>
#include <QVector2D>
#include <QVector3D>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

export module Math.Vec;

export namespace ArtifactCore {

// =====================================================================
// 公式数学型 (glm ラップ)
// ---------------------------------------------------------------------
// QVector3D 排除の到達点となる型群。glm を直接 alias するため
// glm の全 API (swizzle 除く)・SIMD 最適化がそのまま使え、変換コストはゼロ。
//
// 移行ポリシー:
// - 新規コードはこのモジュールの型を使用する
// - 既存 QVector3D/QVector2D は触ったファイルから toVec3()/toQVector3D()
//   等の明示変換経由で段階的に置換する (暗黙変換は作らない)
//
// 規約: glm 1.0.3 / column-major / right-handed / float 既定。
// =====================================================================

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat2 = glm::mat2;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using quat = glm::quat;

// double 精度版 (PropertyTypes の double ベース Point/Rect 連携用)
using dvec2 = glm::dvec2;
using dvec3 = glm::dvec3;
using dvec4 = glm::dvec4;
using dmat4 = glm::dmat4;

using ivec2 = glm::ivec2;
using ivec3 = glm::ivec3;

// =====================================================================
// 空間タグ付き座標 (既存 vec2/vec3 と段階的に共存する新規 API)
// ---------------------------------------------------------------------
// 異なる Space 間の暗黙変換・演算は定義しない。座標変換は呼び出し側が
// 意味を持つ名前付き関数で実装し、Qt / glm / GPU 境界でだけ値を取り出す。
// 点と方向を分け、Point + Point も定義しない。
// =====================================================================

namespace Coordinates {

struct ScreenLogicalSpace {};
struct ScreenPhysicalSpace {};
struct CompositionSpace {};
struct LayerLocalSpace {};
struct WorldSpace {};
struct ViewSpace {};
struct ClipSpace {};
struct TextureUVSpace {};

template<class Space>
struct Point2 {
    float x = 0.0f;
    float y = 0.0f;
};

template<class Space>
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    [[nodiscard]] constexpr Vector2 operator+(Vector2 rhs) const noexcept
    {
        return {x + rhs.x, y + rhs.y};
    }
    [[nodiscard]] constexpr Vector2 operator-(Vector2 rhs) const noexcept
    {
        return {x - rhs.x, y - rhs.y};
    }
    [[nodiscard]] constexpr Vector2 operator*(float scale) const noexcept
    {
        return {x * scale, y * scale};
    }
    [[nodiscard]] constexpr Vector2 operator/(float scale) const noexcept
    {
        return {x / scale, y / scale};
    }
};

template<class Space>
[[nodiscard]] constexpr Point2<Space> operator+(
    Point2<Space> point, Vector2<Space> offset) noexcept
{
    return {point.x + offset.x, point.y + offset.y};
}

template<class Space>
[[nodiscard]] constexpr Point2<Space> operator-(
    Point2<Space> point, Vector2<Space> offset) noexcept
{
    return {point.x - offset.x, point.y - offset.y};
}

template<class Space>
[[nodiscard]] constexpr Vector2<Space> operator-(
    Point2<Space> lhs, Point2<Space> rhs) noexcept
{
    return {lhs.x - rhs.x, lhs.y - rhs.y};
}

template<class Space>
struct Point3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

template<class Space>
struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] constexpr Vector3 operator+(Vector3 rhs) const noexcept
    {
        return {x + rhs.x, y + rhs.y, z + rhs.z};
    }
    [[nodiscard]] constexpr Vector3 operator-(Vector3 rhs) const noexcept
    {
        return {x - rhs.x, y - rhs.y, z - rhs.z};
    }
    [[nodiscard]] constexpr Vector3 operator*(float scale) const noexcept
    {
        return {x * scale, y * scale, z * scale};
    }
    [[nodiscard]] constexpr Vector3 operator/(float scale) const noexcept
    {
        return {x / scale, y / scale, z / scale};
    }
};

template<class Space>
[[nodiscard]] constexpr Point3<Space> operator+(
    Point3<Space> point, Vector3<Space> offset) noexcept
{
    return {point.x + offset.x, point.y + offset.y, point.z + offset.z};
}

template<class Space>
[[nodiscard]] constexpr Point3<Space> operator-(
    Point3<Space> point, Vector3<Space> offset) noexcept
{
    return {point.x - offset.x, point.y - offset.y, point.z - offset.z};
}

template<class Space>
[[nodiscard]] constexpr Vector3<Space> operator-(
    Point3<Space> lhs, Point3<Space> rhs) noexcept
{
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

template<class Space>
[[nodiscard]] constexpr Vector3<Space> cross(
    Vector3<Space> lhs, Vector3<Space> rhs) noexcept
{
    return {lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.z * rhs.x - lhs.x * rhs.z,
            lhs.x * rhs.y - lhs.y * rhs.x};
}

using ScreenLogicalPoint2 = Point2<ScreenLogicalSpace>;
using ScreenPhysicalPoint2 = Point2<ScreenPhysicalSpace>;
using CompositionPoint2 = Point2<CompositionSpace>;
using LayerLocalPoint2 = Point2<LayerLocalSpace>;
using WorldPoint3 = Point3<WorldSpace>;
using ViewPoint3 = Point3<ViewSpace>;
using ClipPoint3 = Point3<ClipSpace>;
using TextureUVPoint2 = Point2<TextureUVSpace>;

using ScreenLogicalVector2 = Vector2<ScreenLogicalSpace>;
using ScreenPhysicalVector2 = Vector2<ScreenPhysicalSpace>;
using CompositionVector2 = Vector2<CompositionSpace>;
using LayerLocalVector2 = Vector2<LayerLocalSpace>;
using WorldVector3 = Vector3<WorldSpace>;
using ViewVector3 = Vector3<ViewSpace>;
using ClipVector3 = Vector3<ClipSpace>;
using TextureUVVector2 = Vector2<TextureUVSpace>;

} // namespace Coordinates

// =====================================================================
// ギャップ補完 (監査で判明した不足操作)。純粋数学部。Qt 非依存。
// =====================================================================

/// ゼロベクトル時の NaN を防ぐ正規化。長さが eps 以下なら fallback を返す。
[[nodiscard]] inline vec3 safeNormalize(const vec3& v, const vec3& fallback = vec3(0.0f, 0.0f, 1.0f)) noexcept
{
    const float lenSq = glm::length2(v);
    if (lenSq <= 1e-12f) {
        return fallback;
    }
    return v / std::sqrt(lenSq);
}

[[nodiscard]] inline vec2 safeNormalize(const vec2& v, const vec2& fallback = vec2(0.0f, 1.0f)) noexcept
{
    const float lenSq = glm::length2(v);
    if (lenSq <= 1e-12f) {
        return fallback;
    }
    return v / std::sqrt(lenSq);
}

template<typename T>
[[nodiscard]] inline float distanceSq(const T& a, const T& b) noexcept
{
    return static_cast<float>(glm::length2(a - b));
}

[[nodiscard]] inline vec3 perComponentMin(const vec3& a, const vec3& b) noexcept
{
    return vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}

[[nodiscard]] inline vec3 perComponentMax(const vec3& a, const vec3& b) noexcept
{
    return vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}

[[nodiscard]] inline vec3 perComponentAbs(const vec3& v) noexcept
{
    return vec3(std::fabs(v.x), std::fabs(v.y), std::fabs(v.z));
}

[[nodiscard]] inline vec3 clampComponents(const vec3& v, float lo, float hi) noexcept
{
    return vec3(std::clamp(v.x, lo, hi), std::clamp(v.y, lo, hi), std::clamp(v.z, lo, hi));
}

template<typename T>
[[nodiscard]] inline bool isFiniteVec(const T& v) noexcept
{
    for (int i = 0; i < T::length(); ++i) {
        if (!std::isfinite(v[i])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool isFinite(const vec2& v) noexcept { return isFiniteVec(v); }
[[nodiscard]] inline bool isFinite(const vec3& v) noexcept { return isFiniteVec(v); }
[[nodiscard]] inline bool isFinite(const vec4& v) noexcept { return isFiniteVec(v); }

[[nodiscard]] inline bool isFinite(const mat4& m) noexcept
{
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            if (!std::isfinite(m[c][r])) {
                return false;
            }
        }
    }
    return true;
}

/// epsilon 付き成分比較 (operator== の float 完全一致問題への対抗)。
[[nodiscard]] inline bool epsilonEqual(const vec3& a, const vec3& b, float eps = 1e-5f) noexcept
{
    const vec3 d = perComponentAbs(a - b);
    return d.x <= eps && d.y <= eps && d.z <= eps;
}

[[nodiscard]] inline bool epsilonEqual(const vec2& a, const vec2& b, float eps = 1e-5f) noexcept
{
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps;
}

// =====================================================================
// Qt 境界変換 (明示関数のみ。暗黙変換は意図的に提供しない)。
// QVector3D/QMatrix4x4 と glm は column-major 互換のため
// 生データコピーで一致する。
// =====================================================================

[[nodiscard]] inline QVector3D toQVector3D(const vec3& v) noexcept
{
    return QVector3D(v.x, v.y, v.z);
}

[[nodiscard]] inline vec3 toVec3(const QVector3D& v) noexcept
{
    return vec3(v.x(), v.y(), v.z());
}

[[nodiscard]] inline QVector2D toQVector2D(const vec2& v) noexcept
{
    return QVector2D(v.x, v.y);
}

[[nodiscard]] inline vec2 toVec2(const QVector2D& v) noexcept
{
    return vec2(v.x(), v.y());
}

[[nodiscard]] inline QPointF toQPointF(const vec2& v) noexcept
{
    return QPointF(static_cast<qreal>(v.x), static_cast<qreal>(v.y));
}

[[nodiscard]] inline vec2 toVec2(const QPointF& p) noexcept
{
    return vec2(static_cast<float>(p.x()), static_cast<float>(p.y()));
}

[[nodiscard]] inline QMatrix4x4 toQMatrix4x4(const mat4& m) noexcept
{
    QMatrix4x4 q;
    // QMatrix4x4::data() は row-major の 16 floats (column-major の
    // 転置表現ではない点に注意: Qt は内部的に column-major で保持し、
    // data() アクセスは column-major 順)。glm も column-major のため
    // 直接コピーで一致する。
    std::copy(m.data(), m.data() + 16, q.data());
    return q;
}

[[nodiscard]] inline mat4 toGlmMat4(const QMatrix4x4& q) noexcept
{
    mat4 m;
    std::copy(q.constData(), q.constData() + 16, &m[0].x);
    return m;
}

} // namespace ArtifactCore
