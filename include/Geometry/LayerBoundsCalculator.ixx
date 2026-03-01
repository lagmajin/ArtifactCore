module;

#include <QPointF>
#include <QVector>
#include <QRectF>
#include "../Define/DllExportMacro.hpp"

export module Geometry.LayerBounds;

import std;

export namespace ArtifactCore {

struct LayerCorners {
    QPointF topLeft;
    QPointF topRight;
    QPointF bottomLeft;
    QPointF bottomRight;
};

class LIBRARY_DLL_API LayerBoundsCalculator2D {
public:
    static LayerCorners calculate(
        const QPointF& position, 
        const QPointF& anchor, 
        float width, 
        float height, 
        float rotation, // Degrees
        float scaleX = 1.0f, 
        float scaleY = 1.0f);

    // バウンディングボックス（軸平行）を取得
    static QRectF calculateAABB(const LayerCorners& corners);

    // 特定の座標がこの範囲内に含まれるか（ヒットテスト）
    static bool contains(const LayerCorners& corners, const QPointF& point);
};

} // namespace ArtifactCore
