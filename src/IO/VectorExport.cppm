module;

#include <QString>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <cmath>
#include <vector>
#include <memory>

module IO.VectorExport;

import Shape.Layer;
import Shape.Path;
import Shape.Types;

namespace ArtifactCore {

// ============================================================
// SvgFrameExporter
// ============================================================

QString SvgFrameExporter::exportLayerToSvg(const ShapeLayer& layer, const QTransform& transform)
{
    // ShapeLayer already has toSvg() on the core layer,
    // but we need to handle the transform and composition context.
    // The core ShapeLayer::toSvg() applies its own transform internally.
    // For external transform, we wrap in a <g transform="...">.
    QString svg = layer.toSvg();
    if (!transform.isIdentity() && !svg.isEmpty()) {
        // Wrap the entire content in a transformed group
        const double a = transform.m11();
        const double b = transform.m12();
        const double c = transform.m21();
        const double d = transform.m22();
        const double e = transform.dx();
        const double f = transform.dy();
        QString matrixStr = QStringLiteral("matrix(%1,%2,%3,%4,%5,%6)")
            .arg(a, 0, 'f', 6).arg(b, 0, 'f', 6)
            .arg(c, 0, 'f', 6).arg(d, 0, 'f', 6)
            .arg(e, 0, 'f', 6).arg(f, 0, 'f', 6);
        // Replace <svg> with <g transform="...">, strip </svg>
        if (svg.startsWith(QStringLiteral("<svg"))) {
            int closeTag = svg.indexOf('>');
            if (closeTag > 0) {
                svg = svg.mid(closeTag + 1);
            }
        }
        if (svg.endsWith(QStringLiteral("</svg>\n"))) {
            svg.chop(7);
        }
        svg = QStringLiteral("<g transform=\"%1\">\n%2</g>\n").arg(matrixStr, svg);
    }
    return svg;
}

QString SvgFrameExporter::exportCompositionFrame(
    const std::vector<const ShapeLayer*>& layers,
    const QSize& viewBox,
    int frameNumber)
{
    QString svg;
    QTextStream out(&svg);

    out << QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\""
        " xmlns:xlink=\"http://www.w3.org/1999/xlink\""
        " width=\"%1\" height=\"%2\""
        " viewBox=\"0 0 %1 %2\""
        ">\n").arg(viewBox.width()).arg(viewBox.height());

    out << QStringLiteral("<!-- Frame %1 -->\n").arg(frameNumber);

    for (const auto* layer : layers) {
        if (!layer || !layer->isVisible()) continue;

        const auto& tf = layer->transform();
        QTransform matrix;
        matrix.translate(tf.position.x, tf.position.y);
        matrix.translate(tf.anchor.x, tf.anchor.y);
        matrix.rotate(tf.rotation);
        matrix.scale(tf.scale.x, tf.scale.y);
        matrix.translate(-tf.anchor.x, -tf.anchor.y);

        const QString layerSvg = exportLayerToSvg(*layer, matrix);
        if (!layerSvg.isEmpty()) {
            out << QStringLiteral("<!-- Layer: %1 -->\n").arg(layer->name());
            out << layerSvg;
        }
    }

    out << QStringLiteral("</svg>\n");
    return svg;
}

bool SvgFrameExporter::writeSvgSequence(
    const std::vector<VectorExportFrame>& frames,
    const QString& outputDir,
    const QString& baseName)
{
    QDir dir(outputDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    for (const auto& frame : frames) {
        const QString filename = QStringLiteral("%1_%2.svg")
            .arg(baseName)
            .arg(frame.frameNumber, 4, 10, QChar('0'));
        const QString filePath = dir.filePath(filename);

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        QTextStream stream(&file);
        stream << frame.svgContent;
        file.close();
    }
    return true;
}

// ============================================================
// CssAnimationExporter
// ============================================================

CssAnimationData CssAnimationExporter::extractAnimationData(
    const ShapeLayer& layer,
    const QString& name,
    double durationSec,
    int startFrame,
    int endFrame,
    double frameRate)
{
    CssAnimationData data;
    data.animationName = name;
    data.durationSec = durationSec;

    // Evaluate properties at each keyframe time
    const int totalFrames = endFrame - startFrame;
    if (totalFrames <= 0) return data;

    auto addProperty = [&](const QString& propName) {
        CssKeyframeProperty prop;
        prop.propertyName = propName;

        for (int f = startFrame; f <= endFrame; ++f) {
            // We evaluate the layer at each frame to get the value
            // For now, sample at 0%, 25%, 50%, 75%, 100% keyframes
            // Full frame sampling would be done via the render queue loop
            Q_UNUSED(f);
        }
        data.properties.push_back(prop);
    };

    // Collect animatable properties from the layer
    const auto& tf = layer.transform();
    Q_UNUSED(tf);

    // Transform properties are always present
    addProperty(QStringLiteral("transform"));

    return data;
}

static QString formatTransformValue(const ShapeTransform& tf)
{
    return QStringLiteral("translate(%1px, %2px) rotate(%3deg) scale(%4, %5)")
        .arg(tf.position.x, 0, 'f', 3)
        .arg(tf.position.y, 0, 'f', 3)
        .arg(tf.rotation, 0, 'f', 3)
        .arg(tf.scale.x, 0, 'f', 3)
        .arg(tf.scale.y, 0, 'f', 3);
}

QString CssAnimationExporter::generateCss(
    const CssAnimationData& data,
    const QSize& compositionSize)
{
    QString css;
    QTextStream out(&css);

    out << QStringLiteral(
        "/* ArtifactStudio CSS Animation Export */\n"
        "/* Animation: %1 */\n"
        "/* Duration: %2s */\n"
        "\n"
        ".composition {\n"
        "  width: %3px;\n"
        "  height: %4px;\n"
        "  position: relative;\n"
        "  overflow: hidden;\n"
        "}\n"
        "\n")
        .arg(data.animationName)
        .arg(data.durationSec, 0, 'f', 3)
        .arg(compositionSize.width())
        .arg(compositionSize.height());

    out << QStringLiteral("@keyframes %1 {\n").arg(data.animationName);

    for (const auto& prop : data.properties) {
        if (prop.keyframes.empty()) continue;

        for (const auto& [frame, value] : prop.keyframes) {
            const double pct = data.durationSec > 0.0
                ? (static_cast<double>(frame) / data.durationSec) * 100.0
                : 0.0;
            out << QStringLiteral("  %1% { %2: %3; }\n")
                .arg(pct, 0, 'f', 1)
                .arg(prop.propertyName)
                .arg(value);
        }
    }

    out << QStringLiteral("}\n\n");

    out << QStringLiteral(".layer-%1 {\n")
        .arg(data.animationName.toLower());
    out << QStringLiteral("  animation: %1 %2s linear infinite;\n")
        .arg(data.animationName)
        .arg(data.durationSec, 0, 'f', 3);
    out << QStringLiteral("}\n");

    return css;
}

} // namespace ArtifactCore
