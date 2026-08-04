module;
#include <utility>
#include <vector>
#include <algorithm>
#include <cmath>

module Physics.Fracture;

namespace ArtifactCore {

namespace {

QVector2D polygonCentroid(const std::vector<QVector2D>& vertices)
{
    if (vertices.size() < 3) {
        return QVector2D(0, 0);
    }

    double twiceArea = 0.0;
    double cx = 0.0;
    double cy = 0.0;

    for (size_t i = 0; i < vertices.size(); ++i) {
        const QVector2D& p0 = vertices[i];
        const QVector2D& p1 = vertices[(i + 1) % vertices.size()];
        const double cross = static_cast<double>(p0.x()) * p1.y() - static_cast<double>(p1.x()) * p0.y();
        twiceArea += cross;
        cx += (static_cast<double>(p0.x()) + p1.x()) * cross;
        cy += (static_cast<double>(p0.y()) + p1.y()) * cross;
    }

    if (std::abs(twiceArea) <= 1e-8) {
        QVector2D sum(0, 0);
        for (const auto& v : vertices) {
            sum += v;
        }
        return sum / static_cast<float>(vertices.size());
    }

    const double inv = 1.0 / (3.0 * twiceArea);
    return QVector2D(static_cast<float>(cx * inv), static_cast<float>(cy * inv));
}

std::vector<QVector2D> normalizePolygon(const std::vector<QVector2D>& polygon)
{
    std::vector<QVector2D> result = polygon;
    if (result.size() >= 2 && result.front() == result.back()) {
        result.pop_back();
    }
    return result;
}

std::vector<QVector2D> clipPolygonToHalfPlane(const std::vector<QVector2D>& polygon,
                                              const QVector2D& splitPoint,
                                              const QVector2D& splitNormal,
                                              bool keepPositiveSide)
{
    std::vector<QVector2D> clipped;
    if (polygon.empty()) {
        return clipped;
    }

    auto sideValue = [&](const QVector2D& p) {
        return QVector2D::dotProduct(p - splitPoint, splitNormal);
    };

    for (size_t i = 0; i < polygon.size(); ++i) {
        const QVector2D& current = polygon[i];
        const QVector2D& next = polygon[(i + 1) % polygon.size()];
        const float currentSide = sideValue(current);
        const float nextSide = sideValue(next);
        const bool currentInside = keepPositiveSide ? (currentSide >= 0.0f) : (currentSide <= 0.0f);
        const bool nextInside = keepPositiveSide ? (nextSide >= 0.0f) : (nextSide <= 0.0f);

        if (currentInside) {
            clipped.push_back(current);
        }

        if (currentInside != nextInside) {
            const float denom = currentSide - nextSide;
            if (std::abs(denom) > 1e-8f) {
                const float t = currentSide / denom;
                clipped.push_back(current + t * (next - current));
            }
        }
    }

    return clipped;
}

} // namespace

std::vector<FractureEngine::FragmentInfo> FractureEngine::splitPolygon(
    const std::vector<QVector2D>& originalVertices, 
    const QVector2D& splitPoint, 
    const QVector2D& splitNormal
) {
    std::vector<QVector2D> front, back;
    
    if (originalVertices.size() < 3) {
        return {};
    }

    for (size_t i = 0; i < originalVertices.size(); ++i) {
        const auto& v1 = originalVertices[i];
        const auto& v2 = originalVertices[(i + 1) % originalVertices.size()];

        float d1 = QVector2D::dotProduct(v1 - splitPoint, splitNormal);
        float d2 = QVector2D::dotProduct(v2 - splitPoint, splitNormal);

        if (d1 >= 0) front.push_back(v1);
        else back.push_back(v1);

        if ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) {
            float t = d1 / (d1 - d2);
            QVector2D intersection = v1 + t * (v2 - v1);
            front.push_back(intersection);
            back.push_back(intersection);
        }
    }

    std::vector<FragmentInfo> fragments;
    if (front.size() >= 3) fragments.push_back({front, polygonCentroid(front)});
    if (back.size() >= 3) fragments.push_back({back, polygonCentroid(back)});

    return fragments;
}

std::vector<FractureEngine::FragmentInfo> FractureEngine::voronoiFracture(
    const std::vector<QVector2D>& bounds, 
    const std::vector<QVector2D>& seedPoints
) {
    std::vector<FragmentInfo> fragments;
    const std::vector<QVector2D> basePolygon = normalizePolygon(bounds);
    if (basePolygon.size() < 3 || seedPoints.empty()) {
        return fragments;
    }

    for (size_t i = 0; i < seedPoints.size(); ++i) {
        std::vector<QVector2D> cell = basePolygon;
        const QVector2D& seed = seedPoints[i];

        for (size_t j = 0; j < seedPoints.size(); ++j) {
            if (i == j) {
                continue;
            }

            const QVector2D& other = seedPoints[j];
            QVector2D normal = other - seed;
            if (normal.lengthSquared() <= 1e-8f) {
                continue;
            }

            const QVector2D midpoint = (seed + other) * 0.5f;
            cell = clipPolygonToHalfPlane(cell, midpoint, normal, false);
            if (cell.size() < 3) {
                break;
            }
        }

        if (cell.size() >= 3) {
            fragments.push_back({cell, polygonCentroid(cell)});
        }
    }

    return fragments;
}

} // namespace ArtifactCore
