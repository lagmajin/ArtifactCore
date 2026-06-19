module;
#include <algorithm>
#include <utility>
#include <QDateTime>
#include <QString>

module Core.Diagnostics.ProjectDiagnostic;

import Container.NamedVector;

namespace ArtifactCore {

// ============================================================================
// ProjectDiagnostic
// ============================================================================

ProjectDiagnostic::ProjectDiagnostic()
    : id_(nextId())
    , timestamp_(QDateTime::currentDateTime()) {
}

ProjectDiagnostic::ProjectDiagnostic(
    DiagnosticSeverity severity,
    DiagnosticCategory category,
    const QString& message)
    : id_(nextId())
    , severity_(severity)
    , category_(category)
    , message_(message)
    , timestamp_(QDateTime::currentDateTime()) {
}

auto ProjectDiagnostic::nextId() -> int {
    static int counter = 0;
    return ++counter;
}

auto ProjectDiagnostic::createMissingFile(const QString& filePath, const QString& layerId) -> ProjectDiagnostic {
    ProjectDiagnostic diag(
        DiagnosticSeverity::Error,
        DiagnosticCategory::File,
        QString("Missing file: %1").arg(filePath)
    );
    diag.setDescription(QString("The source file '%1' could not be found.").arg(filePath));
    diag.setSourceLayerId(layerId);
    diag.setFixAction("Relink the file or remove the layer");
    return diag;
}

auto ProjectDiagnostic::createMissingMatte(const QString& matteName, const QString& layerId) -> ProjectDiagnostic {
    ProjectDiagnostic diag(
        DiagnosticSeverity::Error,
        DiagnosticCategory::Matte,
        QString("Missing matte: %1").arg(matteName)
    );
    diag.setDescription(QString("The referenced matte layer '%1' does not exist.").arg(matteName));
    diag.setSourceLayerId(layerId);
    diag.setFixAction("Create the matte layer or fix the reference");
    return diag;
}

auto ProjectDiagnostic::createCircularDependency(const QString& chain, const QString& compId) -> ProjectDiagnostic {
    ProjectDiagnostic diag(
        DiagnosticSeverity::Error,
        DiagnosticCategory::CircularDep,
        QString("Circular dependency detected: %1").arg(chain)
    );
    diag.setDescription(QString("The following dependency cycle was found: %1").arg(chain));
    diag.setSourceCompId(compId);
    diag.setFixAction("Remove one of the dependencies to break the cycle");
    return diag;
}

auto ProjectDiagnostic::createExpressionError(const QString& expression, const QString& layerId) -> ProjectDiagnostic {
    ProjectDiagnostic diag(
        DiagnosticSeverity::Error,
        DiagnosticCategory::Expression,
        QString("Expression error: %1").arg(expression)
    );
    diag.setDescription(QString("The expression could not be evaluated: %1").arg(expression));
    diag.setSourceLayerId(layerId);
    diag.setFixAction("Fix the expression syntax or remove it");
    return diag;
}

auto ProjectDiagnostic::createPerformanceWarning(const QString& message, const QString& sourceId) -> ProjectDiagnostic {
    ProjectDiagnostic diag(
        DiagnosticSeverity::Warning,
        DiagnosticCategory::Performance,
        message
    );
    diag.setSourceLayerId(sourceId);
    diag.setFixAction("Consider optimizing the asset or reducing resolution");
    return diag;
}

// ============================================================================
// DiagnosticResult
// ============================================================================

void DiagnosticResult::addDiagnostic(const ProjectDiagnostic& diagnostic) {
    diagnostics_.add(diagnostic);
}

void DiagnosticResult::addDiagnostics(const std::vector<ProjectDiagnostic>& diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        diagnostics_.add(diagnostic);
    }
}

void DiagnosticResult::addDiagnostics(const NamedVector<ProjectDiagnostic>& diagnostics) {
    diagnostics.each([&](const auto& diagnostic) {
        diagnostics_.add(diagnostic);
    });
}

void DiagnosticResult::clear() {
    diagnostics_.clear();
}

auto DiagnosticResult::getDiagnostics() const -> std::vector<ProjectDiagnostic> {
    auto result = makeNamedVector<ProjectDiagnostic>(ContainerName{"ProjectDiagnosticsSnapshot"});
    result.reserve(diagnostics_.count());
    diagnostics_.each([&](const auto& diagnostic) {
        result.add(diagnostic);
    });
    return result.toStdVector();
}

auto DiagnosticResult::getErrorCount() const -> int {
    int count = 0;
    diagnostics_.each([&](const auto& d) {
        if (d.getSeverity() == DiagnosticSeverity::Error) {
            ++count;
        }
    });
    return count;
}

auto DiagnosticResult::getWarningCount() const -> int {
    int count = 0;
    diagnostics_.each([&](const auto& d) {
        if (d.getSeverity() == DiagnosticSeverity::Warning) {
            ++count;
        }
    });
    return count;
}

auto DiagnosticResult::getInfoCount() const -> int {
    int count = 0;
    diagnostics_.each([&](const auto& d) {
        if (d.getSeverity() == DiagnosticSeverity::Info) {
            ++count;
        }
    });
    return count;
}

auto DiagnosticResult::getErrors() const -> std::vector<ProjectDiagnostic> {
    auto result = makeNamedVector<ProjectDiagnostic>(ContainerName{"ProjectDiagnosticsErrors"});
    diagnostics_.each([&](const auto& d) {
        if (d.getSeverity() == DiagnosticSeverity::Error) {
            result.add(d);
        }
    });
    return result.toStdVector();
}

auto DiagnosticResult::getWarnings() const -> std::vector<ProjectDiagnostic> {
    auto result = makeNamedVector<ProjectDiagnostic>(ContainerName{"ProjectDiagnosticsWarnings"});
    diagnostics_.each([&](const auto& d) {
        if (d.getSeverity() == DiagnosticSeverity::Warning) {
            result.add(d);
        }
    });
    return result.toStdVector();
}

auto DiagnosticResult::getByCategory(DiagnosticCategory category) const -> std::vector<ProjectDiagnostic> {
    auto result = makeNamedVector<ProjectDiagnostic>(ContainerName{"ProjectDiagnosticsByCategory"});
    diagnostics_.each([&](const auto& d) {
        if (d.getCategory() == category) {
            result.add(d);
        }
    });
    return result.toStdVector();
}

auto DiagnosticResult::getBySource(const QString& layerId, const QString& compId) const -> std::vector<ProjectDiagnostic> {
    auto result = makeNamedVector<ProjectDiagnostic>(ContainerName{"ProjectDiagnosticsBySource"});
    diagnostics_.each([&](const auto& d) {
        if (d.getSourceLayerId() == layerId || d.getSourceCompId() == compId) {
            result.add(d);
        }
    });
    return result.toStdVector();
}

void DiagnosticResult::merge(const DiagnosticResult& other) {
    other.diagnostics_.each([&](const auto& diagnostic) {
        diagnostics_.add(diagnostic);
    });
}

} // namespace ArtifactCore
