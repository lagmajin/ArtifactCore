module;
#include <algorithm>
#include <utility>
#include <QDateTime>
#include <QString>

module Core.Diagnostics.ProjectDiagnostic;

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
    diagnostics_.push_back(diagnostic);
}

void DiagnosticResult::addDiagnostics(const std::vector<ProjectDiagnostic>& diagnostics) {
    diagnostics_.insert(diagnostics_.end(), diagnostics.begin(), diagnostics.end());
}

void DiagnosticResult::clear() {
    diagnostics_.clear();
}

auto DiagnosticResult::getErrorCount() const -> int {
    return static_cast<int>(std::count_if(diagnostics_.begin(), diagnostics_.end(),
        [](const auto& d) { return d.getSeverity() == DiagnosticSeverity::Error; }));
}

auto DiagnosticResult::getWarningCount() const -> int {
    return static_cast<int>(std::count_if(diagnostics_.begin(), diagnostics_.end(),
        [](const auto& d) { return d.getSeverity() == DiagnosticSeverity::Warning; }));
}

auto DiagnosticResult::getInfoCount() const -> int {
    return static_cast<int>(std::count_if(diagnostics_.begin(), diagnostics_.end(),
        [](const auto& d) { return d.getSeverity() == DiagnosticSeverity::Info; }));
}

auto DiagnosticResult::getErrors() const -> std::vector<ProjectDiagnostic> {
    std::vector<ProjectDiagnostic> result;
    std::copy_if(diagnostics_.begin(), diagnostics_.end(), std::back_inserter(result),
        [](const auto& d) { return d.getSeverity() == DiagnosticSeverity::Error; });
    return result;
}

auto DiagnosticResult::getWarnings() const -> std::vector<ProjectDiagnostic> {
    std::vector<ProjectDiagnostic> result;
    std::copy_if(diagnostics_.begin(), diagnostics_.end(), std::back_inserter(result),
        [](const auto& d) { return d.getSeverity() == DiagnosticSeverity::Warning; });
    return result;
}

auto DiagnosticResult::getByCategory(DiagnosticCategory category) const -> std::vector<ProjectDiagnostic> {
    std::vector<ProjectDiagnostic> result;
    std::copy_if(diagnostics_.begin(), diagnostics_.end(), std::back_inserter(result),
        [category](const auto& d) { return d.getCategory() == category; });
    return result;
}

auto DiagnosticResult::getBySource(const QString& layerId, const QString& compId) const -> std::vector<ProjectDiagnostic> {
    std::vector<ProjectDiagnostic> result;
    std::copy_if(diagnostics_.begin(), diagnostics_.end(), std::back_inserter(result),
        [&layerId, &compId](const auto& d) {
            return d.getSourceLayerId() == layerId || d.getSourceCompId() == compId;
        });
    return result;
}

void DiagnosticResult::merge(const DiagnosticResult& other) {
    diagnostics_.insert(diagnostics_.end(), other.diagnostics_.begin(), other.diagnostics_.end());
}

} // namespace ArtifactCore
