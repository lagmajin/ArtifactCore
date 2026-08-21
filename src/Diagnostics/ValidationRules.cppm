module;
#include <algorithm>
#include <functional>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <vector>

module Core.Diagnostics.ValidationRules;

import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Composition;
import Artifact.Project;
import Artifact.Project.Items;
import Layer.Matte;
import Container.NamedVector;

namespace ArtifactCore {

namespace {

void collectSourcePaths(const QJsonValue& value, QStringList& paths)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            const QString& key = it.key();
            if ((key == QStringLiteral("sourcePath") || key.endsWith(QStringLiteral(".sourcePath")))
                && it.value().isString()) {
                const QString path = it.value().toString().trimmed();
                if (!path.isEmpty()) {
                    paths.push_back(path);
                }
            }
            collectSourcePaths(it.value(), paths);
        }
    } else if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const auto& entry : array) {
            collectSourcePaths(entry, paths);
        }
    }
}

template <typename Fn>
void visitProjectItems(Artifact::ProjectItem* item, const Fn& fn)
{
    if (!item) {
        return;
    }
    fn(item);
    for (auto* child : item->children) {
        visitProjectItems(child, fn);
    }
}

bool isCompositionItem(const Artifact::ProjectItem* item)
{
    return item && item->type() == Artifact::eProjectItemType::Composition;
}

QString layerDisplayName(const Artifact::ArtifactAbstractLayerPtr& layer)
{
    return layer ? layer->layerName() : QString();
}

} // namespace

auto MissingFileValidationRule::validate(const void* project) -> std::vector<ProjectDiagnostic>
{
    NamedVector<ProjectDiagnostic> diagnostics{
        makeNamedVector<ProjectDiagnostic>(ContainerName{"ValidationDiagnostics"})};
    auto* projectPtr = static_cast<Artifact::ArtifactProject*>(const_cast<void*>(project));
    if (!projectPtr) {
        return diagnostics.toStdVector();
    }

    const auto items = projectPtr->projectItems();
    auto handleComposition = [&](Artifact::CompositionItem* compItem) {
        if (!compItem) {
            return;
        }
        const auto findResult = projectPtr->findComposition(compItem->compositionId);
        if (!findResult.success) {
            return;
        }
        const auto comp = findResult.ptr.lock();
        if (!comp) {
            return;
        }

        for (const auto& layer : comp->allLayer()) {
            if (!layer) {
                continue;
            }

            QStringList sourcePaths;
            collectSourcePaths(layer->toJson(), sourcePaths);
            sourcePaths.removeDuplicates();

            for (const auto& sourcePath : sourcePaths) {
                if (!QFileInfo::exists(sourcePath)) {
                    diagnostics.append(ProjectDiagnostic::createMissingFile(
                        sourcePath,
                        layer->id().toString()));
                }
            }
        }
    };

    std::function<void(Artifact::ProjectItem*)> walk = [&](Artifact::ProjectItem* item) {
        if (!item) {
            return;
        }
        if (isCompositionItem(item)) {
            handleComposition(static_cast<Artifact::CompositionItem*>(item));
        }
        for (auto* child : item->children) {
            walk(child);
        }
    };

    for (auto* root : items) {
        walk(root);
    }

    return diagnostics.toStdVector();
}

auto CircularDependencyValidationRule::validate(const void* project) -> std::vector<ProjectDiagnostic>
{
    NamedVector<ProjectDiagnostic> diagnostics{
        makeNamedVector<ProjectDiagnostic>(ContainerName{"ValidationDiagnostics"})};

    for (const auto& cycle : detectCycles(project)) {
        diagnostics.append(ProjectDiagnostic::createCircularDependency(cycle));
    }

    return diagnostics.toStdVector();
}

