module;
class tst_QList;

#include <QVector>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QMatrix3x3>
#include <QQuaternion>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

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
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <limits>
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
#include <meshoptimizer.h>
module Mesh;

import Memory.SharedPtr;
import Container.NamedVector;
import Core.ArtifactArray;

namespace ArtifactCore {

namespace {
constexpr int kTriangleIndexCount = 3;

struct DualQuaternion {
    QQuaternion real;
    QQuaternion dual;
};

bool makeDualQuaternion(const QMatrix4x4& matrix, DualQuaternion& result)
{
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (!std::isfinite(matrix(row, column))) return false;
        }
    }
    const float determinant = matrix.determinant();
    if (!std::isfinite(determinant) || determinant <= 0.0f ||
        std::abs(determinant - 1.0f) > 1.0e-3f) {
        return false;
    }
    const QVector3D axisX(matrix(0, 0), matrix(1, 0), matrix(2, 0));
    const QVector3D axisY(matrix(0, 1), matrix(1, 1), matrix(2, 1));
    const QVector3D axisZ(matrix(0, 2), matrix(1, 2), matrix(2, 2));
    constexpr float kRigidTolerance = 1.0e-3f;
    if (std::abs(axisX.lengthSquared() - 1.0f) > kRigidTolerance ||
        std::abs(axisY.lengthSquared() - 1.0f) > kRigidTolerance ||
        std::abs(axisZ.lengthSquared() - 1.0f) > kRigidTolerance ||
        std::abs(QVector3D::dotProduct(axisX, axisY)) > kRigidTolerance ||
        std::abs(QVector3D::dotProduct(axisX, axisZ)) > kRigidTolerance ||
        std::abs(QVector3D::dotProduct(axisY, axisZ)) > kRigidTolerance) {
        return false;
    }
    QMatrix3x3 rotation;
    rotation(0, 0) = matrix(0, 0);
    rotation(0, 1) = matrix(0, 1);
    rotation(0, 2) = matrix(0, 2);
    rotation(1, 0) = matrix(1, 0);
    rotation(1, 1) = matrix(1, 1);
    rotation(1, 2) = matrix(1, 2);
    rotation(2, 0) = matrix(2, 0);
    rotation(2, 1) = matrix(2, 1);
    rotation(2, 2) = matrix(2, 2);
    result.real = QQuaternion::fromRotationMatrix(rotation);
    const float realLength = result.real.length();
    if (!std::isfinite(realLength) || realLength <= 1.0e-8f) return false;
    result.real /= realLength;
    const QQuaternion translation(0.0f, matrix(0, 3), matrix(1, 3),
                                  matrix(2, 3));
    result.dual = (translation * result.real) * 0.5f;
    return std::isfinite(result.dual.x()) &&
           std::isfinite(result.dual.y()) &&
           std::isfinite(result.dual.z()) &&
           std::isfinite(result.dual.scalar());
}

QVector3D transformDualQuaternion(const DualQuaternion& dualQuaternion,
                                  const QVector3D& position)
{
    const QQuaternion conjugate = dualQuaternion.real.conjugated();
    const QQuaternion translation =
        (dualQuaternion.dual * conjugate) * 2.0f;
    const QQuaternion point(0.0f, position.x(), position.y(), position.z());
    const QQuaternion rotated = dualQuaternion.real * point * conjugate;
    return QVector3D(rotated.x() + translation.x(),
                     rotated.y() + translation.y(),
                     rotated.z() + translation.z());
}

int resolveObjIndex(const QString& token, const int elementCount)
{
    bool ok = false;
    int index = token.toInt(&ok);
    if (!ok || index == 0) {
        return -1;
    }

    if (index < 0) {
        index = elementCount + index;
    } else {
        index -= 1;
    }

    return (index >= 0 && index < elementCount) ? index : -1;
}

