module;
class tst_QList;

#include <QPointF>
#include <QVector>
#include <cmath>
#include <algorithm>

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
module Math.Bezier.Sampler;

import Math.Bezier;

namespace ArtifactCore {

QVector<QPointF> BezierPathSampler::sampleStandard(const QVector<BezierPoint>& points, int subdivisionsPerSegment, bool closed) {
    QVector<QPointF> sampledPoints;
    if (points.size() < 2) return sampledPoints;

    int segmentCount = closed ? points.size() : points.size() - 1;

    for (int i = 0; i < segmentCount; i++) {
        const auto& p0 = points[i];
        const auto& p1 = points[(i + 1) % points.size()];

        for (int j = 0; j < subdivisionsPerSegment; j++) {
            float t = static_cast<float>(j) / subdivisionsPerSegment;
            sampledPoints.push_back(BezierCalculator::evaluateCubic(
                p0.pos, p0.pos + p0.handleOut, p1.pos + p1.handleIn, p1.pos, t));
        }
    }

    if (!closed) {
        sampledPoints.push_back(points.last().pos);
    } else {
        // 重複を避けるか、繋げるか
    }

    return sampledPoints;
}

float BezierPathSampler::calculatePathLength(const QVector<BezierPoint>& points, bool closed) {
    float length = 0.0f;
    if (points.size() < 2) return 0.0f;

    int segmentCount = closed ? points.size() : points.size() - 1;
    int precision = 20;

    for (int i = 0; i < segmentCount; i++) {
        const auto& p0 = points[i];
        const auto& p1 = points[(i + 1) % points.size()];

        QPointF prev = p0.pos;
        for (int j = 1; j <= precision; j++) {
            float t = static_cast<float>(j) / precision;
            QPointF current = BezierCalculator::evaluateCubic(
                p0.pos, p0.pos + p0.handleOut, p1.pos + p1.handleIn, p1.pos, t);
            
            float dx = current.x() - prev.x();
            float dy = current.y() - prev.y();
            length += std::sqrt(dx * dx + dy * dy);
            prev = current;
        }
    }
    return length;
}

QVector<QPointF> BezierPathSampler::sampleEquidistant(const QVector<BezierPoint>& points, float segmentLength, bool closed) {
    QVector<QPointF> sampledPoints;
    float totalLen = calculatePathLength(points, closed);
    if (totalLen <= 0.0f) return sampledPoints;

    int pointCount = static_cast<int>(totalLen / segmentLength);
    if (pointCount < 2) return {points[0].pos, points.last().pos};

    for (int i = 0; i <= pointCount; i++) {
        float normalizedT = static_cast<float>(i) / pointCount;
        sampledPoints.push_back(BezierCalculator::evaluatePath(points, normalizedT, closed));
    }

    return sampledPoints;
}

} // namespace ArtifactCore
