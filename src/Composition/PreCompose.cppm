module;

#include <QString>
#include <QVector>
#include <QMap>
#include <memory>

module Composition.PreCompose;

import std;
import Utils.Id;

namespace ArtifactCore {

// ============================================================================
// PreComposeManager::Impl
// ============================================================================

class PreComposeManager::Impl {
public:
    QString defaultNamePrefix = "Pre-comp";
    bool autoNaming = true;
    
    // ネスト構造管理
    // 親コンポジション -> 子コンポジションのマッピング
    QMap<CompositionID, QVector<CompositionID>> nestingMap;
    
    // レイヤー -> 元コンポジションのマッピング
    QMap<LayerID, CompositionID> layerSourceMap;
    
    // コンポジション -> ネスト情報
    QMap<CompositionID, CompositionNesting> nestingInfo;
    
    // プリコンポーズレイヤー判定用
    QSet<LayerID> precomposeLayers;
};

// ============================================================================
// PreComposeManager 実装
// ============================================================================

PreComposeManager::PreComposeManager() : impl_(new Impl()) {}

PreComposeManager::~PreComposeManager() {
    delete impl_;
}

PreComposeManager& PreComposeManager::instance() {
    static PreComposeManager manager;
    return manager;
}

// ========================================
// プリコンポーズ実行
// ========================================

PreComposeResult PreComposeManager::precompose(
    CompositionID parentCompositionId,
    const QVector<LayerID>& layerIds,
    const PreComposeOptions& options)
{
    PreComposeResult result;
    
    if (layerIds.isEmpty()) {
        result.errorMessage = "No layers selected";
        return result;
    }
    
    // 新しいコンポジションIDを生成
    CompositionID newCompId;
    newCompId.generate();  // 仮のID生成
    
    // 新しいレイヤーIDを生成
    LayerID newLayerId;
    newLayerId.generate();
    
    // ネスト情報を記録
    CompositionNesting nesting;
    nesting.compositionId = newCompId;
    nesting.parentCompositionId = parentCompositionId;
    nesting.parentLayerId = newLayerId;
    nesting.nestingLevel = 0;  // TODO: 実際の階層を計算
    
    impl_->nestingInfo[newCompId] = nesting;
    impl_->nestingMap[parentCompositionId].append(newCompId);
    
    // 移動されたレイヤーを記録
    result.newCompositionId = newCompId;
    result.newLayerId = newLayerId;
    result.movedLayerIds = layerIds;
    result.success = true;
    
    // プリコンポーズレイヤーとしてマーク
    impl_->precomposeLayers.insert(newLayerId);
    impl_->layerSourceMap[newLayerId] = newCompId;
    
    return result;
}

PreComposeResult PreComposeManager::precomposeSingle(
    CompositionID parentCompositionId,
    LayerID layerId,
    const PreComposeOptions& options)
{
    return precompose(parentCompositionId, {layerId}, options);
}

bool PreComposeManager::unprecompose(
    CompositionID compositionId,
    LayerID precompLayerId,
    const UnprecomposeOptions& options)
{
    // プリコンポーズレイヤーかチェック
    if (!impl_->precomposeLayers.contains(precompLayerId)) {
        return false;
    }
    
    // ネスト情報を削除
    CompositionID childCompId = impl_->layerSourceMap.value(precompLayerId);
    impl_->nestingInfo.remove(childCompId);
    impl_->layerSourceMap.remove(precompLayerId);
    impl_->precomposeLayers.remove(precompLayerId);
    
    if (!options.keepComposition) {
        // コンポジション自体も削除する場合は、
        // 親のネストマップからも削除
        for (auto it = impl_->nestingMap.begin(); it != impl_->nestingMap.end(); ++it) {
            it.value().removeOne(childCompId);
        }
    }
    
    return true;
}

// ========================================
// ユーティリティ
// ========================================

bool PreComposeManager::isPrecomposeLayer(LayerID layerId) const {
    return impl_->precomposeLayers.contains(layerId);
}

CompositionID PreComposeManager::getSourceCompositionId(LayerID precompLayerId) const {
    return impl_->layerSourceMap.value(precompLayerId);
}

QVector<CompositionID> PreComposeManager::getCompositionHierarchy(CompositionID compositionId) const {
    QVector<CompositionID> hierarchy;
    
    // 自分自身を追加
    hierarchy.append(compositionId);
    
    // 親を再帰的に取得
    CompositionID current = compositionId;
    while (impl_->nestingInfo.contains(current)) {
        const auto& nesting = impl_->nestingInfo[current];
        if (nesting.parentCompositionId.isNil()) break;
        hierarchy.prepend(nesting.parentCompositionId);
        current = nesting.parentCompositionId;
    }
    
    return hierarchy;
}

CompositionID PreComposeManager::getParentComposition(CompositionID compositionId) const {
    if (impl_->nestingInfo.contains(compositionId)) {
        return impl_->nestingInfo[compositionId].parentCompositionId;
    }
    return CompositionID();
}

QVector<CompositionID> PreComposeManager::getAllParentCompositions(CompositionID compositionId) const {
    QVector<CompositionID> parents;
    
    CompositionID current = compositionId;
    while (impl_->nestingInfo.contains(current)) {
        const auto& nesting = impl_->nestingInfo[current];
        if (nesting.parentCompositionId.isNil()) break;
        parents.append(nesting.parentCompositionId);
        current = nesting.parentCompositionId;
    }
    
    return parents;
}

bool PreComposeManager::canNestComposition(CompositionID parent, CompositionID child) const {
    // 循環参照チェック
    // childの親としてparentが既に存在する場合は不可
    auto allParents = getAllParentCompositions(child);
    return !allParents.contains(parent) && parent != child;
}

// ========================================
// 設定
// ========================================

void PreComposeManager::setDefaultNamePrefix(const QString& prefix) {
    impl_->defaultNamePrefix = prefix;
}

QString PreComposeManager::defaultNamePrefix() const {
    return impl_->defaultNamePrefix;
}

void PreComposeManager::setAutoNaming(bool enabled) {
    impl_->autoNaming = enabled;
}

bool PreComposeManager::autoNamingEnabled() const {
    return impl_->autoNaming;
}

// ============================================================================
// NestedTimeUtils 実装
// ============================================================================

namespace NestedTimeUtils {

double parentToChildTime(double parentTime, LayerID precompLayerId) {
    // TODO: 実際のレイヤーからイン点・アウト点を取得して変換
    // 現在は単純なオフセットのみ考慮
    Q_UNUSED(precompLayerId);
    return parentTime;
}

double childToParentTime(double childTime, LayerID precompLayerId) {
    // TODO: 実際のレイヤーからイン点・アウト点を取得して変換
    Q_UNUSED(precompLayerId);
    return childTime;
}

double convertTime(double sourceTime, CompositionID sourceComposition, CompositionID targetComposition) {
    if (sourceComposition == targetComposition) {
        return sourceTime;
    }
    
    // ソースからターゲットへの階層パスを計算
    // TODO: 実装
    Q_UNUSED(sourceComposition);
    Q_UNUSED(targetComposition);
    return sourceTime;
}

double getRemappedTime(LayerID precompLayerId, double parentTime) {
    // タイムリマップエフェクトが適用されている場合の変換
    // TODO: 実装
    Q_UNUSED(precompLayerId);
    return parentTime;
}

} // namespace NestedTimeUtils

// ============================================================================
// PreComposeCommand 実装
// ============================================================================

namespace {

class PrecomposeCommandImpl : public PreComposeCommand {
public:
    PrecomposeCommandImpl(
        CompositionID parentComp,
        const QVector<LayerID>& layers,
        const PreComposeOptions& opts)
        : parentCompositionId_(parentComp)
        , layerIds_(layers)
        , options_(opts)
    {}
    