Mesh::Meshlet buildMeshletFromIndexRange(const Mesh::RenderData& renderData,
                                         const QVector<unsigned int>& indices,
                                         const int firstIndex,
                                         const int indexCount,
                                         const int triangleStride)
{
    Mesh::Meshlet meshlet;
    meshlet.firstIndex = static_cast<unsigned int>(firstIndex);
    meshlet.indexCount = static_cast<unsigned int>(indexCount);
    meshlet.sourceTriangleCount = (indexCount / kTriangleIndexCount) * triangleStride;

    QVector3D boundsMin(std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max());
    QVector3D boundsMax(std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest());

    bool hasVertex = false;
    const int endIndex = firstIndex + indexCount;
    for (int indexOffset = firstIndex; indexOffset < endIndex; ++indexOffset) {
        if (indexOffset < 0 || indexOffset >= indices.size()) {
            continue;
        }

        const unsigned int vertexIndex = indices[indexOffset];
        if (vertexIndex >= static_cast<unsigned int>(renderData.positions.size())) {
            continue;
        }

        const QVector3D& p = renderData.positions[static_cast<int>(vertexIndex)];
        boundsMin.setX(std::min(boundsMin.x(), p.x()));
        boundsMin.setY(std::min(boundsMin.y(), p.y()));
        boundsMin.setZ(std::min(boundsMin.z(), p.z()));
        boundsMax.setX(std::max(boundsMax.x(), p.x()));
        boundsMax.setY(std::max(boundsMax.y(), p.y()));
        boundsMax.setZ(std::max(boundsMax.z(), p.z()));
        hasVertex = true;
    }

    if (!hasVertex) {
        return meshlet;
    }

    meshlet.boundsMin = boundsMin;
    meshlet.boundsMax = boundsMax;
    meshlet.boundsCenter = (boundsMin + boundsMax) * 0.5f;
    meshlet.boundsRadius = 0.0f;
    for (int indexOffset = firstIndex; indexOffset < endIndex; ++indexOffset) {
        if (indexOffset < 0 || indexOffset >= indices.size()) {
            continue;
        }

        const unsigned int vertexIndex = indices[indexOffset];
        if (vertexIndex >= static_cast<unsigned int>(renderData.positions.size())) {
            continue;
        }

        meshlet.boundsRadius = std::max(
            meshlet.boundsRadius,
            (renderData.positions[static_cast<int>(vertexIndex)] - meshlet.boundsCenter).length());
    }

    return meshlet;
}
}

    class Mesh::Impl {
    public:
        AttributeContainer vertexAttrs;
        AttributeContainer faceAttrs;
        AttributeContainer faceVertexAttrs;

        QVector<QVector<int>> polygons; // Face -> list of vertex indices
        QVector<Mesh::MaterialSlot> materialSlots;
        QVector<Mesh::SkinBone> skinBones;
        Mesh::SkinningMethod skinningMethod = Mesh::SkinningMethod::LinearBlend;
        QVector<Mesh::SkinAnimationClip> skinAnimationClips;
        QVector<Mesh::BlendShape> blendShapes;
        QVector<QVector3D> blendBasePositions;
        QVector<QVector3D> blendBaseNormals;
        QVector<QMatrix4x4> activeSkinMatrices;
        QVector<QVector3D> skinBasePositions;
        QVector<QVector3D> skinBaseNormals;

        QVector3D minBounds;
        QVector3D maxBounds;
        QVector3D sphereCenter;
        float sphereRadius = 0.0f;
        std::uint64_t revision = 1;

        // Flattened GPU-ready snapshot of the current pose. Rebuilt only when
        // revision changes, so repeated draws of an unchanged mesh skip the
        // triangulation/expansion loop entirely. Animated meshes bump revision
        // every frame, so they rebuild as before.
        mutable RenderData renderDataCache;
        mutable std::uint64_t renderDataRevision = 0;

        Impl() {}
        ~Impl() {}

        // Copy everything except the render cache: the new Impl starts at a
        // different revision, so an inherited cache would be invalid anyway
        // and memcpy-ing a full vertex snapshot would be pure waste.
        Impl(const Impl& other)
            : vertexAttrs(other.vertexAttrs),
              faceAttrs(other.faceAttrs),
              faceVertexAttrs(other.faceVertexAttrs),
              polygons(other.polygons),
              materialSlots(other.materialSlots),
              skinBones(other.skinBones),
              skinningMethod(other.skinningMethod),
              skinAnimationClips(other.skinAnimationClips),
              blendShapes(other.blendShapes),
              blendBasePositions(other.blendBasePositions),
              blendBaseNormals(other.blendBaseNormals),
              activeSkinMatrices(other.activeSkinMatrices),
              skinBasePositions(other.skinBasePositions),
              skinBaseNormals(other.skinBaseNormals),
              minBounds(other.minBounds),
              maxBounds(other.maxBounds),
              sphereCenter(other.sphereCenter),
              sphereRadius(other.sphereRadius),
              revision(other.revision) {}

        Impl& operator=(const Impl& other) {
            if (this != &other) {
                vertexAttrs = other.vertexAttrs;
                faceAttrs = other.faceAttrs;
                faceVertexAttrs = other.faceVertexAttrs;
                polygons = other.polygons;
                materialSlots = other.materialSlots;
                skinBones = other.skinBones;
                skinningMethod = other.skinningMethod;
                skinAnimationClips = other.skinAnimationClips;
                blendShapes = other.blendShapes;
                blendBasePositions = other.blendBasePositions;
                blendBaseNormals = other.blendBaseNormals;
                activeSkinMatrices = other.activeSkinMatrices;
                skinBasePositions = other.skinBasePositions;
                skinBaseNormals = other.skinBaseNormals;
                minBounds = other.minBounds;
                maxBounds = other.maxBounds;
                sphereCenter = other.sphereCenter;
                sphereRadius = other.sphereRadius;
                revision = other.revision;
                renderDataCache = RenderData();
                renderDataRevision = 0;
            }
            return *this;
        }
    };

    Mesh::Mesh() : impl_(new Impl()) {}
    
    Mesh::Mesh(const Mesh& other) : impl_(new Impl(*other.impl_)) {}
    
    Mesh::Mesh(Mesh&& other) noexcept : impl_(other.impl_) {
        other.impl_ = nullptr;
    }
    
    Mesh::~Mesh() {
        delete impl_;
    }

    Mesh& Mesh::operator=(const Mesh& other) {
        if (this != &other) {
            const auto nextRevision = impl_->revision + 1;
            *impl_ = *other.impl_;
            impl_->revision = nextRevision;
        }
        return *this;
    }

    Mesh& Mesh::operator=(Mesh&& other) noexcept {
        if (this != &other) {
            const auto nextRevision = impl_ ? impl_->revision + 1 : 1;
            delete impl_;
            impl_ = other.impl_;
            other.impl_ = nullptr;
            if (impl_) impl_->revision = nextRevision;
        }
        return *this;
    }

    std::uint64_t Mesh::revision() const {
        return impl_ ? impl_->revision : 0;
    }

    void Mesh::setVertexCount(int count) {
        ++impl_->revision;
        impl_->vertexAttrs.setElementCount(count);
        impl_->skinBasePositions.clear();
        impl_->skinBaseNormals.clear();
    }

    int Mesh::vertexCount() const {
        return impl_->vertexAttrs.elementCount();
    }

    int Mesh::addPolygon(const QVector<int>& vertexIndices) {
        ++impl_->revision;
        impl_->polygons.push_back(vertexIndices);
        impl_->faceAttrs.setElementCount(impl_->polygons.size());
        return impl_->polygons.size() - 1;
    }

    int Mesh::polygonCount() const {
        return impl_->polygons.size();
    }

    Mesh::TopologyStatistics Mesh::analyzeTopology(double areaTolerance, bool weldExactPositions) const {
        TopologyStatistics result;
        if (!impl_) { result.topologyAnalysisAvailable = false; return result; }
        const int vertexCount = impl_->vertexAttrs.elementCount();
        const auto positions = impl_->vertexAttrs.get<QVector3D>("position");
        result.positionAnalysisAvailable = positions && positions->size() >= vertexCount;
        if (!std::isfinite(areaTolerance) || areaTolerance < 0.0) areaTolerance = 0.0;

        Array<int> identity(static_cast<size_t>(vertexCount), 0);
        for (int i = 0; i < vertexCount; ++i) identity[i] = i;
        if (weldExactPositions) {
            if (!result.positionAnalysisAvailable) { result.topologyAnalysisAvailable = false; return result; }
            Array<int> order(static_cast<size_t>(vertexCount), 0);
            for (int i = 0; i < vertexCount; ++i) {
                const auto& p = (*positions)[i];
                if (!std::isfinite(p.x()) || !std::isfinite(p.y()) || !std::isfinite(p.z())) {
                    result.topologyAnalysisAvailable = false;
                    return result;
                }
                order[i] = i;
            }
            if (order.size() > 1) std::sort(order.begin(), order.end(), [&positions](int a, int b) {
                const auto& x = (*positions)[a]; const auto& y = (*positions)[b];
                if (x.x() != y.x()) return x.x() < y.x();
                if (x.y() != y.y()) return x.y() < y.y();
                if (x.z() != y.z()) return x.z() < y.z();
                return a < b;
            });
            int representative = -1;
            for (size_t i = 0; i < order.size(); ++i) {
                if (i == 0) {
                    representative = order[i];
                } else {
                    const auto& a = (*positions)[order[i]];
                    const auto& b = (*positions)[order[i - 1]];
                    if (a.x() != b.x() || a.y() != b.y() || a.z() != b.z()) representative = order[i];
                }
                identity[order[i]] = representative;
            }
        }
        for (int i = 0; i < vertexCount; ++i) if (identity[i] == i) ++result.topologyVertexCount;

        struct Edge { int a; int b; int face; bool forward; };
        struct Face { Array<int> vertices; };
        Array<Edge> edges;
        Array<Face> faces;
        Array<int> parent(static_cast<size_t>(vertexCount), 0);
        Array<int> used(static_cast<size_t>(vertexCount), 0);
        Array<int> windingFaces(static_cast<size_t>(impl_->polygons.size()), 0);
        for (int i = 0; i < vertexCount; ++i) parent[i] = i;
        auto root = [&parent](int v) {
            while (parent[v] != v) {
                parent[v] = parent[parent[v]];
                v = parent[v];
            }
            return v;
        };
        for (int faceIndex = 0; faceIndex < impl_->polygons.size(); ++faceIndex) {
            const auto& polygon = impl_->polygons[faceIndex];
            bool valid = polygon.size() >= 3;
            bool indicesValid = true;
            Array<int> sorted;
            sorted.reserve(static_cast<size_t>(polygon.size()));
            for (int v : polygon) {
                if (v < 0 || v >= vertexCount) { valid = false; indicesValid = false; }
                else sorted.append(identity[v]);
            }
            if (sorted.size() > 1) {
                std::sort(sorted.begin(), sorted.end());
                for (size_t i = 1; i < sorted.size(); ++i)
                    if (sorted[i] == sorted[i - 1]) valid = false;
            }
            if (!valid) ++result.invalidPolygonCount;

            if (indicesValid && result.positionAnalysisAvailable) {
                for (int i = 1; i + 1 < polygon.size(); ++i) {
                    const auto& a = (*positions)[polygon[0]];
                    const auto& b = (*positions)[polygon[i]];
                    const auto& c = (*positions)[polygon[i + 1]];
                    if (!std::isfinite(a.x()) || !std::isfinite(a.y()) || !std::isfinite(a.z()) ||
                        !std::isfinite(b.x()) || !std::isfinite(b.y()) || !std::isfinite(b.z()) ||
                        !std::isfinite(c.x()) || !std::isfinite(c.y()) || !std::isfinite(c.z())) {
                        ++result.nonFinitePositionTriangleCount;
                        continue;
                    }
                    const double ux = double(b.x()) - a.x(), uy = double(b.y()) - a.y(), uz = double(b.z()) - a.z();
                    const double vx = double(c.x()) - a.x(), vy = double(c.y()) - a.y(), vz = double(c.z()) - a.z();
                    const double x = uy * vz - uz * vy, y = uz * vx - ux * vz, z = ux * vy - uy * vx;
                    if (0.5 * std::sqrt(x*x + y*y + z*z) <= areaTolerance) ++result.degenerateTriangleCount;
                }
            }
            if (!valid) continue;
            ++result.validPolygonCount;

            // Canonical cyclic sequence preserves polygon adjacency, while
            // treating rotations and reversed winding as the same face.
            Face face;
            const int n = static_cast<int>(polygon.size());
            int start = 0;
            for (int i = 1; i < n; ++i) if (identity[polygon[i]] < identity[polygon[start]]) start = i;
            const int direction = identity[polygon[(start + 1) % n]] < identity[polygon[(start + n - 1) % n]] ? 1 : -1;
            face.vertices.reserve(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) face.vertices.append(identity[polygon[(start + direction * i + n) % n]]);
            faces.append(std::move(face));
            for (int i = 0; i < n; ++i) {
                const int a = identity[polygon[i]];
                const int b = identity[polygon[(i + 1) % n]];
                used[a] = 1;
                const int ra = root(a), rb = root(b);
                if (ra != rb) parent[rb] = ra;
                edges.append(Edge{std::min(a, b), std::max(a, b), faceIndex, a < b});
            }
        }
        if (edges.size() > 1) std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
            return a.a != b.a ? a.a < b.a : a.b < b.b;
        });
        for (size_t i = 0; i < edges.size();) {
            size_t j = i + 1;
            while (j < edges.size() && edges[j].a == edges[i].a && edges[j].b == edges[i].b) ++j;
            ++result.edgeCount;
            if (j - i == 1) ++result.boundaryEdgeCount;
            else if (j - i > 2) ++result.nonManifoldEdgeCount;
            else if (edges[i].forward == edges[i + 1].forward) {
                ++result.inconsistentWindingEdgeCount;
                windingFaces[edges[i].face] = 1;
                windingFaces[edges[i + 1].face] = 1;
            }
            i = j;
        }
        for (int marked : windingFaces) result.inconsistentWindingFaceCount += marked;
        if (faces.size() > 1) std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) {
            return std::lexicographical_compare(a.vertices.begin(), a.vertices.end(), b.vertices.begin(), b.vertices.end());
        });
        for (size_t i = 1; i < faces.size(); ++i) {
            const auto& a = faces[i - 1].vertices;
            const auto& b = faces[i].vertices;
            if (a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin())) ++result.duplicateFaceCount;
        }
        for (int i = 0; i < vertexCount; ++i) {
            if (identity[i] != i) continue;
            if (!used[i]) ++result.isolatedVertexCount;
            if (root(i) == i) ++result.connectedComponentCount;
        }
        result.eulerCharacteristic = std::int64_t(result.topologyVertexCount) - result.edgeCount + result.validPolygonCount;
        return result;
    }

    Mesh::SelfIntersectionStatistics Mesh::analyzeSelfIntersections(double tolerance, bool weldExactPositions) const {
        SelfIntersectionStatistics result;
        if (!impl_) return result;
        const int vertexCount = impl_->vertexAttrs.elementCount();
        const auto positions = impl_->vertexAttrs.get<QVector3D>("position");
        result.positionAnalysisAvailable = positions && positions->size() >= vertexCount;
        if (!result.positionAnalysisAvailable) return result;
        if (!std::isfinite(tolerance) || tolerance < 0.0) tolerance = 0.0;
        struct Vec { double x, y, z; };
        const auto sub = [](Vec a, Vec b) { return Vec{a.x-b.x, a.y-b.y, a.z-b.z}; };
        const auto cross = [](Vec a, Vec b) { return Vec{a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; };
        const auto dot = [](Vec a, Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; };
        struct Triangle { int polygon; int index[3]; Vec p[3]; Vec boundsMin; Vec boundsMax; };
        Array<Triangle> triangles;
        for (int face = 0; face < impl_->polygons.size(); ++face) {
            const auto& polygon = impl_->polygons[face];
            bool valid = polygon.size() >= 3;
            Array<int> sorted;
            sorted.reserve(static_cast<size_t>(polygon.size()));
            for (int v : polygon) { if (v < 0 || v >= vertexCount) valid = false; sorted.append(v); }
            if (sorted.size() > 1) {
                std::sort(sorted.begin(), sorted.end());
                for (size_t i = 1; i < sorted.size(); ++i) if (sorted[i] == sorted[i-1]) valid = false;
            }
            if (!valid) { ++result.invalidPolygonCount; continue; }
            for (int i = 1; i + 1 < polygon.size(); ++i) {
                Triangle t{face, {polygon[0], polygon[i], polygon[i+1]}, {}, {}, {}};
                bool finite = true;
                for (int k = 0; k < 3; ++k) {
                    const auto& p = (*positions)[t.index[k]];
                    t.p[k] = Vec{p.x(), p.y(), p.z()};
                    finite = finite && std::isfinite(p.x()) && std::isfinite(p.y()) && std::isfinite(p.z());
                }
                const Vec normal = cross(sub(t.p[1], t.p[0]), sub(t.p[2], t.p[0]));
                if (!finite || dot(normal, normal) == 0.0) { ++result.skippedTriangleCount; continue; }
                t.boundsMin = t.boundsMax = t.p[0];
                for (int k = 1; k < 3; ++k) {
                    t.boundsMin.x = std::min(t.boundsMin.x, t.p[k].x);
                    t.boundsMin.y = std::min(t.boundsMin.y, t.p[k].y);
                    t.boundsMin.z = std::min(t.boundsMin.z, t.p[k].z);
                    t.boundsMax.x = std::max(t.boundsMax.x, t.p[k].x);
                    t.boundsMax.y = std::max(t.boundsMax.y, t.p[k].y);
                    t.boundsMax.z = std::max(t.boundsMax.z, t.p[k].z);
                }
                triangles.append(t);
            }
        }
        for (size_t i = 0; i < triangles.size(); ++i) {
            for (size_t j = i + 1; j < triangles.size(); ++j) {
                const auto& a = triangles[i];
                const auto& b = triangles[j];
                if (a.polygon == b.polygon) { ++result.samePolygonPairCount; continue; }
                bool adjacent = false;
                for (int x = 0; x < 3; ++x) {
                    for (int y = 0; y < 3; ++y) {
                        if (a.index[x] == b.index[y] ||
                            (weldExactPositions && a.p[x].x == b.p[y].x &&
                             a.p[x].y == b.p[y].y && a.p[x].z == b.p[y].z)) adjacent = true;
                    }
                }
                if (adjacent) { ++result.adjacentPairCount; continue; }
                // At zero tolerance, disjoint AABBs cannot intersect. Leave
                // the tolerance path unchanged: its finite set of SAT axes
                // has a different approximate proximity interpretation.
                if (tolerance == 0.0 &&
                    (a.boundsMax.x < b.boundsMin.x || b.boundsMax.x < a.boundsMin.x ||
                     a.boundsMax.y < b.boundsMin.y || b.boundsMax.y < a.boundsMin.y ||
                     a.boundsMax.z < b.boundsMin.z || b.boundsMax.z < a.boundsMin.z)) continue;
                const Vec ae[3] = {sub(a.p[1], a.p[0]), sub(a.p[2], a.p[1]), sub(a.p[0], a.p[2])};
                const Vec be[3] = {sub(b.p[1], b.p[0]), sub(b.p[2], b.p[1]), sub(b.p[0], b.p[2])};
                const Vec an = cross(ae[0], ae[1]);
                const Vec bn = cross(be[0], be[1]);
                // Triangle separating axes plus in-plane edge normals for
                // coplanar triangles. Projection uses a local origin to avoid
                // losing precision for meshes far from the origin.
                auto separated = [&](Vec axis) {
                    const double length = std::sqrt(dot(axis, axis));
                    if (length == 0.0) return false;
                    double amin = 0.0, amax = 0.0;
                    for (int k = 1; k < 3; ++k) {
                        const double p = dot(sub(a.p[k], a.p[0]), axis);
                        amin = std::min(amin, p); amax = std::max(amax, p);
                    }
                    double bmin = dot(sub(b.p[0], a.p[0]), axis), bmax = bmin;
                    for (int k = 1; k < 3; ++k) {
                        const double p = dot(sub(b.p[k], a.p[0]), axis);
                        bmin = std::min(bmin, p); bmax = std::max(bmax, p);
                    }
                    const double margin = tolerance * length;
                    return amax + margin < bmin || bmax + margin < amin;
                };
                bool apart = separated(an) || separated(bn);
                for (int x = 0; x < 3 && !apart; ++x) {
                    apart = separated(cross(an, ae[x])) || separated(cross(bn, be[x]));
                    for (int y = 0; y < 3 && !apart; ++y) apart = separated(cross(ae[x], be[y]));
                }
                if (!apart) ++result.intersectingPairCount;
            }
        }
        return result;
    }

    int Mesh::addMaterialSlot(int firstIndex, int indexCount, int materialIndex) {
        ++impl_->revision;
        impl_->materialSlots.push_back(
            MaterialSlot{firstIndex, indexCount, materialIndex});
        return impl_->materialSlots.size() - 1;
    }

    int Mesh::materialSlotCount() const {
        return impl_->materialSlots.size();
    }

    const QVector<Mesh::MaterialSlot>& Mesh::materialSlots() const {
        return impl_->materialSlots;
    }

    void Mesh::clearMaterialSlots() {
        ++impl_->revision;
        impl_->materialSlots.clear();
    }

    AttributeContainer& Mesh::vertexAttributes() { ++impl_->revision; return impl_->vertexAttrs; }
    const AttributeContainer& Mesh::vertexAttributes() const { return impl_->vertexAttrs; }

    AttributeContainer& Mesh::faceAttributes() { ++impl_->revision; return impl_->faceAttrs; }
    const AttributeContainer& Mesh::faceAttributes() const { return impl_->faceAttrs; }

    AttributeContainer& Mesh::faceVertexAttributes() { ++impl_->revision; return impl_->faceVertexAttrs; }
    const AttributeContainer& Mesh::faceVertexAttributes() const { return impl_->faceVertexAttrs; }

    const QVector<Mesh::SkinBone>& Mesh::skinBones() const {
        static const QVector<Mesh::SkinBone> empty;
        return impl_ ? impl_->skinBones : empty;
    }

    void Mesh::setSkinBones(const QVector<Mesh::SkinBone>& bones) {
        if (!impl_) return;
        impl_->skinBones = bones;
        impl_->activeSkinMatrices.clear();
        impl_->skinBasePositions.clear();
        impl_->skinBaseNormals.clear();
        ++impl_->revision;
    }

    bool Mesh::setSkinPoseMatrices(
        const QVector<QMatrix4x4>& poseMatrices) {
        if (!impl_ || poseMatrices.size() != impl_->skinBones.size() ||
            poseMatrices.isEmpty()) {
            return false;
        }
        for (int index = 0; index < poseMatrices.size(); ++index) {
            impl_->skinBones[index].poseMatrix = poseMatrices[index];
        }
        ++impl_->revision;
        return true;
    }

    QVector<QMatrix4x4> Mesh::skinPoseMatrices() const {
        QVector<QMatrix4x4> result;
        if (!impl_) return result;
        result.reserve(impl_->skinBones.size());
        for (const Mesh::SkinBone& bone : impl_->skinBones) {
            // ufbx's geometry_to_world already contains the evaluated
            // bone-to-world pose multiplied by geometry_to_bone.
            result.push_back(bone.poseMatrix);
        }
        return result;
    }

    void Mesh::invalidateSkinningBase() {
        if (!impl_) return;
        impl_->skinBasePositions.clear();
        impl_->skinBaseNormals.clear();
    }

    Mesh::SkinningMethod Mesh::skinningMethod() const {
        return impl_ ? impl_->skinningMethod
                     : Mesh::SkinningMethod::LinearBlend;
    }

    void Mesh::setSkinningMethod(const Mesh::SkinningMethod method) {
        if (!impl_) return;
        Mesh::SkinningMethod normalized = Mesh::SkinningMethod::LinearBlend;
        switch (method) {
        case Mesh::SkinningMethod::LinearBlend:
        case Mesh::SkinningMethod::Rigid:
        case Mesh::SkinningMethod::DualQuaternion:
        case Mesh::SkinningMethod::BlendedDualQuaternion:
            normalized = method;
            break;
        default:
            break;
        }
        if (impl_->skinningMethod != normalized) {
            impl_->skinningMethod = normalized;
            ++impl_->revision;
        }
    }

    bool Mesh::hasExtendedSkinningWeights() const {
        if (!impl_) return false;
        const auto weights = impl_->vertexAttrs.get<QVector4D>(
            "boneWeightsExtra");
        if (!weights) return false;
        for (const QVector4D& value : weights->data()) {
            if (std::isfinite(value.x()) && value.x() > 0.0f) return true;
            if (std::isfinite(value.y()) && value.y() > 0.0f) return true;
            if (std::isfinite(value.z()) && value.z() > 0.0f) return true;
            if (std::isfinite(value.w()) && value.w() > 0.0f) return true;
        }
        return false;
    }

    void Mesh::restoreSkinningBase() {
        if (!impl_ || impl_->skinBasePositions.isEmpty()) return;
        auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
        auto normAttr = impl_->vertexAttrs.get<QVector3D>("normal");
        auto tangentAttr = impl_->vertexAttrs.get<QVector4D>("tangent");
        if (!posAttr || posAttr->size() != impl_->skinBasePositions.size()) {
            return;
        }
        posAttr->data() = impl_->skinBasePositions;
        if (normAttr && normAttr->size() == impl_->skinBaseNormals.size()) {
            normAttr->data() = impl_->skinBaseNormals;
        }
        ++impl_->revision;
        updateBounds();
    }

    const QVector<Mesh::SkinAnimationClip>& Mesh::skinAnimationClips() const {
        static const QVector<Mesh::SkinAnimationClip> empty;
        return impl_ ? impl_->skinAnimationClips : empty;
    }

    const Mesh::SkinAnimationClip* Mesh::skinAnimationClip(const int index) const {
        if (!impl_ || index < 0 || index >= impl_->skinAnimationClips.size()) {
            return nullptr;
        }
        return &impl_->skinAnimationClips[index];
    }

    void Mesh::setSkinAnimationClips(const QVector<Mesh::SkinAnimationClip>& clips) {
        if (!impl_) return;
        impl_->skinAnimationClips = clips;
        for (Mesh::SkinAnimationClip& clip : impl_->skinAnimationClips) {
            if (!std::isfinite(clip.timeBegin) ||
                !std::isfinite(clip.timeEnd)) {
                clip.timeBegin = 0.0;
                clip.timeEnd = 0.0;
            } else if (clip.timeEnd < clip.timeBegin) {
                std::swap(clip.timeBegin, clip.timeEnd);
            }
        }
        ++impl_->revision;
    }

    const QVector<Mesh::BlendShape>& Mesh::blendShapes() const {
        static const QVector<Mesh::BlendShape> empty;
        return impl_ ? impl_->blendShapes : empty;
    }

    void Mesh::setBlendShapes(const QVector<Mesh::BlendShape>& shapes) {
        if (!impl_) return;
        impl_->blendShapes = shapes;
        impl_->blendBasePositions.clear();
        impl_->blendBaseNormals.clear();
        ++impl_->revision;
    }

    float Mesh::blendShapeWeight(const int index) const {
        if (!impl_ || index < 0 || index >= impl_->blendShapes.size()) {
            return 0.0f;
        }
        return impl_->blendShapes[index].weight;
    }

    void Mesh::setBlendShapeWeight(const int index, const float weight) {
        if (!impl_ || index < 0 || index >= impl_->blendShapes.size() ||
            !std::isfinite(weight)) return;
        impl_->blendShapes[index].weight = weight;
        applyDeformers(impl_->activeSkinMatrices);
    }

    void Mesh::applyBlendShapes() {
        if (!impl_ || impl_->blendShapes.isEmpty()) return;
        auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
        auto normAttr = impl_->vertexAttrs.get<QVector3D>("normal");
        if (!posAttr) return;
        const int vertexCount = impl_->vertexAttrs.elementCount();
        if (impl_->blendBasePositions.size() != vertexCount) {
            impl_->blendBasePositions = posAttr->data();
            impl_->blendBaseNormals.clear();
            if (normAttr) impl_->blendBaseNormals = normAttr->data();
        }
        for (int vertex = 0; vertex < vertexCount; ++vertex) {
            QVector3D position = impl_->blendBasePositions[vertex];
            if (!std::isfinite(position.x()) ||
                !std::isfinite(position.y()) ||
                !std::isfinite(position.z())) {
                position = QVector3D(0, 0, 0);
            }
            QVector3D normal = !impl_->blendBaseNormals.isEmpty()
                ? impl_->blendBaseNormals[vertex] : QVector3D(0, 1, 0);
            if (!std::isfinite(normal.x()) || !std::isfinite(normal.y()) ||
                !std::isfinite(normal.z()) ||
                normal.lengthSquared() <= 1.0e-12f) {
                normal = QVector3D(0, 1, 0);
            }
            bool hasNormalOffset = false;
            for (const Mesh::BlendShape& shape : impl_->blendShapes) {
                if (!std::isfinite(shape.weight) || shape.weight == 0.0f) {
                    continue;
                }
                if (vertex < shape.positionOffsets.size()) {
                    const QVector3D positionOffset = shape.positionOffsets[vertex];
                    if (std::isfinite(positionOffset.x()) &&
                        std::isfinite(positionOffset.y()) &&
                        std::isfinite(positionOffset.z())) {
                        position += positionOffset * shape.weight;
                    }
                }
                if (vertex < shape.normalOffsets.size()) {
                    const QVector3D normalOffset = shape.normalOffsets[vertex];
                    if (std::isfinite(normalOffset.x()) &&
                        std::isfinite(normalOffset.y()) &&
                        std::isfinite(normalOffset.z())) {
                        normal += normalOffset * shape.weight;
                        hasNormalOffset = true;
                    }
                }
            }
            if (!std::isfinite(position.x()) ||
                !std::isfinite(position.y()) ||
                !std::isfinite(position.z())) {
                position = QVector3D(0, 0, 0);
            }
            (*posAttr)[vertex] = position;
            const float normalLengthSquared = normal.lengthSquared();
            if (normAttr && hasNormalOffset &&
                std::isfinite(normalLengthSquared) &&
                normalLengthSquared > 1.0e-12f) {
                (*normAttr)[vertex] = normal.normalized();
            }
        }
        ++impl_->revision;
        updateBounds();
    }

    QVector<int> Mesh::getConnectedPolygons(int vertexIndex) const {
        QVector<int> result;
        for (int i = 0; i < impl_->polygons.size(); ++i) {
            if (impl_->polygons[i].contains(vertexIndex)) {
                result.push_back(i);
            }
        }
        return result;
    }

    QVector<int> Mesh::getPolygonVertices(int polygonIndex) const {
        if (polygonIndex >= 0 && polygonIndex < impl_->polygons.size()) {
            return impl_->polygons[polygonIndex];
        }
        return QVector<int>();
    }

    // スキニング用のウェイト構造体 (最大4ボーン)
    struct BoneWeight {
        int boneIndices[4] = {0, 0, 0, 0};
        float weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    SharedPtr<Mesh> Mesh::createSubdivided(int level) const {
        if (level <= 0 || impl_->polygons.empty()) return makeShared<Mesh>(*this);

        auto subdivided = makeShared<Mesh>();
        auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
        auto normAttr = impl_->vertexAttrs.get<QVector3D>("normal");
        if (!posAttr) return subdivided;

        auto newPosAttr = subdivided->vertexAttributes().add<QVector3D>("position");
        auto newNormAttr = subdivided->vertexAttributes().add<QVector3D>("normal");

        int originalVertexCount = impl_->vertexAttrs.elementCount();
        int faceCount = impl_->polygons.size();
        
        // 元の頂点をコピー
        subdivided->setVertexCount(originalVertexCount);
        for (int i = 0; i < originalVertexCount; ++i) {
            (*newPosAttr)[i] = (*posAttr)[i];
            if (normAttr) (*newNormAttr)[i] = (*normAttr)[i];
        }

        // 簡易 Catmull-Clark (Linear Subdivision): 各ポリゴンをN個の四角形に分割
        // 1. 各面の中点 (Face Point) を計算して追加
        QVector<int> facePointIndices;
        facePointIndices.resize(faceCount);
        for (int i = 0; i < faceCount; ++i) {
            const auto& poly = impl_->polygons[i];
            QVector3D faceCenter(0, 0, 0);
            QVector3D faceNormal(0, 0, 0);
            for (int vIdx : poly) {
                faceCenter += (*posAttr)[vIdx];
                if (normAttr) faceNormal += (*normAttr)[vIdx];
            }
            faceCenter /= std::max<int>(1, (int)poly.size());
            if (normAttr) faceNormal.normalize();

            int fpIdx = subdivided->vertexCount();
            subdivided->setVertexCount(fpIdx + 1);
            (*newPosAttr)[fpIdx] = faceCenter;
            if (normAttr) (*newNormAttr)[fpIdx] = faceNormal;
            facePointIndices[i] = fpIdx;
        }

        // 2. 新しいポリゴン(Quad)を構築
        for (int i = 0; i < faceCount; ++i) {
            const auto& poly = impl_->polygons[i];
            int fpIdx = facePointIndices[i];
            int n = poly.size();

            for (int j = 0; j < n; ++j) {
                int v0 = poly[j];
                int v1 = poly[(j + 1) % n];
                int v_prev = poly[(j - 1 + n) % n];

                // エッジの中点 (簡易版: 共有エッジを考慮せず独自に生成)
                QVector3D edgeCenter1 = ((*posAttr)[v0] + (*posAttr)[v1]) * 0.5f;
                QVector3D edgeCenter2 = ((*posAttr)[v0] + (*posAttr)[v_prev]) * 0.5f;

                int epIdx1 = subdivided->vertexCount();
                subdivided->setVertexCount(epIdx1 + 2);
                (*newPosAttr)[epIdx1] = edgeCenter1;
                (*newPosAttr)[epIdx1 + 1] = edgeCenter2;

                if (normAttr) {
                    (*newNormAttr)[epIdx1] = ((*normAttr)[v0] + (*normAttr)[v1]).normalized();
                    (*newNormAttr)[epIdx1 + 1] = ((*normAttr)[v0] + (*normAttr)[v_prev]).normalized();
                }

                // 新しい四角形: [元の頂点, 次のエッジ中点, 面の中点, 前のエッジ中点]
                subdivided->addPolygon({v0, epIdx1, fpIdx, epIdx1 + 1});
            }
        }

        // レベルが2以上の場合は再帰的に分割
        if (level > 1) {
            return subdivided->createSubdivided(level - 1);
        }
        
        subdivided->updateBounds();
        return subdivided;
    }

    Mesh::RenderData Mesh::generateRenderData() const {
        // Serve the previous snapshot while the mesh is unchanged. Callers get
        // a copy, so the cached data can never be mutated through the result.
        if (impl_->renderDataRevision == impl_->revision) {
            return impl_->renderDataCache;
        }

        RenderData data;
        auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
        auto normAttr = impl_->vertexAttrs.get<QVector3D>("normal");
        auto tangentAttr = impl_->vertexAttrs.get<QVector4D>("tangent");
        auto uvAttr = impl_->vertexAttrs.get<QVector2D>("uv");
        auto uv1Attr = impl_->vertexAttrs.get<QVector2D>("uv1");
        auto colorAttr = impl_->vertexAttrs.get<QVector4D>("color");

        if (!posAttr) return data;

        int generatedTriangleCount = 0;
        for (const auto& polygon : impl_->polygons) {
            if (polygon.size() >= 3) generatedTriangleCount += polygon.size() - 2;
        }
        const bool hasMaterialSlots = !impl_->materialSlots.isEmpty();

        // 簡単なTriangulate（ファン分割）
        for (int i = 0; i < impl_->polygons.size(); ++i) {
            const auto& poly = impl_->polygons[i];
            if (poly.size() < 3) continue;

            int v0 = poly[0];
            for (int j = 1; j < poly.size() - 1; ++j) {
                int v1 = poly[j];
                int v2 = poly[j + 1];

                // 頂点の展開 (Flat化)
                data.indices.push_back(data.positions.size());
                data.positions.push_back((*posAttr)[v0]);
                if (normAttr) data.normals.push_back((*normAttr)[v0]);
                if (tangentAttr) data.tangents.push_back((*tangentAttr)[v0]);
                if (uvAttr) data.uvs.push_back((*uvAttr)[v0]);
                if (uv1Attr) data.uvs1.push_back((*uv1Attr)[v0]);
                data.colors.push_back(colorAttr ? (*colorAttr)[v0] : QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

                data.indices.push_back(data.positions.size());
                data.positions.push_back((*posAttr)[v1]);
                if (normAttr) data.normals.push_back((*normAttr)[v1]);
                if (tangentAttr) data.tangents.push_back((*tangentAttr)[v1]);
                if (uvAttr) data.uvs.push_back((*uvAttr)[v1]);
                if (uv1Attr) data.uvs1.push_back((*uv1Attr)[v1]);
                data.colors.push_back(colorAttr ? (*colorAttr)[v1] : QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

                data.indices.push_back(data.positions.size());
                data.positions.push_back((*posAttr)[v2]);
                if (normAttr) data.normals.push_back((*normAttr)[v2]);
                if (tangentAttr) data.tangents.push_back((*tangentAttr)[v2]);
                if (uvAttr) data.uvs.push_back((*uvAttr)[v2]);
                if (uv1Attr) data.uvs1.push_back((*uv1Attr)[v2]);
                data.colors.push_back(colorAttr ? (*colorAttr)[v2] : QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }

        if (hasMaterialSlots) {
            data.triangleMaterialIndices.resize(generatedTriangleCount);
            for (const auto& slot : impl_->materialSlots) {
                if (slot.firstIndex < 0 || slot.indexCount <= 0 ||
                    slot.materialIndex < 0 || (slot.firstIndex % 3) != 0 ||
                    (slot.indexCount % 3) != 0) {
                    continue;
                }
                const size_t firstTriangle =
                    static_cast<size_t>(slot.firstIndex / 3);
                const size_t triangleCount =
                    static_cast<size_t>(slot.indexCount / 3);
                const size_t endTriangle = std::min(
                    static_cast<size_t>(data.triangleMaterialIndices.size()),
                    firstTriangle + triangleCount);
                for (size_t triangle = firstTriangle;
                     triangle < endTriangle; ++triangle) {
                    data.triangleMaterialIndices[static_cast<qsizetype>(triangle)] =
                        static_cast<unsigned int>(slot.materialIndex);
                }
            }
        }

        impl_->renderDataCache = data;
        impl_->renderDataRevision = impl_->revision;
        return data;
    }

    Mesh::MeshletLODData Mesh::generateMeshletLODData() const
    {
        return generateMeshletLODData(MeshletLODConfig{});
    }

    Mesh::MeshletLODData Mesh::generateMeshletLODData(const MeshletLODConfig& config) const
    {
        MeshletLODData data;
        data.renderData = generateRenderData();
        const size_t sourceIndexCount = static_cast<size_t>(data.renderData.indices.size());
        const size_t sourceVertexCount = static_cast<size_t>(data.renderData.positions.size());
        if (sourceIndexCount < kTriangleIndexCount || sourceVertexCount == 0) {
            return data;
        }

        const int maxTrianglesPerMeshlet = std::max(1, config.maxTrianglesPerMeshlet);
        const int maxLODLevels = std::max(1, config.maxLODLevels);

        struct PackedVertex {
            QVector3D position;
            QVector3D normal;
            QVector4D tangent;
            QVector2D uv;
            QVector2D uv1;
            QVector4D color;
        };

        std::vector<PackedVertex> vertices(sourceVertexCount);
        const bool hasNormals = data.renderData.normals.size() == data.renderData.positions.size();
        const bool hasTangents = data.renderData.tangents.size() == data.renderData.positions.size();
        const bool hasUVs = data.renderData.uvs.size() == data.renderData.positions.size();
        const bool hasUVs1 = data.renderData.uvs1.size() == data.renderData.positions.size();
        const bool hasColors = data.renderData.colors.size() == data.renderData.positions.size();
        for (size_t i = 0; i < sourceVertexCount; ++i) {
            vertices[i].position = data.renderData.positions[static_cast<qsizetype>(i)];
            if (hasNormals) {
                vertices[i].normal = data.renderData.normals[static_cast<qsizetype>(i)];
            }
            if (hasTangents) {
                vertices[i].tangent = data.renderData.tangents[static_cast<qsizetype>(i)];
            }
            if (hasUVs) {
                vertices[i].uv = data.renderData.uvs[static_cast<qsizetype>(i)];
            }
            if (hasUVs1) {
                vertices[i].uv1 = data.renderData.uvs1[static_cast<qsizetype>(i)];
            }
            if (hasColors) {
                vertices[i].color = data.renderData.colors[static_cast<qsizetype>(i)];
            } else {
                vertices[i].color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
            }
        }

        NamedVector<unsigned int> sourceIndices;
        sourceIndices.reserve(sourceIndexCount);
        for (const unsigned int index : data.renderData.indices) {
            sourceIndices.add(index);
        }

        std::vector<unsigned int> remap(sourceVertexCount);
        const size_t remappedVertexCount = meshopt_generateVertexRemap(
            remap.data(), sourceIndices.data(), sourceIndices.size(),
            vertices.data(), vertices.size(), sizeof(PackedVertex));

        std::vector<PackedVertex> remappedVertices(remappedVertexCount);
        meshopt_remapVertexBuffer(remappedVertices.data(), vertices.data(), vertices.size(),
                                   sizeof(PackedVertex), remap.data());
        std::vector<unsigned int> optimizedIndices(sourceIndices.size());
        meshopt_remapIndexBuffer(optimizedIndices.data(), sourceIndices.data(), sourceIndices.size(), remap.data());
        meshopt_optimizeVertexCache(optimizedIndices.data(), optimizedIndices.data(),
                                    optimizedIndices.size(), remappedVertexCount);

        data.renderData.positions.resize(static_cast<qsizetype>(remappedVertexCount));
        if (hasNormals) data.renderData.normals.resize(static_cast<qsizetype>(remappedVertexCount));
        if (hasTangents) data.renderData.tangents.resize(static_cast<qsizetype>(remappedVertexCount));
        if (hasUVs) data.renderData.uvs.resize(static_cast<qsizetype>(remappedVertexCount));
        if (hasUVs1) data.renderData.uvs1.resize(static_cast<qsizetype>(remappedVertexCount));
        data.renderData.colors.resize(static_cast<qsizetype>(remappedVertexCount));
        for (size_t i = 0; i < remappedVertexCount; ++i) {
            data.renderData.positions[static_cast<qsizetype>(i)] = remappedVertices[i].position;
            if (hasNormals) data.renderData.normals[static_cast<qsizetype>(i)] = remappedVertices[i].normal;
            if (hasTangents) data.renderData.tangents[static_cast<qsizetype>(i)] = remappedVertices[i].tangent;
            if (hasUVs) data.renderData.uvs[static_cast<qsizetype>(i)] = remappedVertices[i].uv;
            if (hasUVs1) data.renderData.uvs1[static_cast<qsizetype>(i)] = remappedVertices[i].uv1;
            data.renderData.colors[static_cast<qsizetype>(i)] = remappedVertices[i].color;
        }

        const int triangleCount = static_cast<int>(optimizedIndices.size() / kTriangleIndexCount);
        int triangleStride = 1;

        for (int level = 0; level < maxLODLevels; ++level) {
            const int levelOffset = data.meshlets.size();
            const int levelFirstIndex = data.lodIndices.size();
            NamedVector<unsigned int> levelIndices;
            if (level == 0) {
                levelIndices.reserve(optimizedIndices.size());
                for (const auto index : optimizedIndices) {
                    levelIndices.append(index);
                }
            } else {
                const int targetTriangleCount = std::max(
                    1, triangleCount / triangleStride);
                const size_t targetIndexCount = static_cast<size_t>(targetTriangleCount) *
                                                kTriangleIndexCount;
                // meshopt_simplify may return more than the target when topology
                // or the error limit prevents further reduction. Its output
                // buffer must hold the full input index count.
                levelIndices.resize(optimizedIndices.size());
                const float targetError = std::min(
                    1.0f, static_cast<float>(triangleStride - 1) /
                              static_cast<float>(std::max(1, triangleCount)));
                const size_t simplifiedIndexCount = meshopt_simplify(
                    levelIndices.data(), optimizedIndices.data(), optimizedIndices.size(),
                    reinterpret_cast<const float*>(&remappedVertices[0].position),
                    remappedVertices.size(), sizeof(PackedVertex), targetIndexCount,
                    targetError);
                levelIndices.resize(simplifiedIndexCount - (simplifiedIndexCount % kTriangleIndexCount));
                if (levelIndices.isEmpty()) {
                    levelIndices.clear();
                    for (const auto index : optimizedIndices) {
                        levelIndices.add(index);
                    }
                }
            }

            const int levelIndexCount = static_cast<int>(levelIndices.size());
            if (levelIndexCount > 0) {
                const size_t maxMeshletVertices = 64;
                const size_t maxMeshletTriangles = static_cast<size_t>(maxTrianglesPerMeshlet);
                const size_t maxMeshlets = meshopt_buildMeshletsBound(
                    static_cast<size_t>(levelIndexCount), maxMeshletVertices, maxMeshletTriangles);
                std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
                std::vector<unsigned int> meshletVertices(maxMeshlets * maxMeshletVertices);
                std::vector<unsigned char> meshletTriangles(maxMeshlets * maxMeshletTriangles * 3);
                const size_t meshletCount = meshopt_buildMeshlets(
                    meshlets.data(), meshletVertices.data(), meshletTriangles.data(),
                    levelIndices.data(), static_cast<size_t>(levelIndexCount),
                    reinterpret_cast<const float*>(&remappedVertices[0].position), remappedVertices.size(),
                    sizeof(PackedVertex), maxMeshletVertices, maxMeshletTriangles, 0.0f);
                for (size_t i = 0; i < meshletCount; ++i) {
                    const meshopt_Meshlet& source = meshlets[i];
                    const int firstIndex = data.lodIndices.size();
                    const int indexCount = static_cast<int>(source.triangle_count * 3);
                    for (size_t triangle = 0; triangle < source.triangle_count; ++triangle) {
                        const size_t triangleOffset = source.triangle_offset + triangle * 3;
                        for (size_t corner = 0; corner < 3; ++corner) {
                            const unsigned int localVertex = meshletTriangles[triangleOffset + corner];
                            data.lodIndices.push_back(meshletVertices[source.vertex_offset + localVertex]);
                        }
                    }
                    data.meshlets.push_back(buildMeshletFromIndexRange(
                        data.renderData, data.lodIndices, firstIndex, indexCount, triangleStride));
                }
            }

            MeshletLODLevel lodLevel;
            lodLevel.level = level;
            lodLevel.meshletOffset = levelOffset;
            lodLevel.meshletCount = data.meshlets.size() - levelOffset;
            lodLevel.firstIndex = static_cast<unsigned int>(levelFirstIndex);
            lodLevel.indexCount = static_cast<unsigned int>(data.lodIndices.size() - levelFirstIndex);
            lodLevel.triangleStride = triangleStride;
            lodLevel.switchDistancePixels = config.lodSwitchPixels / static_cast<float>(triangleStride);
            data.levels.push_back(lodLevel);

            if (triangleStride >= triangleCount) {
                break;
            }

            triangleStride *= 2;
        }

        return data;
    }

    int Mesh::chooseMeshletLODLevel(const MeshletLODData& data, const float projectedRadiusPixels)
    {
        if (data.levels.isEmpty()) {
            return 0;
        }

        int selectedLevel = data.levels.front().level;
        for (const MeshletLODLevel& level : data.levels) {
            if (projectedRadiusPixels <= level.switchDistancePixels) {
                selectedLevel = level.level;
            }
        }

        return selectedLevel;
    }

    void Mesh::applySkinning(const QVector<QMatrix4x4>& boneMatrices) {
        auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
        auto normAttr = impl_->vertexAttrs.get<QVector3D>("normal");
        // PMD and ufbx importers store the common packed representation;
        // ufbx may provide a second packed block for four additional weights.
        // Keep support for the historical internal BoneWeight type as well.
        auto weightAttr = impl_->vertexAttrs.get<BoneWeight>("boneWeights");
        auto packedIndexAttr = impl_->vertexAttrs.get<QVector4D>("boneIndices");
        auto packedWeightAttr = impl_->vertexAttrs.get<QVector4D>("boneWeights");
        auto packedIndexExtraAttr =
            impl_->vertexAttrs.get<QVector4D>("boneIndicesExtra");
        auto packedWeightExtraAttr =
            impl_->vertexAttrs.get<QVector4D>("boneWeightsExtra");
        auto skinDQWeightAttr = impl_->vertexAttrs.get<float>("skinDQWeight");
        auto skinMethodAttr = impl_->vertexAttrs.get<float>("skinMethod");

        if (!posAttr || boneMatrices.isEmpty() ||
            (!weightAttr && (!packedIndexAttr || !packedWeightAttr))) return;
        if (impl_->activeSkinMatrices.size() != boneMatrices.size()) {
            impl_->activeSkinMatrices.resize(boneMatrices.size());
        }
        for (int matrixIndex = 0; matrixIndex < boneMatrices.size();
             ++matrixIndex) {
            impl_->activeSkinMatrices[matrixIndex] = boneMatrices[matrixIndex];
        }

        // Preserve bind-space data so repeated pose updates never compound
        // the previous deformation. The first call captures the imported
        // vertex data; subsequent calls always start from that same source.
        const int vertexCount = impl_->vertexAttrs.elementCount();
        if (impl_->skinBasePositions.size() != vertexCount) {
            impl_->skinBasePositions = posAttr->data();
            impl_->skinBaseNormals.clear();
            if (normAttr) impl_->skinBaseNormals = normAttr->data();
        }
        ++impl_->revision;

        // Linear Blend Skinning (LBS) の評価
        for (int i = 0; i < impl_->vertexAttrs.elementCount(); ++i) {
            const auto packedIndices = packedIndexAttr ? (*packedIndexAttr)[i] : QVector4D();
            const auto packedWeights = packedWeightAttr ? (*packedWeightAttr)[i] : QVector4D();
            const auto packedIndicesExtra = packedIndexExtraAttr
                ? (*packedIndexExtraAttr)[i] : QVector4D(-1, -1, -1, -1);
            const auto packedWeightsExtra = packedWeightExtraAttr
                ? (*packedWeightExtraAttr)[i] : QVector4D();
            const auto& bw = weightAttr ? (*weightAttr)[i] : BoneWeight{};
            QVector3D originalPos = impl_->skinBasePositions[i];
            if (!std::isfinite(originalPos.x()) ||
                !std::isfinite(originalPos.y()) ||
                !std::isfinite(originalPos.z())) {
                originalPos = QVector3D(0, 0, 0);
            }
            QVector3D originalNorm =
                !impl_->skinBaseNormals.isEmpty() ? impl_->skinBaseNormals[i]
                                                  : QVector3D(0,1,0);
            if (!std::isfinite(originalNorm.x()) ||
                !std::isfinite(originalNorm.y()) ||
                !std::isfinite(originalNorm.z()) ||
                originalNorm.lengthSquared() <= 1.0e-12f) {
                originalNorm = QVector3D(0, 1, 0);
            }

            Mesh::SkinningMethod vertexSkinningMethod = impl_->skinningMethod;
            if (skinMethodAttr && i < skinMethodAttr->size() &&
                std::isfinite((*skinMethodAttr)[i])) {
                const float rawMethod = (*skinMethodAttr)[i];
                if (std::trunc(rawMethod) == rawMethod &&
                    rawMethod >= 0.0f && rawMethod <= 3.0f) {
                    vertexSkinningMethod = static_cast<Mesh::SkinningMethod>(
                        static_cast<int>(rawMethod));
                }
            }
            if (vertexSkinningMethod == Mesh::SkinningMethod::DualQuaternion ||
                vertexSkinningMethod == Mesh::SkinningMethod::BlendedDualQuaternion) {
                QQuaternion blendedReal(0, 0, 0, 0);
                QQuaternion blendedDual(0, 0, 0, 0);
                QVector3D linearPos(0, 0, 0);
                QVector3D linearNorm(0, 0, 0);
                QQuaternion referenceReal;
                bool hasReference = false;
                float totalWeight = 0.0f;
                float linearWeight = 0.0f;
                const int influenceCount = packedWeightAttr ? 8 : 4;
                for (int j = 0; j < influenceCount; ++j) {
                    const float w = packedWeightAttr
                        ? (j < 4 ? packedWeights[j] : packedWeightsExtra[j - 4])
                        : bw.weights[j];
                    if (!std::isfinite(w) || w <= 0.0f) continue;
                    const float packedIndex = j < 4
                        ? packedIndices[j] : packedIndicesExtra[j - 4];
                    if (packedIndexAttr &&
                        (!std::isfinite(packedIndex) ||
                         std::trunc(packedIndex) != packedIndex ||
                         packedIndex < static_cast<float>(std::numeric_limits<int>::min()) ||
                         packedIndex > static_cast<float>(std::numeric_limits<int>::max()))) continue;
                    const int boneIndex = packedIndexAttr
                        ? static_cast<int>(packedIndex)
                        : bw.boneIndices[j];
                    if (boneIndex < 0 || boneIndex >= boneMatrices.size()) {
                        continue;
                    }
                    const QMatrix4x4& matrix = boneMatrices[boneIndex];
                    const QVector3D transformedPos = matrix.map(originalPos);
                    const QMatrix3x3 normalMatrix = matrix.normalMatrix();
                    const QVector3D transformedNorm(
                        normalMatrix(0, 0) * originalNorm.x() +
                            normalMatrix(0, 1) * originalNorm.y() +
                            normalMatrix(0, 2) * originalNorm.z(),
                        normalMatrix(1, 0) * originalNorm.x() +
                            normalMatrix(1, 1) * originalNorm.y() +
                            normalMatrix(1, 2) * originalNorm.z(),
                        normalMatrix(2, 0) * originalNorm.x() +
                            normalMatrix(2, 1) * originalNorm.y() +
                            normalMatrix(2, 2) * originalNorm.z());
                    if (std::isfinite(transformedPos.x()) &&
                        std::isfinite(transformedPos.y()) &&
                        std::isfinite(transformedPos.z()) &&
                        std::isfinite(transformedNorm.x()) &&
                        std::isfinite(transformedNorm.y()) &&
                        std::isfinite(transformedNorm.z())) {
                        linearPos += transformedPos * w;
                        linearNorm += transformedNorm * w;
                        linearWeight += w;
                    }
                    DualQuaternion dualQuaternion;
                    if (!makeDualQuaternion(boneMatrices[boneIndex],
                                             dualQuaternion)) continue;
                    if (!hasReference) {
                        referenceReal = dualQuaternion.real;
                        hasReference = true;
                    }
                    if (QQuaternion::dotProduct(referenceReal,
                                                dualQuaternion.real) < 0.0f) {
                        dualQuaternion.real = -dualQuaternion.real;
                        dualQuaternion.dual = -dualQuaternion.dual;
                    }
                    blendedReal += dualQuaternion.real * w;
                    blendedDual += dualQuaternion.dual * w;
                    totalWeight += w;
                }
                const float realLength = blendedReal.length();
                if (hasReference && std::isfinite(totalWeight) &&
                    totalWeight > 0.0f && std::isfinite(realLength) &&
                    realLength > 1.0e-8f) {
                    blendedReal /= realLength;
                    blendedDual /= realLength;
                    const DualQuaternion blended{blendedReal, blendedDual};
                    const QVector3D transformedPosition =
                        transformDualQuaternion(blended, originalPos);
                    const QVector3D transformedNormal =
                        blendedReal.rotatedVector(originalNorm);
                    float dqBlendWeight = 1.0f;
                    if (vertexSkinningMethod ==
                        Mesh::SkinningMethod::BlendedDualQuaternion &&
                        skinDQWeightAttr && i < skinDQWeightAttr->size() &&
                        std::isfinite((*skinDQWeightAttr)[i])) {
                        dqBlendWeight = std::clamp((*skinDQWeightAttr)[i],
                                                   0.0f, 1.0f);
                    }
                    QVector3D outputPosition = transformedPosition;
                    QVector3D outputNormal = transformedNormal;
                    if (vertexSkinningMethod ==
                        Mesh::SkinningMethod::BlendedDualQuaternion &&
                        linearWeight > 0.0f) {
                        const QVector3D normalizedLinearPos =
                            linearPos / linearWeight;
                        const QVector3D normalizedLinearNorm =
                            linearNorm / linearWeight;
                        outputPosition = normalizedLinearPos *
                                              (1.0f - dqBlendWeight) +
                                         transformedPosition * dqBlendWeight;
                        outputNormal = normalizedLinearNorm *
                                           (1.0f - dqBlendWeight) +
                                       transformedNormal * dqBlendWeight;
                    }
                    if (std::isfinite(outputPosition.x()) &&
                        std::isfinite(outputPosition.y()) &&
                        std::isfinite(outputPosition.z()) &&
                        std::isfinite(outputNormal.x()) &&
                        std::isfinite(outputNormal.y()) &&
                        std::isfinite(outputNormal.z())) {
                        (*posAttr)[i] = outputPosition;
                        if (normAttr) {
                            const float normalLengthSquared =
                                outputNormal.lengthSquared();
                            (*normAttr)[i] =
                                std::isfinite(normalLengthSquared) &&
                                        normalLengthSquared > 1.0e-12f
                                    ? outputNormal.normalized()
                                    : originalNorm;
                        }
                        continue;
                    }
                }
                if (vertexSkinningMethod ==
                        Mesh::SkinningMethod::BlendedDualQuaternion &&
                    std::isfinite(linearWeight) && linearWeight > 0.0f) {
                    linearPos /= linearWeight;
                    linearNorm /= linearWeight;
                    if (std::isfinite(linearPos.x()) &&
                        std::isfinite(linearPos.y()) &&
                        std::isfinite(linearPos.z()) &&
                        std::isfinite(linearNorm.x()) &&
                        std::isfinite(linearNorm.y()) &&
                        std::isfinite(linearNorm.z())) {
                        (*posAttr)[i] = linearPos;
                        if (normAttr) {
                            const float normalLengthSquared =
                                linearNorm.lengthSquared();
                            (*normAttr)[i] =
                                std::isfinite(normalLengthSquared) &&
                                        normalLengthSquared > 1.0e-12f
                                    ? linearNorm.normalized()
                                    : originalNorm;
                        }
                        continue;
                    }
                }
                (*posAttr)[i] = originalPos;
                if (normAttr) (*normAttr)[i] = originalNorm;
                continue;
            }

            int rigidBoneIndex = -1;
            float rigidBoneWeight = 0.0f;
            if (vertexSkinningMethod == Mesh::SkinningMethod::Rigid) {
                const int rigidInfluenceCount = packedWeightAttr ? 8 : 4;
                for (int j = 0; j < rigidInfluenceCount; ++j) {
                    const float weight = packedWeightAttr
                        ? (j < 4 ? packedWeights[j]
                                 : packedWeightsExtra[j - 4])
                        : bw.weights[j];
                    const float index = j < 4
                        ? packedIndices[j] : packedIndicesExtra[j - 4];
                    if (!std::isfinite(weight) || weight <= 0.0f ||
                        (packedIndexAttr &&
                         (!std::isfinite(index) || std::trunc(index) != index ||
                          index < static_cast<float>(std::numeric_limits<int>::min()) ||
                          index > static_cast<float>(std::numeric_limits<int>::max())))) {
                        continue;
                    }
                    const int boneIndex = packedIndexAttr
                        ? static_cast<int>(index) : bw.boneIndices[j];
                    if (boneIndex >= 0 && boneIndex < boneMatrices.size() &&
                        weight > rigidBoneWeight) {
                        rigidBoneIndex = boneIndex;
                        rigidBoneWeight = weight;
                    }
                }
            }

            QVector3D skinnedPos(0, 0, 0);
            QVector3D skinnedNorm(0, 0, 0);

            float totalWeight = 0.0f;

            const int influenceCount = packedWeightAttr ? 8 : 4;
            for (int j = 0; j < influenceCount; ++j) {
                const float w = packedWeightAttr
                    ? (j < 4 ? packedWeights[j] : packedWeightsExtra[j - 4])
                    : bw.weights[j];
                if (std::isfinite(w) && w > 0.0f) {
                    if (packedIndexAttr) {
                        const float packedIndex = j < 4
                            ? packedIndices[j] : packedIndicesExtra[j - 4];
                        if (!std::isfinite(packedIndex) ||
                            std::trunc(packedIndex) != packedIndex ||
                            packedIndex < static_cast<float>(std::numeric_limits<int>::min()) ||
                            packedIndex > static_cast<float>(std::numeric_limits<int>::max())) {
                            continue;
                        }
                    }
                    const int bIdx = packedIndexAttr
                        ? static_cast<int>(j < 4 ? packedIndices[j]
                                                : packedIndicesExtra[j - 4])
                        : bw.boneIndices[j];
                    if (vertexSkinningMethod == Mesh::SkinningMethod::Rigid &&
                        bIdx != rigidBoneIndex) {
                        continue;
                    }
                    if (bIdx >= 0 && bIdx < boneMatrices.size()) {
                        const QMatrix4x4& mat = boneMatrices[bIdx];
                        const QVector3D transformedPos = mat.map(originalPos);
                        const QMatrix3x3 normalMatrix = mat.normalMatrix();
                        const QVector3D transformedNorm(
                            normalMatrix(0, 0) * originalNorm.x() +
                                normalMatrix(0, 1) * originalNorm.y() +
                                normalMatrix(0, 2) * originalNorm.z(),
                            normalMatrix(1, 0) * originalNorm.x() +
                                normalMatrix(1, 1) * originalNorm.y() +
                                normalMatrix(1, 2) * originalNorm.z(),
                            normalMatrix(2, 0) * originalNorm.x() +
                                normalMatrix(2, 1) * originalNorm.y() +
                                normalMatrix(2, 2) * originalNorm.z());
                        if (!std::isfinite(transformedPos.x()) ||
                            !std::isfinite(transformedPos.y()) ||
                            !std::isfinite(transformedPos.z()) ||
                            !std::isfinite(transformedNorm.x()) ||
                            !std::isfinite(transformedNorm.y()) ||
                            !std::isfinite(transformedNorm.z())) {
                            continue;
                        }
                        skinnedPos += transformedPos * w;
                        // 法線はboneの逆転置3x3（normal matrix）を適用
                        skinnedNorm += transformedNorm * w;
                        totalWeight += w;
                    }
                }
            }

            const bool finiteSums =
                std::isfinite(skinnedPos.x()) &&
                std::isfinite(skinnedPos.y()) &&
                std::isfinite(skinnedPos.z()) &&
                std::isfinite(skinnedNorm.x()) &&
                std::isfinite(skinnedNorm.y()) &&
                std::isfinite(skinnedNorm.z());
            if (finiteSums && std::isfinite(totalWeight) &&
                totalWeight > 0.0f) {
                if (std::abs(totalWeight - 1.0f) > 1.0e-5f) {
                    skinnedPos /= totalWeight;
                }
                (*posAttr)[i] = skinnedPos;
                if (normAttr) {
                    const float normalLengthSquared =
                        skinnedNorm.lengthSquared();
                    (*normAttr)[i] =
                        std::isfinite(normalLengthSquared) &&
                                normalLengthSquared > 1.0e-12f
                            ? skinnedNorm.normalized()
                            : originalNorm;
                }
            } else {
                // Vertices without a valid influence must also be restored
                // on every evaluation; otherwise a previous animation pose
                // can remain latched on the next frame.
                (*posAttr)[i] = originalPos;
                if (normAttr && !impl_->skinBaseNormals.isEmpty()) {
                    (*normAttr)[i] = originalNorm;
                }
            }
        }
        
        updateBounds();
    }

    void Mesh::applyDeformers(const QVector<QMatrix4x4>& boneMatrices) {
        if (impl_ && impl_->blendShapes.isEmpty() &&
            !boneMatrices.isEmpty()) {
            // Skinning reads from its retained bind-space buffers and writes
            // directly to the existing vertex arrays. Restoring by QVector
            // assignment here would make each animated frame detach and copy
            // the full position/normal arrays.
            applySkinning(boneMatrices);
            return;
        }
        restoreSkinningBase();
        applyBlendShapes();

        // Blend shapes are evaluated in bind/source space before LBS. Keep
        // the resulting deformed source as the input for this skin pass;
        // applyBlendShapes() will reconstruct it from blendBasePositions on
        // the next evaluation, so repeated updates do not compound.
        if (impl_) {
            auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
            auto normAttr = impl_->vertexAttrs.get<QVector3D>("normal");
            if (posAttr) {
                impl_->skinBasePositions = posAttr->data();
                impl_->skinBaseNormals.clear();
                if (normAttr) impl_->skinBaseNormals = normAttr->data();
            }
        }
        applySkinning(boneMatrices);
    }

    void Mesh::updateBounds() {
        auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
        if (!posAttr || posAttr->size() == 0) return;

        QVector3D minB(std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max(),
                       std::numeric_limits<float>::max());
        QVector3D maxB(std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest(),
                       std::numeric_limits<float>::lowest());
        bool foundFinitePosition = false;

        for (int i = 0; i < posAttr->size(); ++i) {
            const auto& p = (*posAttr)[i];
            if (!std::isfinite(p.x()) || !std::isfinite(p.y()) ||
                !std::isfinite(p.z())) {
                continue;
            }
            foundFinitePosition = true;
            minB.setX(std::min(minB.x(), p.x()));
            minB.setY(std::min(minB.y(), p.y()));
            minB.setZ(std::min(minB.z(), p.z()));
            
            maxB.setX(std::max(maxB.x(), p.x()));
            maxB.setY(std::max(maxB.y(), p.y()));
            maxB.setZ(std::max(maxB.z(), p.z()));
        }

        if (!foundFinitePosition) return;
        impl_->minBounds = minB;
        impl_->maxBounds = maxB;
        const QVector3D center = (minB + maxB) * 0.5f;
        float radius = 0.0f;
        for (int i = 0; i < posAttr->size(); ++i) {
            const auto& p = (*posAttr)[i];
            if (!std::isfinite(p.x()) || !std::isfinite(p.y()) ||
                !std::isfinite(p.z())) {
                continue;
            }
            radius = std::max(radius, (p - center).length());
        }
        impl_->sphereCenter = center;
        impl_->sphereRadius = radius;
    }

    QVector3D Mesh::boundingBoxMin() const { return impl_->minBounds; }
    QVector3D Mesh::boundingBoxMax() const { return impl_->maxBounds; }
    QVector3D Mesh::boundingSphereCenter() const { return impl_->sphereCenter; }
    float Mesh::boundingSphereRadius() const { return impl_->sphereRadius; }

    void Mesh::computeVertexNormals() {
        auto posAttr = impl_->vertexAttrs.get<QVector3D>("position");
        if (!posAttr || posAttr->size() == 0 || impl_->polygons.isEmpty()) return;
        auto normAttr = impl_->vertexAttrs.get<QVector3D>("normal");
        if (!normAttr) {
            normAttr = impl_->vertexAttrs.add<QVector3D>("normal");
        }
        const int count = posAttr->size();
        if (normAttr->size() != count) normAttr->resize(count);
        QVector<QVector3D> accumulated(count, QVector3D(0.0f, 0.0f, 0.0f));
        const auto& positions = posAttr->data();
        for (const QVector<int>& polygon : impl_->polygons) {
            if (polygon.size() < 3) continue;
            // Fan-triangulate the n-gon; each triangle contributes its
            // (unnormalized) cross product, i.e. area-weighted normals.
            const int v0 = polygon[0];
            if (v0 < 0 || v0 >= count) continue;
            for (int j = 1; j < polygon.size() - 1; ++j) {
                const int v1 = polygon[j];
                const int v2 = polygon[j + 1];
                if (v1 < 0 || v1 >= count || v2 < 0 || v2 >= count) continue;
                const QVector3D& a = positions[v0];
                const QVector3D& b = positions[v1];
                const QVector3D& c = positions[v2];
                if (!std::isfinite(a.x()) || !std::isfinite(a.y()) || !std::isfinite(a.z()) ||
                    !std::isfinite(b.x()) || !std::isfinite(b.y()) || !std::isfinite(b.z()) ||
                    !std::isfinite(c.x()) || !std::isfinite(c.y()) || !std::isfinite(c.z())) {
                    continue;
                }
                const QVector3D faceNormal = QVector3D::crossProduct(b - a, c - a);
                if (!(faceNormal.lengthSquared() > 1.0e-12f) ||
                    !std::isfinite(faceNormal.lengthSquared())) {
                    continue;
                }
                accumulated[v0] += faceNormal;
                accumulated[v1] += faceNormal;
                accumulated[v2] += faceNormal;
            }
        }
        auto& normals = normAttr->data();
        for (int i = 0; i < count; ++i) {
            const float lengthSquared = accumulated[i].lengthSquared();
            if (lengthSquared > 1.0e-12f && std::isfinite(lengthSquared)) {
                normals[i] = accumulated[i] / std::sqrt(lengthSquared);
            } else {
                normals[i] = QVector3D(0.0f, 0.0f, 1.0f);
            }
        }
        ++impl_->revision;
    }

    bool Mesh::loadFromFile(const QString& filePath)
    {
        ++impl_->revision;
        const QString trimmed = filePath.trimmed();
        if (trimmed.isEmpty()) {
            return false;
        }

        QFile file(trimmed);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }

        QVector<QVector3D> positions;
        QVector<QVector3D> normals;
        QVector<QVector2D> texCoords;
        QVector<QVector<int>> polygons;

        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#')) {
                continue;
            }

            const QStringList tokens = line.split(QChar::Space, Qt::SkipEmptyParts);
            if (tokens.isEmpty()) {
                continue;
            }

            const QString& tag = tokens.front();
            if (tag == QStringLiteral("v") && tokens.size() >= 4) {
                bool okX = false, okY = false, okZ = false;
                const float x = tokens[1].toFloat(&okX);
                const float y = tokens[2].toFloat(&okY);
                const float z = tokens[3].toFloat(&okZ);
                if (okX && okY && okZ) {
                    positions.append(QVector3D(x, y, z));
                }
            } else if (tag == QStringLiteral("vn") && tokens.size() >= 4) {
                bool okX = false, okY = false, okZ = false;
                const float x = tokens[1].toFloat(&okX);
                const float y = tokens[2].toFloat(&okY);
                const float z = tokens[3].toFloat(&okZ);
                if (okX && okY && okZ) {
                    normals.append(QVector3D(x, y, z));
                }
            } else if (tag == QStringLiteral("vt") && tokens.size() >= 3) {
                bool okU = false, okV = false;
                const float u = tokens[1].toFloat(&okU);
                const float v = tokens[2].toFloat(&okV);
                if (okU && okV) {
                    texCoords.append(QVector2D(u, v));
                }
            } else if (tag == QStringLiteral("f") && tokens.size() >= 4) {
                QVector<int> polygon;
                polygon.reserve(tokens.size() - 1);
                for (int i = 1; i < tokens.size(); ++i) {
                    const QStringList vertexParts = tokens[i].split(QChar('/'));
                    const int posIndex = resolveObjIndex(vertexParts.value(0), positions.size());
                    if (posIndex >= 0) {
                        polygon.append(posIndex);
                    }
                }
                if (polygon.size() >= 3) {
                    polygons.append(std::move(polygon));
                }
            }
        }

        if (positions.isEmpty() || polygons.isEmpty()) {
            clear();
            return false;
        }

        clear();
        setVertexCount(positions.size());

        auto posAttr = vertexAttributes().add<QVector3D>("position");
        for (int i = 0; i < positions.size(); ++i) {
            (*posAttr)[i] = positions[i];
        }

        if (normals.size() == positions.size()) {
            auto normAttr = vertexAttributes().add<QVector3D>("normal");
            for (int i = 0; i < normals.size(); ++i) {
                (*normAttr)[i] = normals[i];
            }
        }

        if (texCoords.size() == positions.size()) {
            auto uvAttr = vertexAttributes().add<QVector2D>("uv");
            for (int i = 0; i < texCoords.size(); ++i) {
                (*uvAttr)[i] = texCoords[i];
            }
        }

        for (const auto& polygon : polygons) {
            addPolygon(polygon);
        }

        updateBounds();
        return true;
    }

    bool Mesh::saveToFile(const QString& filePath) const
    {
        const QString trimmed = filePath.trimmed();
        if (trimmed.isEmpty() || !isValid()) {
            return false;
        }

        QFile file(trimmed);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            return false;
        }

        const auto posAttr = vertexAttributes().get<QVector3D>("position");
        if (!posAttr || posAttr->size() == 0) {
            return false;
        }

        const auto normAttr = vertexAttributes().get<QVector3D>("normal");
        const auto uvAttr = vertexAttributes().get<QVector2D>("uv");

        QTextStream out(&file);
        out << "# ArtifactCore Mesh OBJ export\n";
        for (int i = 0; i < posAttr->size(); ++i) {
            const QVector3D& p = (*posAttr)[i];
            out << "v " << p.x() << ' ' << p.y() << ' ' << p.z() << '\n';
        }

        if (normAttr && normAttr->size() == posAttr->size()) {
            for (int i = 0; i < normAttr->size(); ++i) {
                const QVector3D& n = (*normAttr)[i];
                out << "vn " << n.x() << ' ' << n.y() << ' ' << n.z() << '\n';
            }
        }

        if (uvAttr && uvAttr->size() == posAttr->size()) {
            for (int i = 0; i < uvAttr->size(); ++i) {
                const QVector2D& uv = (*uvAttr)[i];
                out << "vt " << uv.x() << ' ' << uv.y() << '\n';
            }
        }

        for (const auto& polygon : impl_->polygons) {
            if (polygon.size() < 3) {
                continue;
            }
            out << "f";
            for (const int vertexIndex : polygon) {
                out << ' ' << (vertexIndex + 1);
            }
            out << '\n';
        }

        return out.status() == QTextStream::Ok;
    }

    void Mesh::clear() {
        ++impl_->revision;
        impl_->vertexAttrs.setElementCount(0);
        impl_->faceAttrs.setElementCount(0);
        impl_->faceVertexAttrs.setElementCount(0);
        impl_->polygons.clear();
        impl_->materialSlots.clear();
    }

    bool Mesh::isValid() const {
        return impl_->vertexAttrs.elementCount() > 0 && impl_->polygons.size() > 0;
    }

}
