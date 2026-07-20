module;
#include <QString>
#include <QVector3D>
#include <vector>

export module Core.Simulation.OpenVDBVolumeReference;

export namespace ArtifactCore {

struct OpenVDBVolumeReference {
    QString filePath;
    QString densityGrid = QStringLiteral("density");
    QString temperatureGrid = QStringLiteral("temperature");
    QString velocityGrid = QStringLiteral("vel");
    int frame = 0;
    QVector3D boundsMin;
    QVector3D boundsMax;
    float densityScale = 1.0f;
    bool enabled = true;

    bool isValid() const noexcept {
        return enabled && !filePath.trimmed().isEmpty();
    }
};

struct OpenVDBVolumeMetadata {
    bool loaded = false;
    bool densityGridFound = false;
    bool temperatureGridFound = false;
    bool velocityGridFound = false;
    QString error;
    QVector3D boundsMin;
    QVector3D boundsMax;
    std::vector<QString> gridNames;
};

OpenVDBVolumeMetadata inspectOpenVDBVolume(const OpenVDBVolumeReference& reference);

} // namespace ArtifactCore
