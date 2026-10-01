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
#include <type_traits>

export module Math.Vec;

export namespace ArtifactCore {

// =====================================================================
// 公式数学型 (glm ラップ)
// ---------------------------------------------------------------------
// QVector3D 排除の到達点となる型群。glm を直接 alias するため
// glm の全 API (swizzle 除く)・SIMD 最適化がそのまま使え、変換コストはゼロ。
//
// 移行ポリシー:
// - 純粋な線形代数の局所計算には vec/mat alias を使う。
// - 空間・点/方向・単位の意味をAPI境界や共有stateで区別する場合は
//   Coordinates の型を使い、境界でのみ明示変換する。全計算を強型で包まない。
// - 既存 QVector3D/QVector2D は触った範囲から toVec3()/toQVector3D()
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
struct LayerParentSpace {};
struct WorldSpace {};
struct ViewSpace {};
struct ClipSpace {};
struct TextureUVSpace {};
struct SourcePixelSpace {};

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
struct Extent2 {
    float width = 0.0f;
    float height = 0.0f;
};

template<class Space>
struct BoundsPoint2 {
    double x = 0.0;
    double y = 0.0;
};

template<class Space>
struct Bounds2 {
    BoundsPoint2<Space> minimum{};
    BoundsPoint2<Space> maximum{};
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
using LayerParentPoint2 = Point2<LayerParentSpace>;
using LayerLocalPoint3 = Point3<LayerLocalSpace>;
using LayerParentPoint3 = Point3<LayerParentSpace>;
using WorldPoint3 = Point3<WorldSpace>;
using ViewPoint3 = Point3<ViewSpace>;
using ClipPoint3 = Point3<ClipSpace>;
using TextureUVPoint2 = Point2<TextureUVSpace>;
using LayerParentBoundsPoint2 = BoundsPoint2<LayerParentSpace>;
using CompositionBounds2 = Bounds2<CompositionSpace>;
using SourcePixelBounds2 = Bounds2<SourcePixelSpace>;
using ScreenPhysicalBounds2 = Bounds2<ScreenPhysicalSpace>;
using SourcePixelExtent2 = Extent2<SourcePixelSpace>;
using ScreenLogicalExtent2 = Extent2<ScreenLogicalSpace>;
using ScreenPhysicalExtent2 = Extent2<ScreenPhysicalSpace>;
using CompositionExtent2 = Extent2<CompositionSpace>;

using ScreenLogicalVector2 = Vector2<ScreenLogicalSpace>;
using ScreenPhysicalVector2 = Vector2<ScreenPhysicalSpace>;
using CompositionVector2 = Vector2<CompositionSpace>;
using LayerLocalVector2 = Vector2<LayerLocalSpace>;
using LayerParentVector2 = Vector2<LayerParentSpace>;
using LayerLocalVector3 = Vector3<LayerLocalSpace>;
using WorldVector3 = Vector3<WorldSpace>;
using ViewVector3 = Vector3<ViewSpace>;
using ClipVector3 = Vector3<ClipSpace>;
using TextureUVVector2 = Vector2<TextureUVSpace>;

[[nodiscard]] inline ScreenPhysicalPoint2 toScreenPhysical(
    ScreenLogicalPoint2 point, float devicePixelRatio) noexcept
{
    return {point.x * devicePixelRatio, point.y * devicePixelRatio};
}

[[nodiscard]] inline ScreenPhysicalVector2 toScreenPhysical(
    ScreenLogicalVector2 vector, float devicePixelRatio) noexcept
{
    return {vector.x * devicePixelRatio, vector.y * devicePixelRatio};
}

[[nodiscard]] inline ScreenPhysicalExtent2 toScreenPhysical(
    ScreenLogicalExtent2 extent, float devicePixelRatio) noexcept
{
    return {extent.width * devicePixelRatio,
            extent.height * devicePixelRatio};
}

[[nodiscard]] constexpr ScreenLogicalExtent2 screenLogicalExtent(
    float width, float height) noexcept
{
    return {width, height};
}

[[nodiscard]] inline ScreenLogicalPoint2 screenLogicalPointFromQPointF(
    const QPointF& point) noexcept
{
    return {static_cast<float>(point.x()), static_cast<float>(point.y())};
}

[[nodiscard]] inline ScreenLogicalVector2 screenLogicalVectorFromQPointF(
    const QPointF& vector) noexcept
{
    return {static_cast<float>(vector.x()), static_cast<float>(vector.y())};
}

[[nodiscard]] inline CompositionPoint2 compositionPointFromQPointF(
    const QPointF& point) noexcept
{
    return {static_cast<float>(point.x()), static_cast<float>(point.y())};
}

[[nodiscard]] inline LayerLocalPoint2 layerLocalPoint2FromQPointF(
    const QPointF& point) noexcept
{
    return {static_cast<float>(point.x()), static_cast<float>(point.y())};
}

[[nodiscard]] inline LayerParentPoint2 layerParentPoint2FromQPointF(
    const QPointF& point) noexcept
{
    return {static_cast<float>(point.x()), static_cast<float>(point.y())};
}

[[nodiscard]] constexpr LayerParentPoint2 layerParentPoint2FromComposition(
    CompositionPoint2 point) noexcept
{
    return {point.x, point.y};
}

[[nodiscard]] inline QPointF toQPointF(ScreenLogicalPoint2 point) noexcept
{
    return QPointF(static_cast<qreal>(point.x), static_cast<qreal>(point.y));
}

[[nodiscard]] inline QPointF toQPointF(ScreenPhysicalPoint2 point) noexcept
{
    return QPointF(static_cast<qreal>(point.x), static_cast<qreal>(point.y));
}

[[nodiscard]] inline QPointF toQPointF(CompositionPoint2 point) noexcept
{
    return QPointF(static_cast<qreal>(point.x), static_cast<qreal>(point.y));
}

[[nodiscard]] inline QPointF toQPointF(LayerLocalPoint2 point) noexcept
{
    return QPointF(static_cast<qreal>(point.x), static_cast<qreal>(point.y));
}

[[nodiscard]] inline QPointF toQPointF(LayerParentPoint2 point) noexcept
{
    return QPointF(static_cast<qreal>(point.x), static_cast<qreal>(point.y));
}

[[nodiscard]] inline QPointF toQPointF(LayerParentVector2 vector) noexcept
{
    return QPointF(static_cast<qreal>(vector.x), static_cast<qreal>(vector.y));
}

template<class Space>
[[nodiscard]] inline QPointF toQPointF(BoundsPoint2<Space> point) noexcept
{
    return QPointF(point.x, point.y);
}

[[nodiscard]] inline WorldVector3 worldVectorFromQVector3D(
    const QVector3D& vector) noexcept
{
    return {vector.x(), vector.y(), vector.z()};
}

[[nodiscard]] inline WorldPoint3 worldPoint3FromQVector3D(
    const QVector3D& point) noexcept
{
    return {point.x(), point.y(), point.z()};
}

[[nodiscard]] inline LayerParentPoint3 layerParentPoint3FromQVector3D(
    const QVector3D& point) noexcept
{
    return {point.x(), point.y(), point.z()};
}

[[nodiscard]] inline QVector3D toQVector3D(WorldVector3 vector) noexcept
{
    return QVector3D(vector.x, vector.y, vector.z);
}

[[nodiscard]] inline QVector3D toQVector3D(WorldPoint3 point) noexcept
{
    return QVector3D(point.x, point.y, point.z);
}

[[nodiscard]] inline QVector3D toQVector3D(LayerParentPoint3 point) noexcept
{
    return QVector3D(point.x, point.y, point.z);
}

[[nodiscard]] inline LayerLocalPoint3 layerLocalPoint3FromQVector3D(
    const QVector3D& point) noexcept
{
    return {point.x(), point.y(), point.z()};
}

[[nodiscard]] inline LayerLocalVector3 layerLocalVector3FromQVector3D(
    const QVector3D& vector) noexcept
{
    return {vector.x(), vector.y(), vector.z()};
}

[[nodiscard]] inline QVector3D toQVector3D(LayerLocalPoint3 point) noexcept
{
    return QVector3D(point.x, point.y, point.z);
}

} // namespace Coordinates

namespace Units {

struct Millimeters {
    float value = 0.0f;
};

struct Meters {
    float value = 0.0f;
};

struct Pixels {
    float value = 0.0f;
};

// Percentage points: 100 represents a normalized factor of 1.0.
struct Percent {
    float value = 0.0f;
};

// Per-axis dimensionless scale factors. Keep this distinct from spatial vectors.
struct Scale2 {
    float x = 1.0f;
    float y = 1.0f;
};

struct Scale3 {
    float x = 1.0f;
    float y = 1.0f;
    float z = 1.0f;
};

// Scalar distance in the coordinate units used by world-space transforms.
struct WorldLength {
    float value = 0.0f;

