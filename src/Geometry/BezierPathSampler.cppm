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
    if (!std::isfinite(segmentLength) || segmentLength <= 0.0f) return sampledPoints;
    if (points.size() < 2) {
        if (!points.isEmpty()) sampledPoints.push_back(points[0].pos);
        return sampledPoints;
    }
    float totalLen = calculatePathLength(points, closed);
    if (totalLen <= 0.0f) return {points[0].pos, points.last().pos};

    int pointCount = static_cast<int>(totalLen / segmentLength) + 1;
    if (pointCount < 2) return {points[0].pos, points.last().pos};

    return sampleArcLength(points, pointCount, closed);
}

QVector<QPointF> BezierPathSampler::sampleByCount(const QVector<BezierPoint>& points, int count, bool closed) {
    return sampleArcLength(points, count, closed);
}

namespace {

// Cumulative arc-length table over fine polyline samples. Maps normalized
// arc length s in [0,1] back to the segment-parameter t used by evaluatePath.
struct ArcLengthTable {
    QVector<float> sampleT;
    QVector<float> cumulative;
    float total = 0.0f;

    bool build(const QVector<BezierPoint>& points, bool closed, int subdivPerSegment = 32) {
        sampleT.clear();
        cumulative.clear();
        total = 0.0f;
        if (points.size() < 2) return false;
        const int segmentCount = closed ? points.size() : points.size() - 1;
        sampleT.reserve(segmentCount * (subdivPerSegment + 1));
        cumulative.reserve(segmentCount * (subdivPerSegment + 1));
        QPointF prev;
        bool first = true;
        for (int i = 0; i < segmentCount; ++i) {
            const auto& p0 = points[i];
            const auto& p1 = points[(i + 1) % points.size()];
            for (int j = (i == 0 ? 0 : 1); j <= subdivPerSegment; ++j) {
                const float localT = static_cast<float>(j) / subdivPerSegment;
                const float globalT = (static_cast<float>(i) + localT) / segmentCount;
                const QPointF current = BezierCalculator::evaluateCubic(
                    p0.pos, p0.pos + p0.handleOut, p1.pos + p1.handleIn, p1.pos, localT);
                if (!first) {
                    const float dx = current.x() - prev.x();
                    const float dy = current.y() - prev.y();
                    const float step = std::sqrt(dx * dx + dy * dy);
                    if (std::isfinite(step)) total += step;
                }
                first = false;
                prev = current;
                sampleT.push_back(globalT);
                cumulative.push_back(total);
            }
        }
        return total > 0.0f && !sampleT.isEmpty();
    }

    float mapToT(float s) const {
        if (sampleT.isEmpty() || total <= 0.0f) return 0.0f;
        const float target = std::clamp(s, 0.0f, 1.0f) * total;
        int lo = 0;
        int hi = cumulative.size() - 1;
        while (lo < hi) {
            const int mid = (lo + hi) / 2;
            if (cumulative[mid] < target) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        if (lo <= 0) return sampleT.front();
        if (lo >= cumulative.size()) return sampleT.back();
        const float d0 = cumulative[lo - 1];
        const float d1 = cumulative[lo];
        const float span = d1 - d0;
        const float f = span > 1.0e-9f ? (target - d0) / span : 0.0f;
        return sampleT[lo - 1] + (sampleT[lo] - sampleT[lo - 1]) * std::clamp(f, 0.0f, 1.0f);
    }
};

void locateSegment(const QVector<BezierPoint>& points, float t, bool closed,
                   int& segmentIndex, float& localT) {
    const int count = points.size();
    if (count < 2) {
        segmentIndex = 0;
        localT = 0.0f;
        return;
    }
    const int segmentCount = closed ? count : count - 1;
    const float scaledT = std::clamp(t, 0.0f, 1.0f) * segmentCount;
    segmentIndex = std::min(static_cast<int>(std::floor(scaledT)), segmentCount - 1);
    localT = scaledT - segmentIndex;
}

QPointF analyticTangentAt(const QVector<BezierPoint>& points, float t, bool closed) {
    if (points.size() < 2) return QPointF(1.0f, 0.0f);
    int segmentIndex = 0;
    float localT = 0.0f;
    locateSegment(points, t, closed, segmentIndex, localT);
    const auto& p0 = points[segmentIndex];
    const auto& p1 = points[(segmentIndex + 1) % points.size()];
    return BezierCalculator::evaluateTangent(
        p0.pos, p0.pos + p0.handleOut, p1.pos + p1.handleIn, p1.pos, localT);
}

} // anonymous namespace

QVector<QPointF> BezierPathSampler::sampleArcLength(const QVector<BezierPoint>& points, int count, bool closed) {
    QVector<QPointF> result;
    if (points.size() < 2 || count < 2) return result;
    ArcLengthTable table;
    if (!table.build(points, closed)) {
        // Degenerate path: repeat the first point.
        result.fill(points[0].pos, count);
        return result;
    }
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        const float s = static_cast<float>(i) / (count - 1);
        result.push_back(BezierCalculator::evaluatePath(points, table.mapToT(s), closed));
    }
    return result;
}

