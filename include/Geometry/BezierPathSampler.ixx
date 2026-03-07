module;

#include <QPointF>
#include <QVector>

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
};

} // namespace ArtifactCore
