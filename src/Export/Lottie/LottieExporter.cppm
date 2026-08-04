module;
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImageReader>
#include <QSaveFile>
#include <QString>
#include <algorithm>
#include <cmath>
#include <unordered_map>

module Export.Lottie.Exporter;

namespace ArtifactCore::Export::Lottie {
namespace {

bool normalizedColor(const std::vector<double>& values);

QJsonArray numbers(const std::vector<double>& values) {
    QJsonArray result;
    for (const double value : values) result.append(value);
    return result;
}

QJsonObject keyframe(const LottieKeyframe& value) {
    QJsonObject result;
    result.insert(QStringLiteral("t"), value.t);
    result.insert(QStringLiteral("s"), numbers(value.s));
    result.insert(QStringLiteral("e"), numbers(value.e));
    result.insert(QStringLiteral("h"), value.hold != 0 ? 1 : 0);
    if (value.hold == 0) {
        QJsonObject outTangent;
        outTangent.insert(QStringLiteral("x"), value.bezierX1);
        outTangent.insert(QStringLiteral("y"), value.bezierY1);
        QJsonObject inTangent;
        inTangent.insert(QStringLiteral("x"), value.bezierX2);
        inTangent.insert(QStringLiteral("y"), value.bezierY2);
        result.insert(QStringLiteral("o"), outTangent);
        result.insert(QStringLiteral("i"), inTangent);
    }
    return result;
}

QJsonValue animatedValue(const std::vector<double>& values,
                         const std::vector<LottieKeyframe>& keyframes) {
    if (keyframes.empty()) return numbers(values);
    QJsonArray result;
    for (const auto& item : keyframes) result.append(keyframe(item));
    return result;
}

QJsonObject point(const LottiePoint& value) {
    QJsonObject result;
    result.insert(QStringLiteral("a"), value.keyframes.empty() ? 0 : 1);
    result.insert(QStringLiteral("k"), animatedValue(value.k, value.keyframes));
    return result;
}

QJsonObject color(const LottieColor& value) {
    QJsonObject result;
    result.insert(QStringLiteral("a"), value.keyframes.empty() ? 0 : 1);
    result.insert(QStringLiteral("k"), animatedValue(value.k, value.keyframes));
    return result;
}

QJsonObject gradientColors(const LottieColor& value) {
    QJsonObject result;
    const int componentCount = static_cast<int>(value.k.size());
    result.insert(QStringLiteral("p"), componentCount > 0 ? componentCount / 4 : 0);
    result.insert(QStringLiteral("k"), animatedValue(value.k, value.keyframes));
    return result;
}

QJsonArray dashEntries(const std::vector<double>& pattern) {
    QJsonArray result;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        QJsonObject entry;
        entry.insert(QStringLiteral("n"), i % 2 == 0 ? QStringLiteral("d")
                                                     : QStringLiteral("g"));
        QJsonObject value;
        value.insert(QStringLiteral("a"), 0);
        value.insert(QStringLiteral("k"), pattern[i]);
        entry.insert(QStringLiteral("v"), value);
        result.append(entry);
    }
    return result;
}

QJsonObject layer(const LottieLayer& value);

QJsonObject shape(const std::any& item) {
    if (const auto* rect = std::any_cast<LottieShapeRect>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(rect->ty));
        result.insert(QStringLiteral("d"), rect->direction);
        result.insert(QStringLiteral("p"), point(rect->p));
        result.insert(QStringLiteral("s"), point(rect->s));
        result.insert(QStringLiteral("r"), point(rect->r));
        return result;
    }
    if (const auto* ellipse = std::any_cast<LottieShapeEllipse>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(ellipse->ty));
        result.insert(QStringLiteral("d"), ellipse->direction);
        result.insert(QStringLiteral("p"), point(ellipse->p));
        result.insert(QStringLiteral("s"), point(ellipse->s));
        return result;
    }
    if (const auto* star = std::any_cast<LottieShapeStar>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(star->ty));
        result.insert(QStringLiteral("sy"), star->starType);
        result.insert(QStringLiteral("d"), star->direction);
        result.insert(QStringLiteral("p"), point(star->position));
        result.insert(QStringLiteral("pt"), point(star->points));
        result.insert(QStringLiteral("r"), point(star->rotation));
        result.insert(QStringLiteral("or"), point(star->outerRadius));
        result.insert(QStringLiteral("ir"), point(star->innerRadius));
        return result;
    }
    if (const auto* transform = std::any_cast<LottieShapeTransform>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(transform->ty));
        result.insert(QStringLiteral("a"), point(transform->anchor));
        result.insert(QStringLiteral("p"), point(transform->position));
        result.insert(QStringLiteral("s"), point(transform->scale));
        result.insert(QStringLiteral("r"), point(transform->rotation));
        result.insert(QStringLiteral("o"), point(transform->opacity));
        return result;
    }
    if (const auto* trim = std::any_cast<LottieShapeTrim>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(trim->ty));
        result.insert(QStringLiteral("m"), trim->mode);
        result.insert(QStringLiteral("s"), point(trim->start));
        result.insert(QStringLiteral("e"), point(trim->end));
        result.insert(QStringLiteral("o"), point(trim->offset));
        return result;
    }
    if (const auto* repeater = std::any_cast<LottieShapeRepeater>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(repeater->ty));
        result.insert(QStringLiteral("c"), point(repeater->copies));
        result.insert(QStringLiteral("o"), point(repeater->offset));
        QJsonObject transform;
        transform.insert(QStringLiteral("a"), point(repeater->transform.anchor));
        transform.insert(QStringLiteral("p"), point(repeater->transform.position));
        transform.insert(QStringLiteral("s"), point(repeater->transform.scale));
        transform.insert(QStringLiteral("r"), point(repeater->transform.rotation));
        transform.insert(QStringLiteral("o"), point(repeater->transform.opacity));
        result.insert(QStringLiteral("tr"), transform);
        result.insert(QStringLiteral("so"), repeater->startOpacity);
        result.insert(QStringLiteral("eo"), repeater->endOpacity);
        return result;
    }
    if (const auto* merge = std::any_cast<LottieShapeMergePaths>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(merge->ty));
        result.insert(QStringLiteral("mm"), merge->mode);
        result.insert(QStringLiteral("hd"), false);
        result.insert(QStringLiteral("d"), merge->direction);
        return result;
    }
    if (const auto* fill = std::any_cast<LottieShapeFill>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(fill->ty));
        result.insert(QStringLiteral("c"), color(fill->c));
        result.insert(QStringLiteral("o"), point(fill->o));
        result.insert(QStringLiteral("bm"), fill->blendMode);
        result.insert(QStringLiteral("hd"), fill->enabled == 0);
        return result;
    }
    if (const auto* gradient = std::any_cast<LottieShapeGradient>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(gradient->ty));
        result.insert(QStringLiteral("t"), gradient->gradientType);
        result.insert(QStringLiteral("g"), gradientColors(gradient->colors));
        result.insert(QStringLiteral("o"), point(gradient->opacity));
        result.insert(QStringLiteral("s"), point(gradient->startPoint));
        result.insert(QStringLiteral("e"), point(gradient->endPoint));
        result.insert(QStringLiteral("bm"), gradient->blendMode);
        result.insert(QStringLiteral("hd"), gradient->enabled == 0);
        return result;
    }
    if (const auto* stroke = std::any_cast<LottieShapeStroke>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(stroke->ty));
        result.insert(QStringLiteral("c"), color(stroke->c));
        result.insert(QStringLiteral("o"), point(stroke->o));
        result.insert(QStringLiteral("w"), point(stroke->w));
        result.insert(QStringLiteral("bm"), stroke->blendMode);
        result.insert(QStringLiteral("hd"), stroke->enabled == 0);
        result.insert(QStringLiteral("lc"), stroke->lineCap);
        result.insert(QStringLiteral("lj"), stroke->lineJoin);
        result.insert(QStringLiteral("ml"), stroke->miterLimit);
        if (!stroke->dashPattern.empty())
            result.insert(QStringLiteral("d"), dashEntries(stroke->dashPattern));
        return result;
    }
    if (const auto* gradient = std::any_cast<LottieShapeGradientStroke>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(gradient->ty));
        result.insert(QStringLiteral("t"), gradient->gradientType);
        result.insert(QStringLiteral("g"), gradientColors(gradient->colors));
        result.insert(QStringLiteral("o"), point(gradient->opacity));
        result.insert(QStringLiteral("w"), point(gradient->width));
        result.insert(QStringLiteral("s"), point(gradient->startPoint));
        result.insert(QStringLiteral("e"), point(gradient->endPoint));
        result.insert(QStringLiteral("bm"), gradient->blendMode);
        result.insert(QStringLiteral("hd"), gradient->enabled == 0);
        return result;
    }
    if (const auto* path = std::any_cast<LottieShapePath>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(path->ty));
        QJsonObject pathValue;
        pathValue.insert(QStringLiteral("i"), numbers(path->inTangents));
        pathValue.insert(QStringLiteral("o"), numbers(path->outTangents));
        pathValue.insert(QStringLiteral("v"), numbers(path->vertices));
        pathValue.insert(QStringLiteral("c"), path->closed);
        QJsonObject pathProperty;
        pathProperty.insert(QStringLiteral("a"), 0);
        pathProperty.insert(QStringLiteral("k"), pathValue);
        result.insert(QStringLiteral("ks"), pathProperty);
        result.insert(QStringLiteral("d"), path->direction);
        return result;
    }
    if (const auto* group = std::any_cast<LottieShapeGroup>(&item)) {
        QJsonArray items;
        for (const auto& child : group->items) items.append(shape(child));
        QJsonObject result;
        result.insert(QStringLiteral("ty"), QString::fromStdString(group->ty));
        result.insert(QStringLiteral("it"), items);
        return result;
    }
    return {};
}

