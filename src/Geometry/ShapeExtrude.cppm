module;
#include <algorithm>
#include <array>
#include <cmath>
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
        if (points.size() < 3) continue;
        bool degenerate = false;
        for (size_t i = 0; i < points.size(); ++i) {
            const QPointF& a = points[i];
            const QPointF& b = points[(i + 1) % points.size()];
            if (a == b) { degenerate = true; break; }
        }
        if (degenerate) continue;
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

    QRectF bounds;
    bool boundsValid = false;
    for (const auto& contour : cleaned) {
        for (const auto& p : contour.points) {
            if (!boundsValid) { bounds = QRectF(p, p); boundsValid = true; }
            else bounds = bounds.united(QRectF(p, p));
        }
    }
    if (!boundsValid || bounds.width() <= 0.0 || bounds.height() <= 0.0) return false;
    const double bw = bounds.width();
    const double bh = bounds.height();
    const auto uvOf = [&](const QPointF& p) {
        return QVector2D(
            static_cast<float>((p.x() - bounds.left()) / bw),
            static_cast<float>((p.y() - bounds.top()) / bh));
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

    // 5) キャップ（外周の ear clipping + 穴内部の三角形除去）
    for (size_t c = 0; c < cleaned.size(); ++c) {
        if (cleaned[c].hole) continue;
        const auto& outer = cleaned[c].points;
        const size_t count = outer.size();
        const float capZFront =
            segments > 0 ? bevelOffsetZ(segments).second : halfDepth;
        for (const bool front : {true, false}) {
            const float z = front ? capZFront : -capZFront;
            std::vector<QPointF> inset(count);
            for (size_t i = 0; i < count; ++i) {
                if (segments > 0) {
                    const float offset = bevelOffsetZ(segments).first;
                    const QPointF n = cleaned[c].normalAt(i);
                    const double sign = cleaned[c].hole ? 1.0 : -1.0;
                    inset[i] = QPointF(outer[i].x() + n.x() * sign * offset,
                                       outer[i].y() + n.y() * sign * offset);
                } else {
                    inset[i] = outer[i];
                }
            }
            auto triangles = earClipPolygon(inset, /*ccw=*/true);
            triangles.erase(
                std::remove_if(triangles.begin(), triangles.end(),
                               [&](const std::array<int, 3>& tri) {
                                   const QPointF centroid =
                                       (inset[static_cast<size_t>(tri[0])] +
                                        inset[static_cast<size_t>(tri[1])] +
                                        inset[static_cast<size_t>(tri[2])]) /
                                       3.0;
                                   for (const auto& hole : cleaned) {
                                       if (!hole.hole) continue;
                                       if (pointInContour(centroid, hole.points)) {
                                           return true;
                                       }
                                   }
                                   return false;
                               }),
                triangles.end());
            std::vector<int> vertexIndices;
            vertexIndices.reserve(count);
            for (size_t i = 0; i < count; ++i) {
                vertexIndices.push_back(addVertex(
                    QVector3D(static_cast<float>(inset[i].x()),
                              static_cast<float>(inset[i].y()), z),
                    QVector3D(0.0f, 0.0f, front ? 1.0f : -1.0f), uvOf(inset[i])));
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

