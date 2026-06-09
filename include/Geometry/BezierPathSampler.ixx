module;
class tst_QList;

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QPointF>
#include <QVector>

export module Math.Bezier.Sampler;

import Math.Bezier;

namespace ArtifactCore {

export class BezierPathSampler {
public:
    // パス全体を指定した数（密度）で均等にかつ「等間隔」に近い形で点群に分割
    static QVector<QPointF> sampleEquidistant(const QVector<BezierPoint>& points, float segmentLength = 10.0f, bool closed = false);

    // 単純な分割。カーブごとに等分割する。
    static QVector<QPointF> sampleStandard(const QVector<BezierPoint>& points, int subdivisionsPerSegment = 10, bool closed = false);

    // 曲線の「長さ」を計算。
    static float calculatePathLength(const QVector<BezierPoint>& points, bool closed = false);

    // 指定した数の点で等距離サンプリング（closed の始点重複を除去）
    static QVector<QPointF> sampleByCount(const QVector<BezierPoint>& points, int count, bool closed = false);

    // 曲率適応サンプリング：曲がりが大きい箇所ほど密にサンプリング
    // maxAngle: 隣接セグメント間の角度閾値（ラジアン、小さいほど密）
    static QVector<QPointF> sampleAdaptive(const QVector<BezierPoint>& points, float maxAngle = 0.15f, bool closed = false);

    // 位置＋接線ベクトルを同時にサンプリング
    struct SampledPoint {
        QPointF position;
        QPointF tangent;  // 正規化済み
    };
    static QVector<SampledPoint> sampleWithTangents(const QVector<BezierPoint>& points, int count, bool closed = false);

    // パラメータ t (0..1) における位置を取得
    static QPointF pointAt(const QVector<BezierPoint>& points, float t, bool closed = false);

    // パラメータ t (0..1) における接線ベクトル（正規化済み）
    static QPointF tangentAt(const QVector<BezierPoint>& points, float t, bool closed = false);
};

} // namespace ArtifactCore
