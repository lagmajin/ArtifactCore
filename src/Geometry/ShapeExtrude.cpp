module;
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include <QPointF>
#include <QVector2D>
#include <QVector3D>

module Geometry.ShapeExtrude;

import Mesh;

namespace ArtifactCore {

namespace {

struct CleanContour {
    std::vector<QPointF> points;  // 閉輪郭（終点は始点を含まない）
    bool hole = false;            // true なら穴（CWで格納）
    QPointF normalAt(size_t index) const {
        const size_t count = points.size();
        const QPointF& a = points[index];
        const QPointF& b = points[(index + 1) % count];
        const double dx = b.x() - a.x();
        const double dy = b.y() - a.y();
        const double length = std::sqrt(dx * dx + dy * dy);
        if (length <= 1e-12) return QPointF(0.0, 0.0);
        // 外周CCW / 穴CW のとき、(ey, -ex) が素材の外側を向く
        return QPointF(dy / length, -dx / length);
    }
};

double contourSignedArea(const std::vector<QPointF>& contour) {
    double area = 0.0;
    const size_t count = contour.size();
    for (size_t i = 0; i < count; ++i) {
        const QPointF& a = contour[i];
        const QPointF& b = contour[(i + 1) % count];
        area += a.x() * b.y() - b.x() * a.y();
    }
    return area * 0.5;
}

bool pointInContour(const QPointF& p, const std::vector<QPointF>& contour) {
    bool inside = false;
    const size_t count = contour.size();
    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        const QPointF& a = contour[i];
        const QPointF& b = contour[j];
        if ((a.y() > p.y()) != (b.y() > p.y())) {
            const double t = (p.y() - a.y()) / (b.y() - a.y());
            const double x = a.x() + t * (b.x() - a.x());
            if (p.x() < x) inside = !inside;
        }
    }
    return inside;
}

// 単一閉多角形の ear clipping。ccw は多角形の回転方向。
std::vector<std::array<int, 3>> earClipPolygon(const std::vector<QPointF>& polygon,
                                               bool ccw) {
    std::vector<std::array<int, 3>> triangles;
    const auto cross = [](const QPointF& a, const QPointF& b, const QPointF& c) {
        return (b.x() - a.x()) * (c.y() - a.y()) -
               (b.y() - a.y()) * (c.x() - a.x());
    };
    std::vector<size_t> indices(polygon.size());
    for (size_t i = 0; i < polygon.size(); ++i) indices[i] = i;
    size_t guard = 0;
    while (indices.size() > 2 && guard++ < polygon.size() * polygon.size()) {
        bool clipped = false;
        for (size_t i = 0; i < indices.size(); ++i) {
            const size_t prev = indices[(i + indices.size() - 1) % indices.size()];
            const size_t curr = indices[i];
            const size_t next = indices[(i + 1) % indices.size()];
            const double turn = cross(polygon[prev], polygon[curr], polygon[next]);
            if ((ccw && turn <= 1e-9) || (!ccw && turn >= -1e-9)) continue;
            bool containsVertex = false;
            for (const size_t candidate : indices) {
                if (candidate == prev || candidate == curr || candidate == next) continue;
                const auto inside = [&](const QPointF& point, const QPointF& a,
                                        const QPointF& b, const QPointF& c) {
                    return ccw ? cross(a, b, point) >= -1e-9 &&
                                     cross(b, c, point) >= -1e-9 &&
                                     cross(c, a, point) >= -1e-9
                               : cross(a, b, point) <= 1e-9 &&
                                     cross(b, c, point) <= 1e-9 &&
                                     cross(c, a, point) <= 1e-9;
                };
                if (inside(polygon[candidate], polygon[prev], polygon[curr],
                           polygon[next])) {
                    containsVertex = true;
                    break;
                }
            }
            if (containsVertex) continue;
            if (ccw) {
                triangles.push_back({static_cast<int>(prev),
                                     static_cast<int>(curr),
                                     static_cast<int>(next)});
            } else {
                triangles.push_back({static_cast<int>(prev),
                                     static_cast<int>(next),
                                     static_cast<int>(curr)});
            }
            indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
            clipped = true;
            break;
        }
        if (!clipped) break;
    }
    return triangles;
}

double triangleCross(const QPointF& a, const QPointF& b, const QPointF& c) {
    return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

bool pointInTriangleInclusive(const QPointF& p, const QPointF& a, const QPointF& b,
                              const QPointF& c) {
    return triangleCross(a, b, p) >= -1e-9 && triangleCross(b, c, p) >= -1e-9 &&
           triangleCross(c, a, p) >= -1e-9;
}

// Joins a hole (stored CW) to the outer polygon (CCW) through a zero-width bridge
// edge. The merged polygon stays simple, so ear clipping opens the hole instead
// of covering it with a cap triangle whose centroid happens to fall outside.
bool mergeHoleIntoOuter(std::vector<QPointF>* outer, const std::vector<QPointF>& hole) {
    if (outer->size() < 3 || hole.size() < 3) return false;

    size_t holeIndex = 0;
    for (size_t i = 1; i < hole.size(); ++i) {
        if (hole[i].x() > hole[holeIndex].x()) holeIndex = i;
    }
    const QPointF m = hole[holeIndex];

    constexpr size_t npos = std::numeric_limits<size_t>::max();
    double bestX = std::numeric_limits<double>::max();
    size_t bestEdge = npos;
    const size_t count = outer->size();
    for (size_t i = 0; i < count; ++i) {
        const QPointF& a = (*outer)[i];
        const QPointF& b = (*outer)[(i + 1) % count];
        if ((a.y() > m.y()) == (b.y() > m.y())) continue;
        const double t = (m.y() - a.y()) / (b.y() - a.y());
        const double x = a.x() + t * (b.x() - a.x());
        if (x >= m.x() && x < bestX) {
            bestX = x;
            bestEdge = i;
        }
    }
    if (bestEdge == npos) return false;
    const QPointF intersection(bestX, m.y());

    const QPointF& edgeA = (*outer)[bestEdge];
    const QPointF& edgeB = (*outer)[(bestEdge + 1) % count];
    size_t bridgeIndex = (edgeA.x() > edgeB.x()) ? bestEdge : (bestEdge + 1) % count;
    const QPointF initialTarget = (*outer)[bridgeIndex];

    double bestMetric = std::numeric_limits<double>::max();
    for (size_t i = 0; i < count; ++i) {
        if (i == bridgeIndex) continue;
        const QPointF& candidate = (*outer)[i];
        if (candidate.x() < m.x()) continue;
        const QPointF& prev = (*outer)[(i + count - 1) % count];
        const QPointF& next = (*outer)[(i + 1) % count];
        if (triangleCross(prev, candidate, next) >= 0.0) continue;
        if (!pointInTriangleInclusive(candidate, m, intersection, initialTarget)) continue;
        const double dx = candidate.x() - m.x();
        if (dx <= 0.0) continue;
        const double metric = std::abs(candidate.y() - m.y()) / dx;
        if (metric < bestMetric) {
            bestMetric = metric;
            bridgeIndex = i;
        }
    }

    std::vector<QPointF> merged;
    merged.reserve(outer->size() + hole.size() + 2);
    for (size_t i = 0; i <= bridgeIndex; ++i) merged.push_back((*outer)[i]);
    for (size_t i = 0; i <= hole.size(); ++i) {
        merged.push_back(hole[(holeIndex + i) % hole.size()]);
    }
    merged.push_back((*outer)[bridgeIndex]);
    for (size_t i = bridgeIndex + 1; i < count; ++i) {
        merged.push_back((*outer)[i]);
    }
    *outer = std::move(merged);
    return true;
}

} // namespace

bool extrudeContourMesh(const std::vector<std::vector<QPointF>>& contours,
                        const ShapeExtrudeParams& params,
                        Mesh& outMesh) {
    // 1) 輪郭の正規化
    std::vector<CleanContour> cleaned;
    cleaned.reserve(contours.size());
    for (const auto& contour : contours) {
        std::vector<QPointF> points = contour;
        if (points.size() >= 2 && points.front() == points.back()) points.pop_back();
        // The contract is all-or-nothing: an invalid contour fails the call
        // instead of silently dropping part of the input (a dropped contour
        // could be a hole).
        if (points.size() < 3) return false;
        bool degenerate = false;
        for (size_t i = 0; i < points.size(); ++i) {
            const QPointF& a = points[i];
            const QPointF& b = points[(i + 1) % points.size()];
            if (a == b) { degenerate = true; break; }
        }
        if (degenerate) return false;
        CleanContour entry;
        entry.points = std::move(points);
        entry.hole = false;
        cleaned.push_back(std::move(entry));
    }
    if (cleaned.empty()) return false;

    // even-odd で穴を分類し、外周=CCW / 穴=CW へ回転方向を揃える
    for (size_t i = 0; i < cleaned.size(); ++i) {
        int crossings = 0;
        for (size_t j = 0; j < cleaned.size(); ++j) {
            if (i == j) continue;
            if (pointInContour(cleaned[i].points.front(), cleaned[j].points)) {
                ++crossings;
            }
        }
        cleaned[i].hole = (crossings % 2) != 0;
        const double area = contourSignedArea(cleaned[i].points);
        if (std::abs(area) <= 1e-9) continue;
        const bool makeCcw = !cleaned[i].hole;
        if ((area > 0.0) != makeCcw) {
            std::reverse(cleaned[i].points.begin(), cleaned[i].points.end());
        }
    }

    const float depth = params.depth;
    if (!(depth > 0.0f)) return false;
    float bevel = params.bevelWidth;
    int segments = std::max(1, params.bevelSegments);
    if (!(bevel > 0.0f)) bevel = 0.0f;
    // ベベルは奥行の半分を超えられない
    bevel = std::min(bevel, depth * 0.5f);
    if (bevel <= 0.0f) segments = 0;
    const float halfDepth = depth * 0.5f;
    const float wallHalf = halfDepth - bevel;

    // 2) 頂点バッファ構築
    QVector<QVector3D> positions;
    QVector<QVector3D> normals;
    QVector<QVector2D> uvs;
    QVector<QVector<int>> faces;

    // Accumulate min/max by hand: QRectF(p, p) is a null rectangle and
    // QRectF::united() ignores nulls, so uniting point rectangles would leave
    // the bounds empty and reject every valid contour.
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    bool boundsValid = false;
    for (const auto& contour : cleaned) {
        for (const auto& p : contour.points) {
            if (!boundsValid) {
                minX = maxX = p.x();
                minY = maxY = p.y();
                boundsValid = true;
            } else {
                minX = std::min(minX, p.x());
                minY = std::min(minY, p.y());
                maxX = std::max(maxX, p.x());
                maxY = std::max(maxY, p.y());
            }
        }
    }
    if (!boundsValid || maxX - minX <= 0.0 || maxY - minY <= 0.0) return false;
    const double bw = maxX - minX;
    const double bh = maxY - minY;
    const auto uvOf = [&](const QPointF& p) {
        return QVector2D(static_cast<float>((p.x() - minX) / bw),
                         static_cast<float>((p.y() - minY) / bh));
    };

    const auto addVertex = [&](const QVector3D& position,
                               const QVector3D& normal,
                               const QVector2D& uv) {
        const int index = positions.size();
        positions << position;
        normals << normal;
        uvs << uv;
        return index;
    };

    // ベベル円弧リング。ringIndex r のオフセット s = b*(1-cosθ)、z = wallHalf + b*sinθ
    const auto bevelTheta = [&](int ringIndex) {
        return static_cast<float>(ringIndex) / static_cast<float>(segments) *
               1.57079637f;  // π/2
    };
    const auto bevelOffsetZ = [&](int ringIndex) {
        const float theta = bevelTheta(ringIndex);
        return std::pair<float, float>{bevel * (1.0f - std::cos(theta)),
                                       wallHalf + bevel * std::sin(theta)};
    };

    // 3) 各輪郭のベベルリング + 側壁
    std::vector<std::vector<std::vector<int>>> frontRings(cleaned.size());
    std::vector<std::vector<std::vector<int>>> backRings(cleaned.size());
    std::vector<std::vector<int>> wallBottom(cleaned.size());
    std::vector<std::vector<int>> wallTop(cleaned.size());

    for (size_t c = 0; c < cleaned.size(); ++c) {
        const auto& contour = cleaned[c];
        const size_t count = contour.points.size();

        // 側壁（全輪郭: z in [-wallHalf, wallHalf]）
        wallBottom[c].resize(count);
        wallTop[c].resize(count);
        for (size_t i = 0; i < count; ++i) {
            const QPointF n = contour.normalAt(i);
            const QPointF& p = contour.points[i];
            const QVector3D normal(static_cast<float>(n.x()),
                                   static_cast<float>(n.y()), 0.0f);
            wallBottom[c][i] = addVertex(
                QVector3D(static_cast<float>(p.x()), static_cast<float>(p.y()), -wallHalf),
                normal, QVector2D(uvOf(p).x(), 0.0f));
            wallTop[c][i] = addVertex(
                QVector3D(static_cast<float>(p.x()), static_cast<float>(p.y()), wallHalf),
                normal, QVector2D(uvOf(p).x(), 1.0f));
        }

        // ベベルリング（bevel 無効時はスキップ）
        frontRings[c].assign(static_cast<size_t>(segments) + 1, {});
        backRings[c].assign(static_cast<size_t>(segments) + 1, {});
        for (int r = 0; segments > 0 && r <= segments; ++r) {
            const auto [offset, zFront] = bevelOffsetZ(r);
            const float zBack = -zFront;
            const float theta = bevelTheta(r);
            const float horizontal = std::cos(theta);
            const float vertical = std::sin(theta);
            frontRings[c][static_cast<size_t>(r)].resize(count);
            backRings[c][static_cast<size_t>(r)].resize(count);
            for (size_t i = 0; i < count; ++i) {
                const QPointF n = contour.normalAt(i);
                const QPointF& p = contour.points[i];
                // 外周は内側（-n）、穴はキャップ開口が広がる方向（+n）へ寄せる
                const double sign = contour.hole ? 1.0 : -1.0;
                const double ox = p.x() + n.x() * sign * offset;
                const double oy = p.y() + n.y() * sign * offset;
                const QVector3D frontNormal(
                    static_cast<float>(n.x()) * horizontal,
                    static_cast<float>(n.y()) * horizontal, vertical);
                QVector3D backNormal = frontNormal;
                backNormal.setZ(-vertical);
                frontRings[c][static_cast<size_t>(r)][i] = addVertex(
                    QVector3D(static_cast<float>(ox), static_cast<float>(oy), zFront),
                    frontNormal, uvOf(QPointF(ox, oy)));
                backRings[c][static_cast<size_t>(r)][i] = addVertex(
                    QVector3D(static_cast<float>(ox), static_cast<float>(oy), zBack),
                    backNormal, uvOf(QPointF(ox, oy)));
            }
        }
    }

    // 4) リング間 + 壁との接続クワッド
    for (size_t c = 0; c < cleaned.size(); ++c) {
        const size_t count = cleaned[c].points.size();
        for (size_t i = 0; i < count; ++i) {
            const size_t j = (i + 1) % count;
            // 側壁クワッド（外から見た向き）
            faces << QVector<int>{wallBottom[c][i], wallBottom[c][j],
                                  wallTop[c][j], wallTop[c][i]};
            for (int r = 0; r < segments; ++r) {
                const auto& fr = frontRings[c][static_cast<size_t>(r)];
                const auto& fn = frontRings[c][static_cast<size_t>(r) + 1];
                const auto& br = backRings[c][static_cast<size_t>(r)];
                const auto& bn = backRings[c][static_cast<size_t>(r) + 1];
                // 前面ベベル: 内側リングから外へ
                faces << QVector<int>{fr[j], fr[i], fn[i], fn[j]};
                // 背面ベベル: 外側リングから内へ
                faces << QVector<int>{bn[i], bn[j], br[j], br[i]};
            }
        }
    }

    // 5) キャップ（穴は外輪郭へブリッジしてから ear clipping）
    const float capInset = segments > 0 ? bevelOffsetZ(segments).first : 0.0f;
    const auto insetOf = [&](const CleanContour& contour) {
        std::vector<QPointF> inset = contour.points;
        if (capInset <= 0.0f) return inset;
        const double sign = contour.hole ? 1.0 : -1.0;
        for (size_t i = 0; i < inset.size(); ++i) {
            const QPointF n = contour.normalAt(i);
            inset[i] = QPointF(contour.points[i].x() + n.x() * sign * capInset,
                               contour.points[i].y() + n.y() * sign * capInset);
        }
        return inset;
    };

    for (size_t c = 0; c < cleaned.size(); ++c) {
        if (cleaned[c].hole) continue;
        const auto& outerPoints = cleaned[c].points;
        const float capZFront =
            segments > 0 ? bevelOffsetZ(segments).second : halfDepth;

        std::vector<QPointF> capPolygon = insetOf(cleaned[c]);
        std::vector<std::vector<QPointF>> unmergedHoles;
        for (size_t h = 0; h < cleaned.size(); ++h) {
            if (!cleaned[h].hole) continue;
            if (!pointInContour(cleaned[h].points.front(), outerPoints)) continue;
            std::vector<QPointF> holePolygon = insetOf(cleaned[h]);
            if (contourSignedArea(holePolygon) > 0.0) {
                std::reverse(holePolygon.begin(), holePolygon.end());
            }
            if (!mergeHoleIntoOuter(&capPolygon, holePolygon)) {
                // Fall back to centroid rejection if the bridge could not be built.
                unmergedHoles.push_back(cleaned[h].points);
            }
        }

        auto triangles = earClipPolygon(capPolygon, /*ccw=*/true);
        if (!unmergedHoles.empty()) {
            triangles.erase(
                std::remove_if(triangles.begin(), triangles.end(),
                               [&](const std::array<int, 3>& tri) {
                                   const QPointF centroid =
                                       (capPolygon[static_cast<size_t>(tri[0])] +
                                        capPolygon[static_cast<size_t>(tri[1])] +
                                        capPolygon[static_cast<size_t>(tri[2])]) /
                                       3.0;
                                   for (const auto& hole : unmergedHoles) {
                                       if (pointInContour(centroid, hole)) return true;
                                   }
                                   return false;
                               }),
                triangles.end());
        }

        const size_t count = capPolygon.size();
        for (const bool front : {true, false}) {
            const float z = front ? capZFront : -capZFront;
            std::vector<int> vertexIndices;
            vertexIndices.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                vertexIndices.push_back(addVertex(
                    QVector3D(static_cast<float>(capPolygon[i].x()),
                              static_cast<float>(capPolygon[i].y()), z),
                    QVector3D(0.0f, 0.0f, front ? 1.0f : -1.0f), uvOf(capPolygon[i])));
            }
            for (const auto& tri : triangles) {
                if (front) {
                    faces << QVector<int>{vertexIndices[static_cast<size_t>(tri[0])],
                                          vertexIndices[static_cast<size_t>(tri[1])],
                                          vertexIndices[static_cast<size_t>(tri[2])]};
                } else {
                    // 背面は回転方向を反転
                    faces << QVector<int>{vertexIndices[static_cast<size_t>(tri[0])],
                                          vertexIndices[static_cast<size_t>(tri[2])],
                                          vertexIndices[static_cast<size_t>(tri[1])]};
                }
            }
        }
    }

    // 6) メッシュへ格納
    Mesh result;
    result.setVertexCount(positions.size());
    auto& vertexAttrs = result.vertexAttributes();
    auto positionAttr = vertexAttrs.add<QVector3D>("position");
    positionAttr->data() = positions;
    auto normalAttr = vertexAttrs.add<QVector3D>("normal");
    normalAttr->data() = normals;
    auto uvAttr = vertexAttrs.add<QVector2D>("uv");
    uvAttr->data() = uvs;
    for (const auto& face : faces) {
        result.addPolygon(face);
    }

    outMesh = std::move(result);
    return true;
}



} // namespace ArtifactCore