QJsonObject asset(const std::any& item) {
    if (const auto* image = std::any_cast<LottieImageAsset>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("id"), QString::fromStdString(image->id));
        result.insert(QStringLiteral("w"), image->width);
        result.insert(QStringLiteral("h"), image->height);
        result.insert(QStringLiteral("u"), QString::fromStdString(image->directory));
        result.insert(QStringLiteral("p"), QString::fromStdString(image->fileName));
        if (image->embedded && !image->embeddedData.empty()) {
            result.insert(QStringLiteral("e"), 1);
            result.insert(QStringLiteral("p"), QString::fromStdString(image->embeddedData));
        }
        return result;
    }
    if (const auto* precomp = std::any_cast<LottiePrecompAsset>(&item)) {
        QJsonObject result;
        result.insert(QStringLiteral("id"), QString::fromStdString(precomp->id));
        result.insert(QStringLiteral("w"), precomp->width);
        result.insert(QStringLiteral("h"), precomp->height);
        QJsonArray layers;
        for (const auto& layerValue : precomp->layers) layers.append(layer(layerValue));
        result.insert(QStringLiteral("layers"), layers);
        return result;
    }
    return {};
}

QJsonObject layer(const LottieLayer& value) {
    QJsonObject result;
    result.insert(QStringLiteral("ddd"), value.ddd);
    result.insert(QStringLiteral("ind"), value.ind);
    result.insert(QStringLiteral("ty"), value.ty);
    result.insert(QStringLiteral("nm"), QString::fromStdString(value.nm));
    if (!value.refId.empty())
        result.insert(QStringLiteral("refId"), QString::fromStdString(value.refId));
    result.insert(QStringLiteral("ip"), value.ip);
    result.insert(QStringLiteral("op"), value.op);
    result.insert(QStringLiteral("parent"), value.parent);
    result.insert(QStringLiteral("hd"), value.hidden != 0);
    result.insert(QStringLiteral("ao"), value.autoOrient ? 1 : 0);
    result.insert(QStringLiteral("sr"), value.stretch);
    result.insert(QStringLiteral("st"), value.startTime);
    if (value.matteType != 0)
        result.insert(QStringLiteral("tt"), value.matteType);
    if (value.matteTarget != 0)
        result.insert(QStringLiteral("tp"), value.matteTarget);
    result.insert(QStringLiteral("bm"), value.blendMode);
    if (value.ty == 1) {
        result.insert(QStringLiteral("sw"), value.solidWidth);
        result.insert(QStringLiteral("sh"), value.solidHeight);
        const auto channel = [](double component) {
            return static_cast<int>(std::lround(
                std::clamp(component, 0.0, 1.0) * 255.0));
        };
        const auto componentAt = [&value](std::size_t index) {
            return index < value.solidColor.size() ? value.solidColor[index] : 0.0;
        };
        const QString color = QStringLiteral("#%1%2%3")
            .arg(channel(componentAt(0)), 2, 16, QLatin1Char('0'))
            .arg(channel(componentAt(1)), 2, 16, QLatin1Char('0'))
            .arg(channel(componentAt(2)), 2, 16, QLatin1Char('0'))
            .toUpper();
        result.insert(QStringLiteral("sc"), color);
    }
    if (value.ty == 5) {
        QJsonObject textDocument;
        textDocument.insert(QStringLiteral("t"),
                            QString::fromStdString(value.text));
        textDocument.insert(QStringLiteral("f"),
                            QString::fromStdString(value.textFont));
        textDocument.insert(QStringLiteral("s"), value.textFontSize);
        QJsonArray color;
        for (const double component : value.textColor) color.append(component);
        textDocument.insert(QStringLiteral("fc"), color);
        textDocument.insert(QStringLiteral("j"), 0);
        textDocument.insert(QStringLiteral("tr"), 0);
        QJsonObject keyframe;
        keyframe.insert(QStringLiteral("t"), 0);
        keyframe.insert(QStringLiteral("s"), textDocument);
        QJsonArray keyframes;
        keyframes.append(keyframe);
        QJsonObject documentData;
        documentData.insert(QStringLiteral("k"), keyframes);
        QJsonObject textData;
        textData.insert(QStringLiteral("d"), documentData);
        result.insert(QStringLiteral("t"), textData);
    }

    QJsonObject transform;
    transform.insert(QStringLiteral("a"), point(value.anchor));
    transform.insert(QStringLiteral("p"), point(value.position));
    transform.insert(QStringLiteral("s"), point(value.scale));
    transform.insert(QStringLiteral("r"), point(value.rotation));
    transform.insert(QStringLiteral("o"), point(value.opacity));
    result.insert(QStringLiteral("ks"), transform);

    if (!value.shapes.empty()) {
        QJsonArray shapes;
        for (const auto& item : value.shapes) shapes.append(shape(item));
        result.insert(QStringLiteral("shapes"), shapes);
    }
    return result;
}

