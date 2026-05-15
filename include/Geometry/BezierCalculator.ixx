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

export module Math.Bezier;

export namespace ArtifactCore {

struct BezierPoint {
    QPointF pos;
    QPointF handleIn;  // 相対座標 or 絶対座標 (実装で要定義, ここでは相対)
    QPointF handleOut;
};

class BezierCalculator {
public:
    // 2次ベジェ (p0, p1, p2)
    static QPointF evaluateQuadratic(const QPointF& p0, const QPointF& p1, const QPointF& p2, float t);
    
    // 3次ベジェ (p0, p1, p2, p3)
    static QPointF evaluateCubic(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, float t);

    // 複数の点を通るパス上での補間 (0.0 - 1.0)
    static QPointF evaluatePath(const QVector<BezierPoint>& points, float t, bool closed = false);

    // 接線角度 (Degrees)
    static float getTangentAngle(const QPointF& p0, const QPointF& p1, const QPointF& p2, const QPointF& p3, float t);
};

} // namespace ArtifactCore
