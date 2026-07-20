module;
#include <QString>
#include <exception>
#include <algorithm>
#include <cmath>

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
        for (const auto& name : result.gridNames) {
            result.densityGridFound = result.densityGridFound || name == reference.densityGrid;
            result.temperatureGridFound = result.temperatureGridFound || name == reference.temperatureGrid;
            result.velocityGridFound = result.velocityGridFound || name == reference.velocityGrid;
        }
        if (!result.loaded) {
            result.error = QStringLiteral("OpenVDB file contains no grids");
        } else if (!result.densityGridFound) {
            result.error = QStringLiteral("OpenVDB density grid was not found: %1").arg(reference.densityGrid);
        }
    } catch (const std::exception& error) {
        result.error = QString::fromUtf8(error.what());
    }
#else
    result.error = QStringLiteral("OpenVDB support is not available in this build");
#endif
    return result;
}

OpenVDBScalarSnapshot loadOpenVDBDensitySnapshot(const OpenVDBVolumeReference& reference)
{
    OpenVDBScalarSnapshot result;
    if (!reference.isValid()) {
        result.error = QStringLiteral("OpenVDB volume reference is empty or disabled");
        return result;
    }

#if defined(ARTIFACTCORE_HAS_OPENVDB)
    try {
        openvdb::initialize();
        openvdb::io::File file(reference.filePath.toStdString());
        file.open();
        const auto baseGrid = file.readGrid(reference.densityGrid.toStdString());
        const auto densityGrid = openvdb::gridConstPtrCast<openvdb::FloatGrid>(baseGrid);
        if (!densityGrid) {
            file.close();
            result.error = QStringLiteral("Density Grid is not a FloatGrid: %1").arg(reference.densityGrid);
            return result;
        }

        openvdb::CoordBBox bbox;
        densityGrid->evalActiveVoxelBoundingBox(bbox);
        if (bbox.empty()) {
            file.close();
            result.error = QStringLiteral("Density Grid has no active voxels");
            return result;
        }

        const auto dim = bbox.dim();
        result.width = static_cast<int>(dim.x());
        result.height = static_cast<int>(dim.y());
        result.depth = static_cast<int>(dim.z());
        const auto worldMin = densityGrid->transform().indexToWorld(bbox.min());
        const auto worldMax = densityGrid->transform().indexToWorld(bbox.max());
        result.origin = QVector3D(static_cast<float>(worldMin.x()), static_cast<float>(worldMin.y()), static_cast<float>(worldMin.z()));
        result.voxelSize = QVector3D(
            static_cast<float>(std::abs(worldMax.x() - worldMin.x()) / std::max(1, result.width - 1)),
            static_cast<float>(std::abs(worldMax.y() - worldMin.y()) / std::max(1, result.height - 1)),
            static_cast<float>(std::abs(worldMax.z() - worldMin.z()) / std::max(1, result.depth - 1)));
        result.values.resize(static_cast<std::size_t>(result.width) * result.height * result.depth);

        std::size_t index = 0;
        for (int z = bbox.min().z(); z <= bbox.max().z(); ++z) {
            for (int y = bbox.min().y(); y <= bbox.max().y(); ++y) {
                for (int x = bbox.min().x(); x <= bbox.max().x(); ++x) {
                    result.values[index++] = densityGrid->tree().getValue(openvdb::Coord(x, y, z)) * reference.densityScale;
                }
            }
        }
        file.close();
        result.valid = true;
    } catch (const std::exception& error) {
        result.error = QString::fromUtf8(error.what());
    }
#else
    result.error = QStringLiteral("OpenVDB support is not available in this build");
#endif
    return result;
}

} // namespace ArtifactCore
