module;

#include <memory>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QMap>
#include <QString>

module Serialization.ProjectSerializer;

import Serialization.Document;
import Serialization.SplitDocumentStore;
import Serialization.Envelope;
import Serialization.ISerializable;

namespace ArtifactCore::Serialization {

SerializationFormat ProjectSerializer::detectFormat(const QString& path)
{
    return SerializationDocument::detectFile(path);
}

bool ProjectSerializer::isSplitDirectory(const QString& path)
{
    const QDir root(path);
    return root.exists() &&
           (QFile::exists(root.filePath(QStringLiteral("manifest.json"))) ||
            QFile::exists(root.filePath(QStringLiteral("manifest.cbor"))));
}

bool ProjectSerializer::save(const QString& path, const QJsonObject& project,
                             SerializationFormat format)
{
    return SerializationDocument::writeFile(path, project, format);
}

bool ProjectSerializer::saveSerializable(const QString& path,
                                         const ISerializable& value,
                                         SerializationFormat format)
{
    return save(path, makeEnvelope(value), format);
}

bool ProjectSerializer::saveArtifact(const QString& path,
                                     const QJsonObject& project)
{
    return save(path, project, SerializationFormat::Cbor);
}

bool ProjectSerializer::load(const QString& path, QJsonObject& project)
{
    return SerializationDocument::readFile(path, project);
}

bool ProjectSerializer::load(const QString& path, QJsonObject& project,
                             QString* errorMessage)
{
    return SerializationDocument::readFile(
        path, project, SerializationFormat::Unknown, errorMessage);
}

std::unique_ptr<ISerializable> ProjectSerializer::loadSerializable(
    const QString& path)
{
    QJsonObject envelope;
    if (!load(path, envelope)) {
        return nullptr;
    }
    return deserializeEnvelope(envelope);
}

bool ProjectSerializer::loadArtifact(const QString& path,
                                     QJsonObject& project,
                                     QString* errorMessage)
{
    return SerializationDocument::readFile(
        path, project, SerializationFormat::Cbor, errorMessage);
}

bool ProjectSerializer::saveSplit(
    const QString& directory, const QJsonObject& manifest,
    const QMap<QString, QJsonObject>& documents, SerializationFormat format)
{
    return SplitDocumentStore::save(directory, manifest, documents, format);
}

bool ProjectSerializer::loadSplit(
    const QString& directory, QJsonObject& manifest,
    QMap<QString, QJsonObject>& documents)
{
    return SplitDocumentStore::load(directory, manifest, documents);
}

} // namespace ArtifactCore::Serialization
