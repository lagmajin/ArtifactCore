module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <QDateTime>
#include <QString>

export module Core.Diagnostics.ProjectDiagnostic;

import Core.Define;
import Core.Layer.LayerTypes;

export namespace ArtifactCore {

/// <summary>
/// 問題の重大度
/// </summary>
export enum class DiagnosticSeverity {
    Error,      // 修正必須（レンダリング失敗など）
    Warning,    // 警告（パフォーマンス低下など）
    Info        // 情報（推奨設定など）
};

/// <summary>
/// 問題のカテゴリ
/// </summary>
export enum class DiagnosticCategory {
    Reference,      // 無効な参照
    Matte,          // マット関連
    CircularDep,    // 循環依存
    Expression,     // エクスプレッション
    Performance,    // パフォーマンス
    File,           // ファイル
    Configuration,  // 設定
    Custom          // カスタム
};

/// <summary>
/// プロジェクトの個別の問題
/// </summary>
export class ProjectDiagnostic {
public:
    ProjectDiagnostic();
    ProjectDiagnostic(
        DiagnosticSeverity severity,
        DiagnosticCategory category,
        const QString& message);
    ~ProjectDiagnostic() = default;

    // 基本情報
    auto getSeverity() const -> DiagnosticSeverity { return severity_; }
    void setSeverity(DiagnosticSeverity severity) { severity_ = severity; }

    auto getCategory() const -> DiagnosticCategory { return category_; }
    void setCategory(DiagnosticCategory category) { category_ = category; }

    auto getMessage() const -> const QString& { return message_; }
    void setMessage(const QString& message) { message_ = message; }

    auto getDescription() const -> const QString& { return description_; }
    void setDescription(const QString& description) { description_ = description; }

    // 発生源
    auto getSourceLayerId() const -> const QString& { return sourceLayerId_; }
    void setSourceLayerId(const QString& id) { sourceLayerId_ = id; }

    auto getSourceCompId() const -> const QString& { return sourceCompId_; }
    void setSourceCompId(const QString& id) { sourceCompId_ = id; }

    // 修復アクション
    auto getFixAction() const -> const QString& { return fixAction_; }
    void setFixAction(const QString& action) { fixAction_ = action; }

    // メタデータ
    auto getTimestamp() const -> QDateTime { return timestamp_; }
    auto getId() const -> int { return id_; }

    // ファクトリ
    static auto createMissingFile(const QString& filePath, const QString& layerId = "") -> ProjectDiagnostic;
    static auto createMissingMatte(const QString& matteName, const QString& layerId = "") -> ProjectDiagnostic;
    static auto createCircularDependency(const QString& chain, const QString& compId = "") -> ProjectDiagnostic;
    static auto createExpressionError(const QString& expression, const QString& layerId = "") -> ProjectDiagnostic;
    static auto createPerformanceWarning(const QString& message, const QString& sourceId = "") -> ProjectDiagnostic;

    // ユーティリティ
    auto isError() const -> bool { return severity_ == DiagnosticSeverity::Error; }
    auto isWarning() const -> bool { return severity_ == DiagnosticSeverity::Warning; }
    auto isInfo() const -> bool { return severity_ == DiagnosticSeverity::Info; }

private:
    static auto nextId() -> int;

    int id_;
    DiagnosticSeverity severity_ = DiagnosticSeverity::Info;
    DiagnosticCategory category_ = DiagnosticCategory::Custom;
    QString message_;
    QString description_;
    QString sourceLayerId_;
    QString sourceCompId_;
    QString fixAction_;
    QDateTime timestamp_;
};

/// <summary>
/// 問題のリスト（型安全なコレクション）
/// </summary>
export class DiagnosticResult {
public:
    DiagnosticResult() = default;
    ~DiagnosticResult() = default;

    void addDiagnostic(const ProjectDiagnostic& diagnostic);
    void addDiagnostics(const std::vector<ProjectDiagnostic>& diagnostics);
    void clear();

    auto getDiagnostics() const -> const std::vector<ProjectDiagnostic>& { return diagnostics_; }
    auto getErrorCount() const -> int;
    auto getWarningCount() const -> int;
    auto getInfoCount() const -> int;
    auto hasErrors() const -> bool { return getErrorCount() > 0; }
    auto hasWarnings() const -> bool { return getWarningCount() > 0; }

    // フィルタ
    auto getErrors() const -> std::vector<ProjectDiagnostic>;
    auto getWarnings() const -> std::vector<ProjectDiagnostic>;
    auto getByCategory(DiagnosticCategory category) const -> std::vector<ProjectDiagnostic>;
    auto getBySource(const QString& layerId, const QString& compId = "") const -> std::vector<ProjectDiagnostic>;

    // マージ
    void merge(const DiagnosticResult& other);

private:
    std::vector<ProjectDiagnostic> diagnostics_;
};

} // namespace ArtifactCore
