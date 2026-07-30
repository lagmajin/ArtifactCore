module;
class tst_QList;
#include <utility>
#include <QString>
#include <QVariant>
#include <QHash>
#include <QVector>
#include <QJsonObject>
#include "../Define/DllExportMacro.hpp"

export module Property;

import Property.Abstract;
import Property.Group;
import Property.Path;
import Property.SerializationBridge;
import Memory.SharedPtr;

export namespace ArtifactCore {

/**
 * @brief AbstractProperty の便利な共有ポインタラッパークラス
 */
class LIBRARY_DLL_API Property {
public:
    using element_type = AbstractProperty;

    Property() = default;
    Property(std::nullptr_t) noexcept {}
    explicit Property(const SharedPtr<AbstractProperty>& property) noexcept : property_(property) {}
    explicit Property(SharedPtr<AbstractProperty>&& property) noexcept : property_(std::move(property)) {}

    // 便利コンストラクタ群
    
    // Float
    Property(const QString& name, float value)
        : property_(makeShared<AbstractProperty>())
    {
        property_->setName(name);
        property_->setType(PropertyType::Float);
        property_->setValue(value);
        property_->setDefaultValue(value);
    }

    Property(const QString& name, float value, float min, float max, const QString& unit = "")
        : property_(makeShared<AbstractProperty>())
    {
        property_->setName(name);
        property_->setType(PropertyType::Float);
        property_->setValue(value);
        property_->setDefaultValue(value);
        property_->setHardRange(min, max);
        if (!unit.isEmpty()) {
            property_->setUnit(unit);
        }
    }

    // Boolean
    Property(const QString& name, bool value)
        : property_(makeShared<AbstractProperty>())
    {
        property_->setName(name);
        property_->setType(PropertyType::Boolean);
        property_->setValue(value);
        property_->setDefaultValue(value);
    }

    // Integer
    Property(const QString& name, int value)
        : property_(makeShared<AbstractProperty>())
    {
        property_->setName(name);
        property_->setType(PropertyType::Integer);
        property_->setValue(value);
        property_->setDefaultValue(value);
    }

    // String
    Property(const QString& name, const QString& value)
        : property_(makeShared<AbstractProperty>())
    {
        property_->setName(name);
        property_->setType(PropertyType::String);
        property_->setValue(value);
        property_->setDefaultValue(value);
    }

    // Point2D
    Property(const QString& name, const QPointF& value)
        : property_(makeShared<AbstractProperty>())
    {
        property_->setName(name);
        property_->setType(PropertyType::Point2D);
        property_->setValue(value);
        property_->setDefaultValue(value);
    }

    AbstractProperty* get() const noexcept
    {
        return property_.get();
    }

    AbstractProperty* operator->() const noexcept
    {
        return property_.get();
    }

    AbstractProperty& operator*() const noexcept
    {
        return *property_;
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>(property_);
    }

    SharedPtr<AbstractProperty> shared() const noexcept
    {
        return property_;
    }

    operator SharedPtr<AbstractProperty>() const
    {
        return property_;
    }

    // Builder-style extensions for properties
    Property& setHardRange(double min, double max) {
        property_->setHardRange(min, max);
        return *this;
    }
    
    Property& setSoftRange(double min, double max) {
        property_->setSoftRange(min, max);
        return *this;
    }

    Property& setStep(double step) {
        property_->setStep(step);
        return *this;
    }

    Property& setUnit(const QString& unit) {
        property_->setUnit(unit);
        return *this;
    }

    Property& setTooltip(const QString& tooltip) {
        property_->setTooltip(tooltip);
        return *this;
    }

    Property& setDisplayPriority(int priority) {
        property_->setDisplayPriority(priority);
        return *this;
    }

private:
    SharedPtr<AbstractProperty> property_;
};

/**
 * @brief プロパティ所有者の記述
 */
struct PropertyOwnerDescriptor {
    QString ownerPath;
    QString displayName;
    QString ownerType;
    bool readOnly = false;
};

/**
 * @brief プロパティ参照ハンドル
 */
struct PropertyHandle {
    QString ownerPath;
    QString propertyName;
    AbstractPropertyPtr property;

    bool isValid() const
    {
        return static_cast<bool>(property);
    }

    QString path() const
    {
        if (ownerPath.isEmpty()) {
            return propertyName;
        }
        return ownerPath + QStringLiteral(".") + propertyName;
    }

