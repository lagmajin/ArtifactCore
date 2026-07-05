module;
#include <utility>
#include <QString>
#include <QUuid>

export module Asset.Importer;

import AssetType;

export namespace ArtifactCore {

/**
 * @brief Logic for importing various file types into the project
 */
class AssetImporter {
public:
    AssetImporter() = default;
    
    /**
     * @brief Import a file from disk
     * @return UUID of the newly registered asset, or null UUID on failure
     */
    static QUuid importFile(const QString& filePath);

    /**
     * @brief Check if a file extension is supported
     */
    static bool isSupported(const QString& extension);

    /**
     * @brief Detect AssetType from file extension
     */
    static AssetType detectType(const QString& filePath);
};

} // namespace ArtifactCore
