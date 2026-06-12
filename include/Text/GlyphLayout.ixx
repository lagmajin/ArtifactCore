module;
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <QFont>
#include <QFontMetricsF>
#include <QPointF>
#include <QRawFont>
#include <QRectF>
#include <QString>

export module Text.GlyphLayout;

import std;

export import Text.LayoutContract;
import Text.Style;
import Text.ShapingBackend;
import Font.FreeFont;
import Utils.String.UniString;
import Shape.Types;

namespace ArtifactCore {

export class TextLayoutEngine {
public:
  static std::vector<GlyphItem> layout(const UniString &text,
                                       const TextStyle &style,
                                       const ParagraphStyle &paragraph);

  static std::vector<GlyphItem> layout(const UniString &text,
                                       const TextStyle &style,
                                       const ParagraphStyle &paragraph,
                                       ITextShapingBackend &backend,
                                       TextWritingMode writingMode = TextWritingMode::Horizontal,
                                       TextDirection baseDirection = TextDirection::Auto);

  // Path text: layout glyphs along a bezier path
  static std::vector<GlyphItem> layoutOnPath(const UniString &text,
                                              const TextStyle &style,
                                              const ParagraphStyle &paragraph,
                                              const std::vector<BezierSegment> &pathSegments);

private:
  static float getCharWidth(char32_t code, const TextStyle &style);
};



} // namespace ArtifactCore
