module;

#include <QJsonObject>
#include <QMap>
#include <QDir>
#include <QFile>
#include <QString>
#include <memory>

export module Serialization.ProjectSerializer;

import Serialization.Document;
import Serialization.SplitDocumentStore;
import Serialization.Envelope;
import Serialization.ISerializable;

export namespace ArtifactCore::Serialization {

/** Format facade used by project adapters while domain types migrate incrementally. */
class ProjectSerializer {
public:
    // Detects single-document JSON/CBOR. Use isSplitDirectory() for split projects.
    static SerializationFormat detectFormat(const QString& path)
    {
        return SerializationDocument::detectFile(path);
    }

    static bool isSplitDirectory(const QString& path)
    {
        const QDir root(path);
        return root.exists() &&
               (QFile::exists(root.filePath(QStringLiteral("manifest.json"))) ||
                QFile::exists(root.filePath(QStringLiteral("manifest.cbor"))));
    }

    static bool save(const QString& path, const QJsonObject& project,
                     SerializationFormat format = SerializationFormat::Json)
    {
        return SerializationDocument::writeFile(path, project, format);
    }

    static bool saveSerializable(const QString& path, const ISerializable& value,
                                 SerializationFormat format = SerializationFormat::Json)
    {
        return save(path, makeEnvelope(value), format);
    }

    static bool saveArtifact(const QString& path, const QJsonObject& project)
    {
        return save(path, project, SerializationFormat::Cbor);
    }

    static bool load(const QString& path, QJsonObject& project)
    {
        return SerializationDocument::readFile(path, project);
    }

    static bool load(const QString& path, QJsonObject& project,
                     QString* errorMessage)
    {
        return SerializationDocument::readFile(path, project,
                                                SerializationFormat::Unknown,
                                                errorMessage);
    }

    static std::unique_ptr<ISerializable> loadSerializable(const QString& path)
    {
        QJsonObject envelope;
        if (!load(path, envelope)) {
            return nullptr;
        }
        return deserializeEnvelope(envelope);
    }

    static bool loadArtifact(const QString& path, QJsonObject& project,
                            QString* errorMessage = nullptr)
    {
        return SerializationDocument::readFile(path, project,
                                                SerializationFormat::Cbor,
                                                errorMessage);
    }

    static bool saveSplit(const QString& directory, const QJsonObject& manifest,
                          const QMap<QString, QJsonObject>& documents,
                          SerializationFormat format = SerializationFormat::Cbor)
    {
        return SplitDocumentStore::save(directory, manifest, documents, format);
    }

    static bool loadSplit(const QString& directory, QJsonObject& manifest,
                          QMap<QString, QJsonObject>& documents)
    {
        return SplitDocumentStore::load(directory, manifest, documents);
    }
};

} // namespace ArtifactCore::Serialization
