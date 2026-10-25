module;

#include <QJsonObject>
#include <QMap>
#include <QDir>
#include <QFile>
#include <QString>
#include <memory>

export module Serialization.ProjectSerializer;

import Serialization.Document;
import Serialization.ISerializable;

export namespace ArtifactCore::Serialization {

/** Format facade used by project adapters while domain types migrate incrementally. */
class ProjectSerializer {
public:
    // Detects single-document JSON/CBOR. Use isSplitDirectory() for split projects.
    static SerializationFormat detectFormat(const QString& path);
    static bool isSplitDirectory(const QString& path);
    static bool save(const QString& path, const QJsonObject& project,
                     SerializationFormat format = SerializationFormat::Json);
    static bool saveSerializable(const QString& path, const ISerializable& value,
                                SerializationFormat format = SerializationFormat::Json);
    static bool saveArtifact(const QString& path, const QJsonObject& project);
    static bool load(const QString& path, QJsonObject& project);
    static bool load(const QString& path, QJsonObject& project,
                     QString* errorMessage);
    static std::unique_ptr<ISerializable> loadSerializable(const QString& path);
    static bool loadArtifact(const QString& path, QJsonObject& project,
                             QString* errorMessage = nullptr);
    static bool saveSplit(const QString& directory, const QJsonObject& manifest,
                          const QMap<QString, QJsonObject>& documents,
                          SerializationFormat format = SerializationFormat::Cbor);
    static bool loadSplit(const QString& directory, QJsonObject& manifest,
                          QMap<QString, QJsonObject>& documents);
};

} // namespace ArtifactCore::Serialization
