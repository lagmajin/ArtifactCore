module;
//#include <assimp/>
#include <memory>
export module MeshImporter;

import std;
import Mesh;
import Utils.String.UniString; // 必要に応じて有効化

export namespace ArtifactCore {

class MeshImporter {
private:
    class Impl;
    Impl* impl_;
public:
    MeshImporter();
    ~MeshImporter();
    // ファイルからMeshを生成
    std::shared_ptr<Mesh> importMeshFromFile(const UniString& path);
};

}