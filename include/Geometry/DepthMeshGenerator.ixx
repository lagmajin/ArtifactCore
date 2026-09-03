module;
#include <algorithm>
#include <QVector3D>
export module Geometry.DepthMeshGenerator;
import Image.DepthMap;
import Mesh;

export namespace ArtifactCore {
struct DepthMeshOptions {
  int columns = 64;
  int rows = 64;
  float width = 1.0f;
  float height = 1.0f;
  float depthScale = 1.0f;
  bool invertDepth = false;
};
class DepthMeshGenerator {
public:
  static Mesh generate(const DepthMap& map, const DepthMeshOptions& options) {
    Mesh mesh; if (map.isEmpty()) return mesh;
    const int w = std::max(2, options.columns);
    const int h = std::max(2, options.rows);
    mesh.setVertexCount(w * h);
    auto positions = mesh.vertexAttributes().add<QVector3D>("position");
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
      const int sx = std::min(map.width() - 1, x * map.width() / w);
      const int sy = std::min(map.height() - 1, y * map.height() / h);
      const float depth = map.value(sx, sy) * options.depthScale;
      (*positions)[y * w + x] = QVector3D(
          (float(x) / float(w - 1) - 0.5f) * options.width,
          (float(y) / float(h - 1) - 0.5f) * options.height,
          options.invertDepth ? -depth : depth);
      }
    }
    for (int y = 0; y + 1 < h; ++y) for (int x = 0; x + 1 < w; ++x) {
      mesh.addPolygon({y*w+x, y*w+x+1, (y+1)*w+x+1, (y+1)*w+x});
    }
    return mesh;
  }
};
}
