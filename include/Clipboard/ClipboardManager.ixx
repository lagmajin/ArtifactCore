module;
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

    static constexpr const char* kArtifactMimePrefix = "application/x-artifact-";
    static constexpr const char* kLayerMime = "application/x-artifact-layer";
    static constexpr const char* kEffectMime = "application/x-artifact-effect";
    static constexpr const char* kKeyframeMime = "application/x-artifact-keyframe";
    static constexpr const char* kPropertyMime = "application/x-artifact-property";
};

// ============================================================
// Implementation
// ============================================================

// --- Layer ---
void ClipboardManager::copyLayer(const QJsonObject& layerJson, const QString& layerName) {
    QJsonArray arr;
    arr.append(layerJson);
    copyLayers(arr, 1);
}

void ClipboardManager::copyLayers(const QJsonArray& layersJson, int count) {
    QJsonObject clipObj;
    clipObj["artifact_clipboard"] = QStringLiteral("layer");
    clipObj["layers"] = layersJson;

    internalClip_.type = ClipboardType::Layer;
    internalClip_.mimeType = kLayerMime;
    internalClip_.data = clipObj;
    internalClip_.description = QString("%1 layer(s)").arg(count);
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
    clipObj["artifact_clipboard"] = QStringLiteral("effect");
    clipObj["effect"] = effectJson;

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
    return internalClip_.data["effect"].toObject();
}

QString ClipboardManager::pasteEffectSourceLayerId() const {
    return internalClip_.sourceLayerId;
}

// --- Keyframes ---
void ClipboardManager::copyKeyframes(const QString& propertyPath, const QJsonArray& keyframes, const QString& layerId) {
    QJsonObject clipObj;
    clipObj["artifact_clipboard"] = QStringLiteral("keyframes");
    clipObj["propertyPath"] = propertyPath;
    clipObj["keyframes"] = keyframes;

    internalClip_.type = ClipboardType::Keyframes;
    internalClip_.mimeType = kKeyframeMime;
    internalClip_.data = clipObj;
    internalClip_.description = QString("%1 keyframes on %2").arg(keyframes.size()).arg(propertyPath);
    internalClip_.sourceLayerId = layerId;
    internalClip_.sourcePropertyPath = propertyPath;

    syncToSystemClipboard();
}

bool ClipboardManager::hasKeyframeData() const {
    return internalClip_.type == ClipboardType::Keyframes;
}

QJsonArray ClipboardManager::pasteKeyframes() const {
    if (internalClip_.type != ClipboardType::Keyframes) return {};
    return internalClip_.data["keyframes"].toArray();
}

QString ClipboardManager::pasteKeyframesPropertyPath() const {
    return internalClip_.sourcePropertyPath;
}

// --- Property Value ---
void ClipboardManager::copyPropertyValue(const QString& propertyPath, const QVariant& value, const QString& layerId) {
    QJsonObject clipObj;
    clipObj["artifact_clipboard"] = QStringLiteral("property");
    clipObj["propertyPath"] = propertyPath;
    clipObj["value"] = QJsonValue::fromVariant(value);

    internalClip_.type = ClipboardType::PropertyValue;
    internalClip_.mimeType = kPropertyMime;
    internalClip_.data = clipObj;
    internalClip_.description = QString("%1 = %2").arg(propertyPath, value.toString());
    internalClip_.sourceLayerId = layerId;
    internalClip_.sourcePropertyPath = propertyPath;

    syncToSystemClipboard();
}

bool ClipboardManager::hasPropertyValue() const {
    return internalClip_.type == ClipboardType::PropertyValue;
}

QVariant ClipboardManager::pastePropertyValue() const {
    if (internalClip_.type != ClipboardType::PropertyValue) return {};
    return internalClip_.data["value"].toVariant();
}

QString ClipboardManager::pastePropertyPath() const {
    return internalClip_.sourcePropertyPath;
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
        clipboard->setText(QJsonDocument(internalClip_.data).toJson(QJsonDocument::Compact));
    }
}

void ClipboardManager::syncFromSystemClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard) return;

    const QString text = clipboard->text();
    if (text.isEmpty()) return;

    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    const QString type = obj["artifact_clipboard"].toString();

    if (type == "layer") {
        internalClip_.type = ClipboardType::Layer;
        internalClip_.data = obj;
        cachedLayers_ = obj["layers"].toArray();
        internalClip_.description = QString("%1 layer(s)").arg(cachedLayers_.size());
    } else if (type == "effect") {
        internalClip_.type = ClipboardType::Effect;
        internalClip_.data = obj;
        internalClip_.description = obj["effect"].toObject()["displayName"].toString();
    } else if (type == "keyframes") {
        internalClip_.type = ClipboardType::Keyframes;
        internalClip_.data = obj;
        internalClip_.sourcePropertyPath = obj["propertyPath"].toString();
        auto kf = obj["keyframes"].toArray();
        internalClip_.description = QString("%1 keyframes on %2").arg(kf.size()).arg(internalClip_.sourcePropertyPath);
    } else if (type == "property") {
        internalClip_.type = ClipboardType::PropertyValue;
        internalClip_.data = obj;
        internalClip_.sourcePropertyPath = obj["propertyPath"].toString();
        internalClip_.description = QString("%1 = %2").arg(internalClip_.sourcePropertyPath, obj["value"].toVariant().toString());
    }
}

}