auto CircularDependencyValidationRule::detectCycles(const void* project) -> std::vector<QString>
{
    std::vector<QString> cycles;
    auto* projectPtr = static_cast<Artifact::ArtifactProject*>(const_cast<void*>(project));
    if (!projectPtr) {
        return cycles;
    }

    QHash<QString, QStringList> adjacency;
    QHash<QString, QString> compositionNames;

    const auto items = projectPtr->projectItems();
    std::function<void(Artifact::ProjectItem*)> gather = [&](Artifact::ProjectItem* item) {
        if (!item) {
            return;
        }

        if (isCompositionItem(item)) {
            auto* compItem = static_cast<Artifact::CompositionItem*>(item);
            const auto findResult = projectPtr->findComposition(compItem->compositionId);
            if (findResult.success) {
                const auto comp = findResult.ptr.lock();
                if (comp) {
                    const QString compId = comp->id().toString();
                    compositionNames.insert(compId, compItem->name.toQString());

                    for (const auto& layer : comp->allLayer()) {
                        auto* compLayer = dynamic_cast<Artifact::ArtifactCompositionLayer*>(layer.get());
                        if (!compLayer) {
                            continue;
                        }
                        const QString targetId = compLayer->sourceCompositionId().toString();
                        if (!targetId.isEmpty()) {
                            adjacency[compId].push_back(targetId);
                        }
                    }
                }
            }
        }

        for (auto* child : item->children) {
            gather(child);
        }
    };

    for (auto* root : items) {
        gather(root);
    }

    QSet<QString> visited;
    QSet<QString> stackSet;
    QStringList stack;
    QSet<QString> reported;

    std::function<void(const QString&)> dfs = [&](const QString& node) {
        visited.insert(node);
        stackSet.insert(node);
        stack.push_back(node);

        const auto neighbors = adjacency.value(node);
        for (const auto& next : neighbors) {
            if (!visited.contains(next)) {
                dfs(next);
            } else if (stackSet.contains(next)) {
                const int startIndex = stack.indexOf(next);
                if (startIndex >= 0) {
                    QStringList cycleNames;
                    for (int i = startIndex; i < stack.size(); ++i) {
                        const QString& compId = stack.at(i);
                        cycleNames.push_back(compositionNames.value(compId, compId));
                    }
                    cycleNames.push_back(compositionNames.value(next, next));
                    const QString cycle = cycleNames.join(QStringLiteral(" -> "));
                    if (!reported.contains(cycle)) {
                        reported.insert(cycle);
                        cycles.push_back(cycle);
                    }
                }
            }
        }

        stack.removeLast();
        stackSet.remove(node);
    };

    for (auto it = adjacency.begin(); it != adjacency.end(); ++it) {
        const QString& node = it.key();
        if (!visited.contains(node)) {
            dfs(node);
        }
    }

    return cycles;
}

