module;

#include <QString>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QPointF>
#include <QRectF>
#include <QSize>
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

static QString escapeHtmlText(const QString& text)
{
    QString escaped = text;
    escaped.replace('&', QStringLiteral("&amp;"));
    escaped.replace('<', QStringLiteral("&lt;"));
    escaped.replace('>', QStringLiteral("&gt;"));
    escaped.replace('"', QStringLiteral("&quot;"));
    return escaped;
}

QString HtmlPlayerWriter::generateHtml(const HtmlPlayerData& data)
{
    return generateFromSequence(data.title, data.compositionSize, data.css, data.svgMarkup);
}

QString HtmlPlayerWriter::generateFromSequence(const QString& title,
                                               const QSize& compositionSize,
                                               const QString& css,
                                               const QString& svgMarkup)
{
    QString html;
    QTextStream out(&html);
    const QString safeTitle = escapeHtmlText(title.isEmpty() ? QStringLiteral("Artifact Web Export") : title);
    out << QStringLiteral("<!doctype html>\n<html lang=\"en\">\n<head>\n");
    out << QStringLiteral("<meta charset=\"utf-8\">\n");
    out << QStringLiteral("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n");
    out << QStringLiteral("<title>%1</title>\n").arg(safeTitle);
    out << QStringLiteral("<style>\n");
    out << QStringLiteral("html,body{margin:0;height:100%%;background:#101318;color:#f3f6fb;font-family:system-ui,sans-serif;}\n");
    out << QStringLiteral(".frame{display:grid;place-items:center;min-height:100vh;padding:24px;box-sizing:border-box;}\n");
    out << QStringLiteral(".composition{position:relative;width:min(100%%, %1px);aspect-ratio:%1/%2;overflow:hidden;box-shadow:0 24px 80px rgba(0,0,0,.35);border-radius:18px;background:#0a0d12;}\n")
               .arg(compositionSize.width())
               .arg(compositionSize.height());
    out << css << '\n';
    out << QStringLiteral("</style>\n</head>\n<body>\n<div class=\"frame\">\n");
    out << QStringLiteral("<div class=\"composition\">\n");
    out << svgMarkup << '\n';
    out << QStringLiteral("</div>\n</div>\n</body>\n</html>\n");
    return html;
}

