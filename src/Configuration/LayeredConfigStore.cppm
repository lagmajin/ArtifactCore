module;
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSaveFile>
#include <QCborMap>
#include <QCborValue>
#include <QStandardPaths>

module Configuration.LayeredConfigStore;

import Core.FastSettingsStore;
import std;

namespace ArtifactCore {

namespace {
constexpr int layerCount = 4;

int indexOf(ConfigLayer layer) noexcept {
    const auto index = static_cast<int>(layer);
    return index >= 0 && index < layerCount ? index : -1;
}

QString keyString(std::string_view key) {
    return QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()));
}
}

class LayeredConfigStore::Impl {
public:
    struct LayerData {
        ConfigLayer layer = ConfigLayer::None;
        QString path;
        std::unique_ptr<FastSettingsStore> store;
        bool loaded = false;
        bool writable = false;
        QDateTime lastModified;
    };

    std::array<LayerData, layerCount> layers;
    std::function<bool(std::string_view, const QVariant&)> validator;

    Impl() {
        for (int i = 0; i < layerCount; ++i) {
            layers[static_cast<size_t>(i)].layer = static_cast<ConfigLayer>(i);
        }
        layers[static_cast<int>(ConfigLayer::System)].writable = false;
        layers[static_cast<int>(ConfigLayer::User)].writable = true;
        layers[static_cast<int>(ConfigLayer::Project)].writable = true;
        layers[static_cast<int>(ConfigLayer::Session)].writable = true;
    }

    LayerData* layer(ConfigLayer value) {
        const int index = indexOf(value);
        return index < 0 ? nullptr : &layers[static_cast<size_t>(index)];
    }
    const LayerData* layer(ConfigLayer value) const {
        const int index = indexOf(value);
        return index < 0 ? nullptr : &layers[static_cast<size_t>(index)];
    }
};

LayeredConfigStore& LayeredConfigStore::instance() {
    static LayeredConfigStore store;
    return store;
}

LayeredConfigStore::LayeredConfigStore() : impl_(new Impl()) {
    // System and Session are intentionally memory-only. User is the single
    // application-wide persistent layer used by callers that do not have a
    // project context yet.
    loadLayer(ConfigLayer::System);
    loadLayer(ConfigLayer::Session);
    const QString userPath = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath(QStringLiteral("settings.cbor"));
    loadLayer(ConfigLayer::User, userPath);
}
LayeredConfigStore::~LayeredConfigStore() { delete impl_; }

bool LayeredConfigStore::loadLayer(ConfigLayer layer, const QString& path) {
    auto* data = impl_->layer(layer);
    if (!data || (layer == ConfigLayer::Session && !path.isEmpty()) ||
        (layer == ConfigLayer::System && !path.isEmpty())) return false;
    if (layer == ConfigLayer::Session || layer == ConfigLayer::System) {
        data->store = std::make_unique<FastSettingsStore>();
        data->path.clear();
        data->loaded = true;
        return true;
    }
    if (path.trimmed().isEmpty()) return false;
    auto store = std::make_unique<FastSettingsStore>();
    if (!store->open(path)) return false;
    data->path = path;
    data->store = std::move(store);
    data->loaded = true;
    data->lastModified = QFileInfo(path).lastModified();
    return true;
}

bool LayeredConfigStore::saveLayer(ConfigLayer layer) {
    auto* data = impl_->layer(layer);
    if (!data || !data->loaded || !data->store) return false;
    return layer == ConfigLayer::Session ? true : data->store->sync();
}

bool LayeredConfigStore::unloadLayer(ConfigLayer layer) {
    if (layer == ConfigLayer::Session) return false;
    auto* data = impl_->layer(layer);
    if (!data || !data->loaded) return false;
    if (data->writable && !saveLayer(layer)) return false;
    data->store.reset();
    data->path.clear();
    data->loaded = false;
    data->lastModified = {};
    return true;
}

QVariant LayeredConfigStore::value(std::string_view key) const {
    const QString qKey = keyString(key);
    for (int i = layerCount - 1; i >= 0; --i) {
        const auto& data = impl_->layers[static_cast<size_t>(i)];
        if (data.loaded && data.store && data.store->contains(qKey)) return data.store->value(qKey);
    }
    return {};
}

QVariant LayeredConfigStore::value(std::string_view key, const QVariant& defaultValue) const {
    const QVariant result = value(key);
    return result.isValid() ? result : defaultValue;
}

QVariant LayeredConfigStore::value(const QString& key) const {
    const QByteArray utf8 = key.toUtf8();
    return value(std::string_view(utf8.constData(), static_cast<size_t>(utf8.size())));
}

QVariant LayeredConfigStore::value(const QString& key, const QVariant& defaultValue) const {
    const QByteArray utf8 = key.toUtf8();
    return value(std::string_view(utf8.constData(), static_cast<size_t>(utf8.size())), defaultValue);
}

bool LayeredConfigStore::valueBool(std::string_view key, bool defaultValue) const {
    const QVariant result = value(key);
    return result.isValid() ? result.toBool() : defaultValue;
}

bool LayeredConfigStore::valueBool(const QString& key, bool defaultValue) const {
    const QByteArray utf8 = key.toUtf8();
    return valueBool(std::string_view(utf8.constData(), static_cast<size_t>(utf8.size())), defaultValue);
}

qlonglong LayeredConfigStore::valueInt64(std::string_view key, qlonglong defaultValue) const {
    const QVariant result = value(key);
    if (!result.isValid()) return defaultValue;
    bool ok = false;
    const auto converted = result.toLongLong(&ok);
    return ok ? converted : defaultValue;
}

