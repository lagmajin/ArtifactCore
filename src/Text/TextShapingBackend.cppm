module;
#include <algorithm>
#include <QFont>
#include <QFontMetricsF>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QTextLayout>
#include <QString>
#include <QStringLiteral>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

module Text.ShapingBackend;

import Container.NamedVector;
import std;
import Font.FreeFont;
import Text.LayoutContract;
import Text.Style;
import Utils.String.UniString;

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

bool isRtlCodepoint(const char32_t code)
{
  return (code >= 0x0590 && code <= 0x08FF) ||
         (code >= 0xFB1D && code <= 0xFDFF) ||
         (code >= 0xFE70 && code <= 0xFEFF);
}

bool isHebrewCodepoint(const char32_t code)
{
  return code >= 0x0590 && code <= 0x05FF;
}

bool isArabicCodepoint(const char32_t code)
{
  return (code >= 0x0600 && code <= 0x06FF) ||
         (code >= 0x0750 && code <= 0x077F) ||
         (code >= 0x08A0 && code <= 0x08FF) ||
         (code >= 0xFB50 && code <= 0xFDFF) ||
         (code >= 0xFE70 && code <= 0xFEFF);
}

bool isThaiCodepoint(const char32_t code)
{
  return code >= 0x0E00 && code <= 0x0E7F;
}

bool isIndicCodepoint(const char32_t code)
{
  return (code >= 0x0900 && code <= 0x097F) ||  // Devanagari
         (code >= 0x0980 && code <= 0x09FF) ||  // Bengali
         (code >= 0x0A00 && code <= 0x0A7F) ||  // Gurmukhi
         (code >= 0x0A80 && code <= 0x0AFF) ||  // Gujarati
         (code >= 0x0B00 && code <= 0x0B7F) ||  // Odia
         (code >= 0x0B80 && code <= 0x0BFF) ||  // Tamil
         (code >= 0x0C00 && code <= 0x0C7F) ||  // Telugu
         (code >= 0x0C80 && code <= 0x0CFF) ||  // Kannada
         (code >= 0x0D00 && code <= 0x0D7F) ||  // Malayalam
         (code >= 0x0D80 && code <= 0x0DFF);    // Sinhala
}

bool isSoutheastAsianCodepoint(const char32_t code)
{
  return (code >= 0x0E80 && code <= 0x0EFF) ||  // Lao
         (code >= 0x1780 && code <= 0x17FF) ||  // Khmer
         (code >= 0x1000 && code <= 0x109F) ||  // Myanmar
         (code >= 0x0F00 && code <= 0x0FFF);     // Tibetan
}

QString scriptTagForCodepoint(const char32_t code)
{
  if (code >= 0xAC00 && code <= 0xD7AF) {
    return QStringLiteral("Hang");
  }
  if ((code >= 0x3040 && code <= 0x30FF) || (code >= 0x4E00 && code <= 0x9FFF)) {
    return QStringLiteral("Hani");
  }
  if (isArabicCodepoint(code)) {
    return QStringLiteral("Arab");
  }
  if (isHebrewCodepoint(code)) {
    return QStringLiteral("Hebr");
  }
  if (isThaiCodepoint(code)) {
    return QStringLiteral("Thai");
  }
  if (isIndicCodepoint(code)) {
    if (code >= 0x0900 && code <= 0x097F) {
      return QStringLiteral("Deva");
    }
    if (code >= 0x0980 && code <= 0x09FF) {
      return QStringLiteral("Beng");
    }
    if (code >= 0x0A00 && code <= 0x0A7F) {
      return QStringLiteral("Guru");
    }
    if (code >= 0x0A80 && code <= 0x0AFF) {
      return QStringLiteral("Gujr");
    }
    if (code >= 0x0B00 && code <= 0x0B7F) {
      return QStringLiteral("Orya");
    }
    if (code >= 0x0B80 && code <= 0x0BFF) {
      return QStringLiteral("Taml");
    }
    if (code >= 0x0C00 && code <= 0x0C7F) {
      return QStringLiteral("Telu");
    }
    if (code >= 0x0C80 && code <= 0x0CFF) {
      return QStringLiteral("Knda");
    }
    if (code >= 0x0D00 && code <= 0x0D7F) {
      return QStringLiteral("Mlym");
    }
    return QStringLiteral("Sinh");
  }
  if (isSoutheastAsianCodepoint(code)) {
    if (code >= 0x0E80 && code <= 0x0EFF) {
      return QStringLiteral("Laoo");
    }
    if (code >= 0x1780 && code <= 0x17FF) {
      return QStringLiteral("Khmr");
    }
    if (code >= 0x1000 && code <= 0x109F) {
      return QStringLiteral("Mymr");
    }
    return QStringLiteral("Tibt");
  }
  if (isRtlCodepoint(code)) {
    return QStringLiteral("Rtl");
  }
  if (code >= 0x1F300 && code <= 0x1FAFF) {
    return QStringLiteral("Emoji");
  }
  return QStringLiteral("Latn");
}

