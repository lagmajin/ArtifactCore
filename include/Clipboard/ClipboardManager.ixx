module;
#include <algorithm>
#include <utility>
#include <memory>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QClipboard>
#include <QApplication>
#include <QHash>
#include <QMimeData>

export module Clipboard.ClipboardManager;

export namespace ArtifactCore {

// ============================================================
// Clipboard Data Types
// ============================================================

enum class ClipboardType {
    None,
    Layer,          // レイヤー全体
    Effect,         // エフェクト設定
    Keyframes,      // キーフレーム範囲
    PropertyValue,  // プロパティ値
    ProjectItems,   // Project View の item snapshot
    ProjectBundle,  // Inter-process project bundle
};

// ============================================================
// Clipboard Entry
// ============================================================

struct ClipboardEntry {
    ClipboardType type = ClipboardType::None;
    QString mimeType;               // "application/x-artifact-layer" 等
    QJsonObject data;               // JSON データ
    QString description;            // "3 layers", "Gaussian Blur" 等
    QString sourceLayerId;          // コピー元レイヤー ID
    QString sourcePropertyPath;     // コピー元プロパティパス
};

// ============================================================
// Clipboard Manager
// ============================================================

class ClipboardManager {
public:
    static ClipboardManager& instance() {
        static ClipboardManager mgr;
        return mgr;
    }

    // --- Layer Operations ---
    void copyLayer(const QJsonObject& layerJson, const QString& layerName);
    void copyLayers(const QJsonArray& layersJson, int count);
    bool hasLayerData() const;
    QJsonArray pasteLayers() const;

    // --- Effect Operations ---
    void copyEffect(const QJsonObject& effectJson, const QString& effectName, const QString& layerId);
    bool hasEffectData() const;
    QJsonObject pasteEffect() const;
    QString pasteEffectSourceLayerId() const;

    // --- Keyframe Operations ---
    void copyKeyframes(const QString& propertyPath, const QJsonArray& keyframes, const QString& layerId);
    bool hasKeyframeData() const;
    QJsonArray pasteKeyframes() const;
    QString pasteKeyframesPropertyPath() const;

    // --- Property Value Operations ---
    void copyPropertyValue(const QString& propertyPath, const QVariant& value, const QString& layerId);
    bool hasPropertyValue() const;
    QVariant pastePropertyValue() const;
    QString pastePropertyPath() const;

    // --- Project Item Snapshot Operations ---
    void copyProjectItems(const QJsonArray& itemsJson, const QString& description = QString());
    bool hasProjectItemData() const;
    QJsonArray pasteProjectItems() const;
    void copyProjectBundle(const QJsonObject& bundleJson, const QString& description = QString());
    bool hasProjectBundleData() const;
    QJsonObject pasteProjectBundle() const;

    // --- Generic ---
    ClipboardType currentType() const;
    QString description() const;
    bool isEmpty() const;

    // System clipboard integration
    void syncToSystemClipboard();
    void syncFromSystemClipboard();

private:
    ClipboardManager() = default;

    ClipboardEntry internalClip_;
    QJsonArray cachedLayers_;

    static constexpr int kClipboardBundleVersion = 1;
    static constexpr const char* kArtifactMimePrefix = "application/x-artifact-";
    static constexpr const char* kClipboardMime = "application/x-artifact-clipboard+json";
    static constexpr const char* kLayerMime = "application/x-artifact-layer";
    static constexpr const char* kEffectMime = "application/x-artifact-effect";
    static constexpr const char* kKeyframeMime = "application/x-artifact-keyframe";
    static constexpr const char* kPropertyMime = "application/x-artifact-property";
    static constexpr const char* kProjectItemsMime = "application/x-artifact-project-items";
    static constexpr const char* kProjectBundleMime = "application/x-artifact-project-bundle";

