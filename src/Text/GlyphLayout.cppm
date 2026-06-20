module;
#include <QFont>
#include <QFontMetricsF>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QTextLayout>
#include <QString>
#include <QStringLiteral>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

module Text.GlyphLayout;

import Text.Style;
import Text.LayoutContract;
import Text.ShapingBackend;
import Font.FreeFont;
import Utils.String.UniString;
import FloatRGBA;

namespace ArtifactCore {

namespace {

std::u32string toU32String(const QString& text)
{
  const QList<uint> ucs4 = text.toUcs4();
  std::u32string result;
  result.reserve(static_cast<size_t>(ucs4.size()));
  for (const uint ch : ucs4) {
    result.push_back(static_cast<char32_t>(ch));
  }
  return result;
}

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

struct ShapedLine {
  int startUtf16 = 0;
  int lengthUtf16 = 0;
  float width = 0.0f;
  float height = 0.0f;
  float ascent = 0.0f;
  std::vector<float> cursorX;
};

bool isLineBreak(char32_t code) {
  return code == U'\n' || code == U'\r';
}

bool isWhitespace(char32_t code) {
  return code == U' ' || code == U'\t' || code == 0x3000;
}

QString selectorTagForCodepoint(char32_t code) {
  if (code >= 0xAC00 && code <= 0xD7AF) {
    return QStringLiteral("Hang");
  }
  if ((code >= 0x3040 && code <= 0x30FF) || (code >= 0x4E00 && code <= 0x9FFF)) {
    return QStringLiteral("Hani");
  }
  if ((code >= 0x0590 && code <= 0x05FF) ||
      (code >= 0x0600 && code <= 0x08FF) ||
      (code >= 0xFB1D && code <= 0xFDFF) ||
      (code >= 0xFE70 && code <= 0xFEFF)) {
    return QStringLiteral("Rtl");
  }
  if (code >= 0x0E00 && code <= 0x0E7F) {
    return QStringLiteral("Thai");
  }
  if ((code >= 0x0900 && code <= 0x097F) || (code >= 0x0980 && code <= 0x09FF) ||
      (code >= 0x0A00 && code <= 0x0A7F) || (code >= 0x0A80 && code <= 0x0AFF) ||
      (code >= 0x0B00 && code <= 0x0B7F) || (code >= 0x0B80 && code <= 0x0BFF) ||
      (code >= 0x0C00 && code <= 0x0C7F) || (code >= 0x0C80 && code <= 0x0CFF) ||
      (code >= 0x0D00 && code <= 0x0D7F) || (code >= 0x0D80 && code <= 0x0DFF) ||
      (code >= 0x1780 && code <= 0x17FF) || (code >= 0x1000 && code <= 0x109F) ||
      (code >= 0x0F00 && code <= 0x0FFF)) {
    return QStringLiteral("Indic");
  }
  if (code >= 0x1F300 && code <= 0x1FAFF) {
    return QStringLiteral("Emoji");
  }
  return QStringLiteral("Latn");
}

QString stableTokenIdForCodepoint(char32_t code, int index) {
  const QString tag = selectorTagForCodepoint(code);
  return QStringLiteral("%1:%2:%3")
      .arg(tag)
      .arg(index)
      .arg(QString::number(static_cast<unsigned int>(code), 16));
}

float whitespaceWidth(char32_t code, const QFontMetricsF &metrics,
                      const TextStyle &style) {
  if (code == U'\t') {
    const float spaceWidth = static_cast<float>(metrics.horizontalAdvance(QStringLiteral(" ")));
    return std::max(1.0f, spaceWidth * 4.0f);
  }
  if (code == 0x3000) {
    return static_cast<float>(metrics.horizontalAdvance(QStringLiteral("\u3000")));
  }
  const QString sample = QString::fromUcs4(&code, 1);
  QFont font = FontManager::makeFont(style, sample);
  const QFontMetricsF charMetrics(font);
  return static_cast<float>(charMetrics.horizontalAdvance(sample));
}

float charWidth(char32_t code, const TextStyle &style) {
  const QString sample = QString::fromUcs4(&code, 1);
  const QFont font = FontManager::makeFont(style, sample);
  const QFontMetricsF metrics(font);
  return static_cast<float>(metrics.horizontalAdvance(sample));
}

void trimTrailingWhitespace(std::vector<LayoutGlyph> &glyphs) {
  while (!glyphs.empty() && glyphs.back().isWhitespace) {
    glyphs.pop_back();
  }
}

LayoutLine makeLine(std::vector<LayoutGlyph> glyphs) {
  trimTrailingWhitespace(glyphs);
  LayoutLine line;
  line.glyphs = std::move(glyphs);
  float width = 0.0f;
  for (const auto &glyph : line.glyphs) {
    width += glyph.width;
  }
  line.width = width;
  return line;
}

std::vector<int> buildUtf16Offsets(const std::u32string &text) {
  std::vector<int> offsets;
  offsets.reserve(text.size());
  int utf16Index = 0;
  for (char32_t code : text) {
    offsets.push_back(utf16Index);
    utf16Index += code > 0xFFFF ? 2 : 1;
  }
  return offsets;
}

QTextOption::WrapMode wrapModeForParagraph(const ParagraphStyle &paragraph) {
  switch (paragraph.wrapMode) {
  case TextWrapMode::NoWrap:
  case TextWrapMode::ManualWrap:
    return QTextOption::NoWrap;
  case TextWrapMode::WrapAnywhere:
    return QTextOption::WrapAnywhere;
  case TextWrapMode::WordWrap:
  default:
    return QTextOption::WordWrap;
  }
}

std::vector<GlyphItem> layoutWithQtTextLayout(const QString &text,
                                              const QFont &font,
                                              const ParagraphStyle &paragraph) {
  std::vector<GlyphItem> result;
  if (text.isEmpty()) {
    return result;
  }

  QTextLayout layout(text, font);
  QTextOption option;
  option.setWrapMode(wrapModeForParagraph(paragraph));
  layout.setTextOption(option);

  const QFontMetricsF metrics(font);
  const float lineHeightFallback = static_cast<float>(
      std::max<qreal>(metrics.lineSpacing(), metrics.height()));
  const qreal wrapWidth = paragraph.boxWidth > 0.0f
                              ? static_cast<qreal>(paragraph.boxWidth)
                              : std::numeric_limits<qreal>::max();

  std::vector<ShapedLine> lines;
  layout.beginLayout();
  while (true) {
    QTextLine line = layout.createLine();
    if (!line.isValid()) {
      break;
    }
    line.setLineWidth(wrapWidth);
    ShapedLine shapedLine;
    shapedLine.startUtf16 = line.textStart();
    shapedLine.lengthUtf16 = line.textLength();
    shapedLine.width = static_cast<float>(
        std::max<qreal>(0.0, line.naturalTextWidth()));
    shapedLine.height = static_cast<float>(
        std::max<qreal>(line.height(), lineHeightFallback));
    shapedLine.ascent = static_cast<float>(line.ascent());
    shapedLine.cursorX.resize(static_cast<size_t>(shapedLine.lengthUtf16 + 1), 0.0f);
    for (int cursor = 0; cursor <= shapedLine.lengthUtf16; ++cursor) {
      shapedLine.cursorX[static_cast<size_t>(cursor)] =
          static_cast<float>(line.cursorToX(cursor));
    }
    lines.push_back(shapedLine);
  }
  layout.endLayout();

  if (lines.empty()) {
    return result;
  }

  const std::u32string u32text = toU32String(text);
  const std::vector<int> utf16Offsets = buildUtf16Offsets(u32text);

  float contentHeight = 0.0f;
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    contentHeight += lines[lineIndex].height;
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

  size_t codepointIndex = 0;
  float y = verticalOffset;
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    const auto &line = lines[lineIndex];
    const int lineStart = line.startUtf16;
    const int lineEnd = line.startUtf16 + line.lengthUtf16;

    while (codepointIndex < u32text.size() &&
           utf16Offsets[codepointIndex] < lineStart) {
      ++codepointIndex;
    }

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
    int whitespaceCount = 0;
    if (shouldJustify) {
      for (size_t probe = codepointIndex; probe < u32text.size(); ++probe) {
        const int utf16Index = utf16Offsets[probe];
        if (utf16Index >= lineEnd) {
          break;
        }
        if (isWhitespace(u32text[probe])) {
          ++whitespaceCount;
        }
      }
    }
    const float justifyStep =
        (shouldJustify && whitespaceCount > 0)
            ? (std::max<qreal>(0.0, paragraph.boxWidth - line.width) /
               static_cast<float>(whitespaceCount))
            : 0.0f;

    float extraAdvance = 0.0f;
    while (codepointIndex < u32text.size()) {
      const int utf16Index = utf16Offsets[codepointIndex];
      if (utf16Index >= lineEnd) {
        break;
      }

      const char32_t code = u32text[codepointIndex];
      const int utf16Length = code > 0xFFFF ? 2 : 1;
      const int lineCursor = utf16Index - lineStart;
      const qreal localX = line.cursorX[static_cast<size_t>(lineCursor)];
      const qreal localEndX = line.cursorX[static_cast<size_t>(lineCursor + utf16Length)];
      const qreal glyphWidth = std::max<qreal>(0.0, localEndX - localX);
      const bool whitespace = isWhitespace(code);

      GlyphItem item;
      item.charCode = code;
      item.index = static_cast<int>(codepointIndex);
      item.selectorTag = selectorTagForCodepoint(code);
      item.stableTokenId = stableTokenIdForCodepoint(code, item.index);
      item.basePosition = QPointF(xOffset + localX + extraAdvance,
                                  y + line.ascent);
      item.baseRotation = 0.0f;
      item.baseScale = 1.0f;
      item.offsetPosition = QPointF(0.0f, 0.0f);
      item.offsetRotation = 0.0f;
      item.offsetScale = 1.0f;
      item.offsetOpacity = 1.0f;
      item.bounds = QRectF(xOffset + localX + extraAdvance, y,
                           glyphWidth + ((shouldJustify && whitespace) ? justifyStep : 0.0f),
                           line.height);
      result.push_back(item);

      if (shouldJustify && whitespace) {
        extraAdvance += justifyStep;
      }
      ++codepointIndex;
    }

    y += line.height;
    if (paragraph.paragraphSpacing > 0.0f) {
      y += paragraph.paragraphSpacing;
    }
  }

