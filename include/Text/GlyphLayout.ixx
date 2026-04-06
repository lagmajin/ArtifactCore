module;
#include <QFont>
#include <QFontMetricsF>
#include <QPointF>
#include <QRawFont>
#include <QRectF>
#include <QString>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

export module Text.GlyphLayout;

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

namespace {

struct LayoutGlyph {
  char32_t code;
  int index;
  float width;
  bool isWhitespace;
};

struct LayoutLine {
  std::vector<LayoutGlyph> glyphs;
  float width = 0.0f;
};

static bool isLineBreak(char32_t code) {
  return code == U'\n' || code == U'\r';
}

static bool isWhitespace(char32_t code) {
  return code == U' ' || code == U'\t' || code == 0x3000;
}

static float whitespaceWidth(char32_t code, const QFontMetricsF &metrics,
                             const TextStyle &style) {
  if (code == U'\t') {
    const float spaceWidth = static_cast<float>(metrics.horizontalAdvance(QStringLiteral(" ")));
    return std::max(1.0f, spaceWidth * 4.0f);
  }
  if (code == 0x3000) {
    return static_cast<float>(metrics.horizontalAdvance(QStringLiteral("　")));
  }
  const QString sample = QString::fromUcs4(&code, 1);
  QFont font = FontManager::makeFont(style, sample);
  const QFontMetricsF charMetrics(font);
  return static_cast<float>(charMetrics.horizontalAdvance(sample));
}

static void trimTrailingWhitespace(std::vector<LayoutGlyph> &glyphs) {
  while (!glyphs.empty() && glyphs.back().isWhitespace) {
    glyphs.pop_back();
  }
}

static void trimLeadingWhitespace(std::vector<LayoutGlyph> &glyphs) {
  while (!glyphs.empty() && glyphs.front().isWhitespace) {
    glyphs.erase(glyphs.begin());
  }
}

static float glyphVectorWidth(const std::vector<LayoutGlyph> &glyphs) {
  float width = 0.0f;
  for (const auto &glyph : glyphs) {
    width += glyph.width;
  }
  return width;
}

static LayoutLine makeLine(std::vector<LayoutGlyph> glyphs) {
  trimTrailingWhitespace(glyphs);
  LayoutLine line;
  line.glyphs = std::move(glyphs);
  line.width = glyphVectorWidth(line.glyphs);
  return line;
}

} // namespace

inline float TextLayoutEngine::getCharWidth(char32_t code,
                                            const TextStyle &style) {
  const QString sample = QString::fromUcs4(&code, 1);
  const QFont font = FontManager::makeFont(style, sample);
  const QFontMetricsF metrics(font);
  return static_cast<float>(metrics.horizontalAdvance(sample));
}

