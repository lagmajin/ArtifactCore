module;
#include <QString>
#include <QVector3D>

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

} // namespace ArtifactCore
