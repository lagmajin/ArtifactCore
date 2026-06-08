module;
#include <QRectF>
#include <QSize>
#include <QPointF>
#include <QTransform>
#include <vector>
#include <cmath>
#include <algorithm>
#include <optional>

export module Geometry.ResolutionRemap;

export namespace ArtifactCore {

export enum class RemapPolicy {
    CenterLocked    = 0,
    TopLeftLocked   = 1,
    StretchToFit    = 2,
    FitWithPadding  = 3,
    FitWithCrop     = 4
};

export struct RemapImpact {
    int maskVertexCount   = 0;
    int keyframeCount     = 0;
    bool hasAnchorPoints  = false;
    bool hasMaskPaths     = false;
    double oldAspectRatio = 1.0;
    double newAspectRatio = 1.0;

    bool hasImpact() const {
        return maskVertexCount > 0 || keyframeCount > 0 || hasAnchorPoints;
    }
};

export class ResolutionRemap {
public:
    static QPointF remapPosition(
        const QPointF& pos,
        const QSize& oldSize,
        const QSize& newSize,
        RemapPolicy policy);

    static QRectF remapBounds(
        const QRectF& bounds,
        const QSize& oldSize,
        const QSize& newSize,
        RemapPolicy policy);

    static double remapScale(
        double oldValue,
        const QSize& oldSize,
        const QSize& newSize,
        RemapPolicy policy);

    static RemapImpact calculateImpact(
        const QSize& oldSize,
        const QSize& newSize,
        bool hasMasks,
        bool hasKeyframes,
        bool hasAnchors);

    static QTransform remapTransform(
        const QSize& oldSize,
        const QSize& newSize,
        RemapPolicy policy);
};

}
