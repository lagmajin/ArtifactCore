module;

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QCborValue>
#include <QString>

export module Serialization.Document;

export namespace ArtifactCore::Serialization {

enum class SerializationFormat {
    Unknown,
    Json,
    Cbor,
};

class SerializationDocument {
public:
    static constexpr qint64 kMaxDocumentBytes = 256LL * 1024LL * 1024LL;

    static QByteArray encode(const QJsonObject& object, SerializationFormat format)
    {
        if (format == SerializationFormat::Cbor) {
            return QCborValue::fromJsonValue(object).toCbor();
        }
        if (format == SerializationFormat::Json) {
            return QJsonDocument(object).toJson(QJsonDocument::Indented);
        }
        return {};
    }

    static bool decode(const QByteArray& bytes, SerializationFormat format,
                       QJsonObject& object)
    {
        if (format == SerializationFormat::Cbor) {
            const QCborValue value = QCborValue::fromCbor(bytes);
            if (!value.isMap()) {
                return false;
            }
            object = value.toJsonValue().toObject();
            return true;
        }
        if (format == SerializationFormat::Json) {
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                return false;
            }
            object = document.object();
            return true;
        }
        return false;
    }

    static SerializationFormat detect(const QByteArray& bytes)
    {
        const QByteArray trimmed = bytes.trimmed();
        if (trimmed.startsWith('{')) {
            return SerializationFormat::Json;
        }
        if (!trimmed.isEmpty()) {
            const auto majorType = static_cast<unsigned char>(trimmed.at(0)) >> 5;
            if (majorType == 5) {
                return SerializationFormat::Cbor;
            }
        }
        return SerializationFormat::Unknown;
    }

    static SerializationFormat detectFile(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return SerializationFormat::Unknown;
        }
        return detect(file.read(256));
    }

    static bool writeFile(const QString& path, const QJsonObject& object,
                          SerializationFormat format)
    {
        const QByteArray encoded = encode(object, format);
        if (encoded.isEmpty() || encoded.size() > kMaxDocumentBytes) {
            return false;
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        if (file.write(encoded) != encoded.size()) {
            file.cancelWriting();
            return false;
        }
        return file.commit();
    }

    static bool readFile(const QString& path, QJsonObject& object,
                         SerializationFormat format = SerializationFormat::Unknown,
                         QString* errorMessage = nullptr)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            if (errorMessage) {
                *errorMessage = file.errorString();
            }
            return false;
        }
        if (file.size() < 0 || file.size() > kMaxDocumentBytes) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Serialized document is too large");
            }
            return false;
        }
        const QByteArray bytes = file.readAll();
        const auto resolved = format == SerializationFormat::Unknown ? detect(bytes) : format;
        if (decode(bytes, resolved, object)) {
            return true;
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid serialized document");
        }
        return false;
    }
};

} // namespace ArtifactCore::Serialization
