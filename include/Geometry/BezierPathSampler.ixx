module;

#include <QPointF>
#include <QVector>

export module Math.Bezier.Sampler;

import std;
import Math.Bezier;

export namespace ArtifactCore {

class BezierPathSampler {
public:
    // パス全体を指定した数（密度）で均等にかつ「等間隔」に近い形で点群に分割
    static QVector<QPointF> sampleEquidistant(const QVector<BezierPoint>& points, float segmentLength = 10.0f, bool closed = false);

    // 単純な分割。カーブごとに等分割する。
    static QVector<QPointF> sampleStandard(const QVector<BezierPoint>& points, int subdivisionsPerSegment = 10, bool closed = false);

    // 曲線の「長さ」を計算。
    static float calculatePathLength(const QVector<BezierPoint>& points, bool closed = false);
};

} // namespace ArtifactCore
