module;
#include <vector>
#include <cmath>
#include <algorithm>
#include <QPointF>
#include <QPainterPath>
#include <QDebug>
module Core.Mask.PathMorph;

import Container.NamedVector;

namespace ArtifactCore {

// -----------------------------------------------------------------------
// ベジェ曲線評価（自前 — BezierPathSampler に依存しない）
// -----------------------------------------------------------------------

namespace {

QPointF cubicPoint(const QPointF& p0, const QPointF& p1,
                   const QPointF& p2, const QPointF& p3, float t)
{
    const float u  = 1.0f - t;
    const float tt = t * t;
    const float uu = u * u;
    const float uuu = uu * u;
    const float ttt = tt * t;
    return uuu * p0 + 3.0f * uu * t * p1 + 3.0f * u * tt * p2 + ttt * p3;
}

// QPainterPath を個別サブパスに分解（各サブパスは連続する点列として返す）
std::vector<std::vector<QPointF>> decomposePainterPath(const QPainterPath& path)
{
    NamedVector<std::vector<QPointF>> subpaths{makeNamedVector<std::vector<QPointF>>(ContainerName{"PathMorphSubpaths"})};
    if (path.isEmpty())
        return subpaths.toStdVector();

    const int n = path.elementCount();
    int i = 0;

    while (i < n) {
        if (path.elementAt(i).type != QPainterPath::MoveToElement) {
            ++i;
            continue;
        }

            NamedVector<QPointF> points{makeNamedVector<QPointF>(ContainerName{"PathMorphPoints"})};
            const QPointF startPt(path.elementAt(i).x, path.elementAt(i).y);
            points.add(startPt);
        ++i;

        while (i < n) {
            const auto& e = path.elementAt(i);
            if (e.type == QPainterPath::MoveToElement)
                break;

            if (e.type == QPainterPath::LineToElement) {
                points.add(QPointF(e.x, e.y));
                ++i;
            }
            else if (e.type == QPainterPath::CurveToElement) {
                const QPointF c1(e.x, e.y);
                const QPointF c2(path.elementAt(i + 1).x, path.elementAt(i + 1).y);
                const QPointF end(path.elementAt(i + 2).x, path.elementAt(i + 2).y);
                i += 3;

                // 直前の点から end までを8分割してベジェサンプリング
                const QPointF p0 = points.empty() ? startPt : points.back();
                for (int s = 1; s <= 8; ++s) {
                    const float t = static_cast<float>(s) / 8.0f;
                    points.add(cubicPoint(p0, c1, c2, end, t));
                }
            }
            else {
                ++i;
            }
        }

        // 閉じているかチェック（最終点≈始点）
        if (points.size() >= 2) {
            const double dx = points.back().x() - startPt.x();
            const double dy = points.back().y() - startPt.y();
            const bool closed = (dx * dx + dy * dy) < 0.0001;

            if (closed && points.size() >= 2) {
                points.back() = startPt;  // 正確に揃える
            }
        }

        if (points.size() >= 2)
            subpaths.add(points.toStdVector());
    }

    return subpaths.toStdVector();
}

// 点列を targetCount 点に等距離リサンプリング
std::vector<QPointF> resampleEquidistant(
    const std::vector<QPointF>& src, int targetCount)
{
    if (src.size() < 2 || targetCount < 2) {
        std::vector<QPointF> r;
        if (!src.empty())
            r.push_back(src.front());
        if (targetCount > 1 && src.size() > 1)
            r.push_back(src.back());
        return r;
    }

    // 全弧長を計算
    std::vector<float> arcLen(src.size(), 0.0f);
    float totalLen = 0.0f;
    for (size_t i = 1; i < src.size(); ++i) {
        const QPointF d = src[i] - src[i - 1];
        totalLen += std::sqrt(d.x() * d.x() + d.y() * d.y());
        arcLen[i] = totalLen;
    }

    if (totalLen < 1e-6f) {
        return std::vector<QPointF>(targetCount, src.front());
    }

    NamedVector<QPointF> result{makeNamedVector<QPointF>(ContainerName{"PathMorphResampled"})};
    for (int i = 0; i < targetCount; ++i) {
        const float target = static_cast<float>(i) / (targetCount - 1) * totalLen;

        // 弧長に沿った位置をバイナリサーチ + 線形補間
        size_t lo = 0, hi = src.size() - 1;
        while (lo + 1 < hi) {
            const size_t mid = (lo + hi) / 2;
            if (arcLen[mid] < target)
                lo = mid;
            else
                hi = mid;
        }

        if (hi >= src.size()) hi = src.size() - 1;
        const float segLen = arcLen[hi] - arcLen[lo];
        float t = (segLen > 1e-6f) ? (target - arcLen[lo]) / segLen : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);

        result.add(src[lo] + (src[hi] - src[lo]) * t);
    }