qlonglong LayeredConfigStore::valueInt64(const QString& key, qlonglong defaultValue) const {
    const QByteArray utf8 = key.toUtf8();
    return valueInt64(std::string_view(utf8.constData(), static_cast<size_t>(utf8.size())), defaultValue);
}

double LayeredConfigStore::valueDouble(std::string_view key, double defaultValue) const {
    const QVariant result = value(key);
    if (!result.isValid()) return defaultValue;
    bool ok = false;
    const double converted = result.toDouble(&ok);
    return ok ? converted : defaultValue;
}

QString LayeredConfigStore::valueString(std::string_view key, const QString& defaultValue) const {
    const QVariant result = value(key);
    return result.isValid() ? result.toString() : defaultValue;
}

QString LayeredConfigStore::valueString(const QString& key, const QString& defaultValue) const {
    const QByteArray utf8 = key.toUtf8();
    return valueString(std::string_view(utf8.constData(), static_cast<size_t>(utf8.size())), defaultValue);
}

bool LayeredConfigStore::setValue(std::string_view key, const QVariant& value) {
    if (value.isValid() && impl_->validator && !impl_->validator(key, value)) return false;
    for (int i = layerCount - 1; i >= 0; --i) {
        auto& data = impl_->layers[static_cast<size_t>(i)];
        if (data.loaded && data.store && data.writable) {
            data.store->setValue(keyString(key), value);
            return true;
        }
    }
    return false;
}

bool LayeredConfigStore::setValue(const QString& key, const QVariant& value) {
    const QByteArray utf8 = key.toUtf8();
    return setValue(std::string_view(utf8.constData(), static_cast<size_t>(utf8.size())), value);
}

void LayeredConfigStore::setValidator(
    std::function<bool(std::string_view, const QVariant&)> validator) {
    impl_->validator = std::move(validator);
}

bool LayeredConfigStore::setValue(ConfigLayer layer, std::string_view key, const QVariant& value) {
    auto* data = impl_->layer(layer);
    if (!data || !data->loaded || !data->store || !data->writable) return false;
    if (value.isValid() && impl_->validator && !impl_->validator(key, value)) return false;
    if (!value.isValid()) data->store->remove(keyString(key));
    else data->store->setValue(keyString(key), value);
    return true;
}

bool LayeredConfigStore::setDefaultValue(std::string_view key, const QVariant& value) {
    auto* data = impl_->layer(ConfigLayer::System);
    if (!data || !data->loaded || !data->store || !value.isValid()) return false;
    if (data->store->contains(keyString(key))) return true;
    data->store->setValue(keyString(key), value);
    return true;
}

bool LayeredConfigStore::removeValue(ConfigLayer layer, std::string_view key) {
    return setValue(layer, key, QVariant());
}

bool LayeredConfigStore::clearLayer(ConfigLayer layer) {
    auto* data = impl_->layer(layer);
    if (!data || !data->loaded || !data->store || !data->writable ||
        layer == ConfigLayer::Session) return false;
    data->store->clear();
    return data->store->sync();
}

ConfigLayer LayeredConfigStore::sourceLayer(std::string_view key) const {
    const QString qKey = keyString(key);
    for (int i = layerCount - 1; i >= 0; --i) {
        const auto& data = impl_->layers[static_cast<size_t>(i)];
        if (data.loaded && data.store && data.store->contains(qKey)) return data.layer;
    }
    return ConfigLayer::None;
}

QStringList LayeredConfigStore::allKeys() const {
    QSet<QString> keys;
    for (const auto& data : impl_->layers) {
        if (!data.loaded || !data.store) continue;
        for (const auto& key : data.store->keys()) keys.insert(key);
    }
    auto result = keys.values();
    result.sort();
    return result;
}

bool LayeredConfigStore::exportLayer(ConfigLayer layer, const QString& path) const {
    const auto* data = impl_->layer(layer);
    if (!data || !data->loaded || !data->store || path.trimmed().isEmpty()) return false;
    const QVariantMap values = [&] {
        QVariantMap map;
        for (const auto& key : data->store->keys()) map.insert(key, data->store->value(key));
        return map;
    }();
    const QByteArray payload = QCborValue(QCborMap::fromVariantMap(values)).toCbor();
    QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size()) return false;
    return file.commit();
}

bool LayeredConfigStore::importLayer(const QString& path, ConfigLayer targetLayer) {
    auto* data = impl_->layer(targetLayer);
    if (!data || !data->loaded || !data->store || !data->writable) return false;
    FastSettingsStore input;
    if (!input.open(path)) return false;
    data->store->beginBatch();
    for (const auto& key : input.keys()) data->store->setValue(key, input.value(key));
    data->store->endBatch(true);
    return true;
}

bool LayeredConfigStore::sync() {
    bool success = true;
    for (const auto& data : impl_->layers) {
        if (data.loaded && data.writable && data.layer != ConfigLayer::Session) {
            success = saveLayer(data.layer) && success;
        }
    }
    return success;
}

bool LayeredConfigStore::reloadChangedLayers(bool discardLocalChanges) {
    bool success = true;
    for (auto& data : impl_->layers) {
        if (!data.loaded || !data.store || data.path.isEmpty()) continue;
        const QDateTime current = QFileInfo(data.path).lastModified();
        if (current == data.lastModified) continue;
        if (!data.store->reload(discardLocalChanges)) {
            success = false;
            continue;
        }
        data.lastModified = current;
    }
    return success;
}

bool LayeredConfigStore::isLoaded(ConfigLayer layer) const {
    const auto* data = impl_->layer(layer);
    return data && data->loaded;
}

QString LayeredConfigStore::layerPath(ConfigLayer layer) const {
    const auto* data = impl_->layer(layer);
    return data ? data->path : QString();
}

}
