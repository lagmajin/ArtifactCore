module;

#include <QJsonObject>
#include <QString>

#include <memory>

export module Serialization.ISerializable;

export namespace ArtifactCore::Serialization {

/** Common JSON-compatible contract for incrementally migrating persisted types. */
class ISerializable {
public:
    virtual ~ISerializable() = default;

    virtual QJsonObject serialize() const = 0;
    virtual bool deserialize(const QJsonObject& data) = 0;
    virtual QString typeName() const = 0;
    virtual int schemaVersion() const = 0;
};

} // namespace ArtifactCore::Serialization
