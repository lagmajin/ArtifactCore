module;

#include <QString>
#include <QVector>
#include <memory>

export module Composition.PreCompose;

import std;
import Utils.Id;

export namespace ArtifactCore {

/// 前方宣言
class Composition;
class Layer;

/// プリコンポーズオプション
struct PreComposeOptions {
    QString name;                       ///< 新規コンポジション名
    bool moveAttributes = true;         ///< 属性（トランスフォーム等）を移動
    bool adjustDuration = true;         ///< 継続時間を調整
    bool includeEffects = true;         ///< エフェクトを含める
    bool keepInPlace = false;           ///< 元の位置に残す（参照コピー）
    
    /// デフォルトオプション
    static PreComposeOptions defaults() {
        PreComposeOptions opt;
        opt.name = "Pre-comp";
        return opt;
    }
};

/// プリコンポーズ結果
struct PreComposeResult {
    CompositionID newCompositionId;     ///< 新規作成されたコンポジションID
    LayerID newLayerId;                 ///< 親コンポジションに追加されたレイヤーID
    QVector<LayerID> movedLayerIds;     ///< 移動されたレイヤーID
    bool success = false;
    QString errorMessage;
};

/// プリコンポーズ解除オプション
struct UnprecomposeOptions {
    bool keepComposition = false;       ///< コンポジション自体は残す
    bool preserveTiming = true;         ///< タイミングを保持
};

/// プリコンポーズマネージャー
/// 
/// レイヤーのプリコンポーズ（グループ化して新しいコンポジションに移動）機能を提供。
/// After Effectsの「プリコンポーズ」機能に相当。
class PreComposeManager {
public:
    /// シングルトンインスタンス
    static PreComposeManager& instance();
    
    // ========================================
    // プリコンポーズ実行
    // ========================================
    
    /// 選択レイヤーをプリコンポーズ
    PreComposeResult precompose(
        CompositionID parentCompositionId,
        const QVector<LayerID>& layerIds,
        const PreComposeOptions& options = PreComposeOptions::defaults()
    );
    
    /// 単一レイヤーをプリコンポーズ
    PreComposeResult precomposeSingle(
        CompositionID parentCompositionId,
        LayerID layerId,
        const PreComposeOptions& options = PreComposeOptions::defaults()
    );
    
    /// プリコンポーズを解除（レイヤーを親コンポジションに戻す）
    bool unprecompose(
        CompositionID compositionId,
        LayerID precompLayerId,
        const UnprecomposeOptions& options = UnprecomposeOptions{}
    );
    
    // ========================================
    // ユーティリティ
    // ========================================
    
    /// レイヤーがプリコンポーズレイヤーかどうか
    bool isPrecomposeLayer(LayerID layerId) const;
    
    /// プリコンポーズレイヤーの元コンポジションID取得
    CompositionID getSourceCompositionId(LayerID precompLayerId) const;
    
    /// コンポジションのネスト階層を取得
    QVector<CompositionID> getCompositionHierarchy(CompositionID compositionId) const;
    
    /// 親コンポジションを取得
    CompositionID getParentComposition(CompositionID compositionId) const;
    
    /// 全親コンポジションを取得（ルートまで）
    QVector<CompositionID> getAllParentCompositions(CompositionID compositionId) const;
    
    /// 循環参照チェック（コンポジションをネスト可能か）
    bool canNestComposition(CompositionID parent, CompositionID child) const;
    
    // ========================================
    // 設定
    // ========================================
    
    /// デフォルトのプリコンポーズ名プレフィックス
    void setDefaultNamePrefix(const QString& prefix);
    QString defaultNamePrefix() const;
    
    /// 自動命名を有効にする
    void setAutoNaming(bool enabled);
    bool autoNamingEnabled() const;

private:
    PreComposeManager();
    ~PreComposeManager();
    
    class Impl;
    Impl* impl_;
};

/// コンポジションネスト情報
struct CompositionNesting {
    CompositionID compositionId;         ///< このコンポジション
    CompositionID parentCompositionId;   ///< 親コンポジション（ルートなら無効）
    LayerID parentLayerId;               ///< 親コンポジション内のレイヤーID
    int nestingLevel;                    ///< ネスト階層（ルート=0）
    
    /// ルートコンポジションかどうか
    bool isRoot() const { return !parentCompositionId.isNil(); }
};

/// ネストしたコンポジションの時間変換ユーティリティ
namespace NestedTimeUtils {
    /// 親コンポジション時間から子コンポジション時間へ変換
    double parentToChildTime(
        double parentTime,
        LayerID precompLayerId
    );
    
    /// 子コンポジション時間から親コンポジション時間へ変換
    double childToParentTime(
        double childTime,
        LayerID precompLayerId
    );
    
    /// ネストした時間変換（複数階層対応）
    double convertTime(
        double sourceTime,
        CompositionID sourceComposition,
        CompositionID targetComposition
    );
    
    /// 子コンポジションのタイムリマップを考慮した時間取得
    double getRemappedTime(
        LayerID precompLayerId,
        double parentTime
    );
}

/// プリコンポーズ操作のエディタコマンド用インターフェース
class PreComposeCommand {
public:
    virtual ~PreComposeCommand() = default;
    
    /// 実行
    virtual bool execute() = 0;
    
    /// 元に戻す
    virtual bool undo() = 0;
    
    /// やり直し
    virtual bool redo() = 0;
    
    /// 説明
    virtual QString description() const = 0;
    
    /// 操作タイプ
    enum class Type {
        Precompose,
        Unprecompose
    };
    virtual Type type() const = 0;
};

/// プリコンポーズコマンド作成
std::unique_ptr<PreComposeCommand> createPrecomposeCommand(
    CompositionID parentCompositionId,
    const QVector<LayerID>& layerIds,
    const PreComposeOptions& options = PreComposeOptions::defaults()
);

/// プリコンポーズ解除コマンド作成
std::unique_ptr<PreComposeCommand> createUnprecomposeCommand(
    CompositionID compositionId,
    LayerID precompLayerId,
    const UnprecomposeOptions& options = UnprecomposeOptions{}
);

} // namespace ArtifactCore