bool finiteValues(const std::vector<double>& values) {
    return std::all_of(values.begin(), values.end(),
                       [](double value) { return std::isfinite(value); });
}

bool normalizedColor(const std::vector<double>& values) {
    return (values.size() == 3 || values.size() == 4) &&
           std::all_of(values.begin(), values.end(), [](double value) {
               return std::isfinite(value) && value >= 0.0 && value <= 1.0;
           });
}

bool validPoint(const LottiePoint& value) {
    if (!finiteValues(value.k)) return false;
    for (const auto& frame : value.keyframes) {
        if (!finiteValues(frame.s) || !finiteValues(frame.e) ||
            !std::isfinite(frame.t) || !std::isfinite(frame.bezierX1) ||
            !std::isfinite(frame.bezierY1) || !std::isfinite(frame.bezierX2) ||
            !std::isfinite(frame.bezierY2)) return false;
    }
    return true;
}

bool validColor(const LottieColor& value) {
    if (!finiteValues(value.k)) return false;
    for (const auto& frame : value.keyframes) {
        if (!finiteValues(frame.s) || !finiteValues(frame.e) ||
            !std::isfinite(frame.t) || !std::isfinite(frame.bezierX1) ||
            !std::isfinite(frame.bezierY1) || !std::isfinite(frame.bezierX2) ||
            !std::isfinite(frame.bezierY2)) return false;
    }
    return true;
}

bool validShape(const std::any& item) {
    if (const auto* rect = std::any_cast<LottieShapeRect>(&item)) {
        return validPoint(rect->p) && validPoint(rect->s) && validPoint(rect->r);
    }
    if (const auto* ellipse = std::any_cast<LottieShapeEllipse>(&item)) {
        return validPoint(ellipse->p) && validPoint(ellipse->s);
    }
    if (const auto* star = std::any_cast<LottieShapeStar>(&item)) {
        return (star->starType == 1 || star->starType == 2) &&
               (star->direction == 1 || star->direction == -1) &&
               validPoint(star->position) && validPoint(star->points) &&
               validPoint(star->rotation) && validPoint(star->outerRadius) &&
               validPoint(star->innerRadius);
    }
    if (const auto* transform = std::any_cast<LottieShapeTransform>(&item)) {
        return validPoint(transform->anchor) && validPoint(transform->position) &&
               validPoint(transform->scale) && validPoint(transform->rotation) &&
               validPoint(transform->opacity);
    }
    if (const auto* trim = std::any_cast<LottieShapeTrim>(&item)) {
        return (trim->mode == 1 || trim->mode == 2) &&
               validPoint(trim->start) && validPoint(trim->end) &&
               validPoint(trim->offset);
    }
    if (const auto* repeater = std::any_cast<LottieShapeRepeater>(&item)) {
        return validPoint(repeater->copies) && validPoint(repeater->offset) &&
               validPoint(repeater->transform.anchor) &&
               validPoint(repeater->transform.position) &&
               validPoint(repeater->transform.scale) &&
               validPoint(repeater->transform.rotation) &&
               validPoint(repeater->transform.opacity) &&
               repeater->startOpacity >= 0 && repeater->startOpacity <= 100 &&
               repeater->endOpacity >= 0 && repeater->endOpacity <= 100;
    }
    if (const auto* merge = std::any_cast<LottieShapeMergePaths>(&item)) {
        return merge->mode >= 1 && merge->mode <= 5 &&
               (merge->direction == 1 || merge->direction == -1);
    }
    if (const auto* fill = std::any_cast<LottieShapeFill>(&item)) {
        return validPoint(fill->c) && validPoint(fill->o);
    }
    if (const auto* gradient = std::any_cast<LottieShapeGradient>(&item)) {
        return (gradient->gradientType == 1 || gradient->gradientType == 2) &&
               gradient->blendMode >= 0 && gradient->blendMode <= 16 &&
               validColor(gradient->colors) && validPoint(gradient->opacity) &&
               validPoint(gradient->startPoint) && validPoint(gradient->endPoint);
    }
    if (const auto* stroke = std::any_cast<LottieShapeStroke>(&item)) {
        return validPoint(stroke->c) && validPoint(stroke->o) && validPoint(stroke->w) &&
               (stroke->lineCap >= 1 && stroke->lineCap <= 3) &&
               (stroke->lineJoin >= 1 && stroke->lineJoin <= 3) &&
               std::isfinite(stroke->miterLimit) && stroke->miterLimit > 0.0 &&
               stroke->dashPattern.size() % 2 == 0 &&
               std::all_of(stroke->dashPattern.begin(), stroke->dashPattern.end(),
                           [](double value) { return std::isfinite(value) && value >= 0.0; });
    }
    if (const auto* gradient = std::any_cast<LottieShapeGradientStroke>(&item)) {
        return (gradient->gradientType == 1 || gradient->gradientType == 2) &&
               gradient->blendMode >= 0 && gradient->blendMode <= 16 &&
               validColor(gradient->colors) && validPoint(gradient->opacity) &&
               validPoint(gradient->width) && validPoint(gradient->startPoint) &&
               validPoint(gradient->endPoint);
    }
    if (const auto* path = std::any_cast<LottieShapePath>(&item)) {
        return finiteValues(path->vertices) && finiteValues(path->inTangents) &&
               finiteValues(path->outTangents) && path->vertices.size() % 2 == 0 &&
               path->inTangents.size() == path->vertices.size() &&
               path->outTangents.size() == path->vertices.size();
    }
    if (const auto* group = std::any_cast<LottieShapeGroup>(&item)) {
        return std::all_of(group->items.begin(), group->items.end(), validShape);
    }
    return false;
}

