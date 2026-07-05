module;
#include <utility>
#include <QString>
#include <QVariant>
#include <vector>
#include <cstdint>

export module Reactive.Events;

export namespace ArtifactCore {

// ============================================================
// Trigger Event Types
// ============================================================

enum class TriggerEventType {
    None = 0,

    // Lifecycle triggers (per-layer)
    OnStart,        // レイヤーの再生開始 (inPoint に到達)
    OnEnd,          // レイヤーの再生終了 (outPoint に到達)
    OnEnterRange,   // レイヤーのアクティブ範囲に入った
    OnExitRange,    // レイヤーのアクティブ範囲から出た
    OnLoop,         // ループ再生でサイクルが繰り返された

    // Spatial triggers (between layers)
    OnContact,      // BB が接触した
    OnSeparation,   // BB が離れた (接触 → 非接触)
    OnProximity,    // BB 中心間の距離が閾値以内

    // Value triggers
    OnValueExceed,  // プロパティ値が閾値を超えた
    OnValueDrop,    // プロパティ値が閾値を下回った
    OnValueCross,   // プロパティ値が閾値を横切った

    // Frame triggers
    OnFrame,        // 特定フレームに到達
};

// ============================================================
// Trigger Condition
// ============================================================

struct TriggerCondition {
    TriggerEventType type = TriggerEventType::None;

    // ソースレイヤー (Spatial/Value/Lifecycle 用)
    QString sourceLayerId;

    // ターゲットレイヤー (Spatial 用: 接触/距離の相手)
    QString targetLayerId;

    // Spatial: 距離閾値 (pixels)
    float proximityThreshold = 50.0f;

    // Value: プロパティパスと閾値
    QString propertyPath;       // "opacity", "position.x" 等
    float valueThreshold = 0.0f;

    // Frame: ターゲットフレーム
    int64_t frameNumber = 0;

    // JSON シリアライズ
    QString toJson() const;
    static TriggerCondition fromJson(const QString& json);
};

// ============================================================
// Reaction Types
// ============================================================

enum class ReactionType {
    None = 0,

    // Property reactions
    SetProperty,        // プロパティ値を即座に設定
    AnimateProperty,    // プロパティ値をアニメーション付きで変更
    RandomizeProperty,  // プロパティ値をランダムに設定

    // Transform reactions
    ApplyImpulse,       // 瞬間的な力を加える
    ApplyForce,         // 継続的な力を加える
    Attract,            // 引力
    Repel,              // 斥力

    // Playback reactions
    PlayAnimation,      // アニメーション再生開始
    PauseAnimation,     // アニメーション一時停止
    GoToFrame,          // 特定フレームにジャンプ

    // Spawn reactions
    SpawnLayer,         // レイヤーを生成
    DestroyLayer,       // レイヤーを削除

    // Audio reactions
    PlaySound,          // サウンド再生
};

// ============================================================
// Reaction Action
// ============================================================

struct Reaction {
    ReactionType type = ReactionType::None;

    // ターゲットレイヤー
    QString targetLayerId;

    // Property reactions
    QString propertyPath;       // "opacity", "fillColor" 等
    QVariant value;             // 目標値
    QVariant valueMax;          // Randomize 用の最大値

    // Animation
    float duration = 0.0f;      // アニメーション持続時間 (秒)
    QString easing;             // "easeIn", "easeOut", "easeInOut", "linear", "bounce", "elastic"

    // Force
    float strength = 1.0f;      // 力の強さ
    float directionX = 0.0f;    // 力の方向 X
    float directionY = 0.0f;    // 力の方向 Y

    // Playback
    int64_t targetFrame = 0;    // GoToFrame 用

    // Spawn
    QString spawnLayerType;     // "Solid", "Particle" 等

    // JSON シリアライズ
    QString toJson() const;
    static Reaction fromJson(const QString& json);
};

// ============================================================
// Reactive Rule
// ============================================================

struct ReactiveRule {
    QString id;                 // UUID
    QString name;               // 表示名
    bool enabled = true;

    TriggerCondition trigger;
    std::vector<Reaction> reactions;

    // 制御オプション
    bool once = false;              // 1回のみ発火
    bool repeating = false;         // リピート発火
    float delay = 0.0f;             // トリガー後の遅延 (秒)
    float cooldown = 0.0f;          // クールダウン (秒)

    // 実行状態 (永続化しない)
    bool fired = false;             // once モード用フラグ
    int64_t lastFiredFrame = -1;    // 最後に発火したフレーム
    float fireAccumulator = 0.0f;   // delay 用累積時間

    // JSON シリアライズ
    QString toJson() const;
    static ReactiveRule fromJson(const QString& json);
    static std::vector<ReactiveRule> fromJsonArray(const QString& jsonArray);
    static QString toJsonArray(const std::vector<ReactiveRule>& rules);
};

// ============================================================
// Trigger Event (評価結果)
// ============================================================

struct TriggerEvent {
    TriggerEventType type = TriggerEventType::None;
    QString ruleId;             // 発火したルールの ID
    QString sourceLayerId;      // トリガーソース
    QString targetLayerId;      // トリガーターゲット
    int64_t frame = 0;          // 発火フレーム
    float deltaTime = 0.0f;     // デルタ時間
    bool entered = true;        // OnEnterRange/OnExitRange 用
};

// ============================================================
// String Conversion
// ============================================================

const char* triggerEventTypeName(TriggerEventType type);
const char* reactionTypeName(ReactionType type);
TriggerEventType triggerEventTypeFromName(const QString& name);
ReactionType reactionTypeFromName(const QString& name);

}
