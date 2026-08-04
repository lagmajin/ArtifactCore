module;
#include <QRectF>
#include <QSize>
#include <QPointF>
#include <QTransform>
#include <cmath>
#include <algorithm>

module Geometry.ResolutionRemap;

namespace ArtifactCore {

static double safeAspect(const QSize& s) {
    return (s.height() > 0) ? static_cast<double>(s.width()) / s.height() : 1.0;
}

static double scaleX(const QSize& oldSize, const QSize& newSize) {
    return static_cast<double>(newSize.width()) / std::max(1, oldSize.width());
}

static double scaleY(const QSize& oldSize, const QSize& newSize) {
    return static_cast<double>(newSize.height()) / std::max(1, oldSize.height());
}

QPointF ResolutionRemap::remapPosition(
    const QPointF& pos,
    const QSize& oldSize,
    const QSize& newSize,
    RemapPolicy policy)
{
    if (oldSize.isEmpty() || newSize.isEmpty() || oldSize == newSize) {
        return pos;
    }

    switch (policy) {
    case RemapPolicy::TopLeftLocked: {
        return QPointF(pos.x() * scaleX(oldSize, newSize),
                       pos.y() * scaleY(oldSize, newSize));
    }
    case RemapPolicy::CenterLocked: {
        const double cx = oldSize.width()  * 0.5;
        const double cy = oldSize.height() * 0.5;
        const double nx = newSize.width()  * 0.5;
        const double ny = newSize.height() * 0.5;
        const double dx = pos.x() - cx;
        const double dy = pos.y() - cy;
        return QPointF(nx + dx * scaleX(oldSize, newSize),
                       ny + dy * scaleY(oldSize, newSize));
    }
    case RemapPolicy::StretchToFit: {
        return QPointF(pos.x() * scaleX(oldSize, newSize),
                       pos.y() * scaleY(oldSize, newSize));
    }
    case RemapPolicy::FitWithPadding: {
        const double oldAspect = safeAspect(oldSize);
        const double newAspect = safeAspect(newSize);
        double sx = scaleX(oldSize, newSize);
        double sy = scaleY(oldSize, newSize);
        if (newAspect > oldAspect) {
            sx = sy;
        } else {
            sy = sx;
        }
        const double padX = (newSize.width()  - oldSize.width()  * sx) * 0.5;
        const double padY = (newSize.height() - oldSize.height() * sy) * 0.5;
        return QPointF(pos.x() * sx + padX, pos.y() * sy + padY);
    }
    case RemapPolicy::FitWithCrop: {
        const double oldAspect = safeAspect(oldSize);
        const double newAspect = safeAspect(newSize);
        double sx = scaleX(oldSize, newSize);
        double sy = scaleY(oldSize, newSize);
        if (newAspect > oldAspect) {
            sy = sx;
        } else {
            sx = sy;
        }
        const double cropX = (oldSize.width()  * sx - newSize.width())  * 0.5;
        const double cropY = (oldSize.height() * sy - newSize.height()) * 0.5;
        return QPointF(pos.x() * sx - cropX, pos.y() * sy - cropY);
    }
    }
    return pos;
}

QRectF ResolutionRemap::remapBounds(
    const QRectF& bounds,
    const QSize& oldSize,
    const QSize& newSize,
    RemapPolicy policy)
{
    const QPointF tl = remapPosition(bounds.topLeft(), oldSize, newSize, policy);
    const QPointF br = remapPosition(bounds.bottomRight(), oldSize, newSize, policy);
    return QRectF(tl, br).normalized();
}

double ResolutionRemap::remapScale(
    double oldValue,
    const QSize& oldSize,
    const QSize& newSize,
    RemapPolicy policy)
{
    if (policy == RemapPolicy::FitWithPadding || policy == RemapPolicy::FitWithCrop) {
        const double oldAspect = safeAspect(oldSize);
        const double newAspect = safeAspect(newSize);
        double s = scaleX(oldSize, newSize);
        if (policy == RemapPolicy::FitWithPadding) {
            s = (newAspect > oldAspect) ? scaleY(oldSize, newSize) : s;
        } else {
            s = (newAspect > oldAspect) ? s : scaleY(oldSize, newSize);
        }
        return oldValue * s;
    }
    const double sx = scaleX(oldSize, newSize);
    const double sy = scaleY(oldSize, newSize);
    return oldValue * std::sqrt(sx * sy);
}

RemapImpact ResolutionRemap::calculateImpact(
    const QSize& oldSize,
    const QSize& newSize,
    bool hasMasks,
    bool hasKeyframes,
    bool hasAnchors)
{
    RemapImpact impact;
    impact.oldAspectRatio = safeAspect(oldSize);
    impact.newAspectRatio = safeAspect(newSize);
    impact.hasMaskPaths    = hasMasks;
    impact.hasAnchorPoints = hasAnchors;
    impact.maskVertexCount  = hasMasks    ? 1 : 0;
    impact.keyframeCount    = hasKeyframes ? 1 : 0;
    return impact;
}

QTransform ResolutionRemap::remapTransform(
    const QSize& oldSize,
    const QSize& newSize,
    RemapPolicy policy)
{
    if (oldSize.isEmpty() || newSize.isEmpty()) {
        return QTransform();
    }
    switch (policy) {
    case RemapPolicy::CenterLocked: {
        const double cx = oldSize.width()  * 0.5;
        const double cy = oldSize.height() * 0.5;
        const double nx = newSize.width()  * 0.5;
        const double ny = newSize.height() * 0.5;
        QTransform t;
        t.translate(nx, ny);
        t.scale(scaleX(oldSize, newSize), scaleY(oldSize, newSize));
        t.translate(-cx, -cy);
        return t;
    }
    case RemapPolicy::TopLeftLocked:
    case RemapPolicy::StretchToFit: {
        QTransform t;
        t.scale(scaleX(oldSize, newSize), scaleY(oldSize, newSize));
        return t;
    }
    case RemapPolicy::FitWithPadding:
    case RemapPolicy::FitWithCrop: {
        const double oldAspect = safeAspect(oldSize);
        const double newAspect = safeAspect(newSize);
        double sx, sy;
        if (policy == RemapPolicy::FitWithPadding) {
            sx = sy = (newAspect > oldAspect) ? scaleY(oldSize, newSize) : scaleX(oldSize, newSize);
        } else {
            sx = sy = (newAspect > oldAspect) ? scaleX(oldSize, newSize) : scaleY(oldSize, newSize);
        }
        const double ox = (newSize.width()  - oldSize.width()  * sx) * 0.5;
        const double oy = (newSize.height() - oldSize.height() * sy) * 0.5;
        QTransform t;
        t.translate(ox, oy);
        t.scale(sx, sy);
        return t;
    }
    }
    return QTransform();
}

}