bool validAsset(const std::any& item) {
    if (const auto* image = std::any_cast<LottieImageAsset>(&item)) {
        if (image->id.empty() || image->width <= 0 || image->height <= 0) return false;
        if (image->embedded && image->embeddedData.empty()) return false;
        return image->embedded || !image->fileName.empty();
    }
    if (const auto* precomp = std::any_cast<LottiePrecompAsset>(&item)) {
        if (precomp->id.empty() || precomp->width <= 0 || precomp->height <= 0) return false;
        return std::all_of(precomp->layers.begin(), precomp->layers.end(),
                           [](const LottieLayer& layerValue) {
                               return layerValue.ind > 0 && layerValue.op > layerValue.ip;
                           });
    }
    return false;
}

} // namespace

std::optional<LottieImageAsset> LottieExporter::makeEmbeddedImageAsset(
    const QString& imagePath, const QString& assetId) {
    const QString path = imagePath.trimmed();
    const QString id = assetId.trimmed();
    if (path.isEmpty() || id.isEmpty()) return std::nullopt;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) return std::nullopt;

    const QImageReader reader(path);
    const QSize size = reader.size();
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0) return std::nullopt;
    QString mime = QStringLiteral("application/octet-stream");
    const QByteArray format = reader.format().toLower();
    if (format == "png") mime = QStringLiteral("image/png");
    else if (format == "jpg" || format == "jpeg") mime = QStringLiteral("image/jpeg");
    else if (format == "webp") mime = QStringLiteral("image/webp");
    else if (format == "gif") mime = QStringLiteral("image/gif");

    LottieImageAsset asset;
    asset.id = id.toStdString();
    const QFileInfo fileInfo(path);
    asset.fileName = fileInfo.fileName().toStdString();
    asset.directory = fileInfo.path().toStdString();
    asset.width = size.width();
    asset.height = size.height();
    asset.embedded = true;
    asset.embeddedData = QStringLiteral("data:%1;base64,%2")
        .arg(mime, QString::fromLatin1(bytes.toBase64())).toStdString();
    return asset;
}

void LottieExporter::compressKeyframes(std::vector<LottieKeyframe>& keyframes,
                                        double tolerance) {
    if (keyframes.size() < 3 || !std::isfinite(tolerance) || tolerance < 0.0) return;
    tolerance = std::max(tolerance, 0.0);
    std::vector<LottieKeyframe> compressed;
    compressed.reserve(keyframes.size());
    compressed.push_back(keyframes.front());
    for (std::size_t i = 1; i + 1 < keyframes.size(); ++i) {
        const auto& previous = compressed.back();
        const auto& current = keyframes[i];
        const auto& next = keyframes[i + 1];
        const double span = next.t - previous.t;
        bool removable = current.hold == 0 && previous.hold == 0 &&
                         next.hold == 0 && std::isfinite(span) && span > 0.0 &&
                         current.s.size() == previous.s.size() &&
                         current.s.size() == next.s.size();
        if (removable) {
            const double factor = (current.t - previous.t) / span;
            removable = std::isfinite(factor) && factor >= 0.0 && factor <= 1.0;
            if (removable) {
                for (std::size_t component = 0; component < current.s.size(); ++component) {
                    const double expected = previous.s[component] +
                        (next.s[component] - previous.s[component]) * factor;
                    if (!std::isfinite(expected) ||
                        std::abs(current.s[component] - expected) > tolerance) {
                        removable = false;
                        break;
                    }
                }
            }
        }
        if (!removable) compressed.push_back(current);
    }
    compressed.push_back(keyframes.back());
    keyframes.swap(compressed);
}

