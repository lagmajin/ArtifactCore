module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <unordered_map>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>

export module Core.Composition.FinalEffect;

import Core.Define;

export namespace ArtifactCore {

/// <summary>
/// Final Effectの種別
/// </summary>
export enum class FinalEffectType {
    ColorGrading,       // カラーグレーディング
    ColorCurves,        // カラーカーブ
    LUT,                // LUT適用
    Exposure,           // 露出補正
    Glow,               // 発光
    Blur,               // ぼかし
    Sharpen,            // シャープ
    Vignette,           // ビネット
    Grain,              // フィルムグレイン
    ToneMapping,        // トーンマッピング（ACES等）
    Custom              // カスタム
};

/// <summary>
/// Final Effectの定義
/// </summary>
export class CompositionFinalEffect {
public:
    CompositionFinalEffect();
    explicit CompositionFinalEffect(FinalEffectType type);
    virtual ~CompositionFinalEffect() = default;

    // 基本情報
    auto getType() const -> FinalEffectType { return type_; }
    auto getName() const -> QString { return name_; }
    void setName(const QString& name) { name_ = name; }

    auto isEnabled() const -> bool { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    auto getOpacity() const -> float { return opacity_; }
    void setOpacity(float opacity) { opacity_ = std::clamp(opacity, 0.0f, 1.0f); }

    // パラメータ管理
    auto getParameters() const -> const std::unordered_map<QString, float>& { return parameters_; }
    void setParameter(const QString& key, float value) { parameters_[key] = value; }
    auto getParameter(const QString& key, float defaultValue = 0.0f) const -> float;

    // シリアライズ
    virtual auto toJson() const -> QJsonObject;
    virtual void fromJson(const QJsonObject& obj);

    // ファクトリ
    static auto createFromType(FinalEffectType type) -> std::unique_ptr<CompositionFinalEffect>;
    static auto createFromJson(const QJsonObject& obj) -> std::unique_ptr<CompositionFinalEffect>;

protected:
    static auto getTypeName(FinalEffectType type) -> QString;

    FinalEffectType type_ = FinalEffectType::Custom;
    QString name_;
    bool enabled_ = true;
    float opacity_ = 1.0f;
    std::unordered_map<QString, float> parameters_;
};

/// <summary>
/// Composition Final Effect Stack
/// 順序付きエフェクトリスト
/// </summary>
export class CompositionFinalEffectStack {
public:
    CompositionFinalEffectStack() = default;
    ~CompositionFinalEffectStack() = default;

    // エフェクト追加/削除
    void addEffect(std::unique_ptr<CompositionFinalEffect> effect);
    void removeEffect(int index);
    void removeEffect(const QString& name);
    void clear();

    // 並べ替え
    void moveEffect(int fromIndex, int toIndex);

    // アクセサ
    auto getEffectCount() const -> int { return static_cast<int>(effects_.size()); }
    auto getEffect(int index) -> CompositionFinalEffect*;
    auto getEffect(int index) const -> const CompositionFinalEffect*;
    auto getEffects() const -> const std::vector<std::unique_ptr<CompositionFinalEffect>>& { return effects_; }

    // 検索
    auto findEffect(const QString& name) -> CompositionFinalEffect*;
    auto findEffect(const QString& name) const -> const CompositionFinalEffect*;

    // 有効なエフェクトのみ取得
    auto getEnabledEffects() const -> std::vector<const CompositionFinalEffect*>;

    // シリアライズ
    auto toJson() const -> QJsonObject;
    void fromJson(const QJsonObject& obj);

private:
    std::vector<std::unique_ptr<CompositionFinalEffect>> effects_;
};

} // namespace ArtifactCore
