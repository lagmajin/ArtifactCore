module;
#include <algorithm>
#include <cmath>
#include <QVector>
#include <QVector2D>

module Geometry.DepthMeshGenerator;

namespace ArtifactCore {

Mesh DepthMeshGenerator::generate(const DepthMap& depth, const DepthMeshOptions& options)
{
    Mesh mesh;
    const int columns = std::clamp(options.columns, 2, 512);
    const int rows = std::clamp(options.rows, 2, 512);
    mesh.setVertexCount(columns * rows);

    auto positions = mesh.vertexAttributes().add<QVector3D>("position");
    auto normals = mesh.vertexAttributes().add<QVector3D>("normal");
    auto uvs = mesh.vertexAttributes().add<QVector2D>("uv");
    for (int y = 0; y < rows; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(rows - 1);
        for (int x = 0; x < columns; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(columns - 1);
            const float sampledDepth = depth.sampleBilinear(u, v);
            const float d = options.invertDepth ? 1.0f - sampledDepth : sampledDepth;
            const int index = y * columns + x;
            (*positions)[index] = QVector3D((u - 0.5f) * options.width,
                                            (0.5f - v) * options.height,
                                            d * options.depthScale);
            (*normals)[index] = QVector3D(0.0f, 0.0f, 1.0f);
            (*uvs)[index] = QVector2D(u, v);
        }
    }

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            const int index = y * columns + x;
            const int left = y * columns + std::max(0, x - 1);
            const int right = y * columns + std::min(columns - 1, x + 1);
            const int up = std::max(0, y - 1) * columns + x;
            const int down = std::min(rows - 1, y + 1) * columns + x;
            const QVector3D dx = (*positions)[right] - (*positions)[left];
            const QVector3D dy = (*positions)[down] - (*positions)[up];
            const QVector3D normal = QVector3D::crossProduct(dx, dy).normalized();
            (*normals)[index] = normal.lengthSquared() > 1.0e-8f
                ? normal : QVector3D(0.0f, 0.0f, 1.0f);
        }
    }

    for (int y = 0; y < rows - 1; ++y) {
        for (int x = 0; x < columns - 1; ++x) {
            const int i0 = y * columns + x;
            const int i1 = i0 + 1;
            const int i2 = i0 + columns;
            const int i3 = i2 + 1;
            mesh.addPolygon(QVector<int>{i0, i2, i3, i1});
        }
    }
    mesh.updateBounds();
    return mesh;
}

}