    PropertyPath typedPath() const
    {
        return PropertyPath(ownerPath, propertyName);
    }
};

inline QString propertyPathJoin(const QString& ownerPath, const QString& propertyName)
{
    if (ownerPath.isEmpty()) {
        return propertyName;
    }
    if (propertyName.isEmpty()) {
        return ownerPath;
    }
    return ownerPath + QStringLiteral(".") + propertyName;
}

inline QString propertyTypeToString(PropertyType type)
{
    switch (type) {
    case PropertyType::Float:
        return QStringLiteral("Float");
    case PropertyType::Integer:
        return QStringLiteral("Integer");
    case PropertyType::Boolean:
        return QStringLiteral("Boolean");
    case PropertyType::Color:
        return QStringLiteral("Color");
    case PropertyType::String:
        return QStringLiteral("String");
    case PropertyType::ObjectReference:
        return QStringLiteral("ObjectReference");
    case PropertyType::Point2D:
        return QStringLiteral("Point2D");
    }
    return QStringLiteral("Unknown");
}

/**
 * @brief UI 非依存のプロパティ registry
 */
class LIBRARY_DLL_API PropertyRegistry {
public:
    struct Entry {
        PropertyOwnerDescriptor descriptor;
        SharedPtr<PropertyGroup> group;
    };

    void clear()
    {
        entries_.clear();
    }

    bool containsOwner(const QString& ownerPath) const
    {
        return entries_.contains(ownerPath);
    }

    void registerOwner(const PropertyOwnerDescriptor& descriptor,
                       const SharedPtr<PropertyGroup>& group)
    {
        if (!group) {
            return;
        }
        Entry entry;
        entry.descriptor = descriptor;
        entry.group = group;
        if (entry.descriptor.ownerPath.isEmpty()) {
            entry.descriptor.ownerPath = group->name();
        }
        entries_[entry.descriptor.ownerPath] = entry;
    }

    void registerOwner(const PropertyOwnerDescriptor& descriptor, const PropertyGroup& group)
    {
        registerOwner(descriptor, makeShared<PropertyGroup>(group));
    }

    void registerOwnerSnapshot(const QString& ownerPath,
                               const QString& displayName,
                               const QString& ownerType,
                               const PropertyGroup& group,
                               bool readOnly = false)
    {
        PropertyOwnerDescriptor descriptor;
        descriptor.ownerPath = ownerPath;
        descriptor.displayName = displayName;
        descriptor.ownerType = ownerType;
        descriptor.readOnly = readOnly;
        registerOwner(descriptor, group);
    }

    void unregisterOwner(const QString& ownerPath)
    {
        entries_.remove(ownerPath);
    }

    QVector<PropertyOwnerDescriptor> owners() const
    {
        QVector<PropertyOwnerDescriptor> result;
        result.reserve(entries_.size());
        for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
            result.push_back(it.value().descriptor);
        }
        return result;
    }

    QStringList ownerPaths() const
    {
        QStringList result;
        result.reserve(entries_.size());
        for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
            result.push_back(it.key());
        }
        return result;
    }

    bool tryGetOwner(const QString& ownerPath, PropertyOwnerDescriptor* descriptor, SharedPtr<PropertyGroup>* group = nullptr) const
    {
        const auto it = entries_.find(ownerPath);
        if (it == entries_.end()) {
            return false;
        }
        if (descriptor) {
            *descriptor = it->descriptor;
        }
        if (group) {
            *group = it->group;
        }
        return true;
    }

    QVector<PropertyHandle> handlesForOwner(const QString& ownerPath) const
    {
        QVector<PropertyHandle> result;
        const auto it = entries_.find(ownerPath);
        if (it == entries_.end() || !it->group) {
            return result;
        }
        const auto properties = it->group->allProperties();
        result.reserve(static_cast<int>(properties.size()));
        for (const auto& property : properties) {
            if (!property) {
                continue;
            }
            result.push_back(PropertyHandle{
                it->descriptor.ownerPath,
                property->getName(),
                property
            });
        }
        return result;
    }

    QVector<PropertyHandle> enumerate() const
    {
        QVector<PropertyHandle> result;
        for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
            const auto& entry = it.value();
            if (!entry.group) {
                continue;
            }
            const auto properties = entry.group->allProperties();
            for (const auto& property : properties) {
                if (!property) {
                    continue;
                }
                result.push_back(PropertyHandle{
                    entry.descriptor.ownerPath,
                    property->getName(),
                    property
                });
            }
        }
        return result;
    }

    AbstractPropertyPtr findProperty(const QString& propertyPath) const
    {
        const int sep = propertyPath.lastIndexOf(QLatin1Char('.'));
        const QString ownerPath = sep >= 0 ? propertyPath.left(sep) : QString();
        const QString propertyName = sep >= 0 ? propertyPath.mid(sep + 1) : propertyPath;

        if (!ownerPath.isEmpty()) {
            const auto it = entries_.find(ownerPath);
            if (it == entries_.end() || !it->group) {
                return {};
            }
            return it->group->findProperty(propertyName);
        }

        for (auto it = entries_.cbegin(); it != entries_.cend(); ++it) {
            const auto& entry = it.value();
            if (!entry.group) {
                continue;
            }
            auto found = entry.group->findProperty(propertyName);
            if (found) {
                return found;
            }
        }
        return {};
    }

    AbstractPropertyPtr findProperty(const PropertyPath& path) const
    {
        return findProperty(path.toString());
    }

    PropertyHandle handleForPath(const QString& propertyPath) const
    {
        const int sep = propertyPath.lastIndexOf(QLatin1Char('.'));
        const QString ownerPath = sep >= 0 ? propertyPath.left(sep) : QString();
        const QString propertyName = sep >= 0 ? propertyPath.mid(sep + 1) : propertyPath;
        auto property = findProperty(propertyPath);
        return PropertyHandle{ownerPath, propertyName, property};
    }

    PropertyHandle handleForPath(const PropertyPath& path) const
    {
        return handleForPath(path.toString());
    }

    SerializedOwner serializeOwnerToSnapshot(const QString& ownerPath) const
    {
        const auto it = entries_.find(ownerPath);
        if (it == entries_.end()) {
            return SerializedOwner{};
        }
        const auto& entry = it.value();
        return PropertySerializationBridge::serializeOwner(
            PropertyPath(ownerPath), entry.descriptor.displayName,
            entry.descriptor.ownerType, entry.descriptor.readOnly, entry.group);
    }

    QJsonObject serializeOwnerToJson(const QString& ownerPath) const
    {
        const auto so = serializeOwnerToSnapshot(ownerPath);
        if (so.properties.empty() && so.ownerPath.isEmpty()) {
            return QJsonObject{};
        }
        return PropertySerializationBridge::serializedOwnerToJson(so);
    }

