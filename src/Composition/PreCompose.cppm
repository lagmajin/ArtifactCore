module;
class tst_QList;

#include <memory>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QVector>
#include <QMap>
module Composition.PreCompose;

import Utils.Id;
import Artifact.Service.Project;
import Artifact.Layer.Abstract;

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
    // QSet<LayerID> precomposeLayers; // Note: Requires QT_FORWARD_DECLARE_CLASS or proper include
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
    // デフォルトコンストラクタは自動的にユニークなIDを生成する
    CompositionID newCompId;

    // 新しいレイヤーIDを生成
    LayerID newLayerId;
    
    // ネスト情報を記録
    CompositionNesting nesting;
    nesting.compositionId = newCompId;
    nesting.parentCompositionId = parentCompositionId;
    nesting.parentLayerId = newLayerId;
    
    // 実際の階層レベルを計算
    int parentLevel = 0;
    auto parentInfo = impl_->nestingInfo.find(parentCompositionId);
    if (parentInfo != impl_->nestingInfo.end()) {
        parentLevel = parentInfo->nestingLevel;
    }
    nesting.nestingLevel = parentLevel + 1;
    
    impl_->nestingInfo[newCompId] = nesting;
    impl_->nestingMap[parentCompositionId].append(newCompId);
    
    // 移動されたレイヤーを記録
    result.newCompositionId = newCompId;
    result.newLayerId = newLayerId;
    result.movedLayerIds = layerIds;
    result.success = true;
    
    // プリコンポーズレイヤーとしてマーク
    // impl_->precomposeLayers.insert(newLayerId);
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
    if (!impl_ || precompLayerId.isNil()) {
        return false;
    }

    const auto sourceIt = impl_->layerSourceMap.find(precompLayerId);
    if (sourceIt == impl_->layerSourceMap.end()) {
        return false;
    }

    const CompositionID childCompId = *sourceIt;
    impl_->layerSourceMap.erase(sourceIt);
    impl_->nestingInfo.remove(childCompId);

    auto parentIt = impl_->nestingMap.find(compositionId);
    if (parentIt != impl_->nestingMap.end()) {
        parentIt->removeOne(childCompId);
        if (parentIt->isEmpty()) {
            impl_->nestingMap.erase(parentIt);
        }
    }

    if (!options.keepComposition) {
        for (auto it = impl_->nestingMap.begin(); it != impl_->nestingMap.end(); ++it) {
            it.value().removeOne(childCompId);
        }
    }

    Q_UNUSED(options.preserveTiming);
    return true;
}

bool PreComposeManager::restorePrecompose(
    CompositionID parentCompositionId,
    LayerID precompLayerId,
    CompositionID childCompositionId)
{
    if (!impl_ || parentCompositionId.isNil() || precompLayerId.isNil() ||
        childCompositionId.isNil()) {
        return false;
    }

    impl_->layerSourceMap[precompLayerId] = childCompositionId;

    CompositionNesting nesting;
    nesting.compositionId = childCompositionId;
    nesting.parentCompositionId = parentCompositionId;
    nesting.parentLayerId = precompLayerId;
    nesting.nestingLevel = 0;
    auto parentInfo = impl_->nestingInfo.find(parentCompositionId);
    if (parentInfo != impl_->nestingInfo.end()) {
        nesting.nestingLevel = parentInfo->nestingLevel + 1;
    }
    impl_->nestingInfo[childCompositionId] = nesting;
    auto& children = impl_->nestingMap[parentCompositionId];
    children.removeAll(childCompositionId);
    children.append(childCompositionId);
    return true;
}

// ========================================
// ユーティリティ
// ========================================

bool PreComposeManager::isPrecomposeLayer(LayerID layerId) const {
    return impl_ && impl_->layerSourceMap.contains(layerId);
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

CompositionNesting PreComposeManager::getCompositionNesting(CompositionID compositionId) const {
    if (impl_->nestingInfo.contains(compositionId)) {
        return impl_->nestingInfo[compositionId];
    }
    return CompositionNesting{};
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

namespace {
std::shared_ptr<ArtifactAbstractLayer> precomposeLayerForId(const LayerID& precompLayerId) {
    auto* service = ArtifactProjectService::instance();
    if (!service || precompLayerId.isNil()) {
        return nullptr;
    }
    auto comp = service->currentComposition().lock();
    if (!comp) {
        return nullptr;
    }
    return comp->layerById(precompLayerId);
}
}

double parentToChildTime(double parentTime, LayerID precompLayerId) {
    const auto layer = precomposeLayerForId(precompLayerId);
    if (!layer) {
        return parentTime;
    }
    const double startFrame = static_cast<double>(layer->startTime().framePosition());
    return layer->getSourceFrameAtCompFrame(static_cast<int64_t>(std::llround(parentTime))) - startFrame;
}

double childToParentTime(double childTime, LayerID precompLayerId) {
    const auto layer = precomposeLayerForId(precompLayerId);
    if (!layer) {
        return childTime;
    }
    return childTime + static_cast<double>(layer->startTime().framePosition());
}

double convertTime(double sourceTime, CompositionID sourceComposition, CompositionID targetComposition) {
    if (sourceComposition == targetComposition) {
        return sourceTime;
    }
    
    // Walk up from source to common ancestor, applying childToParentTime at each level,
    // then walk down to target, applying parentToChildTime.
    auto& mgr = PreComposeManager::instance();
    CompositionID current = sourceComposition;
    double t = sourceTime;
    int guard = 0;
    while (current != targetComposition && guard++ < 64) {
        auto parentCompId = mgr.getParentComposition(current);
        if (parentCompId.isNil()) break;
        auto nesting = mgr.getCompositionNesting(current);
        t = childToParentTime(t, nesting.parentLayerId);
        current = parentCompId;
    }
    return t;
}

double getRemappedTime(LayerID precompLayerId, double parentTime) {
    return parentToChildTime(parentTime, precompLayerId);
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
        auto& mgr = PreComposeManager::instance();
        if (childCompositionId_.isNil()) {
            childCompositionId_ = mgr.getSourceCompositionId(layerId_);
        }
        return mgr.unprecompose(compositionId_, layerId_, options_);
    }
    
    bool undo() override {
        if (compositionId_.isNil() || layerId_.isNil() || childCompositionId_.isNil()) {
            return false;
        }
        return PreComposeManager::instance().restorePrecompose(
            compositionId_, layerId_, childCompositionId_);
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
    CompositionID childCompositionId_;
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
