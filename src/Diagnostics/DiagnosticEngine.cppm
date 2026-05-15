module;
#include <algorithm>
#include <utility>
#include <QString>

module Core.Diagnostics.DiagnosticEngine;

namespace ArtifactCore {

// ============================================================================
// ValidationRuleRegistry
// ============================================================================

void ValidationRuleRegistry::registerRule(std::unique_ptr<IValidationRule> rule) {
    if (!rule) return;
    rules_.push_back(std::move(rule));
}

void ValidationRuleRegistry::unregisterRule(const QString& name) {
    rules_.erase(
        std::remove_if(rules_.begin(), rules_.end(),
            [&name](const auto& rule) {
                return rule && rule->name() == name;
            }),
        rules_.end()
    );
}

void ValidationRuleRegistry::clearRules() {
    rules_.clear();
}

auto ValidationRuleRegistry::getRule(const QString& name) -> IValidationRule* {
    for (auto& rule : rules_) {
        if (rule && rule->name() == name) {
            return rule.get();
        }
    }
    return nullptr;
}

void ValidationRuleRegistry::setRuleEnabled(const QString& name, bool enabled) {
    for (auto& rule : rules_) {
        if (rule && rule->name() == name) {
            rule->setEnabled(enabled);
        }
    }
}

// ============================================================================
// DiagnosticEngine
// ============================================================================

DiagnosticEngine::DiagnosticEngine() = default;

auto DiagnosticEngine::validateAll(const void* project) -> DiagnosticResult {
    DiagnosticResult result;

    for (auto& rule : ruleRegistry_.getRules()) {
        if (!rule || !rule->enabled()) continue;

        auto diagnostics = rule->validate(project);
        result.addDiagnostics(diagnostics);
    }

    // キャッシュ
    cachedResult_ = result;
    hasCachedResult_ = true;

    return result;
}

auto DiagnosticEngine::validateDelta(const void* project, const QStringList& changedIds) -> DiagnosticResult {
    // 変更された部分のみ検証（高速化）
    // TODO: 差分検証の実装
    // 現状は全量検証
    return validateAll(project);
}

void DiagnosticEngine::clearCache() {
    cachedResult_.clear();
    hasCachedResult_ = false;
}

} // namespace ArtifactCore