auto MatteReferenceValidationRule::validate(const void* project) -> std::vector<ProjectDiagnostic>
{
    NamedVector<ProjectDiagnostic> diagnostics{
        makeNamedVector<ProjectDiagnostic>(ContainerName{"ValidationDiagnostics"})};
    auto* projectPtr = static_cast<Artifact::ArtifactProject*>(const_cast<void*>(project));
    if (!projectPtr) {
        return diagnostics.toStdVector();
    }

    const auto items = projectPtr->projectItems();
    std::function<void(Artifact::ProjectItem*)> walk = [&](Artifact::ProjectItem* item) {
        if (!item || !isCompositionItem(item)) {
            if (item) {
                for (auto* child : item->children) {
                    walk(child);
                }
            }
            return;
        }

        auto* compItem = static_cast<Artifact::CompositionItem*>(item);
        const auto findResult = projectPtr->findComposition(compItem->compositionId);
        if (!findResult.success) {
            return;
        }

        const auto comp = findResult.ptr.lock();
        if (!comp) {
            return;
        }

        const QString compId = comp->id().toString();
        const auto layers = comp->allLayer();

        QHash<QString, Artifact::ArtifactAbstractLayerPtr> layerMap;
        for (const auto& layer : layers) {
            if (layer) {
                layerMap.insert(layer->id().toString(), layer);
            }
        }

        QSet<QString> reportedCycles;

        for (const auto& layer : layers) {
            if (!layer) {
                continue;
            }

            const QString layerId = layer->id().toString();
            const QString layerName = layerDisplayName(layer);
            const auto matteRefs = layer->matteReferences();

            for (const auto& ref : matteRefs) {
                if (!ref.enabled) {
                    continue;
                }

                const QString sourceId = ref.sourceLayerId.toString();
                if (sourceId.isEmpty()) {
                    continue;
                }

                if (!layerMap.contains(sourceId)) {
                    diagnostics.append(ProjectDiagnostic::createMissingMatte(
                        QStringLiteral("Matte source '%1' not found").arg(sourceId),
                        layerId));
                    continue;
                }

                if (sourceId == layerId) {
                    ProjectDiagnostic diag(
                        DiagnosticSeverity::Error,
                        DiagnosticCategory::Matte,
                        QStringLiteral("Layer '%1' references itself as matte source").arg(layerName));
                    diag.setSourceLayerId(layerId);
                    diag.setSourceCompId(compId);
                    diag.setFixAction(QStringLiteral("Select a different matte source"));
                    diagnostics.append(diag);
                    continue;
                }

                const auto sourceLayer = layerMap.value(sourceId);
                if (sourceLayer && !sourceLayer->isVisible()) {
                    ProjectDiagnostic diag(
                        DiagnosticSeverity::Warning,
                        DiagnosticCategory::Matte,
                        QStringLiteral("Matte source '%1' for layer '%2' is hidden")
                            .arg(sourceLayer->layerName(), layerName));
                    diag.setSourceLayerId(sourceId);
                    diag.setSourceCompId(compId);
                    diag.setFixAction(QStringLiteral("Show the matte source layer"));
                    diagnostics.append(diag);
                }
            }

            QSet<QString> path;
            QStringList chain;
            const auto reportCycle = [&](const QString& cycleId) -> void {
                const int cycleStart = chain.indexOf(cycleId);
                if (cycleStart < 0) {
                    return;
                }

                QStringList cycleIds;
                for (int i = cycleStart; i < chain.size(); ++i) {
                    cycleIds.push_back(chain.at(i));
                }
                const QStringList sortedCycleIds = [&cycleIds] {
                    QStringList sorted = cycleIds;
                    std::sort(sorted.begin(), sorted.end());
                    return sorted;
                }();
                const QString cycleKey = sortedCycleIds.join(QStringLiteral("|"));
                if (reportedCycles.contains(cycleKey)) {
                    return;
                }
                reportedCycles.insert(cycleKey);

                QStringList cycleNames;
                for (const auto& id : cycleIds) {
                    const auto chainLayer = layerMap.value(id);
                    cycleNames.push_back(chainLayer ? chainLayer->layerName() : id);
                }
                const auto closingLayer = layerMap.value(cycleId);
                cycleNames.push_back(closingLayer ? closingLayer->layerName()
                                                  : cycleId);
                diagnostics.append(ProjectDiagnostic::createCircularDependency(
                    cycleNames.join(QStringLiteral(" -> ")), compId));
            };

            const auto visit = [&](const QString& currentId,
                                   const auto& self) -> void {
                if (path.contains(currentId)) {
                    reportCycle(currentId);
                    return;
                }

                const auto currentLayer = layerMap.value(currentId);
                if (!currentLayer) {
                    return;
                }

                path.insert(currentId);
                chain.push_back(currentId);
                for (const auto& matteRef : currentLayer->matteReferences()) {
                    if (!matteRef.enabled || matteRef.sourceLayerId.isNil()) {
                        continue;
                    }
                    const QString nextId = matteRef.sourceLayerId.toString();
                    if (!layerMap.contains(nextId) || nextId == currentId) {
                        continue;
                    }
                    self(nextId, self);
                }
                chain.removeLast();
                path.remove(currentId);
            };

            visit(layerId, visit);
        }

        for (auto* child : item->children) {
            walk(child);
        }
    };

    for (auto* root : items) {
        walk(root);
    }

    return diagnostics.toStdVector();
}

