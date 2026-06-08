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

import Text.Style;
import Font.FreeFont;
import Utils.String.UniString;
import FloatRGBA;
import Shape.Types;

namespace ArtifactCore {

export struct GlyphItem {
  char32_t charCode;
  int index;

  QPointF basePosition;
  float baseRotation = 0.0f;
  float baseScale = 1.0f;

  QPointF offsetPosition;
  float offsetRotation = 0.0f;
  float offsetScale = 1.0f;
  float offsetOpacity = 1.0f;
  float offsetSkew = 0.0f;
  float offsetTracking = 0.0f;
  float offsetZ = 0.0f;

  bool hasColorOverride = false;
  FloatRGBA fillColorOverride;
  bool hasStrokeOverride = false;
  FloatRGBA strokeColorOverride;
  float offsetStrokeWidth = 0.0f;
  float offsetBlur = 0.0f;

  QRectF bounds;
};

export class TextLayoutEngine {
public:
  static std::vector<GlyphItem> layout(const UniString &text,
                                       const TextStyle &style,
                                       const ParagraphStyle &paragraph);

  // Path text: layout glyphs along a bezier path
  static std::vector<GlyphItem> layoutOnPath(const UniString &text,
                                              const TextStyle &style,
                                              const ParagraphStyle &paragraph,
                                              const std::vector<BezierSegment> &pathSegments);

private:
  static float getCharWidth(char32_t code, const TextStyle &style);
};



} // namespace ArtifactCore
