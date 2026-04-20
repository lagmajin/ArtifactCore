module;
#include <utility>
#include <QString>
#include <QVariant>
#include <QHash>
#include <QVector>
#include <memory>
#include "../Define/DllExportMacro.hpp"

export module Property;

import Property.Abstract;
import Property.Group;

export namespace ArtifactCore {

/**
 * @brief AbstractProperty の便利な共有ポインタラッパークラス
 */
class LIBRARY_DLL_API Property : public std::shared_ptr<AbstractProperty> {
public:
    using std::shared_ptr<AbstractProperty>::shared_ptr;

    // 便利コンストラクタ群
    
    // Float
    Property(const QString& name, float value) 
        : std::shared_ptr<AbstractProperty>(std::make_shared<AbstractProperty>()) 
    {
        (*this)->setName(name);
        (*this)->setType(PropertyType::Float);
        (*this)->setValue(value);
        (*this)->setDefaultValue(value);
    }

    Property(const QString& name, float value, float min, float max, const QString& unit = "")
        : std::shared_ptr<AbstractProperty>(std::make_shared<AbstractProperty>())
    {
        (*this)->setName(name);
        (*this)->setType(PropertyType::Float);
        (*this)->setValue(value);
        (*this)->setDefaultValue(value);
        (*this)->setHardRange(min, max);
        if (!unit.isEmpty()) {
            (*this)->setUnit(unit);
        }
    }

    // Boolean
    Property(const QString& name, bool value)
        : std::shared_ptr<AbstractProperty>(std::make_shared<AbstractProperty>())
    {
        (*this)->setName(name);
        (*this)->setType(PropertyType::Boolean);
        (*this)->setValue(value);
        (*this)->setDefaultValue(value);
    }

    // Integer
    Property(const QString& name, int value)
        : std::shared_ptr<AbstractProperty>(std::make_shared<AbstractProperty>())
    {
        (*this)->setName(name);
        (*this)->setType(PropertyType::Integer);
        (*this)->setValue(value);
        (*this)->setDefaultValue(value);
    }

    // String
    Property(const QString& name, const QString& value)
        : std::shared_ptr<AbstractProperty>(std::make_shared<AbstractProperty>())
    {
        (*this)->setName(name);
        (*this)->setType(PropertyType::String);
        (*this)->setValue(value);
        (*this)->setDefaultValue(value);
    }

    // Builder-style extensions for properties
    Property& setHardRange(double min, double max) {
        (*this)->setHardRange(min, max);
        return *this;
    }
    
    Property& setSoftRange(double min, double max) {
        (*this)->setSoftRange(min, max);
        return *this;
    }

    Property& setStep(double step) {
        (*this)->setStep(step);
        return *this;
    }

    Property& setUnit(const QString& unit) {
        (*this)->setUnit(unit);
        return *this;
    }

    Property& setTooltip(const QString& tooltip) {
        (*this)->setTooltip(tooltip);
        return *this;
    }

    Property& setDisplayPriority(int priority) {
        (*this)->setDisplayPriority(priority);
        return *this;
    }
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

/**
 * @brief UI 非依存のプロパティ registry
 */
class LIBRARY_DLL_API PropertyRegistry {
public:
    struct Entry {
        PropertyOwnerDescriptor descriptor;
        std::shared_ptr<PropertyGroup> group;
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
                       const std::shared_ptr<PropertyGroup>& group)
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
        registerOwner(descriptor, std::make_shared<PropertyGroup>(group));
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

    PropertyHandle handleForPath(const QString& propertyPath) const
    {
        const int sep = propertyPath.lastIndexOf(QLatin1Char('.'));
        const QString ownerPath = sep >= 0 ? propertyPath.left(sep) : QString();
        const QString propertyName = sep >= 0 ? propertyPath.mid(sep + 1) : propertyPath;
        auto property = findProperty(propertyPath);
        return PropertyHandle{ownerPath, propertyName, property};
    }

private:
    QHash<QString, Entry> entries_;
};

inline PropertyRegistry& globalPropertyRegistry()
{
    static PropertyRegistry registry;
    return registry;
}

} // namespace ArtifactCore
