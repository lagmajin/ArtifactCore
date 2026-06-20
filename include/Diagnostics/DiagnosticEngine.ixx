module;
#include <utility>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <QString>

export module Core.Diagnostics.DiagnosticEngine;

import Core.Diagnostics.ProjectDiagnostic;
// import Core.Define; // Module not found

export namespace ArtifactCore {

/// <summary>
/// 検証ルールのインターフェース
/// </summary>
export class IValidationRule {
public:
    virtual ~IValidationRule() = default;

    auto name() const -> QString { return name_; }
    auto enabled() const -> bool { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    virtual auto validate(const void* project) -> std::vector<ProjectDiagnostic> = 0;

protected:
    QString name_;
    bool enabled_ = true;
};

/// <summary>
/// 検証ルールレジストリ
/// </summary>
export class ValidationRuleRegistry {
public:
    void registerRule(std::unique_ptr<IValidationRule> rule);
    void unregisterRule(const QString& name);
    void clearRules();

    auto getRules() const -> const std::vector<std::unique_ptr<IValidationRule>>& { return rules_; }
    auto getRule(const QString& name) -> IValidationRule*;

    void setRuleEnabled(const QString& name, bool enabled);

private:
    std::vector<std::unique_ptr<IValidationRule>> rules_;
};

/// <summary>
/// Diagnostics Engine
/// 
/// プロジェクトの静的解析を行い、問題を検出する。
/// </summary>
export class DiagnosticEngine {
public:
    DiagnosticEngine();
    ~DiagnosticEngine() = default;

    // ルール管理
    auto ruleRegistry() -> ValidationRuleRegistry& { return ruleRegistry_; }
    auto ruleRegistry() const -> const ValidationRuleRegistry& { return ruleRegistry_; }

    // 検証実行
    auto validateAll(const void* project) -> DiagnosticResult;
    auto validateDelta(const void* project, const QStringList& changedIds) -> DiagnosticResult;

    // 結果取得
    auto getCachedResult() const -> const DiagnosticResult& { return cachedResult_; }
    auto hasCachedResult() const -> bool { return hasCachedResult_; }

    // キャッシュクリア
    void clearCache();

private:
    ValidationRuleRegistry ruleRegistry_;
    DiagnosticResult cachedResult_;
    bool hasCachedResult_ = false;
};

} // namespace ArtifactCore