    [[nodiscard]] constexpr WorldLength operator+(WorldLength rhs) const noexcept
    {
        return {value + rhs.value};
    }
    [[nodiscard]] constexpr WorldLength operator-(WorldLength rhs) const noexcept
    {
        return {value - rhs.value};
    }
    [[nodiscard]] constexpr WorldLength operator*(float scale) const noexcept
    {
        return {value * scale};
    }
    [[nodiscard]] constexpr WorldLength operator/(float scale) const noexcept
    {
        return {value / scale};
    }
};

// Scalar distance measured in a layer's local coordinate space.
struct LayerLocalLength {
    float value = 0.0f;
};

// Signed change in normalized opacity; values are added to [0, 1] opacity.
struct OpacityDelta {
    float value = 0.0f;
};

// Dimensionless multiplicative factor for transform scale.
struct ScaleFactor {
    float value = 1.0f;
};

// Camera aperture expressed as an f-number. Zero remains available as the
// existing sentinel for selecting the non-physical DOF model.
struct FStop {
    float value = 0.0f;
};

[[nodiscard]] inline Scale3 scale3FromQVector3D(
    const QVector3D& scale) noexcept
{
    return {scale.x(), scale.y(), scale.z()};
}

[[nodiscard]] inline QVector3D toQVector3D(Scale3 scale) noexcept
{
    return QVector3D(scale.x, scale.y, scale.z);
}

struct Degrees {
    float value = 0.0f;

