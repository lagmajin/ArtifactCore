module;
#include <algorithm>
#include <cmath>
#include <limits>

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

  bool tryHandlePosition(Collider2DEditHandle handle,
                         QPointF& position) const noexcept {
    if (!enabled || !supportsDirectSizeEditing()) return false;
    const auto assignIfFinite = [&](const QPointF& candidate) {
      if (!std::isfinite(candidate.x()) || !std::isfinite(candidate.y())) {
        return false;
      }
      position = candidate;
      return true;
    };
    const QPointF center = resolvedCenter();
    if (handle == Collider2DEditHandle::Offset) {
      return assignIfFinite(center);
    }
    if (shape == Collider2DShape::Circle) {
      if (handle != Collider2DEditHandle::CircleRadius) return false;
      return assignIfFinite(center + QPointF(resolvedRadius(), 0.0));
    }

    const QRectF bounds = resolvedBoxBounds();
    switch (handle) {
      case Collider2DEditHandle::BoxLeft:
        return assignIfFinite(QPointF(bounds.left(), center.y()));
      case Collider2DEditHandle::BoxTop:
        return assignIfFinite(QPointF(center.x(), bounds.top()));
      case Collider2DEditHandle::BoxRight:
        return assignIfFinite(QPointF(bounds.right(), center.y()));
      case Collider2DEditHandle::BoxBottom:
        return assignIfFinite(QPointF(center.x(), bounds.bottom()));
      case Collider2DEditHandle::BoxTopLeft:
        return assignIfFinite(bounds.topLeft());
      case Collider2DEditHandle::BoxTopRight:
        return assignIfFinite(bounds.topRight());
      case Collider2DEditHandle::BoxBottomRight:
        return assignIfFinite(bounds.bottomRight());
      case Collider2DEditHandle::BoxBottomLeft:
        return assignIfFinite(bounds.bottomLeft());
      default:
        return false;
    }
  }

  Collider2DEditHandle hitTestHandle(const QPointF& localPoint,
                                     double tolerance) const noexcept {
    if (!enabled || !supportsDirectSizeEditing() ||
        !std::isfinite(localPoint.x()) || !std::isfinite(localPoint.y()) ||
        !std::isfinite(tolerance) || tolerance < 0.0) {
      return Collider2DEditHandle::None;
    }
    const double toleranceSquared = tolerance * tolerance;
    if (!std::isfinite(toleranceSquared)) {
      return Collider2DEditHandle::None;
    }
    double bestDistanceSquared = toleranceSquared;
    Collider2DEditHandle best = Collider2DEditHandle::None;
    const auto consider = [&](Collider2DEditHandle handle) {
      QPointF position;
      if (!tryHandlePosition(handle, position)) return;
      const double dx = localPoint.x() - position.x();
      const double dy = localPoint.y() - position.y();
      const double distanceSquared = dx * dx + dy * dy;
      if (std::isfinite(distanceSquared) &&
          distanceSquared <= bestDistanceSquared) {
        bestDistanceSquared = distanceSquared;
        best = handle;
      }
    };
    consider(Collider2DEditHandle::Offset);
    if (shape == Collider2DShape::Circle) {
      consider(Collider2DEditHandle::CircleRadius);
      return best;
    }
    consider(Collider2DEditHandle::BoxLeft);
    consider(Collider2DEditHandle::BoxTop);
    consider(Collider2DEditHandle::BoxRight);
    consider(Collider2DEditHandle::BoxBottom);
    consider(Collider2DEditHandle::BoxTopLeft);
    consider(Collider2DEditHandle::BoxTopRight);
    consider(Collider2DEditHandle::BoxBottomRight);
    consider(Collider2DEditHandle::BoxBottomLeft);
    return best;
  }

  bool applyDrag(Collider2DEditHandle handle,
                 const QPointF& localDelta) noexcept {
    if (!enabled || !supportsDirectSizeEditing() ||
        !std::isfinite(localDelta.x()) || !std::isfinite(localDelta.y())) {
      return false;
    }
    if (handle == Collider2DEditHandle::Offset) {
      const QPointF nextOffset = offset + localDelta;
      if (!std::isfinite(nextOffset.x()) || !std::isfinite(nextOffset.y())) {
        return false;
      }
      offset = nextOffset;
      return true;
    }
    if (shape == Collider2DShape::Circle) {
      if (handle != Collider2DEditHandle::CircleRadius) return false;
      const double nextRadius = std::max(
          0.001, static_cast<double>(resolvedRadius()) + localDelta.x());
      if (!std::isfinite(nextRadius) ||
          nextRadius > std::numeric_limits<float>::max()) {
        return false;
      }
      radius = static_cast<float>(nextRadius);
      return true;
    }

    const QRectF bounds = resolvedBoxBounds();
    double left = bounds.left();
    double right = bounds.right();
    double top = bounds.top();
    double bottom = bounds.bottom();
    constexpr double minimumExtent = 0.001;
    switch (handle) {
      case Collider2DEditHandle::BoxLeft:
      case Collider2DEditHandle::BoxTopLeft:
      case Collider2DEditHandle::BoxBottomLeft:
        left = std::min(left + localDelta.x(), right - minimumExtent);
        break;
      case Collider2DEditHandle::BoxRight:
      case Collider2DEditHandle::BoxTopRight:
      case Collider2DEditHandle::BoxBottomRight:
        right = std::max(right + localDelta.x(), left + minimumExtent);
        break;
      default:
        break;
    }
    switch (handle) {
      case Collider2DEditHandle::BoxTop:
      case Collider2DEditHandle::BoxTopLeft:
      case Collider2DEditHandle::BoxTopRight:
        top = std::min(top + localDelta.y(), bottom - minimumExtent);
        break;
      case Collider2DEditHandle::BoxBottom:
      case Collider2DEditHandle::BoxBottomLeft:
      case Collider2DEditHandle::BoxBottomRight:
        bottom = std::max(bottom + localDelta.y(), top + minimumExtent);
        break;
      default:
        break;
    }
    switch (handle) {
      case Collider2DEditHandle::BoxLeft:
      case Collider2DEditHandle::BoxTop:
      case Collider2DEditHandle::BoxRight:
      case Collider2DEditHandle::BoxBottom:
      case Collider2DEditHandle::BoxTopLeft:
      case Collider2DEditHandle::BoxTopRight:
      case Collider2DEditHandle::BoxBottomRight:
      case Collider2DEditHandle::BoxBottomLeft:
        break;
      default:
        return false;
    }
    const double nextWidth = right - left;
    const double nextHeight = bottom - top;
    const QPointF nextCenter((left + right) * 0.5,
                             (top + bottom) * 0.5);
    const QPointF nextOffset = nextCenter - sourceBounds.center();
    if (!std::isfinite(nextWidth) || !std::isfinite(nextHeight) ||
        nextWidth > std::numeric_limits<float>::max() ||
        nextHeight > std::numeric_limits<float>::max() ||
        !std::isfinite(nextOffset.x()) || !std::isfinite(nextOffset.y())) {
      return false;
    }
    width = static_cast<float>(nextWidth);
    height = static_cast<float>(nextHeight);
    offset = nextOffset;
    return true;
  }
};

inline Collider2DShape collider2DShapeFromPersistedValue(int value) noexcept {
  return static_cast<Collider2DShape>(std::clamp(value, 0, 3));
}
inline int collider2DShapeToPersistedValue(Collider2DShape shape) noexcept {
  return static_cast<int>(shape);
}

}  // namespace ArtifactCore
