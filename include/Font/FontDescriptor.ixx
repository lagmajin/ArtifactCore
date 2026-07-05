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

    bool isValid() const { return !fullPath.isEmpty(); }
};

} // namespace ArtifactCore
