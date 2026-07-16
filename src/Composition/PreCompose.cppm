module;

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

import Frame.Position;
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
    QMap<LayerID, double> layerStartFrameMap;
    
    // コンポジション -> ネスト情報
    QMap<CompositionID, CompositionNesting> nestingInfo;

    // 子コンポジション -> プリコンポーズレイヤーID（逆索引）
    QMultiMap<CompositionID, LayerID> childSourceMap;

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
    impl_->layerStartFrameMap[newLayerId] = 0.0;
    
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
    impl_->layerStartFrameMap.remove(precompLayerId);
    impl_->childSourceMap.remove(childCompId);
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

void PreComposeManager::setPrecomposeLayerStartFrame(
    LayerID precompLayerId, double startFrame) {
    if (!impl_ || precompLayerId.isNil() || !std::isfinite(startFrame)) {
        return;
    }
    impl_->layerStartFrameMap[precompLayerId] = startFrame;
}

double PreComposeManager::precomposeLayerStartFrame(
    LayerID precompLayerId) const {
    if (!impl_ || precompLayerId.isNil()) {
        return 0.0;
    }
    return impl_->layerStartFrameMap.value(precompLayerId, 0.0);
}

void PreComposeManager::registerPrecompLayer(CompositionID parentCompId,
                                             LayerID precompLayerId,
                                             CompositionID childCompId) {
    if (!impl_ || precompLayerId.isNil() || childCompId.isNil() || parentCompId.isNil()) {
        return;
    }
    // 実レイヤー作成後に呼ばれる。スタレな bookkeeping を正しい値で上書きする。
    impl_->layerSourceMap[precompLayerId] = childCompId;
    impl_->childSourceMap.insert(childCompId, precompLayerId);
    CompositionNesting nesting;
    nesting.compositionId = childCompId;
    nesting.parentCompositionId = parentCompId;
    nesting.parentLayerId = precompLayerId;
    const int newLevel = impl_->nestingInfo.contains(parentCompId)
                             ? impl_->nestingInfo[parentCompId].nestingLevel + 1
                             : 1;
    nesting.nestingLevel = newLevel;
    impl_->nestingInfo[childCompId] = nesting;
    if (!impl_->nestingMap.contains(parentCompId)) {
        impl_->nestingMap[parentCompId] = QVector<CompositionID>();
    }
    if (!impl_->nestingMap[parentCompId].contains(childCompId)) {
        impl_->nestingMap[parentCompId].append(childCompId);
    }
}

QVector<PrecompLayerRef> PreComposeManager::getPrecompLayersForChild(
    CompositionID childCompId) const {
    QVector<PrecompLayerRef> refs;
    if (!impl_) {
        return refs;
    }
    const auto parentCompId = impl_->nestingInfo.contains(childCompId)
                                  ? impl_->nestingInfo[childCompId].parentCompositionId
                                  : CompositionID();
    const auto layerIds = impl_->childSourceMap.values(childCompId);
    for (const auto& layerId : layerIds) {
        refs.append(PrecompLayerRef{parentCompId, layerId});
    }
    return refs;
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

double parentToChildTime(double parentTime, LayerID precompLayerId) {
    const double startFrame =
        PreComposeManager::instance().precomposeLayerStartFrame(precompLayerId);
    return parentTime - startFrame;
}

double childToParentTime(double childTime, LayerID precompLayerId) {
    return childTime +
           PreComposeManager::instance().precomposeLayerStartFrame(
               precompLayerId);
}

double convertTime(double sourceTime, CompositionID sourceComposition, CompositionID targetComposition) {
    if (sourceComposition == targetComposition) {
        return sourceTime;
    }

    auto& mgr = PreComposeManager::instance();
    QVector<CompositionID> sourceToRoot;
    QVector<CompositionID> targetToRoot;
    auto collectToRoot = [&mgr](CompositionID start) {
        QVector<CompositionID> result;
        CompositionID current = start;
        int guard = 0;
        while (!current.isNil() && guard++ < 64) {
            result.append(current);
            current = mgr.getParentComposition(current);
        }
        return result;
    };
    sourceToRoot = collectToRoot(sourceComposition);
    targetToRoot = collectToRoot(targetComposition);

    CompositionID commonAncestor;
    for (const auto& candidate : sourceToRoot) {
        if (targetToRoot.contains(candidate)) {
            commonAncestor = candidate;
            break;
        }
    }
    if (commonAncestor.isNil()) {
        return sourceTime;
    }

    CompositionID current = sourceComposition;
    double t = sourceTime;
    int guard = 0;
    while (current != commonAncestor && guard++ < 64) {
        auto parentCompId = mgr.getParentComposition(current);
        if (parentCompId.isNil()) {
            return sourceTime;
        }
        auto nesting = mgr.getCompositionNesting(current);
        t = childToParentTime(t, nesting.parentLayerId);
        current = parentCompId;
    }

    QVector<CompositionID> downwardChildren;
    current = targetComposition;
    guard = 0;
    while (current != commonAncestor && guard++ < 64) {
        downwardChildren.prepend(current);
        current = mgr.getParentComposition(current);
        if (current.isNil()) {
            return sourceTime;
        }
    }
    for (const auto& child : downwardChildren) {
        const auto nesting = mgr.getCompositionNesting(child);
        t = parentToChildTime(t, nesting.parentLayerId);
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
