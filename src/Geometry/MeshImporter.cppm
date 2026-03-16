module;
#include <QString>
#include <QDebug>

module MeshImporter;

import Mesh;
import Utils.String.UniString;

namespace ArtifactCore {

    class MeshImporter::Impl {
    public:
        Impl() {}
        ~Impl() {}
    };

    MeshImporter::MeshImporter() : impl_(new Impl()) {}
    MeshImporter::~MeshImporter() { delete impl_; }

    std::shared_ptr<Mesh> MeshImporter::importMeshFromFile(const UniString& path) {
        qWarning() << "MeshImporter: assimp not available, cannot import:" << path.toQString();
        return nullptr;
    }

};
