module;
#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QUuid>

export module Asset;

import AssetType;

export namespace ArtifactCore {

class ArtifactAssetMetaFile {
public:
    static ArtifactAssetMetaFile load(const QString& assetPath);
    static ArtifactAssetMetaFile fromJson(const QByteArray& json,
                                          const QString& assetPath = {});
    static QString metaPathFor(const QString& assetPath);

    bool isValid() const;
    bool save() const;
    QJsonObject toJson() const;

    QString sourcePath() const;
    QUuid uuid() const;
    AssetType type() const;
    QDateTime importedAt() const;

    void setUuid(const QUuid& value);
    void setType(AssetType value);
    void setImportedAt(const QDateTime& value);

    QStringList proxyResolutions() const;
    QString proxyPath(const QString& resolution) const;
    void addProxy(const QString& resolution, const QString& path);

    QStringList tags() const;
    void addTag(const QString& tag);
    void removeTag(const QString& tag);

    QVariant customValue(const QString& key) const;
    void setCustomValue(const QString& key, const QVariant& value);

private:
    explicit ArtifactAssetMetaFile(QString assetPath);
    static QString assetTypeName(AssetType type);
    static AssetType assetTypeFromName(const QString& name);

    QString assetPath_;
    QJsonObject data_;
};

} // namespace ArtifactCore
