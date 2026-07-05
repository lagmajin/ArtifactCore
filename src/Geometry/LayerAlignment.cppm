module;
#include <utility>
#include <vector>
#include <QRectF>
#include <QPointF>
#include <algorithm>
#include <numeric>
#include <limits>

module Geometry.LayerAlignment;

namespace ArtifactCore {

void LayerAlignment::align(
    std::vector<AlignmentObject>& objects, 
    AlignType type, 
    AlignmentTarget target, 
    const QRectF& containerBounds,
    int keyObjectId) {

    if (objects.empty()) return;

    QRectF referenceBounds;
    if (target == AlignmentTarget::KeyObject && keyObjectId != -1) {
        auto it = std::find_if(objects.begin(), objects.end(), [keyObjectId](const AlignmentObject& o) {
            return o.id == keyObjectId;
        });
        if (it != objects.end()) {
            referenceBounds = it->bounds;
        } else {
            target = AlignmentTarget::Selection; // 見つからなければ選択範囲にフォールバック
        }
    }

    if (target == AlignmentTarget::Selection) {
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();

        for (const auto& obj : objects) {
            minX = std::min(minX, (float)obj.bounds.left());
            minY = std::min(minY, (float)obj.bounds.top());
            maxX = std::max(maxX, (float)obj.bounds.right());
            maxY = std::max(maxY, (float)obj.bounds.bottom());
        }
        referenceBounds = QRectF(minX, minY, maxX - minX, maxY - minY);
    } else if (target == AlignmentTarget::Composition) {
        referenceBounds = containerBounds;
    }

    for (auto& obj : objects) {
        // キーオブジェクト自体は移動させない
        if (target == AlignmentTarget::KeyObject && obj.id == keyObjectId) continue;

        float deltaX = 0;
        float deltaY = 0;

        switch (type) {
            case AlignType::Left:
                deltaX = referenceBounds.left() - obj.bounds.left();
                break;
            case AlignType::CenterHorizontal:
                deltaX = referenceBounds.center().x() - obj.bounds.center().x();
                break;
            case AlignType::Right:
                deltaX = referenceBounds.right() - obj.bounds.right();
                break;
            case AlignType::Top:
                deltaY = referenceBounds.top() - obj.bounds.top();
                break;
            case AlignType::CenterVertical:
                deltaY = referenceBounds.center().y() - obj.bounds.center().y();
                break;
            case AlignType::Bottom:
                deltaY = referenceBounds.bottom() - obj.bounds.bottom();
                break;
        }

        obj.currentPosition.setX(obj.currentPosition.x() + deltaX);
        obj.currentPosition.setY(obj.currentPosition.y() + deltaY);
        obj.bounds.translate(deltaX, deltaY);
    }
}

void LayerAlignment::distribute(std::vector<AlignmentObject>& objects, DistributeType type) {
    if (objects.size() < 3) return;

    auto sortAxis = [&](bool horizontal) {
        std::sort(objects.begin(), objects.end(), [horizontal](const AlignmentObject& a, const AlignmentObject& b) {
            return horizontal ? a.bounds.center().x() < b.bounds.center().x() : 
                                a.bounds.center().y() < b.bounds.center().y();
        });
    };

    int n = objects.size();
    bool isHorizontal = (type == DistributeType::Left || type == DistributeType::CenterHorizontal || type == DistributeType::Right);
    sortAxis(isHorizontal);

    auto getPos = [&](const AlignmentObject& obj) {
        if (isHorizontal) {
            return (type == DistributeType::Left) ? (float)obj.bounds.left() : 
                   (type == DistributeType::Right) ? (float)obj.bounds.right() : 
                   (float)obj.bounds.center().x();
        } else {
            return (type == DistributeType::Top) ? (float)obj.bounds.top() : 
                   (type == DistributeType::Bottom) ? (float)obj.bounds.bottom() : 
                   (float)obj.bounds.center().y();
        }
    };

    float startPos = getPos(objects.front());
    float endPos = getPos(objects.back());
    float step = (endPos - startPos) / (n - 1);

    for (int i = 1; i < n - 1; ++i) {
        float targetPos = startPos + i * step;
        float currentPos = getPos(objects[i]);
        float delta = targetPos - currentPos;

        if (isHorizontal) {
            objects[i].currentPosition.setX(objects[i].currentPosition.x() + delta);
            objects[i].bounds.translate(delta, 0);
        } else {
            objects[i].currentPosition.setY(objects[i].currentPosition.y() + delta);
            objects[i].bounds.translate(0, delta);
        }
    }
}

void LayerAlignment::distributeSpacing(
    std::vector<AlignmentObject>& objects, 
    DistributeType type, 
    AlignmentTarget target,
    const QRectF& containerBounds,
    float specifiedSpacing) {

    if (objects.size() < 2) return;

    bool isHorizontal = (type == DistributeType::Left || type == DistributeType::CenterHorizontal || type == DistributeType::Right);
    
    // ソート
    std::sort(objects.begin(), objects.end(), [isHorizontal](const AlignmentObject& a, const AlignmentObject& b) {
        return isHorizontal ? a.bounds.left() < b.bounds.left() : a.bounds.top() < b.bounds.top();
    });

    float totalObjectSize = 0;
    for (const auto& obj : objects) {
        totalObjectSize += isHorizontal ? (float)obj.bounds.width() : (float)obj.bounds.height();
    }

    float gap = 0;
    float currentPos = 0;

    if (specifiedSpacing >= 0) {
        gap = specifiedSpacing;
        currentPos = isHorizontal ? objects.front().bounds.left() : objects.front().bounds.top();
    } else {
        // 全体の幅からオブジェクトの合計サイズを引いて隙間を計算
        float totalSpan = 0;
        if (target == AlignmentTarget::Composition) {
            totalSpan = isHorizontal ? (float)containerBounds.width() : (float)containerBounds.height();
            currentPos = isHorizontal ? (float)containerBounds.left() : (float)containerBounds.top();
        } else {
            float start = isHorizontal ? (float)objects.front().bounds.left() : (float)objects.front().bounds.top();
            float end = isHorizontal ? (float)objects.back().bounds.right() : (float)objects.back().bounds.bottom();
            totalSpan = end - start;
            currentPos = start;
        }
        gap = (totalSpan - totalObjectSize) / (objects.size() - 1);
    }

    for (size_t i = 0; i < objects.size(); ++i) {
        float delta = 0;
        if (isHorizontal) {
            delta = currentPos - objects[i].bounds.left();
            objects[i].currentPosition.setX(objects[i].currentPosition.x() + delta);
            objects[i].bounds.translate(delta, 0);
            currentPos += objects[i].bounds.width() + gap;
        } else {
            delta = currentPos - objects[i].bounds.top();
            objects[i].currentPosition.setY(objects[i].currentPosition.y() + delta);
            objects[i].bounds.translate(0, delta);
            currentPos += objects[i].bounds.height() + gap;
        }
    }
}

} // namespace ArtifactCore
