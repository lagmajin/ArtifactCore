module;
#include <utility>

#include <QString>
#include <QStringList>

export module Font.Descriptor;

namespace ArtifactCore {

export struct FontDescriptor {
    QString family;
    QString style;
    QString fullPath;
    bool isFixedPitch = false;
    int weight = 50; // QFont::Weight 相当
    bool italic = false;

    // Installed system fonts may not expose a filesystem path through Qt.
    // A family/style pair is still a valid logical font descriptor.
    bool isValid() const { return !family.trimmed().isEmpty(); }
};

} // namespace ArtifactCore
