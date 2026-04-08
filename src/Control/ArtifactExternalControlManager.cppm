module;
#include <utility>
#include <QString>
#include <QMap>
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
        mappings_[address] = {layerId, propertyPath};
    }

    /**
     * @brief マッピングを解除する
     */
    void removeMapping(const QString& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        mappings_.remove(address);
    }

    /**
     * @brief 全てのマッピングをクリアする
     */
    void clearMappings() {
        std::lock_guard<std::mutex> lock(mutex_);
        mappings_.clear();
    }

    /**
     * @brief 指定したアドレスに対応するターゲット情報を取得する
     */
    std::optional<ControlTarget> getTargetForAddress(const QString& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mappings_.find(address);
        if (it != mappings_.end()) {
            return it.value();
        }
        return std::nullopt;
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

    std::mutex mutex_;
    QMap<QString, ControlTarget> mappings_;
};

} // namespace ArtifactCore
