module;
#include <memory>
#include <QVector3D>

export module Geometry.DepthMeshGenerator;

import Mesh;
import Image.DepthMap;

export namespace ArtifactCore {

struct DepthMeshOptions {
    int columns = 64;
    int rows = 64;
    float width = 1.0f;
    float height = 1.0f;
    float depthScale = 0.25f;
    bool invertDepth = false;
};

class DepthMeshGenerator {
public:
    static Mesh generate(const DepthMap& depth, const DepthMeshOptions& options = {});
};

}