    [[nodiscard]] constexpr Degrees operator+(Degrees rhs) const noexcept
    {
        return {value + rhs.value};
    }
    [[nodiscard]] constexpr Degrees operator-(Degrees rhs) const noexcept
    {
        return {value - rhs.value};
    }
    [[nodiscard]] constexpr Degrees operator*(float scale) const noexcept
    {
        return {value * scale};
    }
    [[nodiscard]] constexpr Degrees operator/(float scale) const noexcept
    {
        return {value / scale};
    }
};

struct Radians {
    float value = 0.0f;

    [[nodiscard]] constexpr Radians operator+(Radians rhs) const noexcept
    {
        return {value + rhs.value};
    }
    [[nodiscard]] constexpr Radians operator-(Radians rhs) const noexcept
    {
        return {value - rhs.value};
    }
    [[nodiscard]] constexpr Radians operator*(float scale) const noexcept
    {
        return {value * scale};
    }
    [[nodiscard]] constexpr Radians operator/(float scale) const noexcept
    {
        return {value / scale};
    }
};

struct EulerDegrees3 {
    Degrees x{};
    Degrees y{};
    Degrees z{};
};

[[nodiscard]] constexpr Degrees toDegrees(Radians angle) noexcept
{
    return {static_cast<float>(static_cast<double>(angle.value) * 180.0 /
                               3.14159265358979323846)};
}

[[nodiscard]] constexpr Radians toRadians(Degrees angle) noexcept
{
    return {static_cast<float>(static_cast<double>(angle.value) *
                               3.14159265358979323846 / 180.0)};
}

[[nodiscard]] inline EulerDegrees3 eulerDegreesFromQVector3D(
    const QVector3D& rotation) noexcept
{
    return {{rotation.x()}, {rotation.y()}, {rotation.z()}};
}

[[nodiscard]] inline QVector3D toQVector3D(
    EulerDegrees3 rotation) noexcept
{
    return QVector3D(rotation.x.value, rotation.y.value, rotation.z.value);
}

} // namespace Units

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
    // Qt と glm はともに column-major。要素アクセスで列順にコピーする。
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            q.data()[column * 4 + row] = m[column][row];
        }
    }
    return q;
}

