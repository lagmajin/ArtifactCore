module;

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include <memory>
#include <utility>
#include <type_traits>
#include <concepts>

export module Serialization.JsonAdapter;

import Serialization.ISerializable;
import Serialization.Registry;

export namespace ArtifactCore::Serialization {

template <typename T>
class JsonSerializableAdapter final : public ISerializable {
public:
    JsonSerializableAdapter() = default;

    QJsonObject serialize() const override { return value_.toJson(); }

    bool deserialize(const QJsonObject& data) override
    {
        if constexpr (requires { { T::fromJson(data) } -> std::same_as<bool>; }) {
            return T::fromJson(data);
        } else if constexpr (requires { T::fromJson(data); }) {
            value_ = T::fromJson(data);
            return true;
        } else if constexpr (requires { { T::fromJsonStatic(data) } -> std::same_as<bool>; }) {
            return T::fromJsonStatic(data);
        } else if constexpr (requires { T::fromJsonStatic(data); }) {
            value_ = T::fromJsonStatic(data);
            return true;
        } else {
            return false;
        }
    }

    QString typeName() const override { return typeName_; }
    int schemaVersion() const override { return schemaVersion_; }

    const T& value() const noexcept { return value_; }
    T& value() noexcept { return value_; }

    static void configureType(const QString& typeName, int schemaVersion)
    {
        typeName_ = typeName;
        schemaVersion_ = schemaVersion;
    }

private:
    T value_{};
    inline static QString typeName_;
    inline static int schemaVersion_ = 1;
};

template <typename T>
void registerJsonSerializableType(const QString& typeName, int schemaVersion = 1)
{
    using Adapter = JsonSerializableAdapter<T>;
    Adapter::configureType(typeName, schemaVersion);
    SerializationRegistry::instance().registerType(
        typeName, [] { return std::make_unique<Adapter>(); });
}

template <typename T>
class JsonArraySerializableAdapter final : public ISerializable {
public:
    QJsonObject serialize() const override
    {
        return QJsonObject{{QStringLiteral("items"), value_.toJson()}};
    }

    bool deserialize(const QJsonObject& data) override
    {
        const QJsonValue items = data.value(QStringLiteral("items"));
        if (!items.isArray()) return false;
        if constexpr (requires { { T::fromJson(items.toArray()) } -> std::same_as<T>; }) {
            value_ = T::fromJson(items.toArray());
            return true;
        }
        return false;
    }

    QString typeName() const override { return typeName_; }
    int schemaVersion() const override { return schemaVersion_; }

    static void configureType(const QString& typeName, int schemaVersion)
    {
        typeName_ = typeName;
        schemaVersion_ = schemaVersion;
    }

private:
    T value_{};
    inline static QString typeName_;
    inline static int schemaVersion_ = 1;
};

template <typename T>
void registerJsonArraySerializableType(const QString& typeName, int schemaVersion = 1)
{
    using Adapter = JsonArraySerializableAdapter<T>;
    Adapter::configureType(typeName, schemaVersion);
    SerializationRegistry::instance().registerType(
        typeName, [] { return std::make_unique<Adapter>(); });
}

} // namespace ArtifactCore::Serialization
