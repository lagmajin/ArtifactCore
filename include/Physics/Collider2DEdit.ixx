module;
#include <algorithm>
#include <cmath>

#include <QPointF>
#include <QRectF>

export module Physics.Collider2DEdit;

export namespace ArtifactCore {

// Matches persisted component.collision.shape values. This DTO is independent
// from Box2D and renderer state so it can safely represent an authored edit.
enum class Collider2DShape : int { AutoBounds = 0, Box = 1, Circle = 2, Polygon = 3 };

enum class Collider2DEditHandle : unsigned char {
  None, Offset, BoxLeft, BoxTop, BoxRight, BoxBottom, BoxTopLeft,
  BoxTopRight, BoxBottomRight, BoxBottomLeft, CircleRadius,
};

// Cold-path editor state. The fixed-step simulation owns a separate runtime
// collider and must not retain this object.
struct Collider2DEditState {
  bool enabled = false;
  Collider2DShape shape = Collider2DShape::AutoBounds;
  QRectF sourceBounds;
  float width = 0.0f;
  float height = 0.0f;
  float radius = 0.0f;
  QPointF offset;
  int sourceOutlinePointCount = 0;
  int rigidBodyPreviewPointCount = 0;

  bool supportsDirectSizeEditing() const noexcept {
    return shape == Collider2DShape::Box || shape == Collider2DShape::Circle;
  }
  bool isPolygonPreviewOnly() const noexcept { return shape == Collider2DShape::Polygon; }
  float resolvedWidth() const noexcept {
    const float fallback = static_cast<float>(std::max(0.0, sourceBounds.width()));
    return std::max(0.0f, std::isfinite(width) && width > 0.0f ? width : fallback);
  }
  float resolvedHeight() const noexcept {
    const float fallback = static_cast<float>(std::max(0.0, sourceBounds.height()));
    return std::max(0.0f, std::isfinite(height) && height > 0.0f ? height : fallback);
  }
  float resolvedRadius() const noexcept {
    const float fallback = 0.5f * std::min(resolvedWidth(), resolvedHeight());
    return std::max(0.0f, std::isfinite(radius) && radius > 0.0f ? radius : fallback);
  }
  QPointF resolvedCenter() const noexcept { return sourceBounds.center() + offset; }
  QRectF resolvedBoxBounds() const noexcept {
    const QPointF center = resolvedCenter();
    const float resolvedW = resolvedWidth();
    const float resolvedH = resolvedHeight();
    return QRectF(center.x() - resolvedW * 0.5, center.y() - resolvedH * 0.5,
                  resolvedW, resolvedH);
  }
};

inline Collider2DShape collider2DShapeFromPersistedValue(int value) noexcept {
  return static_cast<Collider2DShape>(std::clamp(value, 0, 3));
}
inline int collider2DShapeToPersistedValue(Collider2DShape shape) noexcept {
  return static_cast<int>(shape);
}

}  // namespace ArtifactCore