    static QString descriptionForLayerCount(const QJsonArray& layersJson, int count);
    static QJsonObject makeEnvelope(const QString& kind, const QJsonObject& payload);
    static bool parseClipboardObject(const QJsonObject& obj, ClipboardEntry& outEntry, QJsonArray& outLayers);
};

// ============================================================
// Implementation
// ============================================================

QString ClipboardManager::descriptionForLayerCount(const QJsonArray& layersJson, int count) {
    if (count == 1 && !layersJson.isEmpty()) {
        const QJsonObject first = layersJson.first().toObject();
        const QString layerName = first.value(QStringLiteral("layerName")).toString().trimmed();
        if (!layerName.isEmpty()) {
            return layerName;
        }
    }
    return QStringLiteral("%1 layer(s)").arg(std::max(0, count));
}

QJsonObject ClipboardManager::makeEnvelope(const QString& kind, const QJsonObject& payload) {
    QJsonObject clipObj = payload;
    clipObj[QStringLiteral("artifact_clipboard")] = kind;
    clipObj[QStringLiteral("artifact_clipboard_version")] = kClipboardBundleVersion;
    return clipObj;
}

bool ClipboardManager::parseClipboardObject(const QJsonObject& obj, ClipboardEntry& outEntry, QJsonArray& outLayers) {
    const QString type = obj.value(QStringLiteral("artifact_clipboard")).toString();
    if (type.isEmpty()) {
        return false;
    }

    outEntry = {};
    outEntry.data = obj;

    if (type == QStringLiteral("layer")) {
        outEntry.type = ClipboardType::Layer;
        outEntry.mimeType = kLayerMime;
        outLayers = obj.value(QStringLiteral("layers")).toArray();
        outEntry.description = descriptionForLayerCount(outLayers, outLayers.size());
        return true;
    }
    if (type == QStringLiteral("effect")) {
        outEntry.type = ClipboardType::Effect;
        outEntry.mimeType = kEffectMime;
        outEntry.description = obj.value(QStringLiteral("effect")).toObject()
                                  .value(QStringLiteral("displayName")).toString();
        return true;
    }
    if (type == QStringLiteral("keyframes")) {
        outEntry.type = ClipboardType::Keyframes;
        outEntry.mimeType = kKeyframeMime;
        outEntry.sourcePropertyPath = obj.value(QStringLiteral("propertyPath")).toString();
        const QJsonArray keyframes = obj.value(QStringLiteral("keyframes")).toArray();
        outEntry.description = QStringLiteral("%1 keyframes on %2")
                                   .arg(keyframes.size())
                                   .arg(outEntry.sourcePropertyPath);
        return true;
    }
    if (type == QStringLiteral("property")) {
        outEntry.type = ClipboardType::PropertyValue;
        outEntry.mimeType = kPropertyMime;
        outEntry.sourcePropertyPath = obj.value(QStringLiteral("propertyPath")).toString();
        outEntry.description = QStringLiteral("%1 = %2")
                                   .arg(outEntry.sourcePropertyPath,
                                        obj.value(QStringLiteral("value")).toVariant().toString());
        return true;
    }
    if (type == QStringLiteral("project-items")) {
        outEntry.type = ClipboardType::ProjectItems;
        outEntry.mimeType = kProjectItemsMime;
        const QJsonArray items = obj.value(QStringLiteral("items")).toArray();
        outEntry.description = QStringLiteral("%1 item(s)").arg(items.size());
        return true;
    }
    if (type == QStringLiteral("project-bundle")) {
        outEntry.type = ClipboardType::ProjectBundle;
        outEntry.mimeType = kProjectBundleMime;
        const QJsonArray items = obj.value(QStringLiteral("items")).toArray();
        const QString title = obj.value(QStringLiteral("bundleTitle")).toString().trimmed();
        const QString sourceProjectName = obj.value(QStringLiteral("sourceProjectName")).toString().trimmed();
        if (!title.isEmpty()) {
            outEntry.description = title;
        } else if (!sourceProjectName.isEmpty()) {
            outEntry.description = QStringLiteral("%1 bundle").arg(sourceProjectName);
        } else {
            outEntry.description = QStringLiteral("%1 item(s)").arg(items.size());
        }
        return true;
    }

    return false;
}

// --- Layer ---
void ClipboardManager::copyLayer(const QJsonObject& layerJson, const QString& layerName) {
    QJsonArray arr;
    arr.append(layerJson);
    copyLayers(arr, 1);
    if (!layerName.trimmed().isEmpty()) {
        internalClip_.description = layerName.trimmed();
    }
}

void ClipboardManager::copyLayers(const QJsonArray& layersJson, int count) {
    QJsonObject clipObj;
    clipObj = makeEnvelope(QStringLiteral("layer"), clipObj);
    clipObj[QStringLiteral("layers")] = layersJson;

    internalClip_.type = ClipboardType::Layer;
    internalClip_.mimeType = kLayerMime;
    internalClip_.data = clipObj;
    internalClip_.description = descriptionForLayerCount(layersJson, count);
    cachedLayers_ = layersJson;

    syncToSystemClipboard();
}

bool ClipboardManager::hasLayerData() const {
    return internalClip_.type == ClipboardType::Layer;
}

QJsonArray ClipboardManager::pasteLayers() const {
    if (internalClip_.type != ClipboardType::Layer) return {};
    return cachedLayers_;
}

// --- Effect ---
void ClipboardManager::copyEffect(const QJsonObject& effectJson, const QString& effectName, const QString& layerId) {
    QJsonObject clipObj;
    clipObj = makeEnvelope(QStringLiteral("effect"), clipObj);
    clipObj[QStringLiteral("effect")] = effectJson;

    internalClip_.type = ClipboardType::Effect;
    internalClip_.mimeType = kEffectMime;
    internalClip_.data = clipObj;
    internalClip_.description = effectName;
    internalClip_.sourceLayerId = layerId;

    syncToSystemClipboard();
}

bool ClipboardManager::hasEffectData() const {
    return internalClip_.type == ClipboardType::Effect;
}

QJsonObject ClipboardManager::pasteEffect() const {
    if (internalClip_.type != ClipboardType::Effect) return {};
    return internalClip_.data[QStringLiteral("effect")].toObject();
}

QString ClipboardManager::pasteEffectSourceLayerId() const {
    return internalClip_.sourceLayerId;
}

// --- Keyframes ---
void ClipboardManager::copyKeyframes(const QString& propertyPath, const QJsonArray& keyframes, const QString& layerId) {
    QJsonObject clipObj;
    clipObj = makeEnvelope(QStringLiteral("keyframes"), clipObj);
    clipObj[QStringLiteral("propertyPath")] = propertyPath;
    clipObj[QStringLiteral("keyframes")] = keyframes;

    internalClip_.type = ClipboardType::Keyframes;
    internalClip_.mimeType = kKeyframeMime;
    internalClip_.data = clipObj;
    internalClip_.description = QStringLiteral("%1 keyframes on %2").arg(keyframes.size()).arg(propertyPath);
    internalClip_.sourceLayerId = layerId;
    internalClip_.sourcePropertyPath = propertyPath;

    syncToSystemClipboard();
}

bool ClipboardManager::hasKeyframeData() const {
    return internalClip_.type == ClipboardType::Keyframes;
}

QJsonArray ClipboardManager::pasteKeyframes() const {
    if (internalClip_.type != ClipboardType::Keyframes) return {};
    return internalClip_.data[QStringLiteral("keyframes")].toArray();
}

QString ClipboardManager::pasteKeyframesPropertyPath() const {
    return internalClip_.sourcePropertyPath;
}

// --- Property Value ---
void ClipboardManager::copyPropertyValue(const QString& propertyPath, const QVariant& value, const QString& layerId) {
    QJsonObject clipObj;
    clipObj = makeEnvelope(QStringLiteral("property"), clipObj);
    clipObj[QStringLiteral("propertyPath")] = propertyPath;
    clipObj[QStringLiteral("value")] = QJsonValue::fromVariant(value);

    internalClip_.type = ClipboardType::PropertyValue;
    internalClip_.mimeType = kPropertyMime;
    internalClip_.data = clipObj;
    internalClip_.description = QStringLiteral("%1 = %2").arg(propertyPath, value.toString());
    internalClip_.sourceLayerId = layerId;
    internalClip_.sourcePropertyPath = propertyPath;

    syncToSystemClipboard();
}

bool ClipboardManager::hasPropertyValue() const {
    return internalClip_.type == ClipboardType::PropertyValue;
}

QVariant ClipboardManager::pastePropertyValue() const {
    if (internalClip_.type != ClipboardType::PropertyValue) return {};
    return internalClip_.data[QStringLiteral("value")].toVariant();
}

QString ClipboardManager::pastePropertyPath() const {
    return internalClip_.sourcePropertyPath;
}

// --- Project Items ---
void ClipboardManager::copyProjectItems(const QJsonArray& itemsJson, const QString& description) {
    QJsonObject payload;
    payload[QStringLiteral("items")] = itemsJson;
    payload[QStringLiteral("bundleKind")] = QStringLiteral("project-items");
    payload[QStringLiteral("bundleTitle")] = description.trimmed().isEmpty()
                                                 ? QStringLiteral("%1 item(s)").arg(itemsJson.size())
                                                 : description.trimmed();
    copyProjectBundle(payload, payload.value(QStringLiteral("bundleTitle")).toString());
}

bool ClipboardManager::hasProjectItemData() const {
    return internalClip_.type == ClipboardType::ProjectItems ||
           internalClip_.type == ClipboardType::ProjectBundle;
}

QJsonArray ClipboardManager::pasteProjectItems() const {
    if (internalClip_.type != ClipboardType::ProjectItems &&
        internalClip_.type != ClipboardType::ProjectBundle) return {};
    return internalClip_.data[QStringLiteral("items")].toArray();
}

void ClipboardManager::copyProjectBundle(const QJsonObject& bundleJson, const QString& description) {
    QJsonObject clipObj;
    clipObj = makeEnvelope(QStringLiteral("project-bundle"), bundleJson);

    internalClip_.type = ClipboardType::ProjectBundle;
    internalClip_.mimeType = kProjectBundleMime;
    internalClip_.data = clipObj;

    const QString trimmedDescription = description.trimmed();
    if (!trimmedDescription.isEmpty()) {
        internalClip_.description = trimmedDescription;
    } else {
        const QString title = bundleJson.value(QStringLiteral("bundleTitle")).toString().trimmed();
        if (!title.isEmpty()) {
            internalClip_.description = title;
        } else {
            const QJsonArray items = bundleJson.value(QStringLiteral("items")).toArray();
            internalClip_.description = QStringLiteral("%1 item(s)").arg(items.size());
        }
    }

    syncToSystemClipboard();
}

bool ClipboardManager::hasProjectBundleData() const {
    return internalClip_.type == ClipboardType::ProjectBundle;
}

QJsonObject ClipboardManager::pasteProjectBundle() const {
    if (internalClip_.type != ClipboardType::ProjectBundle &&
        internalClip_.type != ClipboardType::ProjectItems) return {};
    return internalClip_.data;
}

// --- Generic ---
ClipboardType ClipboardManager::currentType() const {
    return internalClip_.type;
}

QString ClipboardManager::description() const {
    return internalClip_.description;
}

bool ClipboardManager::isEmpty() const {
    return internalClip_.type == ClipboardType::None;
}

void ClipboardManager::syncToSystemClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard && !internalClip_.data.isEmpty()) {
        const QByteArray jsonBytes = QJsonDocument(internalClip_.data).toJson(QJsonDocument::Compact);
        auto* mimeData = new QMimeData();
        mimeData->setText(QString::fromUtf8(jsonBytes));
        mimeData->setData(kClipboardMime, jsonBytes);
        if (!internalClip_.mimeType.isEmpty()) {
            mimeData->setData(internalClip_.mimeType.toUtf8(), jsonBytes);
        }
        clipboard->setMimeData(mimeData);
    }
}