QString HtmlPlayerWriter::generateFrameSequencePlayer(const QString& title,
                                                      const QSize& compositionSize,
                                                      const QStringList& frameFiles,
                                                      double durationSec)
{
    const QString safeTitle = escapeHtmlText(title.isEmpty() ? QStringLiteral("Artifact Web Export") : title);
    const int frameCount = std::max(1, frameFiles.size());
    const double fps = durationSec > 0.0
                           ? static_cast<double>(frameCount) / durationSec
                           : 30.0;

    QString css;
    QTextStream out(&css);
    out << QStringLiteral(".player{position:relative;width:100%%;height:100%%;display:grid;grid-template-rows:1fr auto;gap:12px;padding:16px;box-sizing:border-box;}\n");
    out << QStringLiteral(".stage{position:relative;min-height:0;border-radius:14px;overflow:hidden;background:linear-gradient(180deg,#0d1016,#07090d);box-shadow:inset 0 0 0 1px rgba(255,255,255,.08);}\n");
    out << QStringLiteral(".stage img{position:absolute;inset:0;width:100%%;height:100%%;object-fit:contain;opacity:0;transition:opacity 120ms linear;}\n");
    out << QStringLiteral(".stage img.is-active{opacity:1;}\n");
    out << QStringLiteral(".hud{display:flex;justify-content:space-between;align-items:center;gap:12px;font:500 12px/1.2 system-ui,sans-serif;color:#d8e1ee;opacity:.9;}\n");
    out << QStringLiteral(".hud strong{font-size:13px;color:#f4f7fb;}\n");
    out << QStringLiteral(".timeline{appearance:none;width:100%%;height:6px;border-radius:999px;background:#232834;outline:none;}\n");
    out << QStringLiteral(".timeline::-webkit-slider-thumb{appearance:none;width:16px;height:16px;border-radius:50%%;background:#83aef5;border:2px solid #eaf2ff;box-shadow:0 0 0 3px rgba(131,174,245,.18);}\n");
    out << QStringLiteral(".timeline::-moz-range-thumb{width:16px;height:16px;border:none;border-radius:50%%;background:#83aef5;}\n");
    out << QStringLiteral(".help{font-size:11px;opacity:.72;}\n");
    out << QStringLiteral("button{border:0;border-radius:999px;padding:8px 12px;background:#20324c;color:#f4f7fb;font-weight:600;cursor:pointer;}\n");
    out << QStringLiteral("button:hover{background:#29405f;}\n");
    out << QStringLiteral("@media (max-width: 720px){.player{padding:12px}.hud{flex-direction:column;align-items:flex-start}.help{display:none}}\n");
    out << QStringLiteral(":root{--frame-count:%1;}\n").arg(frameCount);

    QString framesMarkup;
    QTextStream ms(&framesMarkup);
    if (frameFiles.isEmpty()) {
        ms << QStringLiteral("<div style=\"position:absolute;inset:0;display:grid;place-items:center;color:#dfe7f2;font:500 14px system-ui,sans-serif;\">No frames available</div>\n");
    } else {
        for (int i = 0; i < frameFiles.size(); ++i) {
            ms << QStringLiteral("<img class=\"frame-%1%2\" src=\"%3\" alt=\"frame %1\" data-index=\"%1\">\n")
                  .arg(i)
                  .arg(i == 0 ? QStringLiteral(" is-active") : QString())
                  .arg(escapeHtmlText(frameFiles[i]));
        }
    }

    QString script;
    QTextStream js(&script);
    js << QStringLiteral("(function(){\n");
    js << QStringLiteral("const frames=[...document.querySelectorAll('.stage img')];\n");
    js << QStringLiteral("const slider=document.querySelector('.timeline');\n");
    js << QStringLiteral("const label=document.querySelector('[data-frame-label]');\n");
    js << QStringLiteral("const total=Math.max(1, frames.length);\n");
    js << QStringLiteral("let current=0;\n");
    js << QStringLiteral("function show(index){\n");
    js << QStringLiteral("  current=(index+total)%total;\n");
    js << QStringLiteral("  frames.forEach((img,i)=>img.classList.toggle('is-active', i===current));\n");
    js << QStringLiteral("  if (slider) slider.value=String(current);\n");
    js << QStringLiteral("  if (label) label.textContent=`Frame ${current+1}/${total}`;\n");
    js << QStringLiteral("}\n");
    js << QStringLiteral("if (slider){slider.max=String(total-1);slider.value='0';slider.addEventListener('input',e=>show(Number(e.target.value)||0));}\n");
    js << QStringLiteral("document.querySelector('[data-prev]')?.addEventListener('click',()=>show(current-1));\n");
    js << QStringLiteral("document.querySelector('[data-next]')?.addEventListener('click',()=>show(current+1));\n");
    js << QStringLiteral("window.addEventListener('keydown',e=>{if(e.key==='ArrowLeft')show(current-1); if(e.key==='ArrowRight')show(current+1);});\n");
    js << QStringLiteral("const rate=%1;\n").arg(fps, 0, 'f', 3);
    js << QStringLiteral("let timer=setInterval(()=>show(current+1), Math.max(33, 1000/rate));\n");
    js << QStringLiteral("document.addEventListener('visibilitychange',()=>{clearInterval(timer); if(!document.hidden){timer=setInterval(()=>show(current+1), Math.max(33, 1000/rate));}});\n");
    js << QStringLiteral("show(0);\n");
    js << QStringLiteral("})();\n");

    QString body;
    QTextStream bodyOut(&body);
    bodyOut << QStringLiteral("<div class=\"player\">\n");
    bodyOut << QStringLiteral("<div class=\"stage\">\n%1</div>\n").arg(framesMarkup);
    bodyOut << QStringLiteral("<div class=\"hud\">\n");
    bodyOut << QStringLiteral("<div><strong>%1</strong><div class=\"help\">Arrow keys or slider to inspect frames</div></div>\n").arg(safeTitle);
    bodyOut << QStringLiteral("<div data-frame-label>Frame 1/%1</div>\n").arg(frameCount);
    bodyOut << QStringLiteral("<div style=\"display:flex;gap:8px;align-items:center;\">\n");
    bodyOut << QStringLiteral("<button type=\"button\" data-prev>Prev</button>\n");
    bodyOut << QStringLiteral("<button type=\"button\" data-next>Next</button>\n");
    bodyOut << QStringLiteral("</div>\n");
    bodyOut << QStringLiteral("</div>\n");
    bodyOut << QStringLiteral("<input class=\"timeline\" type=\"range\" min=\"0\" max=\"%1\" value=\"0\" aria-label=\"frame timeline\">\n").arg(std::max(0, frameCount - 1));
    bodyOut << QStringLiteral("</div>\n");

    return generateFromSequence(safeTitle, compositionSize, css + QStringLiteral("\n") + QStringLiteral(""), body + QStringLiteral("<script>\n") + script + QStringLiteral("</script>\n"));
}

} // namespace ArtifactCore
