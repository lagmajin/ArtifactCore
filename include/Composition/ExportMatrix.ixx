module;
#include <QJsonObject>
#include <QJsonArray>
#include <QMetaType>
#include <QString>
#include <QVector>

export module Composition.ExportMatrix;

export namespace ArtifactCore {

enum class ExportTargetKind {
    Image,
    Video,
    Json,
    Audio,
    Custom
};

struct ExportPresetTarget {
    QString id;
    QString name;
    ExportTargetKind kind = ExportTargetKind::Custom;
    QString format;
    QString codec;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    int bitrateKbps = 0;
    QString outputExtension;
    QString namingRule;
    bool enabled = true;

    QJsonObject toJson() const;
    static ExportPresetTarget fromJson(const QJsonObject& obj);
};

struct ExportMatrixVariant {
    QString id;
    QString displayName;
    QString description;
    bool enabled = true;
    QJsonObject parameters;

    QJsonObject toJson() const;
    static ExportMatrixVariant fromJson(const QJsonObject& obj);
};

struct ExportMatrixCellRule {
    QString variantId;
    QString presetId;
    bool enabled = true;
    QJsonObject overrides;

    QJsonObject toJson() const;
    static ExportMatrixCellRule fromJson(const QJsonObject& obj);
};

struct ExportMatrix {
    QString id;
    QString displayName;
    QVector<ExportMatrixVariant> variants;
    QVector<ExportPresetTarget> presets;
    QVector<ExportMatrixCellRule> rules;

    QJsonObject toJson() const;
    static ExportMatrix fromJson(const QJsonObject& obj);
};

struct ResolvedExportMatrixCell {
    QString variantId;
    QString presetId;
    QString exportName;
    QString outputPath;
    QString format;
    QString codec;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    int bitrateKbps = 0;
    bool enabled = true;

    QJsonObject toJson() const;
};

ExportMatrixCellRule findRule(const ExportMatrix& matrix, const QString& variantId, const QString& presetId);
ResolvedExportMatrixCell resolveExportMatrixCell(const ExportMatrix& matrix,
                                                 const QString& variantId,
                                                 const QString& presetId,
                                                 const QString& baseOutputPath = QString());

} // namespace ArtifactCore

Q_DECLARE_METATYPE(ArtifactCore::ExportPresetTarget)
Q_DECLARE_METATYPE(ArtifactCore::ExportMatrixVariant)
Q_DECLARE_METATYPE(ArtifactCore::ExportMatrixCellRule)
Q_DECLARE_METATYPE(ArtifactCore::ExportMatrix)
Q_DECLARE_METATYPE(ArtifactCore::ResolvedExportMatrixCell)
