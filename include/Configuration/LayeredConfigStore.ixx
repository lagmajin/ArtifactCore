module;
#include <QString>
#include <QStringList>
#include <QVariant>
#include <string_view>
#include <functional>

export module Configuration.LayeredConfigStore;

import Configuration.ConfigLayer;

export namespace ArtifactCore {

class LayeredConfigStore {
public:
    static LayeredConfigStore& instance();

    bool loadLayer(ConfigLayer layer, const QString& path = {});
    bool saveLayer(ConfigLayer layer);
    bool unloadLayer(ConfigLayer layer);

    QVariant value(std::string_view key) const;
    QVariant value(std::string_view key, const QVariant& defaultValue) const;
    QVariant value(const QString& key) const;
    QVariant value(const QString& key, const QVariant& defaultValue) const;
    QVariant value(const char* key) const { return value(std::string_view(key)); }
    QVariant value(const char* key, const QVariant& defaultValue) const {
        return value(std::string_view(key), defaultValue);
    }
    bool valueBool(std::string_view key, bool defaultValue = false) const;
    bool valueBool(const QString& key, bool defaultValue = false) const;
    bool valueBool(const char* key, bool defaultValue = false) const {
        return valueBool(std::string_view(key), defaultValue);
    }
    qlonglong valueInt64(std::string_view key, qlonglong defaultValue = 0) const;
    qlonglong valueInt64(const QString& key, qlonglong defaultValue = 0) const;
    qlonglong valueInt64(const char* key, qlonglong defaultValue = 0) const {
        return valueInt64(std::string_view(key), defaultValue);
    }
    double valueDouble(std::string_view key, double defaultValue = 0.0) const;
    QString valueString(std::string_view key, const QString& defaultValue = {}) const;
    QString valueString(const QString& key, const QString& defaultValue = {}) const;
    QString valueString(const char* key, const QString& defaultValue = {}) const {
        return valueString(std::string_view(key), defaultValue);
    }

    bool setValue(std::string_view key, const QVariant& value);
    bool setValue(const QString& key, const QVariant& value);
    bool setValue(const char* key, const QVariant& value) {
        return setValue(std::string_view(key), value);
    }
    void setValidator(std::function<bool(std::string_view, const QVariant&)> validator);
    bool setValue(ConfigLayer layer, std::string_view key, const QVariant& value);
    // Bootstrap-only write used to materialize schema defaults in System.
    bool setDefaultValue(std::string_view key, const QVariant& value);
    bool removeValue(ConfigLayer layer, std::string_view key);
    bool clearLayer(ConfigLayer layer);

    ConfigLayer sourceLayer(std::string_view key) const;
    QStringList allKeys() const;
    bool exportLayer(ConfigLayer layer, const QString& path) const;
    bool importLayer(const QString& path, ConfigLayer targetLayer);
    bool sync();
    bool reloadChangedLayers(bool discardLocalChanges = false);

    bool isLoaded(ConfigLayer layer) const;
    QString layerPath(ConfigLayer layer) const;

private:
    LayeredConfigStore();
    ~LayeredConfigStore();
    LayeredConfigStore(const LayeredConfigStore&) = delete;
    LayeredConfigStore& operator=(const LayeredConfigStore&) = delete;

    class Impl;
    Impl* impl_ = nullptr;
};

}