QString stableTokenIdForCodepoint(const char32_t code, const int index)
{
  return QStringLiteral("%1:%2:%3")
      .arg(scriptTagForCodepoint(code))
      .arg(index)
      .arg(QString::number(static_cast<unsigned int>(code), 16));
}

bool isLatinCodepoint(const char32_t code)
{
  return (code >= U'A' && code <= U'Z') || (code >= U'a' && code <= U'z') ||
         (code >= 0x00C0 && code <= 0x024F) || (code >= 0x1E00 && code <= 0x1EFF);
}

bool isTateChuYokoCandidate(const char32_t code)
{
  return (code >= U'0' && code <= U'9') || isLatinCodepoint(code);
}

bool isPunctuationCodepoint(const char32_t code)
{
  switch (code) {
  case U'。':
  case U'、':
  case U'，':
  case U'．':
  case U'：':
  case U'；':
  case U'!':
  case U'?':
  case U'.':
  case U',':
  case U':':
  case U';':
    return true;
  default:
    return false;
  }
}

bool isBracketCodepoint(const char32_t code)
{
  switch (code) {
  case U'(':
  case U')':
  case U'[':
  case U']':
  case U'{':
  case U'}':
  case U'「':
  case U'」':
  case U'『':
  case U'』':
  case U'【':
  case U'】':
    return true;
  default:
    return false;
  }
}

bool isOpeningBracketCodepoint(const char32_t code)
{
  return code == U'(' || code == U'[' || code == U'{' || code == U'「' ||
         code == U'『' || code == U'【';
}

bool isClosingBracketCodepoint(const char32_t code)
{
  return code == U')' || code == U']' || code == U'}' || code == U'」' ||
         code == U'』' || code == U'】';
}

bool isJapaneseVerticalPunctuation(const char32_t code)
{
  return code == U'。' || code == U'、' || code == U'，' || code == U'．';
}

bool isJapaneseBracket(const char32_t code)
{
  return code == U'「' || code == U'」' || code == U'『' || code == U'』' ||
         code == U'【' || code == U'】';
}

float verticalRotationForCodepoint(const char32_t code)
{
  if (isTateChuYokoCandidate(code)) {
    return 0.0f;
  }
  if (isBracketCodepoint(code)) {
    return isJapaneseBracket(code) ? 0.0f : 90.0f;
  }
  if (isPunctuationCodepoint(code)) {
    return isJapaneseVerticalPunctuation(code) ? 0.0f : 90.0f;
  }
  return isLatinCodepoint(code) ? 90.0f : 0.0f;
}

TextDirection inferredDirection(const QString& text, const TextDirection fallback)
{
  for (const QChar ch : text) {
    if (isRtlCodepoint(ch.unicode())) {
      return TextDirection::RightToLeft;
    }
  }
  return fallback;
}