QVector<QPointF> BezierPathSampler::sampleAdaptive(const QVector<BezierPoint>& points, float maxAngle, bool closed) {
    QVector<QPointF> result;
    if (points.size() < 2) return result;
    if (!std::isfinite(maxAngle)) maxAngle = 0.15f;
    maxAngle = std::max(maxAngle, 1.0e-6f);

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
    sp.tangent = analyticTangentAt(points, t, closed);
    return sp;
}

BezierPathSampler::SampledPoint sampleTangentAtArcLength(
    const QVector<BezierPoint>& points, float s, bool closed, const ArcLengthTable& table)
{
    BezierPathSampler::SampledPoint sp;
    const float t = table.mapToT(s);
    sp.position = BezierCalculator::evaluatePath(points, t, closed);
    sp.tangent = analyticTangentAt(points, t, closed);
    return sp;
}

} // anonymous namespace

QVector<BezierPathSampler::SampledPoint> BezierPathSampler::sampleWithTangents(
    const QVector<BezierPoint>& points, int count, bool closed)
{
    QVector<SampledPoint> result;
    if (count < 2) return result;
    ArcLengthTable table;
    const bool useArc = table.build(points, closed);
    result.reserve(count);

    for (int i = 0; i < count; ++i) {
        const float s = static_cast<float>(i) / (count - 1);
        if (useArc) {
            result.push_back(sampleTangentAtArcLength(points, s, closed, table));
        } else if (points.size() >= 2) {
            result.push_back(sampleTangentAt(points, s, closed));
        } else if (!points.isEmpty()) {
            SampledPoint sp;
            sp.position = points[0].pos;
            sp.tangent = QPointF(1.0f, 0.0f);
            result.push_back(sp);
        }
    }
    return result;
}

QPointF BezierPathSampler::pointAt(const QVector<BezierPoint>& points, float t, bool closed) {
    return BezierCalculator::evaluatePath(points, std::clamp(t, 0.0f, 1.0f), closed);
}

QPointF BezierPathSampler::pointAtArcLength(const QVector<BezierPoint>& points, float s, bool closed) {
    if (points.size() < 2) {
        return points.isEmpty() ? QPointF() : points[0].pos;
    }
    ArcLengthTable table;
    if (!table.build(points, closed)) {
        return points[0].pos;
    }
    return BezierCalculator::evaluatePath(points, table.mapToT(s), closed);
}

QPointF BezierPathSampler::tangentAt(const QVector<BezierPoint>& points, float t, bool closed) {
    return analyticTangentAt(points, std::clamp(t, 0.0f, 1.0f), closed);
}

QPointF BezierPathSampler::tangentAtArcLength(const QVector<BezierPoint>& points, float s, bool closed) {
    if (points.size() < 2) {
        return QPointF(1.0f, 0.0f);
    }
    ArcLengthTable table;
    if (!table.build(points, closed)) {
        return QPointF(1.0f, 0.0f);
    }
    return analyticTangentAt(points, table.mapToT(s), closed);
}

} // namespace ArtifactCore