    return result.toStdVector();
}

} // anonymous namespace

// =======================================================================
// 公開API
// =======================================================================

MorphPathSample PathMorphEngine::samplePath(
    const QPainterPath& path, int pointsPerSubpath)
{
    MorphPathSample sample;
    sample.closed = false;

    auto subpaths = decomposePainterPath(path);

    for (auto& raw : subpaths) {
        // 閉じているか判定
        if (raw.size() >= 2) {
            const double dx = raw.back().x() - raw.front().x();
            const double dy = raw.back().y() - raw.front().y();
            const bool closed = (dx * dx + dy * dy) < 0.0001;
            if (closed) {
                // 閉じたパスは始点を1回だけ含むようにする
                // 等距離サンプリングにちょうど targetCount に統一
                sample.subpaths.push_back(
                    resampleEquidistant(raw, std::max(3, pointsPerSubpath)));
            } else {
                sample.subpaths.push_back(
                    resampleEquidistant(raw, pointsPerSubpath));
            }
        }
    }

    if (!subpaths.empty())
        sample.closed = true;

    return sample;
}

QPainterPath PathMorphEngine::interpolateSamples(
    const MorphPathSample& sampleA,
    const MorphPathSample& sampleB,
    float t)
{
    QPainterPath result;

    const size_t subCount = std::min(sampleA.subpaths.size(), sampleB.subpaths.size());
    if (subCount == 0)
        return result;

    for (size_t s = 0; s < subCount; ++s) {
        const auto& ptsA = sampleA.subpaths[s];
        const auto& ptsB = sampleB.subpaths[s];

        const size_t n = std::min(ptsA.size(), ptsB.size());
        if (n < 2)
            continue;

        const QPointF startA = ptsA.front();
        const QPointF startB = ptsB.front();
        const QPointF start = startA + (startB - startA) * t;

        result.moveTo(start);

        for (size_t i = 1; i < n; ++i) {
            const QPointF pa = ptsA[i];
            const QPointF pb = ptsB[i];
            const QPointF p = pa + (pb - pa) * t;
            result.lineTo(p);
        }

        // 閉じていたら closeSubpath
        // 各サンプルの閉包判定：先頭と末尾が近い
        if (n >= 3) {
            const double dxA = ptsA.back().x() - ptsA.front().x();
            const double dyA = ptsA.back().y() - ptsA.front().y();
            const double dxB = ptsB.back().x() - ptsB.front().x();
            const double dyB = ptsB.back().y() - ptsB.front().y();
            const double da = dxA * dxA + dyA * dyA;
            const double db = dxB * dxB + dyB * dyB;

            // どちらかが閉じていれば閉じる（補間結果も閉じる）
            if (da < 0.0001 || db < 0.0001) {
                result.closeSubpath();
            }
        }
    }

    return result;
}

QPainterPath PathMorphEngine::interpolate(
    const QPainterPath& pathA,
    const QPainterPath& pathB,
    float t,
    const PathMorphSettings& settings)
{
    if (pathA.isEmpty() && pathB.isEmpty())
        return {};

    if (pathA.isEmpty())
        return pathB;
    if (pathB.isEmpty())
        return pathA;

    if (t <= 0.0f) return pathA;
    if (t >= 1.0f) return pathB;

    const auto sampleA = samplePath(pathA, settings.sampleCount);
    const auto sampleB = samplePath(pathB, settings.sampleCount);

    return interpolateSamples(sampleA, sampleB, t);
}