TextLayoutContract buildContract(const QString& text,
                                 const TextShapingRequest& request)
{
  TextLayoutContract contract;
  contract.writingMode = request.writingMode;
  contract.baseDirection = inferredDirection(text, request.baseDirection);
  contract.rubyAttachments = request.rubyAttachments;

  const std::u32string u32text = toU32String(text);
  contract.scriptRuns.reserve(static_cast<int>(u32text.size()));
  contract.clusters.reserve(static_cast<int>(u32text.size()));
  contract.bidiRuns.reserve(1);
  contract.lineRuns.reserve(4);
  contract.bidiRuns.push_back(TextBidiRun{
      .logicalStart = 0,
      .logicalLength = static_cast<int>(u32text.size()),
      .direction = contract.baseDirection == TextDirection::Auto
                       ? TextDirection::LeftToRight
                       : contract.baseDirection,
      .visualOrder = 0,
  });

  int lineStart = 0;
  int lineIndex = 0;
  int scriptStart = 0;
  QString currentScriptTag;
  TextDirection currentScriptDirection = TextDirection::Auto;
  bool currentScriptComplex = false;
  const auto flushScriptRun = [&](const int scriptEnd) {
    const int scriptLength = std::max(0, scriptEnd - scriptStart);
    if (scriptLength <= 0 || currentScriptTag.isEmpty()) {
      scriptStart = scriptEnd;
      currentScriptTag.clear();
      currentScriptDirection = TextDirection::Auto;
      currentScriptComplex = false;
      return;
    }
    contract.scriptRuns.push_back(TextScriptRun{
        .logicalStart = scriptStart,
        .logicalLength = scriptLength,
        .scriptTag = currentScriptTag,
        .direction = currentScriptDirection,
        .isComplexScript = currentScriptComplex,
    });
    scriptStart = scriptEnd;
    currentScriptTag.clear();
    currentScriptDirection = TextDirection::Auto;
    currentScriptComplex = false;
  };
  const auto flushLineRun = [&](const int lineEnd, const bool verticalColumn) {
    const int lineLength = std::max(0, lineEnd - lineStart);
    contract.lineRuns.push_back(TextLineRun{
        .logicalStart = lineStart,
        .logicalLength = lineLength,
        .visualOrder = lineIndex,
        .lineIndex = lineIndex,
        .isVerticalColumn = verticalColumn,
    });
    lineStart = lineEnd + 1;
    ++lineIndex;
  };

  for (int i = 0; i < static_cast<int>(u32text.size()); ++i) {
    const char32_t code = u32text[static_cast<size_t>(i)];
    if (code == U'\r') {
      flushScriptRun(i);
      flushLineRun(i, request.writingMode == TextWritingMode::Vertical);
      if (i + 1 < static_cast<int>(u32text.size()) &&
          u32text[static_cast<size_t>(i + 1)] == U'\n') {
        ++i;
      }
      continue;
    }
    if (code == U'\n') {
      flushScriptRun(i);
      flushLineRun(i, request.writingMode == TextWritingMode::Vertical);
      continue;
    }
    const QString scriptTag = scriptTagForCodepoint(code);
    const TextDirection scriptDirection = isRtlCodepoint(code)
                                              ? TextDirection::RightToLeft
                                              : TextDirection::LeftToRight;
    const bool scriptComplex = scriptTag != QStringLiteral("Latn");
    if (currentScriptTag.isEmpty()) {
      scriptStart = i;
      currentScriptTag = scriptTag;
      currentScriptDirection = scriptDirection;
      currentScriptComplex = scriptComplex;
    } else if (currentScriptTag != scriptTag ||
               currentScriptDirection != scriptDirection) {
      flushScriptRun(i);
      scriptStart = i;
      currentScriptTag = scriptTag;
      currentScriptDirection = scriptDirection;
      currentScriptComplex = scriptComplex;
    }
    const bool isEmojiSequence = code >= 0x1F300 && code <= 0x1FAFF;
    const bool isLigature = false;
    contract.clusters.push_back(TextClusterSpan{
        .logicalStart = i,
        .logicalLength = 1,
        .visualStart = i,
        .visualLength = 1,
        .clusterId = QStringLiteral("cluster_%1").arg(i),
        .selectorTag = scriptTagForCodepoint(code),
        .stableTokenId = stableTokenIdForCodepoint(code, i),
        .scriptTag = scriptTagForCodepoint(code),
        .isLigature = isLigature,
        .isEmojiSequence = isEmojiSequence,
    });

    if (isPunctuationCodepoint(code)) {
      contract.punctuationRuns.push_back(TextPunctuationRun{
          .logicalStart = i,
          .logicalLength = 1,
          .kind = QStringLiteral("punctuation"),
          .hangingAllowed = true,
          .rotateInVertical = !isJapaneseVerticalPunctuation(code),
      });
    }
    if (isBracketCodepoint(code)) {
      contract.bracketOrientationRuns.push_back(TextBracketOrientationRun{
          .logicalStart = i,
          .logicalLength = 1,
          .bracketKind = QStringLiteral("bracket"),
          .rotateInVertical = isOpeningBracketCodepoint(code) || !isJapaneseBracket(code),
      });
    }
    contract.kinsokuBoundaryInfos.push_back(TextKinsokuBoundaryInfo{
        .logicalStart = i,
        .logicalLength = 1,
        .breakBeforeAllowed = !isPunctuationCodepoint(code) &&
                              !isClosingBracketCodepoint(code),
        .breakAfterAllowed = !isOpeningBracketCodepoint(code),
    });
  }
  flushScriptRun(static_cast<int>(u32text.size()));
  flushLineRun(static_cast<int>(u32text.size()), request.writingMode == TextWritingMode::Vertical);

  if (request.writingMode == TextWritingMode::Vertical) {
    for (int i = 0; i < static_cast<int>(u32text.size());) {
      const char32_t code = u32text[static_cast<size_t>(i)];
      if (!isTateChuYokoCandidate(code)) {
        ++i;
        continue;
      }

      int runLength = 1;
      while (i + runLength < static_cast<int>(u32text.size()) &&
             runLength < 4 &&
             isTateChuYokoCandidate(u32text[static_cast<size_t>(i + runLength)])) {
        ++runLength;
      }

      if (runLength >= 2) {
        contract.tateChuYokoRuns.push_back(TextTateChuYokoRun{
            .logicalStart = i,
            .logicalLength = runLength,
            .maxInlineGlyphs = 4,
        });
        i += runLength;
      } else {
        ++i;
      }
    }
  }
  return contract;
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

bool isLineBreak(char32_t code)
{
  return code == U'\n' || code == U'\r';
}

bool isWhitespace(char32_t code)
{
  return code == U' ' || code == U'\t' || code == 0x3000;
}

float whitespaceWidth(char32_t code, const QFontMetricsF& metrics, const TextStyle& style)
{
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

float charWidth(char32_t code, const TextStyle& style)
{
  const QString sample = QString::fromUcs4(&code, 1);
  const QFont font = FontManager::makeFont(style, sample);
  const QFontMetricsF metrics(font);
  return static_cast<float>(metrics.horizontalAdvance(sample));
}

void trimTrailingWhitespace(std::vector<LayoutGlyph>& glyphs)
{
  while (!glyphs.empty() && glyphs.back().isWhitespace) {
    glyphs.pop_back();
  }
}

LayoutLine makeLine(std::vector<LayoutGlyph> glyphs)
{
  trimTrailingWhitespace(glyphs);
  LayoutLine line;
  line.glyphs = std::move(glyphs);
  float width = 0.0f;
  for (const auto& glyph : line.glyphs) {
    width += glyph.width;
  }
  line.width = width;
  return line;
}

std::vector<int> buildUtf16Offsets(const std::u32string& text)
{
  std::vector<int> offsets;
  offsets.reserve(text.size());
  int utf16Index = 0;
  for (char32_t code : text) {
    offsets.push_back(utf16Index);
    utf16Index += code > 0xFFFF ? 2 : 1;
  }
  return offsets;
}

QTextOption::WrapMode wrapModeForParagraph(const ParagraphStyle& paragraph)
{
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

std::vector<GlyphItem> layoutWithQtTextLayout(const QString& text,
                                              const QFont& font,
                                              const ParagraphStyle& paragraph)
{
  NamedVector<GlyphItem> result{makeNamedVector<GlyphItem>(ContainerName{"TextShapingGlyphs"})};
  if (text.isEmpty()) {
    return result.toStdVector();
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
    shapedLine.width = static_cast<float>(std::max<qreal>(0.0, line.naturalTextWidth()));
    shapedLine.height = static_cast<float>(std::max<qreal>(line.height(), lineHeightFallback));
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
    return result.toStdVector();
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
    const auto& line = lines[lineIndex];
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
      item.clusterId = QStringLiteral("cluster_%1").arg(codepointIndex);
      item.selectorTag = scriptTagForCodepoint(code);
      item.stableTokenId = stableTokenIdForCodepoint(code, item.index);
      item.clusterIndex = static_cast<int>(codepointIndex);
      item.lineIndex = static_cast<int>(lineIndex);
      item.basePosition = QPointF(xOffset + localX + extraAdvance, y + line.ascent);
      item.baseRotation = 0.0f;
      item.baseScale = 1.0f;
      item.offsetPosition = QPointF(0.0f, 0.0f);
      item.offsetRotation = 0.0f;
      item.offsetScale = 1.0f;
      item.offsetOpacity = 1.0f;
      item.bounds = QRectF(xOffset + localX + extraAdvance, y,
                           glyphWidth + ((shouldJustify && whitespace) ? justifyStep : 0.0f),
                           line.height);
      result.add(item);

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

  return result.toStdVector();
}

std::vector<GlyphItem> layoutVerticalWithQtTextLayout(const QString& text,
                                                      const QFont& font,
                                                      const ParagraphStyle& paragraph)
{
  NamedVector<GlyphItem> result{makeNamedVector<GlyphItem>(ContainerName{"TextShapingVerticalGlyphs"})};
  if (text.isEmpty()) {
    return result.toStdVector();
  }

  const QFontMetricsF metrics(font);
  const float lineHeight = static_cast<float>(
      std::max<qreal>(metrics.lineSpacing(), metrics.height()));
  const float columnAdvance = static_cast<float>(
      std::max<qreal>(metrics.horizontalAdvance(QStringLiteral("M")), metrics.height()));
  const float boxHeight = paragraph.boxHeight > 0.0f ? paragraph.boxHeight : std::numeric_limits<float>::max();
  const std::u32string u32text = toU32String(text);

  float x = 0.0f;
  float y = 0.0f;
  float columnTop = 0.0f;
  int columnIndex = 0;
  for (size_t i = 0; i < u32text.size(); ++i) {
    const char32_t code = u32text[i];
    if (isLineBreak(code)) {
      ++columnIndex;
      x = -static_cast<float>(columnIndex) * columnAdvance;
      y = 0.0f;
      columnTop = 0.0f;
      continue;
    }

    int tcyRunLength = 1;
    if (isTateChuYokoCandidate(code)) {
      while (i + static_cast<size_t>(tcyRunLength) < u32text.size() &&
             tcyRunLength < 4 &&
             isTateChuYokoCandidate(u32text[i + static_cast<size_t>(tcyRunLength)])) {
        ++tcyRunLength;
      }
    }

    const QString sample = QString::fromUcs4(&code, 1);
    const float glyphHeight = static_cast<float>(std::max<qreal>(lineHeight, metrics.height()));
    const float glyphWidth = static_cast<float>(std::max<qreal>(metrics.horizontalAdvance(sample), metrics.horizontalAdvance(QStringLiteral(" "))));
    const bool punctuation = isPunctuationCodepoint(code);
    const bool bracket = isBracketCodepoint(code);
    const float rotation = verticalRotationForCodepoint(code);
    const float boxWidth = punctuation
                               ? (isJapaneseVerticalPunctuation(code) ? glyphWidth : glyphWidth * 0.9f)
                               : (bracket ? glyphWidth * 0.95f : glyphWidth);

    if (y + glyphHeight > boxHeight && y > 0.0f) {
      if (isPunctuationCodepoint(code) || isClosingBracketCodepoint(code)) {
        // Kinsoku fallback: keep forbidden line-start characters in the current column.
      } else {
        ++columnIndex;
        x = -static_cast<float>(columnIndex) * columnAdvance;
        y = 0.0f;
        columnTop = 0.0f;
      }
    }

    if (tcyRunLength >= 2) {
      const float inlineAdvance = std::max(1.0f, glyphWidth * 0.85f);
      const float totalInlineWidth = inlineAdvance * static_cast<float>(tcyRunLength);
      const float inlineStartX = x + (columnAdvance - totalInlineWidth) * 0.5f;
      for (int runIndex = 0; runIndex < tcyRunLength; ++runIndex) {
        const char32_t runCode = u32text[i + static_cast<size_t>(runIndex)];
        GlyphItem item;
        item.charCode = runCode;
        item.index = static_cast<int>(i + static_cast<size_t>(runIndex));
        item.clusterId = QStringLiteral("cluster_%1").arg(item.index);
        item.clusterIndex = item.index;
        item.lineIndex = columnIndex;
        item.basePosition = QPointF(inlineStartX + inlineAdvance * static_cast<float>(runIndex) + inlineAdvance * 0.5f,
                                    y + glyphHeight * 0.5f);
        item.baseRotation = 0.0f;
        item.baseScale = 1.0f;
        item.offsetPosition = QPointF(0.0f, 0.0f);
        item.offsetRotation = 0.0f;
        item.offsetScale = 1.0f;
        item.offsetOpacity = 1.0f;
        item.bounds = QRectF(inlineStartX + inlineAdvance * static_cast<float>(runIndex),
                             columnTop + y, inlineAdvance, glyphHeight);
        result.add(item);
      }
      y += glyphHeight;
      i += static_cast<size_t>(tcyRunLength - 1);
      continue;
    }

    GlyphItem item;
    item.charCode = code;
    item.index = static_cast<int>(i);
    item.clusterId = QStringLiteral("cluster_%1").arg(item.index);
    item.selectorTag = scriptTagForCodepoint(code);
    item.stableTokenId = stableTokenIdForCodepoint(code, item.index);
    item.clusterIndex = item.index;
    item.lineIndex = columnIndex;
    item.basePosition = QPointF(x, y + glyphHeight * 0.5f);
    item.baseRotation = rotation;
    item.baseScale = 1.0f;
    item.offsetPosition = QPointF(0.0f, 0.0f);
    item.offsetRotation = 0.0f;
    item.offsetScale = 1.0f;
    item.offsetOpacity = 1.0f;
    item.bounds = QRectF(x, columnTop + y, boxWidth, glyphHeight);
    result.add(item);
    y += glyphHeight;
  }

  return result.toStdVector();
}

void appendRubyOverlays(std::vector<GlyphItem>& glyphs,
                        const TextShapingRequest& request,
                        const QFont& baseFont)
{
  if (request.rubyAttachments.isEmpty() || glyphs.empty()) {
    return;
  }

  const QFont rubyFont = [&]() {
    QFont font = baseFont;
    const qreal currentSize = font.pointSizeF() > 0.0 ? font.pointSizeF()
                                : (font.pixelSize() > 0 ? font.pixelSize() : 12.0);
    font.setPointSizeF(std::max<qreal>(1.0, currentSize * 0.5));
    return font;
  }();
  const QFontMetricsF rubyMetrics(rubyFont);
  const float rubyLineHeight = static_cast<float>(
      std::max<qreal>(rubyMetrics.lineSpacing(), rubyMetrics.height()));
  const float rubyAdvance = static_cast<float>(
      std::max<qreal>(rubyMetrics.horizontalAdvance(QStringLiteral(" ")),
                      rubyMetrics.averageCharWidth()));

  for (const auto& attachment : request.rubyAttachments) {
    if (attachment.rubyText.isEmpty()) {
      continue;
    }

    const auto baseIt = std::find_if(glyphs.begin(), glyphs.end(),
                                     [&](const GlyphItem& item) {
                                       return item.index == attachment.baseLogicalStart;
                                     });
    if (baseIt == glyphs.end()) {
      continue;
    }

    const QPointF anchor = baseIt->basePosition;
    const QString rubyText = attachment.rubyText;
    const float totalRubyWidth = std::max<qreal>(
        rubyAdvance, rubyMetrics.horizontalAdvance(rubyText));
    const float startX = static_cast<float>(anchor.x()) - totalRubyWidth * 0.5f;
    const float startY = static_cast<float>(anchor.y()) - rubyLineHeight * 1.2f - attachment.rubyOffset;

    int charIndex = 0;
    for (const QChar ch : rubyText) {
      const QString sample(1, ch);
      const float sampleWidth = static_cast<float>(
          std::max<qreal>(rubyMetrics.horizontalAdvance(sample), rubyAdvance));
      GlyphItem rubyItem;
      rubyItem.charCode = ch.unicode();
      rubyItem.index = attachment.baseLogicalStart * 1000 + charIndex;
      rubyItem.clusterId = QStringLiteral("ruby_%1_%2").arg(attachment.baseLogicalStart).arg(charIndex);
      rubyItem.selectorTag = QStringLiteral("ruby");
      rubyItem.stableTokenId = QStringLiteral("ruby:%1:%2")
                                   .arg(attachment.baseLogicalStart)
                                   .arg(charIndex);
      rubyItem.clusterIndex = attachment.baseLogicalStart;
      rubyItem.lineIndex = -1;
      rubyItem.basePosition = QPointF(startX + static_cast<float>(charIndex) * sampleWidth + sampleWidth * 0.5f,
                                      startY);
      rubyItem.baseRotation = 0.0f;
      rubyItem.baseScale = attachment.rubyScale;
      rubyItem.offsetPosition = QPointF(0.0f, 0.0f);
      rubyItem.offsetRotation = 0.0f;
      rubyItem.offsetScale = attachment.rubyScale;
      rubyItem.offsetOpacity = 1.0f;
      rubyItem.bounds = QRectF(startX + static_cast<float>(charIndex) * sampleWidth,
                               startY - rubyLineHeight * 0.5f,
                               sampleWidth, rubyLineHeight);
      glyphs.push_back(rubyItem);
      ++charIndex;
    }
  }
}

TextShapingResult makeIdentityResult(const std::vector<GlyphItem>& glyphs,
                                     const TextShapingRequest& request)
{
  TextShapingResult result;
  result.glyphs = glyphs;
  result.contract = buildContract(request.text, request);
  result.logicalToVisual.reserve(static_cast<int>(glyphs.size()));
  result.visualToLogical.reserve(static_cast<int>(glyphs.size()));
  for (int i = 0; i < static_cast<int>(glyphs.size()); ++i) {
    result.logicalToVisual.push_back(i);
    result.visualToLogical.push_back(i);
  }
  return result;
}

} // namespace

TextShapingResult QtShapingBackend::shape(const TextShapingRequest& request)
{
  const QString qText = request.text;
  const QFont font = FontManager::makeFont(request.style, qText);
  std::vector<GlyphItem> glyphs =
      request.writingMode == TextWritingMode::Vertical
          ? layoutVerticalWithQtTextLayout(qText, font, request.paragraph)
          : layoutWithQtTextLayout(qText, font, request.paragraph);
  if (request.writingMode == TextWritingMode::Vertical) {
    appendRubyOverlays(glyphs, request, font);
  }
  return makeIdentityResult(glyphs, request);
}

TextShapingResult HarfBuzzShapingBackend::shape(const TextShapingRequest& request)
{
  // Temporary fallback until the HarfBuzz adapter is wired in.
  return QtShapingBackend{}.shape(request);
}

} // namespace ArtifactCore
