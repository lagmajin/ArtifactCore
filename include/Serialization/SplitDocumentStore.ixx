module;

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QString>
#include <QStringList>

export module Serialization.SplitDocumentStore;

import Serialization.Document;

export namespace ArtifactCore::Serialization {

/** Stores a project manifest and independently replaceable child JSON/CBOR documents. */
class SplitDocumentStore {
public:
    static bool save(const QString& directory, const QJsonObject& manifest,
                     const QMap<QString, QJsonObject>& children,
                     SerializationFormat format = SerializationFormat::Cbor)
    {
        if (format == SerializationFormat::Unknown || directory.isEmpty()) {
            return false;
        }
        QDir root;
        if (!root.mkpath(directory)) {
            return false;
        }
        const QString childDir = QDir(directory).filePath(QStringLiteral("documents"));
        if (!root.mkpath(childDir)) {
            return false;
        }

        QJsonObject storedManifest = manifest;
        QJsonArray childNames;
        constexpr int kMaxDocuments = 100000;
        if (children.size() > kMaxDocuments) {
            return false;
        }
        const QJsonValue rootDocument = manifest.value(QStringLiteral("rootDocument"));
        if (!rootDocument.isUndefined() &&
            (!rootDocument.isString() ||
             !children.contains(rootDocument.toString()))) {
            return false;
        }
        for (auto it = children.constBegin(); it != children.constEnd(); ++it) {
            const QString safeName = it.key();
            if (!isSafeDocumentName(safeName)) {
                return false;
            }
            const QString filePath = QDir(childDir).filePath(safeName + suffix(format));
            if (!SerializationDocument::writeFile(filePath, it.value(), format)) {
                return false;
            }
            childNames.append(safeName);
        }
        storedManifest[QStringLiteral("_documents")] = childNames;
        return SerializationDocument::writeFile(
            QDir(directory).filePath(QStringLiteral("manifest") + suffix(format)),
            storedManifest, format);
    }

    static bool load(const QString& directory, QJsonObject& manifest,
                     QMap<QString, QJsonObject>& children)
    {
        children.clear();
        const QString manifestPath = findManifest(directory);
        if (manifestPath.isEmpty() ||
            !SerializationDocument::readFile(manifestPath, manifest)) {
            return false;
        }
        const auto format = SerializationDocument::detectFile(manifestPath);
        if (format == SerializationFormat::Unknown) {
            return false;
        }
        const QJsonArray names = manifest.value(QStringLiteral("_documents")).toArray();
        if (!manifest.value(QStringLiteral("_documents")).isArray()) {
            return false;
        }
        constexpr int kMaxDocuments = 100000;
        if (names.size() > kMaxDocuments) {
            return false;
        }
        const QDir childDir(QDir(directory).filePath(QStringLiteral("documents")));
        QSet<QString> seenNames;
        for (const auto& value : names) {
            const QString name = value.toString();
            if (!value.isString() || !isSafeDocumentName(name) ||
                seenNames.contains(name)) {
                return false;
            }
            seenNames.insert(name);
            QJsonObject child;
            if (!SerializationDocument::readFile(
                    childDir.filePath(name + suffix(format)), child, format)) {
                return false;
            }
            children.insert(name, child);
        }
        return true;
    }

private:
    static bool isSafeDocumentName(const QString& name)
    {
        return !name.isEmpty() && name != QStringLiteral(".") &&
               name != QStringLiteral("..") && !name.contains(QStringLiteral("..")) &&
               !name.contains(QDir::separator()) && !name.contains(QChar('/')) &&
               !name.contains(QChar('\\')) && !QDir::isAbsolutePath(name);
    }

    static QString suffix(SerializationFormat format)
    {
        return format == SerializationFormat::Cbor ? QStringLiteral(".cbor")
                                                    : QStringLiteral(".json");
    }

    static QString findManifest(const QString& directory)
    {
        const QDir root(directory);
        for (const auto& extension : {QStringLiteral(".cbor"), QStringLiteral(".json")}) {
            const QString path = root.filePath(QStringLiteral("manifest") + extension);
            if (QFile::exists(path)) {
                return path;
            }
        }
        return {};
    }
};

} // namespace ArtifactCore::Serialization