// =======================================================================
// 特徴量ベース対応付け補間
// =======================================================================

QPainterPath PathMorphEngine::interpolateByFeature(
    const QPainterPath& pathA,
    const QPainterPath& pathB,
    float t,
    int sampleCount)
{
    if (pathA.isEmpty() && pathB.isEmpty()) return {};
    if (pathA.isEmpty()) return pathB;
    if (pathB.isEmpty()) return pathA;
    if (t <= 0.0f) return pathA;
    if (t >= 1.0f) return pathB;

    // 両パスをサブパスごとに分解
    const auto rawA = decomposePainterPath(pathA);
    const auto rawB = decomposePainterPath(pathB);
    if (rawA.empty() || rawB.empty()) return t < 0.5f ? pathA : pathB;

    // 各サブパスの特徴量（面積, 重心x, 重心y）を計算
    struct Feature {
        size_t index;
        double area;
        QPointF centroid;
    };

    auto computeFeatures = [&](const std::vector<std::vector<QPointF>>& subs) {
        std::vector<Feature> feats;
        for (size_t i = 0; i < subs.size(); ++i) {
            const auto& pts = subs[i];
            if (pts.size() < 3) continue;

            double a = 0.0;
            double cx = 0.0, cy = 0.0;
            for (size_t j = 0; j < pts.size(); ++j) {
                const auto& p0 = pts[j];
                const auto& p1 = pts[(j + 1) % pts.size()];
                const double cross = p0.x() * p1.y() - p1.x() * p0.y();
                a += cross;
                cx += (p0.x() + p1.x()) * cross;
                cy += (p0.y() + p1.y()) * cross;
            }
            a *= 0.5;
            if (std::abs(a) > 1e-12) {
                cx /= (6.0 * a);
                cy /= (6.0 * a);
            } else {
                // 退化したサブパス → 点の平均を重心に
                for (const auto& p : pts) { cx += p.x(); cy += p.y(); }
                cx /= pts.size();
                cy /= pts.size();
            }
            feats.push_back({i, a, QPointF(cx, cy)});
        }
        return feats;
    };

    const auto featsA = computeFeatures(rawA);
    const auto featsB = computeFeatures(rawB);

    if (featsA.empty() || featsB.empty()) return t < 0.5f ? pathA : pathB;

    // 面積の絶対値でソート（大→小）して対応付け
    auto sortedA = featsA;
    auto sortedB = featsB;
    std::sort(sortedA.begin(), sortedA.end(),
              [](const Feature& a, const Feature& b) { return std::abs(a.area) > std::abs(b.area); });
    std::sort(sortedB.begin(), sortedB.end(),
              [](const Feature& a, const Feature& b) { return std::abs(a.area) > std::abs(b.area); });

    // 対応するサブパス同士を resample → 線形補間
    QPainterPath result;
    const size_t n = std::min(sortedA.size(), sortedB.size());
    for (size_t s = 0; s < n; ++s) {
        const auto& ptsA = rawA[sortedA[s].index];
        const auto& ptsB = rawB[sortedB[s].index];
        const auto resampledA = resampleEquidistant(ptsA, sampleCount);
        const auto resampledB = resampleEquidistant(ptsB, sampleCount);
        const size_t m = std::min(resampledA.size(), resampledB.size());
        if (m < 2) continue;

        const QPointF start = resampledA[0] + (resampledB[0] - resampledA[0]) * t;
        result.moveTo(start);

        for (size_t i = 1; i < m; ++i) {
            const QPointF p = resampledA[i] + (resampledB[i] - resampledA[i]) * t;
            result.lineTo(p);
        }

        // 閉じているか
        const double dxA = ptsA.back().x() - ptsA.front().x();
        const double dyA = ptsA.back().y() - ptsA.front().y();
        const double dxB = ptsB.back().x() - ptsB.front().x();
        const double dyB = ptsB.back().y() - ptsB.front().y();
        if ((dxA * dxA + dyA * dyA < 0.0001) || (dxB * dxB + dyB * dyB < 0.0001))
            result.closeSubpath();
    }

    return result;
}

} // namespace ArtifactCore