inline std::vector<GlyphItem>
TextLayoutEngine::layout(const UniString &text, const TextStyle &style,
                         const ParagraphStyle &paragraph) {
  std::vector<GlyphItem> result;
  if (text.length() == 0) {
    return result;
  }

  const QFont font = FontManager::makeFont(style, text.toQString());
  const QFontMetricsF metrics(font);

  const float lineHeight = static_cast<float>(
      std::max<qreal>(metrics.lineSpacing(), metrics.height()));
  const float baselineOffset = static_cast<float>(metrics.ascent());
  const float wrapWidth = paragraph.boxWidth > 0.0f
                              ? paragraph.boxWidth
                              : std::numeric_limits<float>::infinity();
  const bool allowWrap = paragraph.wrapMode != TextWrapMode::NoWrap &&
                         paragraph.wrapMode != TextWrapMode::ManualWrap &&
                         std::isfinite(wrapWidth);

  std::vector<LayoutLine> lines;
  std::vector<LayoutGlyph> currentLine;
  float currentWidth = 0.0f;
  int lastBreakPoint = -1;

  auto flushCurrentLine = [&](bool forceEmptyLine) {
    if (currentLine.empty()) {
      if (forceEmptyLine) {
        lines.push_back(LayoutLine{});
      }
      currentWidth = 0.0f;
      lastBreakPoint = -1;
      return;
    }

    lines.push_back(makeLine(currentLine));
    currentLine.clear();
    currentWidth = 0.0f;
    lastBreakPoint = -1;
  };

  auto trimRemainder = [&](std::vector<LayoutGlyph> &glyphs) {
    trimLeadingWhitespace(glyphs);
  };

  auto appendGlyph = [&](const LayoutGlyph &glyph) {
    if (glyph.isWhitespace) {
      currentLine.push_back(glyph);
      currentWidth += glyph.width;
      lastBreakPoint = static_cast<int>(currentLine.size());
      return;
    }

    const float nextWidth = currentWidth + glyph.width;
    if (allowWrap && !currentLine.empty() && nextWidth > wrapWidth) {
      if (paragraph.wrapMode == TextWrapMode::WordWrap &&
          lastBreakPoint > 0) {
        std::vector<LayoutGlyph> emit(currentLine.begin(),
                                      currentLine.begin() + lastBreakPoint);
        trimTrailingWhitespace(emit);
        lines.push_back(makeLine(std::move(emit)));

        std::vector<LayoutGlyph> remainder(currentLine.begin() + lastBreakPoint,
                                           currentLine.end());
        trimRemainder(remainder);
        currentLine = std::move(remainder);
        currentWidth = glyphVectorWidth(currentLine);
        lastBreakPoint = -1;
      } else {
        flushCurrentLine(false);
      }
    }

    currentLine.push_back(glyph);
    currentWidth += glyph.width;
  };

  const std::u32string u32str = text.toStdU32String();
  for (size_t i = 0; i < u32str.size(); ++i) {
    const char32_t code = u32str[i];

    if (isLineBreak(code)) {
      flushCurrentLine(true);
      continue;
    }

    LayoutGlyph glyph;
    glyph.code = code;
    glyph.index = static_cast<int>(i);
    glyph.width = whitespaceWidth(code, metrics, style);
    glyph.isWhitespace = isWhitespace(code);

    if (glyph.isWhitespace && currentLine.empty()) {
      continue;
    }

    appendGlyph(glyph);
  }

  flushCurrentLine(false);

  if (lines.empty()) {
    return result;
  }

  float contentHeight = 0.0f;
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    contentHeight += lineHeight;
    if (lineIndex + 1 < lines.size() && paragraph.paragraphSpacing > 0.0f) {
      contentHeight += paragraph.paragraphSpacing;
    }
  }

  float verticalOffset = 0.0f;
  if (paragraph.boxHeight > contentHeight) {
    switch (paragraph.verticalAlignment) {
    case TextVerticalAlignment::Middle:
      verticalOffset = (paragraph.boxHeight - contentHeight) * 0.5f;
      break;
    case TextVerticalAlignment::Bottom:
      verticalOffset = paragraph.boxHeight - contentHeight;
      break;
    case TextVerticalAlignment::Top:
    default:
      verticalOffset = 0.0f;
      break;
    }
  }

  float y = verticalOffset;
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    const auto &line = lines[lineIndex];
    float xOffset = 0.0f;
    if (paragraph.boxWidth > 0.0f && line.width < paragraph.boxWidth) {
      switch (paragraph.horizontalAlignment) {
      case TextHorizontalAlignment::Center:
        xOffset = (paragraph.boxWidth - line.width) * 0.5f;
        break;
      case TextHorizontalAlignment::Right:
        xOffset = paragraph.boxWidth - line.width;
        break;
      case TextHorizontalAlignment::Justify:
      case TextHorizontalAlignment::Left:
      default:
        xOffset = 0.0f;
        break;
      }
    }

    const bool shouldJustify =
        paragraph.horizontalAlignment == TextHorizontalAlignment::Justify &&
        paragraph.boxWidth > 0.0f && line.width < paragraph.boxWidth &&
        lineIndex + 1 < lines.size();
    const float extraWidth = shouldJustify
                                 ? std::max(0.0f, paragraph.boxWidth - line.width)
                                 : 0.0f;
    int expandableSpaces = 0;
    if (shouldJustify) {
      for (const auto &glyph : line.glyphs) {
        if (glyph.isWhitespace) {
          ++expandableSpaces;
        }
      }
    }
    const float justifyStep =
        (shouldJustify && expandableSpaces > 0)
            ? (extraWidth / static_cast<float>(expandableSpaces))
            : 0.0f;

    float x = xOffset;
    for (const auto &glyph : line.glyphs) {
      GlyphItem item;
      item.charCode = glyph.code;
      item.index = glyph.index;
      item.basePosition = QPointF(x, y + baselineOffset);
      item.baseRotation = 0.0f;
      item.baseScale = 1.0f;
      item.offsetPosition = QPointF(0.0f, 0.0f);
      item.offsetRotation = 0.0f;
      item.offsetScale = 1.0f;
      item.offsetOpacity = 1.0f;
      const float glyphWidth =
          glyph.width + ((shouldJustify && glyph.isWhitespace) ? justifyStep : 0.0f);
      item.bounds = QRectF(x, y, glyphWidth, lineHeight);
      result.push_back(item);
      x += glyph.width;
      if (shouldJustify && glyph.isWhitespace) {
        x += justifyStep;
      }
    }

    y += lineHeight;
    if (paragraph.paragraphSpacing > 0.0f) {
      y += paragraph.paragraphSpacing;
    }
  }

  return result;
}

} // namespace ArtifactCore
