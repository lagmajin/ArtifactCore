module;
#include <utility>
#include <algorithm>
#include <QString>
#include <QMap>
#include <QJsonObject>
#include <QVariant>
#include <mutex>
#include <functional>
#include <optional>

export module ArtifactCore.Control.External;

import Utils.Id;
import Property.Abstract;

namespace ArtifactCore {

/**
 * @brief 外部コントローラー（MIDI/OSC等）の割当先を定義する構造体
 */
export struct ControlTarget {
    LayerID layerId;
    QString propertyPath;
    
    bool operator<(const ControlTarget& other) const {
        if (layerId != other.layerId) return layerId < other.layerId;
        return propertyPath < other.propertyPath;
    }
    
    bool operator==(const ControlTarget& other) const {
        return layerId == other.layerId && propertyPath == other.propertyPath;
    }
};

export struct InputValueTransform {
    double gain = 1.0;
    double offset = 0.0;
    bool clampEnabled = false;
    double clampMinimum = 0.0;
    double clampMaximum = 1.0;
    double smoothing = 0.0;
    bool invert = false;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj.insert(QStringLiteral("gain"), gain);
        obj.insert(QStringLiteral("offset"), offset);
        obj.insert(QStringLiteral("clampEnabled"), clampEnabled);
        obj.insert(QStringLiteral("clampMinimum"), clampMinimum);
        obj.insert(QStringLiteral("clampMaximum"), clampMaximum);
        obj.insert(QStringLiteral("smoothing"), smoothing);
        obj.insert(QStringLiteral("invert"), invert);
        return obj;
    }

    static InputValueTransform fromJson(const QJsonObject& obj) {
        InputValueTransform transform;
        transform.gain = obj.value(QStringLiteral("gain")).toDouble(1.0);
        transform.offset = obj.value(QStringLiteral("offset")).toDouble(0.0);
        transform.clampEnabled = obj.value(QStringLiteral("clampEnabled")).toBool(false);
        transform.clampMinimum = obj.value(QStringLiteral("clampMinimum")).toDouble(0.0);
        transform.clampMaximum = obj.value(QStringLiteral("clampMaximum")).toDouble(1.0);
        transform.smoothing = obj.value(QStringLiteral("smoothing")).toDouble(0.0);
        transform.invert = obj.value(QStringLiteral("invert")).toBool(false);
        return transform;
    }

    double apply(double rawValue, std::optional<double> previousValue = std::nullopt) const {
        double value = rawValue;
        if (invert) {
            value = 1.0 - value;
        }
        value = value * gain + offset;
        if (clampEnabled) {
            if (clampMinimum <= clampMaximum) {
                value = std::clamp(value, clampMinimum, clampMaximum);
            } else {
                value = std::clamp(value, clampMaximum, clampMinimum);
            }
        }
        const double clampedSmoothing = std::clamp(smoothing, 0.0, 1.0);
        if (previousValue.has_value() && clampedSmoothing > 0.0) {
            value = previousValue.value() + ((value - previousValue.value()) * (1.0 - clampedSmoothing));
        }
        return value;
    }
};

export struct ExternalControlMapping {
    ControlTarget target;
    InputValueTransform transform;
    bool enabled = true;

    QJsonObject toJson() const {
        QJsonObject obj;
        obj.insert(QStringLiteral("layerId"), target.layerId.toString());
        obj.insert(QStringLiteral("propertyPath"), target.propertyPath);
        obj.insert(QStringLiteral("transform"), transform.toJson());
        obj.insert(QStringLiteral("enabled"), enabled);
        return obj;
    }

    static ExternalControlMapping fromJson(const QJsonObject& obj) {
        ExternalControlMapping mapping;
        mapping.target.layerId = LayerID(obj.value(QStringLiteral("layerId")).toString());
        mapping.target.propertyPath = obj.value(QStringLiteral("propertyPath")).toString();
        mapping.transform = InputValueTransform::fromJson(obj.value(QStringLiteral("transform")).toObject());
        mapping.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
        return mapping;
    }
};

/**
 * @brief 外部コントローラーのマッピングを管理するシングルトンクラス
 * UIを持たず、アドレス（"midi:ch1:cc7"等）とプロパティの紐付けのみを管理する
 */