auto ExpressionValidationRule::validate(const void* project) -> std::vector<ProjectDiagnostic>
{
    NamedVector<ProjectDiagnostic> diagnostics{
        makeNamedVector<ProjectDiagnostic>(ContainerName{"ValidationDiagnostics"})};
    auto* projectPtr = static_cast<Artifact::ArtifactProject*>(const_cast<void*>(project));
    if (!projectPtr) return diagnostics.toStdVector();

    const auto items = projectPtr->projectItems();
    std::function<void(Artifact::ProjectItem*)> walk = [&](Artifact::ProjectItem* item) {
        if (!item) return;
        if (isCompositionItem(item)) {
            auto* compItem = static_cast<Artifact::CompositionItem*>(item);
            const auto findResult = projectPtr->findComposition(compItem->compositionId);
            if (findResult.success) {
                const auto comp = findResult.ptr.lock();
                if (comp) {
                    const auto json = comp->toJson().object();
                    const auto layers = json.value(QStringLiteral("layers")).toArray();
                    for (const auto& layerVal : layers) {
                        const QJsonObject layerObj = layerVal.toObject();
                        if (layerObj.isEmpty()) continue;
                        const auto effects = layerObj.value(QStringLiteral("effects")).toArray();
                        for (const auto& effectVal : effects) {
                            const QJsonObject effectObj = effectVal.toObject();
                            const auto props = effectObj.value(QStringLiteral("properties")).toObject();
                            for (auto it = props.begin(); it != props.end(); ++it) {
                                const QString val = it.value().toString();
                                if (val.contains(QStringLiteral("=")) ||
                                    val.contains(QStringLiteral("loopOut")) ||
                                    val.contains(QStringLiteral("wiggle")) ||
                                    val.contains(QStringLiteral("transform.")) ||
                                    val.contains(QStringLiteral("thisComp")) ||
                                    val.contains(QStringLiteral("valueAtTime")) ||
                                    val.contains(QStringLiteral("Math."))) {
                                    if (val.count(QStringLiteral("(")) != val.count(QStringLiteral(")"))) {
                                        diagnostics.append(ProjectDiagnostic::createExpressionError(
                                            val, layerObj.value(QStringLiteral("id")).toString()));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        for (auto* child : item->children) walk(child);
    };
    for (auto* root : items) walk(root);
    return diagnostics.toStdVector();
}

auto PerformanceValidationRule::validate(const void* project) -> std::vector<ProjectDiagnostic>
{
    NamedVector<ProjectDiagnostic> diagnostics{
        makeNamedVector<ProjectDiagnostic>(ContainerName{"ValidationDiagnostics"})};
    auto* projectPtr = static_cast<Artifact::ArtifactProject*>(const_cast<void*>(project));
    if (!projectPtr) {
        return diagnostics.toStdVector();
    }

    const auto items = projectPtr->projectItems();
    std::function<void(Artifact::ProjectItem*)> walk = [&](Artifact::ProjectItem* item) {
        if (!item) {
            return;
        }

        if (isCompositionItem(item)) {
            auto* compItem = static_cast<Artifact::CompositionItem*>(item);
            const auto findResult = projectPtr->findComposition(compItem->compositionId);
            if (findResult.success) {
                const auto comp = findResult.ptr.lock();
                if (comp) {
                    const auto json = comp->toJson().object();
                    const int width = json.value(QStringLiteral("width")).toInt(0);
                    const int height = json.value(QStringLiteral("height")).toInt(0);

                    if (width > 3840 || height > 2160) {
                        diagnostics.append(ProjectDiagnostic::createPerformanceWarning(
                            QStringLiteral("High-resolution composition: %1x%2").arg(width).arg(height),
                            comp->id().toString()));
                    }

                    if (comp->allLayer().size() > 500) {
                        diagnostics.append(ProjectDiagnostic::createPerformanceWarning(
                            QStringLiteral("Large layer stack: %1 layers").arg(comp->allLayer().size()),
                            comp->id().toString()));
                    }
                }
            }
        }

        for (auto* child : item->children) {
            walk(child);
        }
    };

    for (auto* root : items) {
        walk(root);
    }

    return diagnostics.toStdVector();
}

} // namespace ArtifactCore
