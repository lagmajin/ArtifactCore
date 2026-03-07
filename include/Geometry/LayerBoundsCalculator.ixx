module;

#include <QPointF>
#include <QVector>
#include <QRectF>
#include "../Define/DllExportMacro.hpp"

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
export module Geometry.LayerBounds;





export namespace ArtifactCore {

struct LayerCorners {
    QPointF topLeft;
    QPointF topRight;
    QPointF bottomLeft;
    QPointF bottomRight;
};

class LIBRARY_DLL_API LayerBoundsCalculator2D {
public:
    static LayerCorners calculate(
        const QPointF& position, 
        const QPointF& anchor, 
        float width, 
        float height, 
        float rotation, // Degrees
        float scaleX = 1.0f, 
        float scaleY = 1.0f);

    // バウンディングボックス（軸平行）を取得
    static QRectF calculateAABB(const LayerCorners& corners);

    // 特定の座標がこの範囲内に含まれるか（ヒットテスト）
    static bool contains(const LayerCorners& corners, const QPointF& point);
};

} // namespace ArtifactCore
