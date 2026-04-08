module;
#include <vector>
#include <QVector2D>
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
export module Physics.Fracture;





export namespace ArtifactCore {

/**
 * @brief 剛体を分割する基礎ロジック
 * ポリゴンの切断、質量保存、再構築を管理
 */
class LIBRARY_DLL_API FractureEngine {
public:
    struct FragmentInfo {
        std::vector<QVector2D> vertices;
        QVector2D centroid;
    };

    /**
     * @brief ポリゴンを指定された直線で2つに切断
     * @param originalVertices 元のポリゴンの頂点
     * @param splitPoint 直線上の任意の点
     * @param splitNormal 直線の法線
     */
    static std::vector<FragmentInfo> splitPolygon(const std::vector<QVector2D>& originalVertices, 
                                                const QVector2D& splitPoint, 
                                                const QVector2D& splitNormal);

    /**
     * @brief Voronoi分割を用いた複数破片化
     * @param bounds 外枠
     * @param seedPoints 破片の中心点（ランダム生成など）
     */
    static std::vector<FragmentInfo> voronoiFracture(const std::vector<QVector2D>& bounds, 
                                                   const std::vector<QVector2D>& seedPoints);
};

} // namespace ArtifactCore