export class ExternalControlManager {
public:
    static ExternalControlManager& instance() {
        static ExternalControlManager inst;
        return inst;
    }

    /**
     * @brief 特定のアドレスにレイヤープロパティをマッピングする
     * @param address コントローラーのアドレス（例: "midi:1:1", "osc:/filter/cutoff"）
     * @param layerId ターゲットのレイヤーID
     * @param propertyPath プロパティパス（例: "transform.opacity"）
     */
    void setMapping(const QString& address, LayerID layerId, const QString& propertyPath) {
        std::lock_guard<std::mutex> lock(mutex_);
        mappings_[address] = { {layerId, propertyPath}, {}, true };
    }

    void setMapping(const QString& address, LayerID layerId, const QString& propertyPath, const InputValueTransform& transform) {
        std::lock_guard<std::mutex> lock(mutex_);
        mappings_[address] = { {layerId, propertyPath}, transform, true };
    }

    void setMappingDefinition(const QString& address, const ExternalControlMapping& mapping) {
        std::lock_guard<std::mutex> lock(mutex_);
        mappings_[address] = mapping;
    }

    /**
     * @brief マッピングを解除する
     */
    void removeMapping(const QString& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        mappings_.remove(address);
        smoothedValues_.remove(address);
    }

    /**
     * @brief 全てのマッピングをクリアする
     */
    void clearMappings() {
        std::lock_guard<std::mutex> lock(mutex_);
        mappings_.clear();
        smoothedValues_.clear();
    }

    /**
     * @brief 指定したアドレスに対応するターゲット情報を取得する
     */
    std::optional<ControlTarget> getTargetForAddress(const QString& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mappings_.find(address);
        if (it != mappings_.end()) {
            return it.value().target;
        }
        return std::nullopt;
    }

    std::optional<ExternalControlMapping> getMappingDefinition(const QString& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mappings_.find(address);
        if (it != mappings_.end()) {
            return it.value();
        }
        return std::nullopt;
    }

    std::optional<InputValueTransform> getTransformForAddress(const QString& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mappings_.find(address);
        if (it != mappings_.end()) {
            return it.value().transform;
        }
        return std::nullopt;
    }

    void observeInput(const QString& address, double rawValue) {
        if (address.trimmed().isEmpty()) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        lastObservedAddress_ = address.trimmed();
        lastObservedRawValue_ = rawValue;
        hasObservedInput_ = true;
    }

    QString lastObservedAddress() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastObservedAddress_;
    }

    std::optional<double> lastObservedRawValue() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!hasObservedInput_) {
            return std::nullopt;
        }
        return lastObservedRawValue_;
    }

    std::optional<double> processIncomingValue(const QString& address, double rawValue, bool resetSmoothing = false) {
        observeInput(address, rawValue);
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mappings_.find(address);
        if (it == mappings_.end() || !it.value().enabled) {
            return std::nullopt;
        }
        std::optional<double> previousValue;
        if (!resetSmoothing) {
            auto smoothedIt = smoothedValues_.find(address);
            if (smoothedIt != smoothedValues_.end()) {
                previousValue = smoothedIt.value();
            }
        }
        const double processed = it.value().transform.apply(rawValue, previousValue);
        smoothedValues_[address] = processed;
        return processed;
    }

    void resetProcessedValue(const QString& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        smoothedValues_.remove(address);
    }

    /**
     * @brief 現在登録されている全てのアドレスを取得する
     */
    QStringList getAllMappedAddresses() const {
        // std::lock_guard<std::mutex> lock(mutex_); // mutableにしていないため一旦ロックなし（実際は必要）
        return mappings_.keys();
    }

private:
    ExternalControlManager() = default;
    ~ExternalControlManager() = default;
    
    // シングルトンのためコピー・ムーブ禁止
    ExternalControlManager(const ExternalControlManager&) = delete;
    ExternalControlManager& operator=(const ExternalControlManager&) = delete;

    mutable std::mutex mutex_;
    QMap<QString, ExternalControlMapping> mappings_;
    QMap<QString, double> smoothedValues_;
    QString lastObservedAddress_;
    double lastObservedRawValue_ = 0.0;
    bool hasObservedInput_ = false;
};

} // namespace ArtifactCore
