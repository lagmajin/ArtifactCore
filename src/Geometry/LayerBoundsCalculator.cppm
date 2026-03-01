module;

#include <QPointF>
#include <QMatrix4x4> // Qt6 or Qt5 
#include <QVector2D>
#include <QRectF>
#include <cmath>
#include <algorithm>

module Geometry.LayerBounds;

import std;

namespace ArtifactCore {

LayerCorners LayerBoundsCalculator2D::calculate(
    const QPointF& pos, 
    const QPointF& anchor, 
    float w, 
    float h, 
    float rot, 
    float sx, 
    float sy) {

    // 各頂点のローカル座標（アンカーポイント考慮）
    QPointF p1(-anchor.x(), -anchor.y());
    QPointF p2(w - anchor.x(), -anchor.y());
    QPointF p3(-anchor.x(), h - anchor.y());
    QPointF p4(w - anchor.x(), h - anchor.y());

    auto rotate = [&](const QPointF& p) {
        float rad = rot * M_PI / 180.0f;
        float cosA = std::cos(rad);
        float sinA = std::sin(rad);

        // Scale
        float x = p.x() * sx;
        float y = p.y() * sy;

        // Rotate & Translate
        return QPointF(
            x * cosA - y * sinA + pos.x(),
            x * sinA + y * cosA + pos.y()
        );
    };

    LayerCorners corners;
    corners.topLeft = rotate(p1);
    corners.topRight = rotate(p2);
    corners.bottomLeft = rotate(p3);
    corners.bottomRight = rotate(p4);

    return corners;
}

QRectF LayerBoundsCalculator2D::calculateAABB(const LayerCorners& corners) {
    float minX = std::min({corners.topLeft.x(), corners.topRight.x(), corners.bottomLeft.x(), corners.bottomRight.x()});
    float maxX = std::max({corners.topLeft.x(), corners.topRight.x(), corners.bottomLeft.x(), corners.bottomRight.x()});
    float minY = std::min({corners.topLeft.y(), corners.topRight.y(), corners.bottomLeft.y(), corners.bottomRight.y()});
    float maxY = std::max({corners.topLeft.y(), corners.topRight.y(), corners.bottomLeft.y(), corners.bottomRight.y()});

    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

bool LayerBoundsCalculator2D::contains(const LayerCorners& corners, const QPointF& p) {
    // 凸四角形の包含判定（外積符号を利用）
    auto crossProduct = [](const QPointF& a, const QPointF& b, const QPointF& c) {
        return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
    };

    float c1 = crossProduct(corners.topLeft, corners.topRight, p);
    float c2 = crossProduct(corners.topRight, corners.bottomRight, p);
    float c3 = crossProduct(corners.bottomRight, corners.bottomLeft, p);
    float c4 = crossProduct(corners.bottomLeft, corners.topLeft, p);

    return (c1 >= 0 && c2 >= 0 && c3 >= 0 && c4 >= 0) || 
           (c1 <= 0 && c2 <= 0 && c3 <= 0 && c4 <= 0);
}

} // namespace ArtifactCore