bool LottieExporter::validate(const LottieDocument& document, QString* errorMessage) {
    const auto fail = [errorMessage](const QString& message) {
        if (errorMessage) *errorMessage = message;
        return false;
    };
    if (!std::isfinite(document.fr) || document.fr <= 0.0 || document.fr > 1000.0) {
        return fail(QStringLiteral("Lottie frame rate must be finite and in (0, 1000]."));
    }
    if (document.w <= 0 || document.h <= 0 || document.w > 100000 || document.h > 100000) {
        return fail(QStringLiteral("Lottie canvas dimensions are invalid."));
    }
    if (document.ip < 0 || document.op <= document.ip) {
        return fail(QStringLiteral("Lottie in/out frame range is invalid."));
    }
    std::unordered_map<int, const LottieLayer*> layersByIndex;
    std::unordered_map<std::string, bool> assetIds;
    for (const auto& assetValue : document.assets) {
        if (const auto* image = std::any_cast<LottieImageAsset>(&assetValue)) {
            if (!assetIds.emplace(image->id, true).second)
                return fail(QStringLiteral("Lottie asset IDs must be unique."));
        } else if (const auto* precomp = std::any_cast<LottiePrecompAsset>(&assetValue)) {
            if (!assetIds.emplace(precomp->id, true).second)
                return fail(QStringLiteral("Lottie asset IDs must be unique."));
        }
    }
    for (const auto& assetValue : document.assets) {
        const auto* precomp = std::any_cast<LottiePrecompAsset>(&assetValue);
        if (!precomp) continue;
        std::unordered_map<int, bool> nestedIndices;
        for (const auto& nestedLayer : precomp->layers) {
            if (nestedLayer.ind <= 0 ||
                !nestedIndices.emplace(nestedLayer.ind, true).second ||
                nestedLayer.ty < 0 || nestedLayer.ty > 5 ||
                nestedLayer.ip < 0 || nestedLayer.op <= nestedLayer.ip ||
                nestedLayer.stretch <= 0.0 || !std::isfinite(nestedLayer.stretch) ||
                !std::isfinite(nestedLayer.startTime)) {
                return fail(QStringLiteral("Lottie precomp layer metadata is invalid."));
            }
            if ((nestedLayer.ty == 0 || nestedLayer.ty == 2) &&
                (nestedLayer.refId.empty() || !assetIds.contains(nestedLayer.refId))) {
                return fail(QStringLiteral("Lottie precomp asset reference is invalid."));
            }
            if (nestedLayer.ty == 1 &&
                (nestedLayer.solidWidth <= 0 || nestedLayer.solidHeight <= 0 ||
                 !normalizedColor(nestedLayer.solidColor))) {
                return fail(QStringLiteral("Lottie precomp solid payload is invalid."));
            }
            if (nestedLayer.ty == 5 &&
                (nestedLayer.text.empty() || nestedLayer.textFont.empty() ||
                 !std::isfinite(nestedLayer.textFontSize) ||
                 nestedLayer.textFontSize <= 0.0 ||
                 !normalizedColor(nestedLayer.textColor))) {
                return fail(QStringLiteral("Lottie precomp text payload is invalid."));
            }
        }
        for (const auto& nestedLayer : precomp->layers) {
            if (nestedLayer.parent == nestedLayer.ind ||
                (nestedLayer.parent != 0 &&
                 !nestedIndices.contains(nestedLayer.parent)) ||
                nestedLayer.matteType < 0 || nestedLayer.matteType > 8 ||
                nestedLayer.blendMode < 0 || nestedLayer.blendMode > 16 ||
                nestedLayer.matteTarget == nestedLayer.ind ||
                (nestedLayer.matteTarget != 0 &&
                 !nestedIndices.contains(nestedLayer.matteTarget))) {
                return fail(QStringLiteral("Lottie precomp layer reference is invalid."));
            }
            std::unordered_map<int, bool> visited;
            const LottieLayer* ancestor = &nestedLayer;
            while (ancestor && ancestor->parent != 0) {
                if (visited[ancestor->ind]) {
                    return fail(QStringLiteral("Lottie precomp parent graph contains a cycle."));
                }
                visited[ancestor->ind] = true;
                const auto parentIt = std::find_if(
                    precomp->layers.begin(), precomp->layers.end(),
                    [ancestor](const LottieLayer& candidate) {
                        return candidate.ind == ancestor->parent;
                    });
                ancestor = parentIt == precomp->layers.end() ? nullptr : &*parentIt;
            }
        }
    }
    for (const auto& item : document.layers) {
        if (item.ind <= 0 || !layersByIndex.emplace(item.ind, &item).second) {
            return fail(QStringLiteral("Lottie layer indices must be unique and positive."));
        }
    }
    for (const auto& item : document.layers) {
        if (item.ind <= 0 || item.ip < document.ip || item.op > document.op || item.op <= item.ip ||
            item.ty < 0 || item.ty > 5 || (item.ddd != 0 && item.ddd != 1)) {
            return fail(QStringLiteral("Lottie layer frame range is invalid."));
        }
        if (!std::isfinite(item.stretch) || item.stretch <= 0.0 ||
            !std::isfinite(item.startTime)) {
            return fail(QStringLiteral("Lottie layer time mapping is invalid."));
        }
        if (item.ty == 1 &&
            (item.solidWidth <= 0 || item.solidHeight <= 0 ||
             !normalizedColor(item.solidColor))) {
            return fail(QStringLiteral("Lottie solid layer payload is invalid."));
        }
        if (item.ty == 5 &&
            (item.text.empty() || item.textFont.empty() ||
             !std::isfinite(item.textFontSize) || item.textFontSize <= 0.0 ||
             !normalizedColor(item.textColor))) {
            return fail(QStringLiteral("Lottie text layer payload is invalid."));
        }
        if (item.parent == item.ind ||
            (item.parent != 0 && !layersByIndex.contains(item.parent))) {
            return fail(QStringLiteral("Lottie layer parent reference is invalid."));
        }
        if (item.matteType < 0 || item.matteType > 8 || item.blendMode < 0 ||
            item.blendMode > 16 || item.matteTarget == item.ind ||
            (item.matteTarget != 0 && !layersByIndex.contains(item.matteTarget))) {
            return fail(QStringLiteral("Lottie track matte reference is invalid."));
        }
        if ((item.ty == 0 || item.ty == 2) &&
            (item.refId.empty() || !assetIds.contains(item.refId))) {
            return fail(QStringLiteral("Lottie layer asset reference is invalid."));
        }
        std::unordered_map<int, bool> visited;
        const LottieLayer* ancestor = &item;
        while (ancestor && ancestor->parent != 0) {
            if (visited[ancestor->ind]) {
                return fail(QStringLiteral("Lottie layer parent graph contains a cycle."));
            }
            visited[ancestor->ind] = true;
            const auto parentIt = layersByIndex.find(ancestor->parent);
            ancestor = parentIt == layersByIndex.end() ? nullptr : parentIt->second;
        }
        const auto validateKeyframes = [&fail, &item](const std::vector<LottieKeyframe>& keyframes) {
            double previousTime = -1.0;
            for (const auto& frame : keyframes) {
                if (!std::isfinite(frame.t) || frame.t < previousTime ||
                    frame.t < static_cast<double>(item.ip) ||
                    frame.t > static_cast<double>(item.op)) {
                    return fail(QStringLiteral("Lottie keyframe times must be finite and sorted."));
                }
                previousTime = frame.t;
                for (const double value : frame.s) {
                    if (!std::isfinite(value)) return fail(QStringLiteral("Lottie keyframe contains NaN or Inf."));
                }
                for (const double value : frame.e) {
                    if (!std::isfinite(value)) return fail(QStringLiteral("Lottie keyframe contains NaN or Inf."));
                }
            }
            return true;
        };
        if (!validateKeyframes(item.position.keyframes) ||
            !validateKeyframes(item.anchor.keyframes) ||
            !validateKeyframes(item.scale.keyframes) ||
            !validateKeyframes(item.rotation.keyframes) ||
            !validateKeyframes(item.opacity.keyframes)) {
            return false;
        }
        if (!validPoint(item.position) || !validPoint(item.anchor) ||
            !validPoint(item.scale) || !validPoint(item.rotation) ||
            !validPoint(item.opacity)) {
            return fail(QStringLiteral("Lottie layer contains an invalid transform."));
        }
        if (!std::all_of(item.shapes.begin(), item.shapes.end(), validShape)) {
            return fail(QStringLiteral("Lottie layer contains an invalid shape."));
        }
    }
    if (!std::all_of(document.assets.begin(), document.assets.end(), validAsset)) {
        return fail(QStringLiteral("Lottie document contains an invalid asset."));
    }
    if (errorMessage) errorMessage->clear();
    return true;
}

QString LottieExporter::toJson(const LottieDocument& document, bool prettyPrint) {
    QJsonObject root;
    root.insert(QStringLiteral("v"), QString::fromStdString(document.v));
    root.insert(QStringLiteral("fr"), document.fr);
    root.insert(QStringLiteral("ip"), document.ip);
    root.insert(QStringLiteral("op"), document.op);
    root.insert(QStringLiteral("w"), document.w);
    root.insert(QStringLiteral("h"), document.h);
    root.insert(QStringLiteral("nm"), QString::fromStdString(document.nm));
    root.insert(QStringLiteral("ddd"), 0);

    QJsonArray layers;
    for (const auto& item : document.layers) layers.append(layer(item));
    root.insert(QStringLiteral("layers"), layers);
    QJsonArray assets;
    for (const auto& item : document.assets) assets.append(asset(item));
    root.insert(QStringLiteral("assets"), assets);

    const auto format = prettyPrint ? QJsonDocument::Indented : QJsonDocument::Compact;
    return QString::fromUtf8(QJsonDocument(root).toJson(format));
}