[[nodiscard]] inline mat4 toGlmMat4(const QMatrix4x4& q) noexcept
{
    mat4 m;
    std::copy(q.constData(), q.constData() + 16, &m[0].x);
    return m;
}

} // namespace ArtifactCore

namespace {

template<class Left, class Right>
concept HasSubtraction = requires(Left left, Right right) {
    left - right;
};

template<class Left, class Right>
concept HasAddition = requires(Left left, Right right) {
    left + right;
};

template<class Left, class Right>
concept HasImplicitConversion = std::is_convertible_v<Left, Right>;

using ArtifactCore::Coordinates::CompositionPoint2;
using ArtifactCore::Coordinates::CompositionBounds2;
using ArtifactCore::Coordinates::CompositionExtent2;
using ArtifactCore::Coordinates::CompositionVector2;
using ArtifactCore::Coordinates::LayerLocalPoint2;
using ArtifactCore::Coordinates::LayerLocalPoint3;
using ArtifactCore::Coordinates::LayerParentPoint3;
using ArtifactCore::Coordinates::ScreenLogicalPoint2;
using ArtifactCore::Coordinates::ScreenLogicalVector2;
using ArtifactCore::Coordinates::ScreenLogicalExtent2;
using ArtifactCore::Coordinates::ScreenPhysicalPoint2;
using ArtifactCore::Coordinates::ScreenPhysicalBounds2;
using ArtifactCore::Coordinates::ScreenPhysicalExtent2;
using ArtifactCore::Coordinates::SourcePixelBounds2;
using ArtifactCore::Coordinates::SourcePixelExtent2;
using ArtifactCore::Coordinates::WorldVector3;
using ArtifactCore::Coordinates::WorldPoint3;
using LayerLocalVector3 =
    ArtifactCore::Coordinates::Vector3<ArtifactCore::Coordinates::LayerLocalSpace>;
using ArtifactCore::Units::Degrees;
using ArtifactCore::Units::FStop;
using ArtifactCore::Units::Meters;
using ArtifactCore::Units::LayerLocalLength;
using ArtifactCore::Units::Millimeters;
using ArtifactCore::Units::OpacityDelta;
using ArtifactCore::Units::Percent;
using ArtifactCore::Units::Pixels;
using ArtifactCore::Units::Radians;
using ArtifactCore::Units::Scale2;
using ArtifactCore::Units::Scale3;
using ArtifactCore::Units::ScaleFactor;
using ArtifactCore::Units::WorldLength;

static_assert(HasSubtraction<CompositionPoint2, CompositionPoint2>);
static_assert(!HasSubtraction<ScreenLogicalPoint2, ScreenPhysicalPoint2>);
static_assert(!HasSubtraction<CompositionPoint2, LayerLocalPoint2>);
static_assert(!HasSubtraction<CompositionPoint2,
                              ArtifactCore::Coordinates::LayerParentPoint2>);
static_assert(!HasSubtraction<CompositionPoint2, WorldPoint3>);
static_assert(!HasSubtraction<WorldPoint3, CompositionPoint2>);
static_assert(!HasSubtraction<WorldPoint3, LayerLocalPoint3>);
static_assert(!HasSubtraction<WorldPoint3, LayerParentPoint3>);
static_assert(HasSubtraction<WorldPoint3, WorldPoint3>);
static_assert(HasAddition<WorldPoint3, WorldVector3>);
static_assert(!HasAddition<WorldPoint3, LayerLocalVector3>);
static_assert(!HasSubtraction<WorldPoint3, LayerLocalVector3>);
static_assert(HasAddition<CompositionPoint2, CompositionVector2>);
static_assert(!HasAddition<CompositionPoint2, LayerLocalPoint2>);
static_assert(!HasAddition<CompositionPoint2,
                           ArtifactCore::Coordinates::LayerParentVector2>);
static_assert(!HasAddition<CompositionPoint2, WorldVector3>);
static_assert(!HasAddition<WorldPoint3, CompositionVector2>);
static_assert(!HasAddition<CompositionPoint2, CompositionPoint2>);
static_assert(!HasAddition<WorldPoint3, WorldPoint3>);
static_assert(!HasAddition<WorldPoint3, LayerLocalPoint3>);
static_assert(HasAddition<ScreenLogicalVector2, ScreenLogicalVector2>);
static_assert(!HasAddition<WorldVector3, LayerLocalVector3>);
static_assert(!HasSubtraction<WorldVector3, LayerLocalVector3>);
static_assert(HasAddition<Degrees, Degrees>);
static_assert(!HasAddition<Degrees, Radians>);
static_assert(!HasImplicitConversion<Radians, Degrees>);
static_assert(!HasImplicitConversion<Degrees, Radians>);
static_assert(!HasImplicitConversion<Millimeters, Degrees>);
static_assert(!HasImplicitConversion<Degrees, Millimeters>);
static_assert(!HasImplicitConversion<Meters, Millimeters>);
static_assert(!HasImplicitConversion<Millimeters, Meters>);
static_assert(!HasImplicitConversion<Meters, Degrees>);
static_assert(!HasImplicitConversion<Degrees, Meters>);
static_assert(!HasImplicitConversion<Meters, Radians>);
static_assert(!HasImplicitConversion<Radians, Meters>);
static_assert(!HasImplicitConversion<Meters, Pixels>);
static_assert(!HasImplicitConversion<Pixels, Meters>);
static_assert(!HasImplicitConversion<Millimeters, Radians>);
static_assert(!HasImplicitConversion<Radians, Millimeters>);
static_assert(!HasImplicitConversion<Millimeters, Pixels>);
static_assert(!HasImplicitConversion<Pixels, Millimeters>);
static_assert(!HasImplicitConversion<Meters, float>);
static_assert(!HasImplicitConversion<float, Meters>);
static_assert(!HasImplicitConversion<Millimeters, float>);
static_assert(!HasImplicitConversion<float, Millimeters>);
static_assert(!HasImplicitConversion<Pixels, Degrees>);
static_assert(!HasImplicitConversion<Percent, Pixels>);
static_assert(!HasImplicitConversion<Pixels, Percent>);
static_assert(!HasImplicitConversion<Percent, ScaleFactor>);
static_assert(!HasImplicitConversion<ScaleFactor, Percent>);
static_assert(!HasImplicitConversion<Percent, Degrees>);
static_assert(!HasImplicitConversion<Degrees, Percent>);
static_assert(!HasImplicitConversion<Percent, Meters>);
static_assert(!HasImplicitConversion<Meters, Percent>);
static_assert(!HasImplicitConversion<Percent, Millimeters>);
static_assert(!HasImplicitConversion<Millimeters, Percent>);
static_assert(!HasImplicitConversion<Percent, WorldLength>);
static_assert(!HasImplicitConversion<WorldLength, Percent>);
static_assert(!HasImplicitConversion<Percent, OpacityDelta>);
static_assert(!HasImplicitConversion<OpacityDelta, Percent>);
static_assert(!HasImplicitConversion<Percent, float>);
static_assert(!HasImplicitConversion<float, Percent>);
static_assert(!HasImplicitConversion<Degrees, Pixels>);
static_assert(!HasImplicitConversion<Pixels, Radians>);
static_assert(!HasImplicitConversion<Radians, Pixels>);
static_assert(!HasImplicitConversion<WorldLength, ScaleFactor>);
static_assert(!HasImplicitConversion<ScaleFactor, WorldLength>);
static_assert(!HasImplicitConversion<LayerLocalLength, WorldLength>);
static_assert(!HasImplicitConversion<WorldLength, LayerLocalLength>);
static_assert(!HasImplicitConversion<LayerLocalLength, Pixels>);
static_assert(!HasImplicitConversion<Pixels, LayerLocalLength>);
static_assert(!HasImplicitConversion<LayerLocalLength, ScaleFactor>);
static_assert(!HasImplicitConversion<ScaleFactor, LayerLocalLength>);
static_assert(!HasImplicitConversion<LayerLocalLength, Degrees>);
static_assert(!HasImplicitConversion<Degrees, LayerLocalLength>);
static_assert(!HasImplicitConversion<LayerLocalLength,
                                     ArtifactCore::Coordinates::LayerLocalPoint2>);
static_assert(!HasImplicitConversion<ArtifactCore::Coordinates::LayerLocalPoint2,
                                     LayerLocalLength>);
static_assert(!HasImplicitConversion<LayerLocalLength,
                                     ArtifactCore::Coordinates::LayerLocalVector2>);
static_assert(!HasImplicitConversion<ArtifactCore::Coordinates::LayerLocalVector2,
                                     LayerLocalLength>);
static_assert(!HasImplicitConversion<LayerLocalLength, float>);
static_assert(!HasImplicitConversion<float, LayerLocalLength>);
static_assert(!HasImplicitConversion<OpacityDelta, LayerLocalLength>);
static_assert(!HasImplicitConversion<LayerLocalLength, OpacityDelta>);
static_assert(!HasImplicitConversion<OpacityDelta, ScaleFactor>);
static_assert(!HasImplicitConversion<ScaleFactor, OpacityDelta>);
static_assert(!HasImplicitConversion<OpacityDelta, Scale2>);
static_assert(!HasImplicitConversion<Scale2, OpacityDelta>);
static_assert(!HasImplicitConversion<OpacityDelta, Degrees>);
static_assert(!HasImplicitConversion<Degrees, OpacityDelta>);
static_assert(!HasImplicitConversion<OpacityDelta, float>);
static_assert(!HasImplicitConversion<float, OpacityDelta>);
static_assert(!HasImplicitConversion<FStop, ScaleFactor>);
static_assert(!HasImplicitConversion<ScaleFactor, FStop>);
static_assert(!HasImplicitConversion<FStop, Pixels>);
static_assert(!HasImplicitConversion<Pixels, FStop>);
static_assert(!HasImplicitConversion<FStop, Millimeters>);
static_assert(!HasImplicitConversion<Millimeters, FStop>);
static_assert(!HasImplicitConversion<FStop, WorldLength>);
static_assert(!HasImplicitConversion<WorldLength, FStop>);
static_assert(!HasImplicitConversion<FStop, Degrees>);
static_assert(!HasImplicitConversion<Degrees, FStop>);
static_assert(!HasImplicitConversion<FStop, Radians>);
static_assert(!HasImplicitConversion<Radians, FStop>);
static_assert(!HasImplicitConversion<FStop, float>);
static_assert(!HasImplicitConversion<float, FStop>);
static_assert(!HasImplicitConversion<WorldLength, Meters>);
static_assert(!HasImplicitConversion<Meters, WorldLength>);
static_assert(!HasImplicitConversion<WorldLength, Pixels>);
static_assert(!HasImplicitConversion<Pixels, WorldLength>);
static_assert(!HasImplicitConversion<WorldLength, Millimeters>);
static_assert(!HasImplicitConversion<Millimeters, WorldLength>);
static_assert(!HasImplicitConversion<WorldLength, Degrees>);
static_assert(!HasImplicitConversion<Degrees, WorldLength>);
static_assert(!HasImplicitConversion<WorldLength, Radians>);
static_assert(!HasImplicitConversion<Radians, WorldLength>);
static_assert(!HasImplicitConversion<WorldLength,
                                     ArtifactCore::Coordinates::WorldPoint3>);
static_assert(!HasImplicitConversion<ArtifactCore::Coordinates::WorldPoint3,
                                     WorldLength>);
static_assert(!HasImplicitConversion<WorldLength,
                                     ArtifactCore::Coordinates::WorldVector3>);
static_assert(!HasImplicitConversion<ArtifactCore::Coordinates::WorldVector3,
                                     WorldLength>);
static_assert(!HasImplicitConversion<ScaleFactor, Degrees>);
static_assert(!HasImplicitConversion<Degrees, ScaleFactor>);
static_assert(!HasImplicitConversion<ScaleFactor, Radians>);
static_assert(!HasImplicitConversion<Radians, ScaleFactor>);
static_assert(!HasImplicitConversion<ScaleFactor, Millimeters>);
static_assert(!HasImplicitConversion<Millimeters, ScaleFactor>);
static_assert(!HasImplicitConversion<ScaleFactor, Meters>);
static_assert(!HasImplicitConversion<Meters, ScaleFactor>);
static_assert(!HasImplicitConversion<ScaleFactor, Pixels>);
static_assert(!HasImplicitConversion<Pixels, ScaleFactor>);
static_assert(!HasImplicitConversion<ScaleFactor, Scale3>);
static_assert(!HasImplicitConversion<Scale3, ScaleFactor>);
static_assert(!HasImplicitConversion<Scale2, ScaleFactor>);
static_assert(!HasImplicitConversion<ScaleFactor, Scale2>);
static_assert(!HasImplicitConversion<Scale2, Scale3>);
static_assert(!HasImplicitConversion<Scale3, Scale2>);
static_assert(!HasImplicitConversion<Scale2, CompositionVector2>);
static_assert(!HasImplicitConversion<CompositionVector2, Scale2>);
static_assert(!HasImplicitConversion<Scale2, LayerLocalPoint2>);
static_assert(!HasImplicitConversion<LayerLocalPoint2, Scale2>);
static_assert(!HasImplicitConversion<Scale2, float>);
static_assert(!HasImplicitConversion<float, Scale2>);
static_assert(!HasImplicitConversion<WorldLength, float>);
static_assert(!HasImplicitConversion<float, WorldLength>);
static_assert(!HasImplicitConversion<ScaleFactor, float>);
static_assert(!HasImplicitConversion<float, ScaleFactor>);
static_assert(!HasImplicitConversion<Pixels, float>);
static_assert(!HasImplicitConversion<float, Pixels>);
static_assert(!HasImplicitConversion<ScreenLogicalPoint2,
                                     ScreenPhysicalPoint2>);
static_assert(!HasImplicitConversion<ScreenLogicalExtent2,
                                     ScreenPhysicalExtent2>);
static_assert(!HasImplicitConversion<CompositionExtent2,
                                     ScreenPhysicalExtent2>);
static_assert(!HasImplicitConversion<ScreenPhysicalExtent2,
                                     CompositionExtent2>);
static_assert(!HasImplicitConversion<CompositionExtent2,
                                     SourcePixelExtent2>);
static_assert(!HasImplicitConversion<SourcePixelExtent2,
                                     ScreenPhysicalExtent2>);
static_assert(!HasImplicitConversion<CompositionPoint2, WorldPoint3>);
static_assert(!HasImplicitConversion<CompositionPoint2, ScreenLogicalPoint2>);
static_assert(!HasImplicitConversion<CompositionBounds2,
                                     ScreenPhysicalBounds2>);
static_assert(!HasImplicitConversion<ScreenPhysicalBounds2,
                                     SourcePixelBounds2>);
static_assert(!HasImplicitConversion<LayerLocalPoint3, WorldPoint3>);
static_assert(!HasImplicitConversion<WorldPoint3, LayerLocalPoint3>);
static_assert(!HasImplicitConversion<LayerParentPoint3, WorldPoint3>);
static_assert(!HasImplicitConversion<WorldPoint3, LayerParentPoint3>);
static_assert(!HasImplicitConversion<WorldVector3, WorldPoint3>);
static_assert(!HasImplicitConversion<Scale3, WorldVector3>);
static_assert(!HasImplicitConversion<WorldVector3, Scale3>);
static_assert(!HasImplicitConversion<Scale3, QVector3D>);
static_assert(!HasImplicitConversion<QVector3D, Scale3>);

} // namespace
