module;
#include <cmath>
#include <map>
#include <mutex>
#include <sstream>

module Configuration.ConfigSchema;

import Configuration.LayeredConfigStore;
import std;

namespace ArtifactCore {

class ConfigSchema::Impl {
public:
    std::map<std::string, ConfigProperty, std::less<>> properties;
    mutable std::shared_mutex mutex;
};

namespace {
QString typeName(QVariant::Type type) {
    return QVariant::typeToName(type);
}

bool sameValue(const QVariant& left, const QVariant& right) {
    return left.userType() == right.userType() ? left == right : left.toString() == right.toString();
}

bool lessThan(const QVariant& left, const QVariant& right) {
    if (left.typeId() == QVariant::String) return left.toString() < right.toString();
    return left.toDouble() < right.toDouble();
}
}

ConfigSchema& ConfigSchema::instance() {
    static ConfigSchema schema;
    return schema;
}

ConfigSchema::ConfigSchema() : impl_(new Impl()) {}
ConfigSchema::~ConfigSchema() { delete impl_; }

bool ConfigSchema::registerProperty(const ConfigProperty& property) {
    if (property.key.empty() || !property.defaultValue.isValid() || property.type == QVariant::Invalid) return false;
    if (property.defaultValue.typeId() != property.type) return false;
    std::unique_lock lock(impl_->mutex);
    if (impl_->properties.contains(property.key)) return false;
    impl_->properties.emplace(property.key, property);
    return true;
}

ConfigSchema::ValidationResult ConfigSchema::validate(std::string_view key, const QVariant& value) const {
    std::shared_lock lock(impl_->mutex);
    const auto it = impl_->properties.find(key);
    if (it == impl_->properties.end()) return {false, "Unknown configuration key"};
    const auto& property = it->second;
    if (!value.isValid() || value.typeId() != property.type) {
        return {false, "Configuration value for '" + property.key + "' must have type " + typeName(property.type).toStdString()};
    }
    if (property.minValue && lessThan(value, *property.minValue)) return {false, "Configuration value is below the minimum"};
    if (property.maxValue && lessThan(*property.maxValue, value)) return {false, "Configuration value is above the maximum"};
    if (!property.allowedValues.empty()) {
        const bool allowed = std::any_of(property.allowedValues.begin(), property.allowedValues.end(),
                                         [&](const QVariant& candidate) { return sameValue(candidate, value); });
        if (!allowed) return {false, "Configuration value is not in the allowed values"};
    }
    return {true, {}};
}

const ConfigProperty* ConfigSchema::find(std::string_view key) const {
    std::shared_lock lock(impl_->mutex);
    const auto it = impl_->properties.find(key);
    return it == impl_->properties.end() ? nullptr : &it->second;
}

std::vector<const ConfigProperty*> ConfigSchema::allProperties() const {
    std::shared_lock lock(impl_->mutex);
    std::vector<const ConfigProperty*> result;
    result.reserve(impl_->properties.size());
    for (const auto& [key, property] : impl_->properties) result.push_back(&property);
    return result;
}

bool ConfigSchema::applyDefaultsToLayer(ConfigLayer layer) {
    const auto* target = LayeredConfigStore::instance().isLoaded(layer) ? &LayeredConfigStore::instance() : nullptr;
    if (!target || layer != ConfigLayer::System) return false;
    std::shared_lock lock(impl_->mutex);
    for (const auto& [key, property] : impl_->properties) {
        if (!target->value(key).isValid()) {
            if (!LayeredConfigStore::instance().setDefaultValue(key, property.defaultValue)) return false;
        }
    }
    return true;
}

}