bool LottieExporter::exportToFile(const LottieDocument& document,
                                  const QString& outputPath,
                                  const LottieExportOptions& options) {
    const QString path = outputPath.trimmed();
    if (path.isEmpty() || !std::isfinite(options.keyframeTolerance) ||
        options.keyframeTolerance < 0.0) return false;
    LottieDocument prepared = document;
    if (!options.embedImages) {
        for (auto& item : prepared.assets) {
            if (auto* image = std::any_cast<LottieImageAsset>(&item)) {
                if (image->embedded && image->fileName.empty()) return false;
                image->embedded = false;
                image->embeddedData.clear();
            }
        }
    }
    if (options.compressKeyframes) {
        for (auto& layerValue : prepared.layers) {
            compressKeyframes(layerValue.position.keyframes, options.keyframeTolerance);
            compressKeyframes(layerValue.anchor.keyframes, options.keyframeTolerance);
            compressKeyframes(layerValue.scale.keyframes, options.keyframeTolerance);
            compressKeyframes(layerValue.rotation.keyframes, options.keyframeTolerance);
            compressKeyframes(layerValue.opacity.keyframes, options.keyframeTolerance);
        }
    }
    if (!options.name.trimmed().isEmpty()) {
        prepared.nm = options.name.trimmed().toStdString();
    }
    if (!validate(prepared)) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray data = toJson(prepared, options.prettyPrint).toUtf8();
    if (file.write(data) != data.size()) return false;
    return file.commit();
}

namespace {

LottiePoint readPoint(const QJsonValue& value)
{
    LottiePoint result;
    if (value.isArray()) {
        for (const auto& item : value.toArray()) {
            if (item.isDouble()) result.k.push_back(item.toDouble());
        }
    } else if (value.isObject()) {
        const auto object = value.toObject();
        const auto key = object.value(QStringLiteral("k"));
        if (key.isDouble()) {
            result.k.push_back(key.toDouble());
        } else if (key.isArray()) {
            const auto values = key.toArray();
            const bool animated = object.value(QStringLiteral("a")).toInt() != 0;
            if (!animated) {
                for (const auto& item : values) {
                    if (item.isDouble()) result.k.push_back(item.toDouble());
                }
            } else {
                for (const auto& item : values) {
                    if (!item.isObject()) continue;
                    const auto keyframe = item.toObject();
                    LottieKeyframe frame;
                    frame.t = keyframe.value(QStringLiteral("t")).toDouble();
                    frame.hold = keyframe.value(QStringLiteral("h")).toInt(frame.hold);
                    const auto start = keyframe.value(QStringLiteral("s"));
                    if (start.isArray()) {
                        for (const auto& component : start.toArray()) {
                            if (component.isDouble()) frame.s.push_back(component.toDouble());
                        }
                    }
                    const auto end = keyframe.value(QStringLiteral("e"));
                    if (end.isArray()) {
                        for (const auto& component : end.toArray()) {
                            if (component.isDouble()) frame.e.push_back(component.toDouble());
                        }
                    }
                    const auto inHandle = keyframe.value(QStringLiteral("i")).toObject();
                    const auto outHandle = keyframe.value(QStringLiteral("o")).toObject();
                    const auto readHandle = [](const QJsonObject& handle,
                                               const QString& axis,
                                               double fallback) {
                        const auto value = handle.value(axis);
                        if (!value.isArray() || value.toArray().isEmpty()) return fallback;
                        return value.toArray().first().toDouble(fallback);
                    };
                    frame.bezierX1 = readHandle(inHandle, QStringLiteral("x"), frame.bezierX1);
                    frame.bezierY1 = readHandle(inHandle, QStringLiteral("y"), frame.bezierY1);
                    frame.bezierX2 = readHandle(outHandle, QStringLiteral("x"), frame.bezierX2);
                    frame.bezierY2 = readHandle(outHandle, QStringLiteral("y"), frame.bezierY2);
                    result.keyframes.push_back(std::move(frame));
                }
            }
        } else if (key.isObject()) {
            const auto components = key.toObject();
            const auto readComponent = [&components](const QString& name) {
                const auto component = components.value(name);
                return component.isDouble() ? component.toDouble() : 0.0;
            };
            result.k = {readComponent(QStringLiteral("x")),
                        readComponent(QStringLiteral("y"))};
        }
    }
    return result;
}

} // namespace