void ClipboardManager::syncFromSystemClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard) {
        internalClip_ = {};
        cachedLayers_ = {};
        return;
    }

    const QMimeData* mime = clipboard->mimeData();
    QByteArray rawBytes;
    if (mime) {
        if (mime->hasFormat(kClipboardMime)) {
            rawBytes = mime->data(kClipboardMime);
        }
        if (rawBytes.isEmpty() && mime->hasFormat(kLayerMime)) {
            rawBytes = mime->data(kLayerMime);
        }
        if (rawBytes.isEmpty() && mime->hasFormat(kEffectMime)) {
            rawBytes = mime->data(kEffectMime);
        }
        if (rawBytes.isEmpty() && mime->hasFormat(kKeyframeMime)) {
            rawBytes = mime->data(kKeyframeMime);
        }
        if (rawBytes.isEmpty() && mime->hasFormat(kPropertyMime)) {
            rawBytes = mime->data(kPropertyMime);
        }
        if (rawBytes.isEmpty() && mime->hasFormat(kProjectItemsMime)) {
            rawBytes = mime->data(kProjectItemsMime);
        }
        if (rawBytes.isEmpty() && mime->hasFormat(kProjectBundleMime)) {
            rawBytes = mime->data(kProjectBundleMime);
        }
        if (rawBytes.isEmpty() && mime->hasText()) {
            rawBytes = mime->text().toUtf8();
        }
    }

    if (rawBytes.isEmpty()) {
        internalClip_ = {};
        cachedLayers_ = {};
        return;
    }

    QJsonParseError error{};
    QJsonDocument doc = QJsonDocument::fromJson(rawBytes, &error);
    if (!doc.isObject()) {
        internalClip_ = {};
        cachedLayers_ = {};
        return;
    }

    QJsonObject obj = doc.object();
    QJsonArray layers;
    if (!parseClipboardObject(obj, internalClip_, layers)) {
        internalClip_ = {};
        cachedLayers_ = {};
        return;
    }

    if (internalClip_.type == ClipboardType::Layer) {
        cachedLayers_ = layers;
    } else {
        cachedLayers_ = {};
    }
}

}
