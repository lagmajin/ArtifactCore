module;
#include <utility>
#include <vector>
#include <QRectF>
#include <QPointF>

export module Geometry.LayerAlignment;

import Geometry.LayerBounds;

namespace ArtifactCore {

export enum class AlignType {
    Left,
    CenterHorizontal,
    Right,
    Top,
    CenterVertical,
    Bottom
};

export enum class DistributeType {
    Left,
    CenterHorizontal,
    Right,
    Top,
    CenterVertical,
    Bottom
};

export enum class AlignmentTarget {
    Selection,
    Composition,
    KeyObject
};

export struct AlignmentObject {
    int id;
    QRectF bounds; // AABB (Axis-Aligned Bounding Box)
    QPointF currentPosition;
};

export enum class LayoutTargetRole {
    Generic,
    Face,
    Logo,
    Subtitle,
    Button,
    SafeArea
};

export struct LayoutCollisionObject {
    int id = -1;
    QRectF bounds;
    QPointF currentPosition;
    LayoutTargetRole role = LayoutTargetRole::Generic;
    int priority = 0;
    bool movable = true;
    bool scalable = true;
    double scale = 1.0;
};

export struct LayoutCollisionResult {
    int movedCount = 0;
    int scaledCount = 0;
    int unresolvedCount = 0;
};

export class LayerAlignment {
public:
    static void align(
        std::vector<AlignmentObject>& objects, 
        AlignType type, 
        AlignmentTarget target, 
        const QRectF& containerBounds,
        int keyObjectId = -1);

    static void distribute(
        std::vector<AlignmentObject>& objects, 
        DistributeType type);

    static void distributeSpacing(
        std::vector<AlignmentObject>& objects,
        DistributeType type,
        AlignmentTarget target,
        const QRectF& containerBounds,
        float specifiedSpacing = -1.0f);

    // Deterministic screen-space collision avoidance. Objects with larger
    // priority remain fixed while lower-priority objects are moved away.
    static LayoutCollisionResult resolveCollisions(
        std::vector<LayoutCollisionObject>& objects,
        const QRectF& safeArea,
        int maxPasses = 4);
};

} // namespace ArtifactCore