private:
    QHash<QString, Entry> entries_;
};

inline PropertyRegistry& globalPropertyRegistry()
{
    static PropertyRegistry registry;
    return registry;
}

/**
 * @brief PropertyRegistry の read-only な外部参照アダプター
 *
 * RPC / CLI / script / headless などの UI 外の入口から
 * プロパティを安全に参照するためのインターフェース。
 * 値の書き換えは行わない（Phase 7 は read-only）。
 */
class LIBRARY_DLL_API PropertyRegistryReadOnlyAdapter {
public:
    struct PropertyOwnerSnapshot {
        QString ownerPath;
        QString displayName;
        QString ownerType;
        bool readOnly = true;
        int propertyCount = 0;
        bool isValid = false;
    };

    struct PropertyValueSnapshot {
        QString ownerPath;
        QString propertyName;
        QString propertyType;
        QVariant currentValue;
        SharedPtr<AbstractProperty> property;
        bool isReadOnly = true;
        bool isValid = false;
    };

    static PropertyValueSnapshot queryProperty(const QString& propertyPath)
    {
        PropertyValueSnapshot result;
        result.ownerPath = propertyPath;
        result.isReadOnly = true;

        auto& registry = globalPropertyRegistry();
        auto handle = registry.handleForPath(propertyPath);
        if (!handle.isValid()) {
            return result;
        }

        result.ownerPath = handle.ownerPath;
        result.propertyName = handle.propertyName;
        result.property = handle.property;
        if (result.property) {
            result.propertyType = propertyTypeToString(result.property->getType());
            result.currentValue = result.property->getValue();
            result.isValid = true;
        }
        return result;
    }

    static PropertyOwnerSnapshot queryOwner(const QString& ownerPath)
    {
        PropertyOwnerSnapshot result;
        result.ownerPath = ownerPath;
        result.readOnly = true;

        auto& registry = globalPropertyRegistry();
        PropertyOwnerDescriptor descriptor;
        SharedPtr<PropertyGroup> group;
        if (!registry.tryGetOwner(ownerPath, &descriptor, &group)) {
            return result;
        }

        result.ownerPath = descriptor.ownerPath;
        result.displayName = descriptor.displayName;
        result.ownerType = descriptor.ownerType;
        result.readOnly = descriptor.readOnly;
        result.propertyCount = group ? static_cast<int>(group->propertyCount()) : 0;
        result.isValid = true;
        return result;
    }

    static QVector<PropertyOwnerSnapshot> queryAllOwners()
    {
        QVector<PropertyOwnerSnapshot> results;
        auto& registry = globalPropertyRegistry();
        const auto ownerPaths = registry.ownerPaths();
        results.reserve(ownerPaths.size());
        for (const auto& ownerPath : ownerPaths) {
            results.push_back(queryOwner(ownerPath));
        }
        return results;
    }

    static QVector<PropertyValueSnapshot> queryAllProperties()
    {
        QVector<PropertyValueSnapshot> results;
        auto& registry = globalPropertyRegistry();
        for (const auto& handle : registry.enumerate()) {
            if (!handle.property) continue;
            PropertyValueSnapshot snap;
            snap.ownerPath = handle.ownerPath;
            snap.propertyName = handle.propertyName;
            snap.property = handle.property;
            snap.propertyType = propertyTypeToString(handle.property->getType());
            snap.currentValue = handle.property->getValue();
            snap.isReadOnly = true;
            snap.isValid = true;
            results.push_back(snap);
        }
        return results;
    }

    static QStringList listOwners()
    {
        QStringList names;
        auto& registry = globalPropertyRegistry();
        for (const auto& ownerPath : registry.ownerPaths()) {
            names.push_back(ownerPath);
        }
        return names;
    }

private:
    AbstractPropertyPtr property;
};

} // namespace ArtifactCore
