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
};

} // namespace ArtifactCore
