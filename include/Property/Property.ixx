module;
#include <utility>
#include <QString>
#include <QVariant>
#include <memory>
#include "../Define/DllExportMacro.hpp"

export module Property;

import Property.Abstract;

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

} // namespace ArtifactCore
