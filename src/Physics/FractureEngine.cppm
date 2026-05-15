module;
#include <utility>
#include <vector>
#include <algorithm>
#include <cmath>

module Physics.Fracture;

namespace ArtifactCore {

std::vector<FractureEngine::FragmentInfo> FractureEngine::splitPolygon(
    const std::vector<QVector2D>& originalVertices, 
    const QVector2D& splitPoint, 
    const QVector2D& splitNormal
) {
    std::vector<QVector2D> front, back;
    
    for (size_t i = 0; i < originalVertices.size(); ++i) {
        const auto& v1 = originalVertices[i];
        const auto& v2 = originalVertices[(i + 1) % originalVertices.size()];

        float d1 = QVector2D::dotProduct(v1 - splitPoint, splitNormal);
        float d2 = QVector2D::dotProduct(v2 - splitPoint, splitNormal);

        if (d1 >= 0) front.push_back(v1);
        else back.push_back(v1);

        // Check for intersection
        if ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) {
            float t = d1 / (d1 - d2);
            QVector2D intersection = v1 + t * (v2 - v1);
            front.push_back(intersection);
            back.push_back(intersection);
        }
    }

    std::vector<FragmentInfo> fragments;
    if (front.size() >= 3) fragments.push_back({front, QVector2D(0, 0)}); // Placeholder for centroid
    if (back.size() >= 3) fragments.push_back({back, QVector2D(0, 0)});

    return fragments;
}

std::vector<FractureEngine::FragmentInfo> FractureEngine::voronoiFracture(
    const std::vector<QVector2D>& bounds, 
    const std::vector<QVector2D>& seedPoints
) {
    // Voronoi 分割の簡易実装（境界を切断していく手法）
    // 実際の実装は Fortune's アルゴリズムや Fortune's Sweep などが必要
    return {}; // TODO: 実装
}

} // namespace ArtifactCore
