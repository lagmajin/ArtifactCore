module;
#include <utility>
#include <vector>
#include <QString>

export module Core.Diagnostics.ValidationRules;

import Core.Diagnostics.ProjectDiagnostic;
import Core.Diagnostics.DiagnosticEngine;
import Container.NamedVector;

export namespace ArtifactCore {

/// <summary>
/// Missingファイル検出ルール
/// </summary>
export class MissingFileValidationRule : public IValidationRule {
public:
    MissingFileValidationRule() {
        name_ = "MissingFileValidation";
    }

    auto validate(const void* project) -> NamedVector<ProjectDiagnostic> override;
};

/// <summary>
/// 循環依存検出ルール
/// </summary>
export class CircularDependencyValidationRule : public IValidationRule {
public:
    CircularDependencyValidationRule() {
        name_ = "CircularDependencyValidation";
    }

    auto validate(const void* project) -> NamedVector<ProjectDiagnostic> override;

private:
    // DFSで循環依存を検出
    auto detectCycles(const void* project) -> NamedVector<QString>;
};

/// <summary>
/// マット参照検証ルール
/// </summary>
export class MatteReferenceValidationRule : public IValidationRule {
public:
    MatteReferenceValidationRule() {
        name_ = "MatteReferenceValidation";
    }

    auto validate(const void* project) -> NamedVector<ProjectDiagnostic> override;
};

/// <summary>
/// エクスプレッション検証ルール
/// </summary>
export class ExpressionValidationRule : public IValidationRule {
public:
    ExpressionValidationRule() {
        name_ = "ExpressionValidation";
    }

    auto validate(const void* project) -> NamedVector<ProjectDiagnostic> override;
};

/// <summary>
/// パフォーマンス警告ルール
/// </summary>
export class PerformanceValidationRule : public IValidationRule {
public:
    PerformanceValidationRule() {
        name_ = "PerformanceValidation";
    }

    auto validate(const void* project) -> NamedVector<ProjectDiagnostic> override;
};

} // namespace ArtifactCore