  return result;
}

} // namespace

float TextLayoutEngine::getCharWidth(char32_t code, const TextStyle &style) {
  return charWidth(code, style);
}

std::vector<GlyphItem>
TextLayoutEngine::layout(const UniString &text, const TextStyle &style,
                         const ParagraphStyle &paragraph,
                         ITextShapingBackend &backend,
                         TextWritingMode writingMode,
                         TextDirection baseDirection) {
  const TextShapingRequest request{
      .text = text.toQString(),
      .style = style,
      .paragraph = paragraph,
      .writingMode = writingMode,
      .baseDirection = baseDirection,
      .locale = QString(),
  };
  return backend.shape(request).glyphs;
}

std::vector<GlyphItem>
TextLayoutEngine::layout(const UniString &text, const TextStyle &style,
                         const ParagraphStyle &paragraph) {
  QtShapingBackend backend;
  return layout(text, style, paragraph, backend);
}

std::vector<GlyphItem>
TextLayoutEngine::layoutOnPath(const UniString &text,
                                const TextStyle &style,
                                const ParagraphStyle &paragraph,
                                const std::vector<BezierSegment> &pathSegments) {
  std::vector<GlyphItem> result;
  if (text.length() == 0 || pathSegments.empty()) return result;
  std::vector<double> segLengths(pathSegments.size());
  double totalLen = 0.0;
  for (size_t i = 0; i < pathSegments.size(); ++i) {
    double len = 0.0;
    QPointF prev = pathSegments[i].p0;
    for (int s = 1; s <= 10; ++s) {
      double t = s / 10.0;
      QPointF pt = pathSegments[i].pointAt(t);
      len += std::sqrt(QPointF::dotProduct(pt - prev, pt - prev));
      prev = pt;
    }
    segLengths[i] = len;
    totalLen += len;
  }
  if (totalLen < 1.0) return result;
  double startOff = paragraph.pathBinding ? paragraph.pathBinding->startOffset : 0.0;
  double endOff = (paragraph.pathBinding && paragraph.pathBinding->endOffset > 0.0) ? paragraph.pathBinding->endOffset : totalLen;
  double available = std::max(0.0, endOff - startOff);
  if (available < 1.0) return result;
  const QString qText = text.toQString();
  const QFont font = FontManager::makeFont(style, qText);
  const QFontMetricsF metrics(font);
  float totalTextWidth = 0.0f;
  const std::u32string u32str = toU32String(text);
  for (char32_t code : u32str) totalTextWidth += charWidth(code, style);
  if (totalTextWidth < 1.0f) return result;
  double scale = available / (double)totalTextWidth;
  auto samplePathAt = [&](double len) -> std::pair<QPointF, double> {
    if (len <= 0.0) return {pathSegments[0].p0, 0.0};
    if (len >= totalLen) return {pathSegments.back().p1, 0.0};
    double accum = 0.0;
    for (size_t i = 0; i < pathSegments.size(); ++i) {
      if (accum + segLengths[i] >= len) {
        double localLen = len - accum;
        double t = (segLengths[i] > 0.0) ? localLen / segLengths[i] : 0.0;
        double clampedT = std::clamp(t, 0.0, 1.0);
        double dt = 0.01;
        QPointF pt = pathSegments[i].pointAt(clampedT);
        QPointF pt1 = pathSegments[i].pointAt(std::min(clampedT+dt,1.0));
        double angle = std::atan2(pt1.y()-pt.y(), pt1.x()-pt.x());
        return {pt, angle};
      }
      accum += segLengths[i];
    }
    return {pathSegments.back().p1, 0.0};
  };
  bool alignToPath = !paragraph.pathBinding || paragraph.pathBinding->alignToPath;
  double cursor = startOff;
  for (size_t i = 0; i < u32str.size(); ++i) {
    char32_t code = u32str[i];
    float charW = charWidth(code, style);
    double advance = (double)charW * scale;
    double mid = cursor + advance * 0.5;
    auto [pos, angle] = samplePathAt(mid);
    GlyphItem item;
    item.charCode = code;
    item.index = (int)i;
    item.selectorTag = selectorTagForCodepoint(code);
    item.stableTokenId = stableTokenIdForCodepoint(code, item.index);
    item.basePosition = pos;
    item.baseRotation = alignToPath ? (float)angle : 0.0f;
    item.baseScale = (float)scale;
    item.bounds = QRectF(pos.x(), pos.y(), (float)advance, (float)metrics.height());
    result.push_back(item);
    cursor += advance;
  }
  return result;
}

} // namespace ArtifactCore
