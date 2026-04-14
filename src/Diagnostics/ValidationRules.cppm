module;
#include <algorithm>
#include <vector>
#include <QString>
#include <QSet>
#include <functional>
#include <unordered_map>

module Core.Diagnostics.ValidationRules;

namespace ArtifactCore {

// ============================================================================
// MissingFileValidationRule
// ============================================================================

auto MissingFileValidationRule::validate(const void* project) -> std::vector<ProjectDiagnostic> {
    std::vector<ProjectDiagnostic> diagnostics;

    // TODO: 実際のComposition型にキャストしてチェック
    // 現在はインターフェースのみ
    // auto comp = static_cast<const ArtifactComposition*>(project);
    // if (!comp) return diagnostics;
    //
    // for (const auto& layer : comp->allLayer()) {
    //     if (!layer) continue;
    //     // ソースファイルがあるかチェック
    //     if (layer->hasSource() && !layer->isSourceAvailable()) {
    //         diagnostics.push_back(
    //             ProjectDiagnostic::createMissingFile(
    //                 layer->sourcePath(),
    //                 layer->id().toString()));
    //     }
    // }

    return diagnostics;
}

// ============================================================================
// CircularDependencyValidationRule
// ============================================================================

auto CircularDependencyValidationRule::validate(const void* project) -> std::vector<ProjectDiagnostic> {
    std::vector<ProjectDiagnostic> diagnostics;

    auto cycles = detectCycles(project);
    for (const auto& cycle : cycles) {
        diagnostics.push_back(
            ProjectDiagnostic::createCircularDependency(cycle));
    }

    return diagnostics;
}

auto CircularDependencyValidationRule::detectCycles(const void* project) -> std::vector<QString> {
    std::vector<QString> cycles;

    // TODO: 実際の依存グラフを構築
    // 実装例（CompositionのPreCompose依存をチェック）:
    //
    // std::unordered_map<QString, std::vector<QString>> adjacencyList;
    // auto comp = static_cast<const ArtifactComposition*>(project);
    // if (!comp) return cycles;
    //
    // // 依存関係構築
    // for (const auto& layer : comp->allLayer()) {
    //     if (layer->isPreCompose()) {
    //         auto targetComp = layer->targetComposition();
    //         if (targetComp) {
    //             adjacencyList[comp->id().toString()].push_back(targetComp->id().toString());
    //         }
    //     }
    // }
    //
    // // DFSで循環検出
    // QSet<QString> visited;
    // QSet<QString> recStack;
    //
    // std::function<bool(const QString&, std::vector<QString>&)> dfs =
    //     [&](const QString& node, std::vector<QString>& path) -> bool {
    //         visited.insert(node);
    //         recStack.insert(node);
    //         path.push_back(node);
    //
    //         if (adjacencyList.contains(node)) {
    //             for (const auto& neighbor : adjacencyList[node]) {
    //                 if (!visited.contains(neighbor)) {
    //                     if (dfs(neighbor, path)) return true;
    //                 } else if (recStack.contains(neighbor)) {
    //                     QString cycle;
    //                     bool recording = false;
    //                     for (const auto& p : path) {
    //                         if (p == neighbor) recording = true;
    //                         if (recording) cycle += p + " → ";
    //                     }
    //                     cycle += neighbor;
    //                     cycles.push_back(cycle);
    //                     return true;
    //                 }
    //             }
    //         }
    //
    //         path.pop_back();
    //         recStack.remove(node);
    //         return false;
    //     };
    //
    // for (const auto& [node, _] : adjacencyList) {
    //     if (!visited.contains(node)) {
    //         std::vector<QString> path;
    //         dfs(node, path);
    //     }
    // }

    return cycles;
}

// ============================================================================
// MatteReferenceValidationRule
// ============================================================================

auto MatteReferenceValidationRule::validate(const void* project) -> std::vector<ProjectDiagnostic> {
    std::vector<ProjectDiagnostic> diagnostics;

    // TODO: マット参照を検証
    // auto comp = static_cast<const ArtifactComposition*>(project);
    // if (!comp) return diagnostics;
    //
    // for (const auto& layer : comp->allLayer()) {
    //     if (layer->usesTrackMatte()) {
    //         auto matteLayerId = layer->trackMatteSourceId();
    //         auto matteLayer = comp->layerById(matteLayerId);
    //         if (!matteLayer) {
    //             diagnostics.push_back(
    //                 ProjectDiagnostic::createMissingMatte(
    //                     layer->trackMatteSourceName(),
    //                     layer->id().toString()));
    //         }
    //     }
    // }

    return diagnostics;
}

// ============================================================================
// ExpressionValidationRule
// ============================================================================

auto ExpressionValidationRule::validate(const void* project) -> std::vector<ProjectDiagnostic> {
    std::vector<ProjectDiagnostic> diagnostics;

    // TODO: エクスプレッションを検証
    // auto comp = static_cast<const ArtifactComposition*>(project);
    // if (!comp) return diagnostics;
    //
    // for (const auto& layer : comp->allLayer()) {
    //     const auto props = layer->getLayerPropertyGroups();
    //     for (const auto& group : props) {
    //         for (const auto& prop : group.properties()) {
    //             if (prop.hasExpression() && !prop.isExpressionValid()) {
    //                 diagnostics.push_back(
    //                     ProjectDiagnostic::createExpressionError(
    //                         prop.expression(),
    //                         layer->id().toString()));
    //             }
    //         }
    //     }
    // }

    return diagnostics;
}

// ============================================================================
// PerformanceValidationRule
// ============================================================================

auto PerformanceValidationRule::validate(const void* project) -> std::vector<ProjectDiagnostic> {
    std::vector<ProjectDiagnostic> diagnostics;

    // TODO: パフォーマンス問題を検出
    // auto comp = static_cast<const ArtifactComposition*>(project);
    // if (!comp) return diagnostics;
    //
    // // 巨大なコンポジション
    // if (comp->width() > 4096 || comp->height() > 4096) {
    //     diagnostics.push_back(
    //         ProjectDiagnostic::createPerformanceWarning(
    //             QString("Large composition: %1x%2").arg(comp->width()).arg(comp->height()),
    //             comp->id().toString()));
    // }
    //
    // // 高解像度未使用アセット
    // for (const auto& layer : comp->allLayer()) {
    //     if (layer->isHidden() && layer->hasHighResSource()) {
    //         diagnostics.push_back(
    //             ProjectDiagnostic::createPerformanceWarning(
    //                 QString("Hidden layer with high-res source: %1").arg(layer->layerName()),
    //                 layer->id().toString()));
    //     }
    // }

    return diagnostics;
}

} // namespace ArtifactCore
