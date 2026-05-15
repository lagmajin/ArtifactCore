module;
class tst_QList;

#include <QPointF>
#include <QVector>
#include <cmath>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Math.Bezier;

namespace ArtifactCore {

QPointF BezierCalculator::evaluateQuadratic(const QPointF& p0, const QPointF& p1, const QPointF& p2, float t) {
    float invT = 1.0f - t;
    return invT * invT * p0 + 2.0f * invT * t * p1 + t * t * p2;
}

QPointF BezierCalculator::evaluateCubic(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, float t) {
    float invT = 1.0f - t;
    return invT * invT * invT * p0 + 
           3.0f * invT * invT * t * p1 + 
           3.0f * invT * t * t * p2 + 
           t * t * t * p3;
}

float BezierCalculator::getTangentAngle(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, float t) {
    float invT = 1.0f - t;
    // 3次ベジェの導関数 (速度ベクトル)
    QPointF tangent = 3.0f * invT * invT * (p1 - p0) +
                      6.0f * invT * t * (p2 - p1) +
                      3.0f * t * t * (p3 - p2);
    
    return std::atan2(tangent.y(), tangent.x()) * 180.0f / 3.14159265f;
}

QPointF BezierCalculator::evaluatePath(const QVector<BezierPoint>& points, float t, bool closed) {
    if (points.isEmpty()) return {};
    if (points.size() == 1) return points[0].pos;

    int numSegments = closed ? points.size() : points.size() - 1;
    float scaledT = t * numSegments;
    int index = static_cast<int>(std::floor(scaledT));
    if (index >= numSegments) index = numSegments - 1;
    
    float localT = scaledT - index;

    const auto& pStart = points[index];
    const auto& pEnd = points[(index + 1) % points.size()];

    // Abs coords for evaluation
    QPointF p0 = pStart.pos;
    QPointF p1 = pStart.pos + pStart.handleOut;
    QPointF p2 = pEnd.pos + pEnd.handleIn;
    QPointF p3 = pEnd.pos;

    return evaluateCubic(p0, p1, p2, p3, localT);
}

} // namespace ArtifactCore
