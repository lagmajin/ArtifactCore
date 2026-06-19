module;

#include <QString>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <vector>
#include <memory>
#include <string>

export module IO.VectorExport;

import Shape.Layer;
import Shape.Path;
import Shape.Types;

export namespace ArtifactCore {

struct VectorExportFrame {
    int frameNumber;
    QString svgContent;
};

struct CssKeyframeProperty {
    QString propertyName;
    std::vector<std::pair<int, QString>> keyframes; // (frame, value)
};

struct CssAnimationData {
    QString animationName;
    double durationSec;
    std::vector<CssKeyframeProperty> properties;
};

struct HtmlPlayerData {
    QString title;
    QSize compositionSize;
    QString css;
    QString svgMarkup;
};

class SvgFrameExporter {
public:
    static QString exportLayerToSvg(const ShapeLayer& layer, const QTransform& transform = QTransform());
    static QString exportCompositionFrame(const std::vector<const ShapeLayer*>& layers,
                                          const QSize& viewBox,
                                          int frameNumber);
    static bool writeSvgSequence(const std::vector<VectorExportFrame>& frames,
                                 const QString& outputDir,
                                 const QString& baseName);
};

class CssAnimationExporter {
public:
    static CssAnimationData extractAnimationData(const ShapeLayer& layer,
                                                  const QString& name,
                                                  double durationSec,
                                                  int startFrame,
                                                  int endFrame,
                                                  double frameRate);

    static QString generateCss(const CssAnimationData& data,
                               const QSize& compositionSize);
};

class HtmlPlayerWriter {
public:
    static QString generateHtml(const HtmlPlayerData& data);
    static QString generateFromSequence(const QString& title,
                                       const QSize& compositionSize,
                                       const QString& css,
                                       const QString& svgMarkup);
    static QString generateFrameSequencePlayer(const QString& title,
                                               const QSize& compositionSize,
                                               const QStringList& frameFiles,
                                               double durationSec);
};

} // namespace ArtifactCore
