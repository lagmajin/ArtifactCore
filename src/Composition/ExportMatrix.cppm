module;
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>

module Composition.ExportMatrix;

namespace ArtifactCore {

QJsonObject ExportPresetTarget::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["name"] = name;
    obj["kind"] = static_cast<int>(kind);
    obj["format"] = format;
    obj["codec"] = codec;
    obj["width"] = width;
    obj["height"] = height;
    obj["fps"] = fps;
    obj["bitrateKbps"] = bitrateKbps;
    obj["outputExtension"] = outputExtension;
    obj["namingRule"] = namingRule;
    obj["enabled"] = enabled;
    return obj;
}

ExportPresetTarget ExportPresetTarget::fromJson(const QJsonObject& obj) {
    ExportPresetTarget target;
    target.id = obj.value("id").toString();
    target.name = obj.value("name").toString(target.id);
    target.kind = static_cast<ExportTargetKind>(obj.value("kind").toInt(static_cast<int>(ExportTargetKind::Custom)));
    target.format = obj.value("format").toString();
    target.codec = obj.value("codec").toString();
    target.width = obj.value("width").toInt();
    target.height = obj.value("height").toInt();
    target.fps = obj.value("fps").toDouble();
    target.bitrateKbps = obj.value("bitrateKbps").toInt();
    target.outputExtension = obj.value("outputExtension").toString();
    target.namingRule = obj.value("namingRule").toString();
    target.enabled = obj.value("enabled").toBool(true);
    return target;
}

QJsonObject ExportMatrixVariant::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["displayName"] = displayName;
    obj["description"] = description;
    obj["enabled"] = enabled;
    obj["parameters"] = parameters;
    return obj;
}

ExportMatrixVariant ExportMatrixVariant::fromJson(const QJsonObject& obj) {
    ExportMatrixVariant variant;
    variant.id = obj.value("id").toString();
    variant.displayName = obj.value("displayName").toString(variant.id);
    variant.description = obj.value("description").toString();
    variant.enabled = obj.value("enabled").toBool(true);
    variant.parameters = obj.value("parameters").toObject();
    return variant;
}

QJsonObject ExportMatrixCellRule::toJson() const {
    QJsonObject obj;
    obj["variantId"] = variantId;
    obj["presetId"] = presetId;
    obj["enabled"] = enabled;
    obj["overrides"] = overrides;
    return obj;
}

ExportMatrixCellRule ExportMatrixCellRule::fromJson(const QJsonObject& obj) {
    ExportMatrixCellRule rule;
    rule.variantId = obj.value("variantId").toString();
    rule.presetId = obj.value("presetId").toString();
    rule.enabled = obj.value("enabled").toBool(true);
    rule.overrides = obj.value("overrides").toObject();
    return rule;
}

QJsonObject ExportMatrix::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["displayName"] = displayName;
    QJsonArray variantsJson;
    for (const auto& variant : variants) variantsJson.append(variant.toJson());
    obj["variants"] = variantsJson;
    QJsonArray presetsJson;
    for (const auto& preset : presets) presetsJson.append(preset.toJson());
    obj["presets"] = presetsJson;
    QJsonArray rulesJson;
    for (const auto& rule : rules) rulesJson.append(rule.toJson());
    obj["rules"] = rulesJson;
    return obj;
}

ExportMatrix ExportMatrix::fromJson(const QJsonObject& obj) {
    ExportMatrix matrix;
    matrix.id = obj.value("id").toString();
    matrix.displayName = obj.value("displayName").toString(matrix.id);
    for (const auto& value : obj.value("variants").toArray()) {
        if (value.isObject()) matrix.variants.append(ExportMatrixVariant::fromJson(value.toObject()));
    }
    for (const auto& value : obj.value("presets").toArray()) {
        if (value.isObject()) matrix.presets.append(ExportPresetTarget::fromJson(value.toObject()));
    }
    for (const auto& value : obj.value("rules").toArray()) {
        if (value.isObject()) matrix.rules.append(ExportMatrixCellRule::fromJson(value.toObject()));
    }
    return matrix;
}

QJsonObject ResolvedExportMatrixCell::toJson() const {
    QJsonObject obj;
    obj["variantId"] = variantId;
    obj["presetId"] = presetId;
    obj["exportName"] = exportName;
    obj["outputPath"] = outputPath;
    obj["format"] = format;
    obj["codec"] = codec;
    obj["width"] = width;
    obj["height"] = height;
    obj["fps"] = fps;
    obj["bitrateKbps"] = bitrateKbps;
    obj["enabled"] = enabled;
    return obj;
}

ExportMatrixCellRule findRule(const ExportMatrix& matrix, const QString& variantId, const QString& presetId) {
    for (const auto& rule : matrix.rules) {
        if (rule.variantId == variantId && rule.presetId == presetId) {
            return rule;
        }
    }
    return {};
}

ResolvedExportMatrixCell resolveExportMatrixCell(const ExportMatrix& matrix,
                                                 const QString& variantId,
                                                 const QString& presetId,
                                                 const QString& baseOutputPath) {
    const ExportPresetTarget* preset = nullptr;
    for (const auto& p : matrix.presets) {
        if (p.id == presetId) {
            preset = &p;
            break;
        }
    }
    const ExportMatrixVariant* variant = nullptr;
    for (const auto& v : matrix.variants) {
        if (v.id == variantId) {
            variant = &v;
            break;
        }
    }
    const ExportMatrixCellRule rule = findRule(matrix, variantId, presetId);
    ResolvedExportMatrixCell cell;
    cell.variantId = variantId;
    cell.presetId = presetId;
    cell.enabled = (variant ? variant->enabled : true) && (preset ? preset->enabled : true) && rule.enabled;
    cell.format = rule.overrides.value("format").toString(preset ? preset->format : QString());
    cell.codec = rule.overrides.value("codec").toString(preset ? preset->codec : QString());
    cell.width = rule.overrides.value("width").toInt(preset ? preset->width : 0);
    cell.height = rule.overrides.value("height").toInt(preset ? preset->height : 0);
    cell.fps = rule.overrides.value("fps").toDouble(preset ? preset->fps : 0.0);
    cell.bitrateKbps = rule.overrides.value("bitrateKbps").toInt(preset ? preset->bitrateKbps : 0);

    QString exportName = rule.overrides.value("exportName").toString();
    if (exportName.isEmpty()) {
        exportName = (variant ? variant->displayName : variantId) + QStringLiteral("_") + (preset ? preset->name : presetId);
    }
    cell.exportName = exportName;

    QString outputPath = rule.overrides.value("outputPath").toString(baseOutputPath);
    if (outputPath.isEmpty()) outputPath = baseOutputPath;
    cell.outputPath = outputPath;
    return cell;
}

} // namespace ArtifactCore