std::optional<LottieDocument> LottieExporter::importFromFile(
    const QString& inputPath, QString* errorMessage)
{
    const auto fail = [errorMessage](const QString& message)
        -> std::optional<LottieDocument> {
        if (errorMessage) *errorMessage = message;
        return std::nullopt;
    };
    QFile file(inputPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("Unable to open Lottie file."));
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        return fail(QStringLiteral("Invalid Lottie JSON: %1")
            .arg(parseError.errorString()));
    }

    const QJsonObject root = json.object();
    LottieDocument result;
    result.v = root.value(QStringLiteral("v")).toString().toStdString();
    result.nm = root.value(QStringLiteral("nm")).toString().toStdString();
    result.fr = root.value(QStringLiteral("fr")).toDouble(result.fr);
    result.ip = root.value(QStringLiteral("ip")).toInt(result.ip);
    result.op = root.value(QStringLiteral("op")).toInt(result.op);
    result.w = root.value(QStringLiteral("w")).toInt(result.w);
    result.h = root.value(QStringLiteral("h")).toInt(result.h);
    const auto layers = root.value(QStringLiteral("layers"));
    if (!layers.isArray()) {
        return fail(QStringLiteral("Lottie document has no layers array."));
    }

    int fallbackIndex = 1;
    for (const auto& value : layers.toArray()) {
        if (!value.isObject()) continue;
        const QJsonObject object = value.toObject();
        LottieLayer layer;
        layer.ddd = object.value(QStringLiteral("ddd")).toInt(layer.ddd);
        layer.ind = object.value(QStringLiteral("ind")).toInt(layer.ind);
        if (layer.ind <= 0) layer.ind = fallbackIndex;
        fallbackIndex = std::max(fallbackIndex, layer.ind + 1);
        layer.ty = object.value(QStringLiteral("ty")).toInt(layer.ty);
        layer.nm = object.value(QStringLiteral("nm")).toString().toStdString();
        layer.refId = object.value(QStringLiteral("refId")).toString().toStdString();
        layer.ip = object.value(QStringLiteral("ip")).toInt(layer.ip);
        layer.op = object.value(QStringLiteral("op")).toInt(layer.op);
        layer.parent = object.value(QStringLiteral("parent")).toInt(layer.parent);
        layer.hidden = object.value(QStringLiteral("hd")).toBool(layer.hidden != 0);
        layer.autoOrient = object.value(QStringLiteral("ao")).toInt() != 0;
        layer.stretch = object.value(QStringLiteral("sr")).toDouble(layer.stretch);
        layer.startTime = object.value(QStringLiteral("st")).toDouble(layer.startTime);
        layer.matteType = object.value(QStringLiteral("tt")).toInt(layer.matteType);
        layer.matteTarget = object.value(QStringLiteral("tp")).toInt(layer.matteTarget);
        layer.blendMode = object.value(QStringLiteral("bm")).toInt(layer.blendMode);
        if (layer.ty == 1) {
            layer.solidWidth = object.value(QStringLiteral("sw")).toInt();
            layer.solidHeight = object.value(QStringLiteral("sh")).toInt();
            const auto color = object.value(QStringLiteral("sc"));
            if (color.isString()) {
                const QString hex = color.toString().trimmed();
                if (hex.size() == 7 && hex.startsWith(QLatin1Char('#'))) {
                    bool ok = false;
                    const int rgb = hex.mid(1).toInt(&ok, 16);
                    if (ok) {
                        layer.solidColor = {
                            ((rgb >> 16) & 0xff) / 255.0,
                            ((rgb >> 8) & 0xff) / 255.0,
                            (rgb & 0xff) / 255.0,
                            1.0};
                    }
                }
            }
        } else if (layer.ty == 5) {
            const QJsonObject text = object.value(QStringLiteral("t")).toObject();
            const QJsonArray keyframes = text.value(QStringLiteral("d"))
                .toObject().value(QStringLiteral("k")).toArray();
            if (!keyframes.isEmpty()) {
                const QJsonObject document = keyframes.front().toObject()
                    .value(QStringLiteral("s")).toObject();
                layer.text = document.value(QStringLiteral("t"))
                    .toString().toStdString();
                layer.textFont = document.value(QStringLiteral("f"))
                    .toString(QString::fromStdString(layer.textFont)).toStdString();
                layer.textFontSize = document.value(QStringLiteral("s"))
                    .toDouble(layer.textFontSize);
                const auto color = document.value(QStringLiteral("fc"));
                if (color.isArray()) {
                    layer.textColor.clear();
                    for (const auto& component : color.toArray()) {
                        if (component.isDouble()) layer.textColor.push_back(component.toDouble());
                    }
                }
            }
        }
        const QJsonObject transform = object.value(QStringLiteral("ks")).toObject();
        layer.anchor = readPoint(transform.value(QStringLiteral("a")));
        layer.position = readPoint(transform.value(QStringLiteral("p")));
        layer.scale = readPoint(transform.value(QStringLiteral("s")));
        layer.rotation = readPoint(transform.value(QStringLiteral("r")));
        layer.opacity = readPoint(transform.value(QStringLiteral("o")));
        const auto shapes = object.value(QStringLiteral("shapes"));
        if (shapes.isArray()) {
            for (const auto& shapeValue : shapes.toArray()) {
                if (!shapeValue.isObject()) continue;
                const QJsonObject shape = shapeValue.toObject();
                const std::string type = shape.value(QStringLiteral("ty"))
                    .toString().toStdString();
                if (type == "rc") {
                    LottieShapeRect item;
                    item.p = readPoint(shape.value(QStringLiteral("p")));
                    item.s = readPoint(shape.value(QStringLiteral("s")));
                    item.r = readPoint(shape.value(QStringLiteral("r")));
                    item.direction = shape.value(QStringLiteral("d")).toInt(item.direction);
                    layer.shapes.emplace_back(std::move(item));
                } else if (type == "el") {
                    LottieShapeEllipse item;
                    item.p = readPoint(shape.value(QStringLiteral("p")));
                    item.s = readPoint(shape.value(QStringLiteral("s")));
                    item.direction = shape.value(QStringLiteral("d")).toInt(item.direction);
                    layer.shapes.emplace_back(std::move(item));
                } else if (type == "sr") {
                    LottieShapeStar item;
                    item.position = readPoint(shape.value(QStringLiteral("p")));
                    item.points = readPoint(shape.value(QStringLiteral("pt")));
                    item.rotation = readPoint(shape.value(QStringLiteral("r")));
                    item.outerRadius = readPoint(shape.value(QStringLiteral("or")));
                    item.innerRadius = readPoint(shape.value(QStringLiteral("ir")));
                    item.starType = shape.value(QStringLiteral("sy")).toInt(item.starType);
                    item.direction = shape.value(QStringLiteral("d")).toInt(item.direction);
                    layer.shapes.emplace_back(std::move(item));
                } else if (type == "fl") {
                    LottieShapeFill item;
                    item.c = LottieColor{readPoint(shape.value(QStringLiteral("c"))).k, {}};
                    item.o = readPoint(shape.value(QStringLiteral("o")));
                    item.blendMode = shape.value(QStringLiteral("r")).toInt(item.blendMode);
                    item.enabled = shape.value(QStringLiteral("hd")).toBool(true) ? 0 : 1;
                    layer.shapes.emplace_back(std::move(item));
                } else if (type == "st") {
                    LottieShapeStroke item;
                    item.c = LottieColor{readPoint(shape.value(QStringLiteral("c"))).k, {}};
                    item.o = readPoint(shape.value(QStringLiteral("o")));
                    item.w = readPoint(shape.value(QStringLiteral("w")));
                    item.blendMode = shape.value(QStringLiteral("r")).toInt(item.blendMode);
                    item.enabled = shape.value(QStringLiteral("hd")).toBool(true) ? 0 : 1;
                    item.lineCap = shape.value(QStringLiteral("lc")).toInt(item.lineCap);
                    item.lineJoin = shape.value(QStringLiteral("lj")).toInt(item.lineJoin);
                    item.miterLimit = shape.value(QStringLiteral("ml")).toDouble(item.miterLimit);
                    layer.shapes.emplace_back(std::move(item));
                } else if (type == "mm") {
                    LottieShapeMergePaths item;
                    item.mode = shape.value(QStringLiteral("mm")).toInt(item.mode);
                    item.direction = shape.value(QStringLiteral("d")).toInt(item.direction);
                    layer.shapes.emplace_back(std::move(item));
                } else if (type == "sh") {
                    LottieShapePath item;
                    item.closed = shape.value(QStringLiteral("closed")).toBool(item.closed);
                    item.direction = shape.value(QStringLiteral("d")).toInt(item.direction);
                    const auto readNumberArray = [](const QJsonValue& value) {
                        std::vector<double> numbers;
                        if (!value.isArray()) return numbers;
                        for (const auto& component : value.toArray()) {
                            if (component.isDouble()) {
                                numbers.push_back(component.toDouble());
                            } else if (component.isArray()) {
                                for (const auto& nested : component.toArray()) {
                                    if (nested.isDouble()) numbers.push_back(nested.toDouble());
                                }
                            }
                        }
                        return numbers;
                    };
                    const QJsonObject path = shape.value(QStringLiteral("ks")).toObject();
                    const QJsonObject pathValue = path.value(QStringLiteral("k")).toObject();
                    const auto vertices = pathValue.value(QStringLiteral("v"));
                    const auto inTangents = pathValue.value(QStringLiteral("i"));
                    const auto outTangents = pathValue.value(QStringLiteral("o"));
                    if (vertices.isArray()) {
                        for (const auto& point : vertices.toArray()) {
                            if (!point.isArray()) continue;
                            for (const auto& component : point.toArray()) {
                                if (component.isDouble()) item.vertices.push_back(component.toDouble());
                            }
                        }
                    }
                    item.inTangents = readNumberArray(inTangents);
                    item.outTangents = readNumberArray(outTangents);
                    layer.shapes.emplace_back(std::move(item));
                } else if (type == "tm") {
                    LottieShapeTrim item;
                    item.start = readPoint(shape.value(QStringLiteral("s")));
                    item.end = readPoint(shape.value(QStringLiteral("e")));
                    item.offset = readPoint(shape.value(QStringLiteral("o")));
                    item.mode = shape.value(QStringLiteral("m")).toInt(item.mode);
                    layer.shapes.emplace_back(std::move(item));
                } else if (type == "rp") {
                    LottieShapeRepeater item;
                    item.copies = readPoint(shape.value(QStringLiteral("c")));
                    item.offset = readPoint(shape.value(QStringLiteral("o")));
                    item.startOpacity = shape.value(QStringLiteral("so")).toInt(item.startOpacity);
                    item.endOpacity = shape.value(QStringLiteral("eo")).toInt(item.endOpacity);
                    const QJsonObject repeaterTransform = shape.value(QStringLiteral("tr")).toObject();
                    item.transform.anchor = readPoint(repeaterTransform.value(QStringLiteral("a")));
                    item.transform.position = readPoint(repeaterTransform.value(QStringLiteral("p")));
                    item.transform.scale = readPoint(repeaterTransform.value(QStringLiteral("s")));
                    item.transform.rotation = readPoint(repeaterTransform.value(QStringLiteral("r")));
                    item.transform.opacity = readPoint(repeaterTransform.value(QStringLiteral("o")));
                    layer.shapes.emplace_back(std::move(item));
                } else if (type == "gf") {
                    LottieShapeGradient item;
                    item.colors = LottieColor{readPoint(shape.value(QStringLiteral("g"))).k, {}};
                    item.opacity = readPoint(shape.value(QStringLiteral("o")));
                    item.startPoint = readPoint(shape.value(QStringLiteral("s")));
                    item.endPoint = readPoint(shape.value(QStringLiteral("e")));
                    item.gradientType = shape.value(QStringLiteral("t")).toInt(item.gradientType);
                    item.blendMode = shape.value(QStringLiteral("r")).toInt(item.blendMode);
                    item.enabled = shape.value(QStringLiteral("hd")).toBool(true) ? 0 : 1;
                    layer.shapes.emplace_back(std::move(item));
                } else if (type == "gs") {
                    LottieShapeGradientStroke item;
                    item.colors = LottieColor{readPoint(shape.value(QStringLiteral("g"))).k, {}};
                    item.opacity = readPoint(shape.value(QStringLiteral("o")));
                    item.width = readPoint(shape.value(QStringLiteral("w")));
                    item.startPoint = readPoint(shape.value(QStringLiteral("s")));
                    item.endPoint = readPoint(shape.value(QStringLiteral("e")));
                    item.gradientType = shape.value(QStringLiteral("t")).toInt(item.gradientType);
                    item.blendMode = shape.value(QStringLiteral("r")).toInt(item.blendMode);
                    item.enabled = shape.value(QStringLiteral("hd")).toBool(true) ? 0 : 1;
                    layer.shapes.emplace_back(std::move(item));
                }
            }
        }
        result.layers.push_back(std::move(layer));
    }
    const auto assets = root.value(QStringLiteral("assets"));
    if (assets.isArray()) {
        for (const auto& value : assets.toArray()) {
            if (!value.isObject()) continue;
            const QJsonObject object = value.toObject();
            if (object.contains(QStringLiteral("layers"))) {
                LottiePrecompAsset asset;
                asset.id = object.value(QStringLiteral("id")).toString().toStdString();
                asset.width = object.value(QStringLiteral("w")).toInt();
                asset.height = object.value(QStringLiteral("h")).toInt();
                const auto nestedLayers = object.value(QStringLiteral("layers"));
                if (nestedLayers.isArray()) {
                    for (const auto& nestedValue : nestedLayers.toArray()) {
                        if (!nestedValue.isObject()) continue;
                        const QJsonObject nested = nestedValue.toObject();
                        LottieLayer nestedLayer;
                        nestedLayer.ddd = nested.value(QStringLiteral("ddd")).toInt();
                        nestedLayer.ind = nested.value(QStringLiteral("ind")).toInt();
                        nestedLayer.ty = nested.value(QStringLiteral("ty")).toInt(nestedLayer.ty);
                        nestedLayer.nm = nested.value(QStringLiteral("nm")).toString().toStdString();
                        nestedLayer.refId = nested.value(QStringLiteral("refId")).toString().toStdString();
                        nestedLayer.ip = nested.value(QStringLiteral("ip")).toInt();
                        nestedLayer.op = nested.value(QStringLiteral("op")).toInt();
                        nestedLayer.parent = nested.value(QStringLiteral("parent")).toInt();
                        nestedLayer.hidden = nested.value(QStringLiteral("hd")).toBool(false);
                        nestedLayer.autoOrient = nested.value(QStringLiteral("ao")).toInt() != 0;
                        nestedLayer.stretch = nested.value(QStringLiteral("sr")).toDouble(1.0);
                        nestedLayer.startTime = nested.value(QStringLiteral("st")).toDouble(0.0);
                        nestedLayer.matteType = nested.value(QStringLiteral("tt")).toInt();
                        nestedLayer.matteTarget = nested.value(QStringLiteral("tp")).toInt();
                        nestedLayer.blendMode = nested.value(QStringLiteral("bm")).toInt();
                        const QJsonObject nestedTransform = nested.value(QStringLiteral("ks")).toObject();
                        nestedLayer.anchor = readPoint(nestedTransform.value(QStringLiteral("a")));
                        nestedLayer.position = readPoint(nestedTransform.value(QStringLiteral("p")));
                        nestedLayer.scale = readPoint(nestedTransform.value(QStringLiteral("s")));
                        nestedLayer.rotation = readPoint(nestedTransform.value(QStringLiteral("r")));
                        nestedLayer.opacity = readPoint(nestedTransform.value(QStringLiteral("o")));
                        asset.layers.push_back(std::move(nestedLayer));
                    }
                }
                result.assets.emplace_back(std::move(asset));
            } else {
                LottieImageAsset asset;
                asset.id = object.value(QStringLiteral("id")).toString().toStdString();
                asset.fileName = object.value(QStringLiteral("p")).toString().toStdString();
                asset.directory = object.value(QStringLiteral("u")).toString().toStdString();
                asset.width = object.value(QStringLiteral("w")).toInt();
                asset.height = object.value(QStringLiteral("h")).toInt();
                asset.embeddedData = object.value(QStringLiteral("e")).toString().toStdString();
                asset.embedded = object.value(QStringLiteral("e")).toInt() != 0;
                result.assets.emplace_back(std::move(asset));
            }
        }
    }
    if (!std::isfinite(result.fr) || result.fr <= 0.0 ||
        result.w <= 0 || result.h <= 0 || result.op < result.ip) {
        return fail(QStringLiteral("Lottie document metadata is invalid."));
    }
    return result;
}

} // namespace ArtifactCore::Export::Lottie
