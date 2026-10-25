module;

#include <algorithm>
#include <cmath>
#include <QVector>
#include <QVector3D>

module Geometry.MeshDeform;

import Mesh;
import Math.Noise;
import Memory.SharedPtr;

namespace ArtifactCore {

namespace {

float componentOf(const QVector3D& value, int axis)
{
    if (axis == 0) return value.x();
    if (axis == 1) return value.y();
    return value.z();
}

void setComponent(QVector3D& value, int axis, float component)
{
    if (axis == 0) value.setX(component);
    else if (axis == 1) value.setY(component);
    else value.setZ(component);
}

int clampedAxis(int axis)
{
    return std::clamp(axis, 0, 2);
}

SharedPtr<MeshAttribute<QVector3D>> mutablePositions(Mesh& mesh)
{
    auto positions = mesh.vertexAttributes().get<QVector3D>("position");
    if (!positions || positions->size() == 0) {
        return nullptr;
    }
    return positions;
}

bool finitePosition(const QVector3D& value)
{
    return std::isfinite(value.x()) && std::isfinite(value.y()) && std::isfinite(value.z());
}

} // namespace

bool bendMesh(Mesh& mesh, const BendParams& params)
{
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    const int axis = clampedAxis(params.axis);
    const int bendPlane = (axis + 1) % 3;
    const float curvature = params.angle;
    if (!(std::abs(curvature) > 1.0e-6f) || !std::isfinite(curvature)) {
        return true;
    }
    const QVector3D center = params.center;
    QVector<QVector3D>& data = positions->data();
    for (QVector3D& position : data) {
        if (!finitePosition(position)) {
            continue;
        }
        const float distance = componentOf(position, axis) - componentOf(center, axis);
        const float planar = componentOf(position, bendPlane) - componentOf(center, bendPlane);
        const float theta = distance * curvature;
        const float sine = std::sin(theta);
        const float cosine = std::cos(theta);
        // Arc of radius 1/curvature: length-axis maps to the arc,
        // the in-plane axis absorbs (1-cos)/curvature. Continuous at k->0.
        setComponent(position, axis, componentOf(center, axis) + sine / curvature);
        setComponent(position, bendPlane,
                     componentOf(center, bendPlane) + planar + (1.0f - cosine) / curvature);
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

bool twistMesh(Mesh& mesh, const TwistParams& params)
{
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    const int axis = clampedAxis(params.axis);
    const int planeA = (axis + 1) % 3;
    const int planeB = (axis + 2) % 3;
    if (!std::isfinite(params.angle)) {
        return true;
    }
    const QVector3D center = params.center;
    QVector<QVector3D>& data = positions->data();
    for (QVector3D& position : data) {
        if (!finitePosition(position)) {
            continue;
        }
        const float distance = componentOf(position, axis) - componentOf(center, axis);
        const float phi = distance * params.angle;
        const float sine = std::sin(phi);
        const float cosine = std::cos(phi);
        const float a = componentOf(position, planeA) - componentOf(center, planeA);
        const float b = componentOf(position, planeB) - componentOf(center, planeB);
        setComponent(position, planeA, componentOf(center, planeA) + a * cosine - b * sine);
        setComponent(position, planeB, componentOf(center, planeB) + a * sine + b * cosine);
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

bool taperMesh(Mesh& mesh, const TaperParams& params)
{
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    const int axis = clampedAxis(params.axis);
    const int planeA = (axis + 1) % 3;
    const int planeB = (axis + 2) % 3;
    if (!std::isfinite(params.amount)) {
        return true;
    }
    const QVector3D center = params.center;
    QVector<QVector3D>& data = positions->data();
    for (QVector3D& position : data) {
        if (!finitePosition(position)) {
            continue;
        }
        const float distance = componentOf(position, axis) - componentOf(center, axis);
        const float scale = std::max(1.0f + distance * params.amount, 0.0f);
        setComponent(position, planeA,
                     componentOf(center, planeA) +
                         (componentOf(position, planeA) - componentOf(center, planeA)) * scale);
        setComponent(position, planeB,
                     componentOf(center, planeB) +
                         (componentOf(position, planeB) - componentOf(center, planeB)) * scale);
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

bool displaceMesh(Mesh& mesh, const DisplaceParams& params)
{
    if (!std::isfinite(params.amount) || !std::isfinite(params.frequency)) {
        return true;
    }
    if (params.subdivLevels > 0) {
        auto subdivided = mesh.createSubdivided(params.subdivLevels);
        if (!subdivided) {
            return false;
        }
        mesh = *subdivided;
    }
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    auto normals = mesh.vertexAttributes().get<QVector3D>("normal");
    if (!normals || normals->size() != positions->size()) {
        mesh.computeVertexNormals();
        normals = mesh.vertexAttributes().get<QVector3D>("normal");
    }
    if (!normals || normals->size() != positions->size()) {
        return false;
    }
    const NoiseField field(params.seed);
    const int octaves = std::clamp(params.octaves, 1, 8);
    QVector<QVector3D>& positionData = positions->data();
    QVector<QVector3D>& normalData = normals->data();
    const int count = positionData.size();
    for (int index = 0; index < count; ++index) {
        const QVector3D& position = positionData[index];
        const QVector3D& normal = normalData[index];
        if (!finitePosition(position) || !finitePosition(normal)) {
            continue;
        }
        const float sample = field.fractal(position.x() * params.frequency,
                                           position.y() * params.frequency,
                                           position.z() * params.frequency,
                                           octaves);
        if (!std::isfinite(sample)) {
            continue;
        }
        positionData[index] = position + normal * (sample * params.amount);
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

float radialFalloff(float distance, float radius)
{
    if (!(radius > 1.0e-6f) || !std::isfinite(radius)) {
        return 1.0f;
    }
    const float scaled = distance / radius;
    return std::exp(-scaled * scaled);
}

QVector3D normalizedDirection(const QVector3D& direction, const QVector3D& fallback)
{
    const float lengthSquared = QVector3D::dotProduct(direction, direction);
    if (!(lengthSquared > 1.0e-12f) || !std::isfinite(lengthSquared)) {
        return fallback;
    }
    return direction / std::sqrt(lengthSquared);
}

SharedPtr<MeshAttribute<QVector3D>> ensuredNormals(Mesh& mesh, int count)
{
    auto normals = mesh.vertexAttributes().get<QVector3D>("normal");
    if (!normals || normals->size() != count) {
        mesh.computeVertexNormals();
        normals = mesh.vertexAttributes().get<QVector3D>("normal");
    }
    if (!normals || normals->size() != count) {
        return nullptr;
    }
    return normals;
}

bool waveMesh(Mesh& mesh, const WaveParams& params)
{
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    const int count = positions->size();
    auto normals = ensuredNormals(mesh, count);
    if (!normals) {
        return false;
    }
    if (!std::isfinite(params.frequency) || !std::isfinite(params.amplitude) ||
        !std::isfinite(params.speed) || !std::isfinite(params.timeSeconds)) {
        return true;
    }
    const QVector3D direction = normalizedDirection(params.direction, QVector3D(0.0f, 1.0f, 0.0f));
    const QVector3D center = params.center;
    QVector<QVector3D>& positionData = positions->data();
    QVector<QVector3D>& normalData = normals->data();
    for (int index = 0; index < count; ++index) {
        const QVector3D& position = positionData[index];
        const QVector3D& normal = normalData[index];
        if (!finitePosition(position) || !finitePosition(normal)) {
            continue;
        }
        const float phase = QVector3D::dotProduct(position - center, direction) *
                                params.frequency +
                            params.timeSeconds * params.speed;
        if (!std::isfinite(phase)) {
            continue;
        }
        positionData[index] = position + normal * (std::sin(phase) * params.amplitude);
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

bool bulgeMesh(Mesh& mesh, const BulgeParams& params)
{
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    if (!std::isfinite(params.amount) || !std::isfinite(params.radius)) {
        return true;
    }
    const QVector3D center = params.center;
    QVector<QVector3D>& data = positions->data();
    for (QVector3D& position : data) {
        if (!finitePosition(position)) {
            continue;
        }
        const QVector3D relative = position - center;
        const float distance = relative.length();
        if (!(distance > 1.0e-9f)) {
            continue;
        }
        const float scale = 1.0f + params.amount * radialFalloff(distance, params.radius);
        position = center + relative * scale;
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

bool spherifyMesh(Mesh& mesh, const SpherifyParams& params)
{
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    const float amount = std::clamp(params.amount, 0.0f, 1.0f);
    if (!(params.radius > 1.0e-6f) || !std::isfinite(params.radius) || !std::isfinite(amount)) {
        return true;
    }
    const QVector3D center = params.center;
    QVector<QVector3D>& data = positions->data();
    for (QVector3D& position : data) {
        if (!finitePosition(position)) {
            continue;
        }
        const QVector3D relative = position - center;
        const QVector3D direction = normalizedDirection(relative, QVector3D(0.0f, 0.0f, 0.0f));
        if (direction.isNull()) {
            continue;
        }
        const QVector3D target = center + direction * params.radius;
        position = position + (target - position) * amount;
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

bool shearMesh(Mesh& mesh, const ShearParams& params)
{
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    const int axis = clampedAxis(params.axis);
    const int driver = clampedAxis(params.shearAxis);
    if (!std::isfinite(params.amount)) {
        return true;
    }
    const QVector3D center = params.center;
    QVector<QVector3D>& data = positions->data();
    for (QVector3D& position : data) {
        if (!finitePosition(position)) {
            continue;
        }
        const float drive = componentOf(position, driver) - componentOf(center, driver);
        setComponent(position, axis, componentOf(position, axis) + drive * params.amount);
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

bool puckerMesh(Mesh& mesh, const PuckerParams& params)
{
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    if (!std::isfinite(params.amount) || !std::isfinite(params.radius)) {
        return true;
    }
    const QVector3D center = params.center;
    QVector<QVector3D>& data = positions->data();
    for (QVector3D& position : data) {
        if (!finitePosition(position)) {
            continue;
        }
        const QVector3D relative = position - center;
        const float distance = relative.length();
        if (!(distance > 1.0e-9f)) {
            continue;
        }
        // amount>0 pulls toward center (pucker), <0 pushes away (bloat).
        const float scale =
            std::max(1.0f - params.amount * radialFalloff(distance, params.radius), 0.0f);
        position = center + relative * scale;
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

bool stretchMesh(Mesh& mesh, const StretchParams& params)
{
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    const int axis = clampedAxis(params.axis);
    if (!std::isfinite(params.amount)) {
        return true;
    }
    const float scale = std::max(1.0f + params.amount, 0.0f);
    const QVector3D center = params.center;
    QVector<QVector3D>& data = positions->data();
    for (QVector3D& position : data) {
        if (!finitePosition(position)) {
            continue;
        }
        setComponent(position, axis,
                     componentOf(center, axis) +
                         (componentOf(position, axis) - componentOf(center, axis)) * scale);
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

bool noiseMesh(Mesh& mesh, const NoiseParams& params)
{
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    if (!std::isfinite(params.amount) || !std::isfinite(params.frequency)) {
        return true;
    }
    const NoiseField field(params.seed);
    const int octaves = std::clamp(params.octaves, 1, 8);
    QVector<QVector3D>& data = positions->data();
    for (QVector3D& position : data) {
        if (!finitePosition(position)) {
            continue;
        }
        const float x = field.fractal(position.x() * params.frequency,
                                      position.y() * params.frequency + 13.7f,
                                      position.z() * params.frequency + 71.3f, octaves);
        const float y = field.fractal(position.x() * params.frequency + 41.2f,
                                      position.y() * params.frequency,
                                      position.z() * params.frequency + 17.9f, octaves);
        const float z = field.fractal(position.x() * params.frequency + 91.7f,
                                      position.y() * params.frequency + 53.1f,
                                      position.z() * params.frequency, octaves);
        const QVector3D offset(x, y, z);
        if (!finitePosition(offset)) {
            continue;
        }
        position = position + offset * params.amount;
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

bool smoothMesh(Mesh& mesh, const SmoothParams& params)
{
    auto positions = mutablePositions(mesh);
    if (!positions) {
        return false;
    }
    const int count = positions->size();
    if (!std::isfinite(params.strength)) {
        return true;
    }
    const float strength = std::clamp(params.strength, 0.0f, 1.0f);
    const int iterations = std::clamp(params.iterations, 1, 100);
    if (strength <= 0.0f) {
        return true;
    }
    // Index-based adjacency: no hash maps, single allocation.
    QVector<QVector<int>> neighbors(count);
    const int polygonCount = mesh.polygonCount();
    for (int polygon = 0; polygon < polygonCount; ++polygon) {
        const QVector<int> corners = mesh.getPolygonVertices(polygon);
        const int cornersCount = corners.size();
        for (int edge = 0; edge < cornersCount; ++edge) {
            const int a = corners[edge];
            const int b = corners[(edge + 1) % cornersCount];
            if (a < 0 || a >= count || b < 0 || b >= count || a == b) {
                continue;
            }
            if (!neighbors[a].contains(b)) {
                neighbors[a].push_back(b);
            }
            if (!neighbors[b].contains(a)) {
                neighbors[b].push_back(a);
            }
        }
    }
    QVector<QVector3D>& data = positions->data();
    QVector<QVector3D> relaxed(count);
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (int index = 0; index < count; ++index) {
            const QVector<int>& ring = neighbors[index];
            if (ring.isEmpty() || !finitePosition(data[index])) {
                relaxed[index] = data[index];
                continue;
            }
            QVector3D average(0.0f, 0.0f, 0.0f);
            int valid = 0;
            for (int neighbor : ring) {
                if (finitePosition(data[neighbor])) {
                    average += data[neighbor];
                    ++valid;
                }
            }
            relaxed[index] = (valid > 0)
                ? data[index] + (average / static_cast<float>(valid) - data[index]) * strength
                : data[index];
        }
        data = relaxed;
    }
    mesh.computeVertexNormals();
    mesh.updateBounds();
    return true;
}

} // namespace ArtifactCore
