module;

#include <QJsonObject>
#include <QString>

#include <cmath>
#include <limits>
#include <utility>

export module Serialization.Envelope;

import Serialization.ISerializable;
import Serialization.Registry;
import Serialization.SchemaMigration;

export namespace ArtifactCore::Serialization {

struct SerializationEnvelope {
    QString typeName;
    int schemaVersion = 0;
    QJsonObject data;
};

inline QJsonObject makeEnvelope(const ISerializable& value)
{
    return {
        {QStringLiteral("_type"), value.typeName()},
        {QStringLiteral("_schemaVersion"), value.schemaVersion()},
        {QStringLiteral("data"), value.serialize()}
    };
}

inline bool readEnvelope(const QJsonObject& object, SerializationEnvelope& envelope)
{
    const auto type = object.value(QStringLiteral("_type"));
    const auto version = object.value(QStringLiteral("_schemaVersion"));
    const auto data = object.value(QStringLiteral("data"));
    if (!type.isString() || type.toString().isEmpty() ||
        !version.isDouble() || !data.isObject()) {
        return false;
    }
    const double numericVersion = version.toDouble();
    if (!std::isfinite(numericVersion) ||
        numericVersion != std::floor(numericVersion) ||
        numericVersion < 0.0 ||
        numericVersion > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }
    envelope.typeName = type.toString();
    envelope.schemaVersion = static_cast<int>(numericVersion);
    envelope.data = data.toObject();
    return envelope.schemaVersion >= 0;
}

inline std::unique_ptr<ISerializable> deserializeEnvelope(const QJsonObject& object)
{
    SerializationEnvelope envelope;
    if (!readEnvelope(object, envelope)) {
        return nullptr;
    }

    auto value = SerializationRegistry::instance().create(envelope.typeName);
    if (!value) {
        return nullptr;
    }

    const int currentVersion = value->schemaVersion();
    if (envelope.schemaVersion > currentVersion) {
        return nullptr;
    }
    if (!SchemaMigrationRegistry::instance().migrate(
            envelope.typeName, envelope.schemaVersion, currentVersion, envelope.data)) {
        return nullptr;
    }
    if (!value->deserialize(envelope.data)) {
        return nullptr;
    }
    return value;
}

} // namespace ArtifactCore::Serialization