    bool execute() override {
        result_ = PreComposeManager::instance().precompose(parentCompositionId_, layerIds_, options_);
        return result_.success;
    }
    
    bool undo() override {
        if (!result_.success) return false;
        return PreComposeManager::instance().unprecompose(
            result_.newCompositionId,
            result_.newLayerId,
            UnprecomposeOptions{true, true}  // コンポジションは残す
        );
    }
    
    bool redo() override {
        return execute();
    }
    
    QString description() const override {
        return QString("Precompose %1 layers").arg(layerIds_.size());
    }
    
    Type type() const override { return Type::Precompose; }
    
private:
    CompositionID parentCompositionId_;
    QVector<LayerID> layerIds_;
    PreComposeOptions options_;
    PreComposeResult result_;
};

class UnprecomposeCommandImpl : public PreComposeCommand {
public:
    UnprecomposeCommandImpl(
        CompositionID compId,
        LayerID layerId,
        const UnprecomposeOptions& opts)
        : compositionId_(compId)
        , layerId_(layerId)
        , options_(opts)
    {}
    
    bool execute() override {
        return PreComposeManager::instance().unprecompose(compositionId_, layerId_, options_);
    }
    
    bool undo() override {
        // 元に戻すには再度プリコンポーズが必要
        // 簡易実装：常に成功とする
        return true;
    }
    
    bool redo() override {
        return execute();
    }
    
    QString description() const override {
        return QString("Unprecompose layer");
    }
    
    Type type() const override { return Type::Unprecompose; }
    
private:
    CompositionID compositionId_;
    LayerID layerId_;
    UnprecomposeOptions options_;
};

} // anonymous namespace

std::unique_ptr<PreComposeCommand> createPrecomposeCommand(
    CompositionID parentCompositionId,
    const QVector<LayerID>& layerIds,
    const PreComposeOptions& options)
{
    return std::make_unique<PrecomposeCommandImpl>(parentCompositionId, layerIds, options);
}

std::unique_ptr<PreComposeCommand> createUnprecomposeCommand(
    CompositionID compositionId,
    LayerID precompLayerId,
    const UnprecomposeOptions& options)
{
    return std::make_unique<UnprecomposeCommandImpl>(compositionId, precompLayerId, options);
}

} // namespace ArtifactCore