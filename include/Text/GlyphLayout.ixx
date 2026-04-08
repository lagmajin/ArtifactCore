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

namespace ArtifactCore {

// 繝医Λ繝・け縺ｧ縺ｯ縺ｪ縺・け繧ｷ繧ｯ繧ｹ繝・け繧ｹ繝昴Ν繝・け繝・け繧ｹ繝√ｂ縺ｭ縺・
// AE 繝励Ν繝ｪ繝医′縺ゅｋ縺ｧ縺ｯ縺ｪ縺上□縺代→縺ｪ縺・△縺・ｉ縺・
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

private:
  static float getCharWidth(char32_t code, const TextStyle &style);
};



} // namespace ArtifactCore
