module;
#include <QVariant>
#include <QString>
#include <vector>
#include <string>
#include <optional>
#include <string_view>

export module Configuration.ConfigSchema;

import Configuration.ConfigLayer;

export namespace ArtifactCore {

struct ConfigProperty {
    std::string key;
    std::string description;
    QVariant::Type type = QVariant::Invalid;
    QVariant defaultValue;
    std::optional<QVariant> minValue;
    std::optional<QVariant> maxValue;
    std::vector<QVariant> allowedValues;
    bool requiresRestart = false;
    bool projectOverrideable = true;
};

class ConfigSchema {
public:
    struct ValidationResult {
        bool valid = false;
        std::string errorMessage;
    };

    static ConfigSchema& instance();
    bool registerProperty(const ConfigProperty& property);
    ValidationResult validate(std::string_view key, const QVariant& value) const;
    const ConfigProperty* find(std::string_view key) const;
    std::vector<const ConfigProperty*> allProperties() const;
    bool applyDefaultsToLayer(ConfigLayer layer);

private:
    ConfigSchema();
    ~ConfigSchema();
    ConfigSchema(const ConfigSchema&) = delete;
    ConfigSchema& operator=(const ConfigSchema&) = delete;
    class Impl;
    Impl* impl_ = nullptr;
};

}
