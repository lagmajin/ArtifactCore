module;

#include <QString>
#include <QStringList>
#include <type_traits>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

export module Serialization.Registry;

import Serialization.ISerializable;

export namespace ArtifactCore::Serialization {

class SerializationRegistry {
public:
    using Factory = std::function<std::unique_ptr<ISerializable>()>;

    static SerializationRegistry& instance()
    {
        static SerializationRegistry registry;
        return registry;
    }

    void registerType(const QString& typeName, Factory factory)
    {
        if (!typeName.trimmed().isEmpty() && factory) {
            std::lock_guard lock(mutex_);
            factories_[typeName] = std::move(factory);
        }
    }

    std::unique_ptr<ISerializable> create(const QString& typeName) const
    {
        Factory factory;
        {
            std::lock_guard lock(mutex_);
            const auto it = factories_.find(typeName);
            if (it == factories_.end()) {
                return nullptr;
            }
            factory = it->second;
        }
        return factory();
    }

    bool isRegistered(const QString& typeName) const
    {
        std::lock_guard lock(mutex_);
        return factories_.find(typeName) != factories_.end();
    }

    QStringList registeredTypes() const
    {
        QStringList result;
        std::lock_guard lock(mutex_);
        for (const auto& [typeName, factory] : factories_) {
            (void)factory;
            result.append(typeName);
        }
        return result;
    }

private:
    std::map<QString, Factory> factories_;
    mutable std::mutex mutex_;
};

template <typename T>
void registerSerializableType()
{
    static_assert(std::is_base_of_v<ISerializable, T>);
    auto instance = std::make_unique<T>();
    SerializationRegistry::instance().registerType(
        instance->typeName(), [] { return std::make_unique<T>(); });
}

} // namespace ArtifactCore::Serialization
