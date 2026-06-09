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
        // closed: skip duplicate of first point
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

QVector<QPointF> BezierPathSampler::sampleByCount(const QVector<BezierPoint>& points, int count, bool closed) {
    QVector<QPointF> result;
    if (points.size() < 2 || count < 2) return result;

    const float totalLen = calculatePathLength(points, closed);
    if (totalLen <= 0.0f) return result;

    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / (count - 1);
        result.push_back(BezierCalculator::evaluatePath(points, t, closed));
    }
    return result;
}

QVector<QPointF> BezierPathSampler::sampleAdaptive(const QVector<BezierPoint>& points, float maxAngle, bool closed) {
    QVector<QPointF> result;
    if (points.size() < 2) return result;

    const int segmentCount = closed ? points.size() : points.size() - 1;

    for (int i = 0; i < segmentCount; ++i) {
        const auto& p0 = points[i];
        const auto& p1 = points[(i + 1) % points.size()];

        auto eval = [&](float t) {
            return BezierCalculator::evaluateCubic(
                p0.pos, p0.pos + p0.handleOut, p1.pos + p1.handleIn, p1.pos, t);
        };

        std::vector<std::pair<float, float>> stack = {{0.0f, 1.0f}};
        QVector<QPointF> segPts;
        segPts.push_back(eval(0.0f));

        while (!stack.empty()) {
            auto [t0, t1] = stack.back();
            stack.pop_back();

            const float tm = (t0 + t1) * 0.5f;
            const QPointF pm = eval(tm);
            const QPointF pa = eval(t0);
            const QPointF pb = eval(t1);

            const QPointF d0 = pm - pa;
            const QPointF d1 = pb - pm;
            const float l0 = std::sqrt(d0.x() * d0.x() + d0.y() * d0.y());
            const float l1 = std::sqrt(d1.x() * d1.x() + d1.y() * d1.y());

            if (l0 < 1e-6f || l1 < 1e-6f) {
                segPts.push_back(pm);
                continue;
            }

            const float dot = (d0.x() * d1.x() + d0.y() * d1.y()) / (l0 * l1);
            const float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));

            if (angle > maxAngle && stack.size() < 64) {
                stack.push_back({tm, t1});
                stack.push_back({t0, tm});
            } else {
                segPts.push_back(pm);
            }
        }

        segPts.push_back(eval(1.0f));

        for (int j = (i == 0) ? 0 : 1; j < segPts.size(); ++j)
            result.push_back(segPts[j]);
    }

    if (!closed && points.size() >= 2)
        result.push_back(points.last().pos);

    return result;
}

namespace {

BezierPathSampler::SampledPoint sampleTangentAt(
    const QVector<BezierPoint>& points, float t, bool closed)
{
    BezierPathSampler::SampledPoint sp;
    sp.position = BezierCalculator::evaluatePath(points, t, closed);

    const float eps = 0.001f;
    const float t0 = std::max(0.0f, t - eps);
    const float t1 = std::min(1.0f, t + eps);
    const QPointF p0 = BezierCalculator::evaluatePath(points, t0, closed);
    const QPointF p1 = BezierCalculator::evaluatePath(points, t1, closed);
    QPointF dir = p1 - p0;
    const float len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
    if (len > 1e-8f) dir /= len;
    sp.tangent = dir;
    return sp;
}

} // anonymous namespace

QVector<BezierPathSampler::SampledPoint> BezierPathSampler::sampleWithTangents(
    const QVector<BezierPoint>& points, int count, bool closed)
{
    QVector<SampledPoint> result;
    if (count < 2) return result;
    result.reserve(count);

    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / (count - 1);
        result.push_back(sampleTangentAt(points, t, closed));
    }
    return result;
}

QPointF BezierPathSampler::pointAt(const QVector<BezierPoint>& points, float t, bool closed) {
    return BezierCalculator::evaluatePath(points, std::clamp(t, 0.0f, 1.0f), closed);
}

QPointF BezierPathSampler::tangentAt(const QVector<BezierPoint>& points, float t, bool closed) {
    return sampleTangentAt(points, std::clamp(t, 0.0f, 1.0f), closed).tangent;
}

} // namespace ArtifactCore
