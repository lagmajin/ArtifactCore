module;
#include <QString>
#include <exception>

#if defined(ARTIFACTCORE_HAS_OPENVDB)
#include <openvdb/openvdb.h>
#endif

module Core.Simulation.OpenVDBVolumeReference;

namespace ArtifactCore {

OpenVDBVolumeMetadata inspectOpenVDBVolume(const OpenVDBVolumeReference& reference)
{
    OpenVDBVolumeMetadata result;
    if (!reference.isValid()) {
        result.error = QStringLiteral("OpenVDB volume reference is empty or disabled");
        return result;
    }

#if defined(ARTIFACTCORE_HAS_OPENVDB)
    try {
        openvdb::initialize();
        openvdb::io::File file(reference.filePath.toStdString());
        file.open();
        for (openvdb::io::File::NameIterator it = file.beginName();
             it != file.endName(); ++it) {
            result.gridNames.emplace_back(QString::fromStdString(it.gridName()));
        }
        file.close();
        result.loaded = !result.gridNames.empty();
        if (!result.loaded) {
            result.error = QStringLiteral("OpenVDB file contains no grids");
        }
    } catch (const std::exception& error) {
        result.error = QString::fromUtf8(error.what());
    }
#else
    result.error = QStringLiteral("OpenVDB support is not available in this build");
#endif
    return result;
}

} // namespace ArtifactCore
