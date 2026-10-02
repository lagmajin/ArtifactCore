module;
#include <algorithm>
#include <QFont>
#include <QFontMetricsF>
#include <QByteArray>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QTextBoundaryFinder>
#include <QTextLayout>
#include <QString>
#include <QStringLiteral>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

#include <unicode/ubidi.h>
#include <unicode/utypes.h>
#include <unicode/uscript.h>

module Text.ShapingBackend;

import Font.FreeFont;
import Container.NamedVector;
import Container.Debug;
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

// ICU-backed ISO 15924 lookup.  The previous hand-written range table covered
// roughly 12% of the scripts in use and silently reported "Latn" for anything
// it did not recognise, which propagated into contract.scriptRuns and from
// there into animator selectors.  Unrecognised characters now keep their real
// Script property instead of being mislabelled as Latin.
QString scriptTagForCodepoint(const char32_t code)
{
  if (code < 0 || code > 0x10FFFF) {
    return QStringLiteral("Zyyy");
  }
  UErrorCode status = U_ZERO_ERROR;
  const UScriptCode script = uscript_getScript(static_cast<UChar32>(code), &status);
  if (U_FAILURE(status)) {
    return QStringLiteral("Zyyy");
  }
  switch (script) {
  case USCRIPT_COMMON:
    return QStringLiteral("Zyyy");
  case USCRIPT_INHERITED:
    return QStringLiteral("Zinh");
  default:
    break;
  }
  const char* shortName = uscript_getShortName(script);
  if (shortName == nullptr) {
    return QStringLiteral("Zyyy");
  }
  return QString::fromLatin1(shortName);
}

// Scripts whose OpenType shaping goes beyond a simple advance per code point.
// Drives feature selection and the "this needs HarfBuzz" decision.
bool isComplexScriptTag(const QString& tag)
{
  return tag == QStringLiteral("Arab") ||
         tag == QStringLiteral("Hebr") ||
         tag == QStringLiteral("Syrc") ||
         tag == QStringLiteral("Thaa") ||
         tag == QStringLiteral("Nkoo") ||
         tag == QStringLiteral("Adlm") ||
         tag == QStringLiteral("Deva") ||
         tag == QStringLiteral("Beng") ||
         tag == QStringLiteral("Guru") ||
         tag == QStringLiteral("Gujr") ||
         tag == QStringLiteral("Orya") ||
         tag == QStringLiteral("Taml") ||
         tag == QStringLiteral("Telu") ||
         tag == QStringLiteral("Knda") ||
         tag == QStringLiteral("Mlym") ||
         tag == QStringLiteral("Sinh") ||
         tag == QStringLiteral("Thai") ||
         tag == QStringLiteral("Laoo") ||
         tag == QStringLiteral("Khmr") ||
         tag == QStringLiteral("Mymr") ||
         tag == QStringLiteral("Tibt");
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

bool isHangingPunctuationCodepoint(const char32_t code)
{
  switch (code) {
  case U'。':
  case U'、':
  case U'，':
  case U'．':
  case U',':
  case U'.':
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

bool isJapaneseMiddleDotLikeCodepoint(const char32_t code)
{
  return code == U'・' || code == U'：' || code == U'；' || code == U':' ||
         code == U';';
}

bool isJapaneseSmallKanaCodepoint(const char32_t code)
{
  switch (code) {
  case U'ぁ':
  case U'ぃ':
  case U'ぅ':
  case U'ぇ':
  case U'ぉ':
  case U'っ':
  case U'ゃ':
  case U'ゅ':
  case U'ょ':
  case U'ゎ':
  case U'ァ':
  case U'ィ':
  case U'ゥ':
  case U'ェ':
  case U'ォ':
  case U'ッ':
  case U'ャ':
  case U'ュ':
  case U'ョ':
  case U'ヮ':
    return true;
  default:
    return false;
  }
}

struct BreakPolicy {
  bool breakBeforeAllowed = true;
  bool breakAfterAllowed = true;
  bool hangingAllowed = false;
  bool keepWithPreviousInVerticalFlow = false;
};

bool isCjkCodepoint(const char32_t code)
{
  return (code >= 0x3040 && code <= 0x30FF) ||
         (code >= 0x3400 && code <= 0x9FFF) ||
         (code >= 0xF900 && code <= 0xFAFF) ||
         (code >= 0xFF00 && code <= 0xFFEF);
}

bool localeStartsWith(const QString& locale, const QString& prefix)
{
  return locale.startsWith(prefix, Qt::CaseInsensitive);
}

bool isJapaneseLocale(const QString& locale)
{
  return localeStartsWith(locale, QStringLiteral("ja"));
}

bool isChineseLocale(const QString& locale)
{
  return localeStartsWith(locale, QStringLiteral("zh"));
}

bool isKinsokuForbiddenLineStartForJapanese(const char32_t code)
{
  return isPunctuationCodepoint(code) || isClosingBracketCodepoint(code) ||
         isJapaneseSmallKanaCodepoint(code) || code == U'ー' || code == U'〜' ||
         code == U'…' || code == U'―';
}

bool isKinsokuForbiddenLineStartForChinese(const char32_t code)
{
  return isPunctuationCodepoint(code) || isClosingBracketCodepoint(code);
}

bool isKinsokuForbiddenLineEndForCjk(const char32_t code)
{
  return isOpeningBracketCodepoint(code);
}

bool allowsHangingPunctuationForCjk(const char32_t code)
{
  return isHangingPunctuationCodepoint(code);
}

BreakPolicy breakPolicyForCodepoint(const char32_t code,
                                    const QString& locale)
{
  Q_UNUSED(locale);

  BreakPolicy policy;
  const QString script = scriptTagForCodepoint(code);
  const bool cjk = isCjkCodepoint(code) || script == QStringLiteral("Hani");

  if (cjk) {
    const bool japaneseLocale = isJapaneseLocale(locale);
    const bool chineseLocale = isChineseLocale(locale);
    const bool forbiddenLineStart =
        japaneseLocale ? isKinsokuForbiddenLineStartForJapanese(code)
                       : (chineseLocale ? isKinsokuForbiddenLineStartForChinese(code)
                                        : (isPunctuationCodepoint(code) ||
                                           isClosingBracketCodepoint(code) ||
                                           isJapaneseSmallKanaCodepoint(code)));
    policy.breakBeforeAllowed = !forbiddenLineStart;
    policy.breakAfterAllowed = !isKinsokuForbiddenLineEndForCjk(code);
    policy.hangingAllowed = allowsHangingPunctuationForCjk(code);
    policy.keepWithPreviousInVerticalFlow = forbiddenLineStart;
    return policy;
  }

  if (script == QStringLiteral("Thai") || script == QStringLiteral("Deva") ||
      script == QStringLiteral("Beng") || script == QStringLiteral("Guru") ||
      script == QStringLiteral("Gujr") || script == QStringLiteral("Orya") ||
      script == QStringLiteral("Taml") || script == QStringLiteral("Telu") ||
      script == QStringLiteral("Knda") || script == QStringLiteral("Mlym") ||
      script == QStringLiteral("Sinh") || script == QStringLiteral("Khmr") ||
      script == QStringLiteral("Mymr") || script == QStringLiteral("Laoo") ||
      script == QStringLiteral("Tibt")) {
    policy.keepWithPreviousInVerticalFlow = false;
    policy.hangingAllowed = false;
    return policy;
  }

  if (script == QStringLiteral("Arab") || script == QStringLiteral("Hebr") ||
      script == QStringLiteral("Rtl")) {
    policy.breakBeforeAllowed = !isClosingBracketCodepoint(code);
    policy.breakAfterAllowed = !isOpeningBracketCodepoint(code);
    policy.hangingAllowed = false;
    policy.keepWithPreviousInVerticalFlow = !policy.breakBeforeAllowed;
    return policy;
  }

  policy.breakBeforeAllowed = !isClosingBracketCodepoint(code);
  policy.breakAfterAllowed = !isOpeningBracketCodepoint(code);
  policy.hangingAllowed = isHangingPunctuationCodepoint(code);
  policy.keepWithPreviousInVerticalFlow = !policy.breakBeforeAllowed;
  return policy;
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

// UAX #9 result for one paragraph.  logicalToVisual / visualToLogical are
// sized to the code point count and hold each code point's counterpart index;
// bidiRuns are the visual runs in left-to-right display order.
struct BidiParagraph
{
  QVector<int> logicalToVisual;
  QVector<int> visualToLogical;
  QVector<TextBidiRun> runs;
  TextDirection resolvedBase = TextDirection::LeftToRight;
  bool valid = false;
};

BidiParagraph computeBidiParagraph(const std::u32string& text,
                                   const TextDirection requestedBase)
{
  BidiParagraph result;
  const int length = static_cast<int>(text.size());
  if (length <= 0) {
    result.valid = true;
    return result;
  }

  // ICU's UBiDi API works in UTF-16, so convert once.  Codepoints outside the
  // BMP become surrogate pairs; run boundaries are reported in UTF-16 units and
  // are mapped back to code point indices below.
  QString utf16;
  utf16.reserve(length);
  for (const char32_t code : text) {
    utf16.append(QString::fromUcs4(&code, 1));
  }

  UErrorCode status = U_ZERO_ERROR;
  UBiDi* bidi = ubidi_openSized(static_cast<int32_t>(utf16.size()), 0, &status);
  if (U_FAILURE(status) || bidi == nullptr) {
    return result;
  }

  // UBIDI_DEFAULT_LTR / UBIDI_DEFAULT_RTL let ICU run P2/P3 to pick the
  // paragraph level from the first strong character.
  UBiDiLevel paraLevel = UBIDI_DEFAULT_LTR;
  if (requestedBase == TextDirection::RightToLeft) {
    paraLevel = UBIDI_DEFAULT_RTL;
  }
  const std::u16string bidiText = utf16.toStdU16String();
  ubidi_setPara(bidi, bidiText.data(),
                static_cast<int32_t>(utf16.size()), paraLevel, nullptr,
                &status);
  if (U_FAILURE(status)) {
    ubidi_close(bidi);
    return result;
  }

  result.resolvedBase = ubidi_getParaLevel(bidi) & 1
                            ? TextDirection::RightToLeft
                            : TextDirection::LeftToRight;

  // Map UTF-16 index -> code point index for run boundary translation.
  QVector<int> utf16ToCodepoint(static_cast<int>(utf16.size()) + 1, 0);
  {
    int codepointCursor = 0;
    for (int utf16Cursor = 0; utf16Cursor < utf16.size();) {
      const QChar unit = utf16.at(utf16Cursor);
      const int unitCount = unit.isHighSurrogate() && utf16Cursor + 1 < utf16.size() &&
                                    utf16.at(utf16Cursor + 1).isLowSurrogate()
                                ? 2
                                : 1;
      for (int offset = 0; offset < unitCount; ++offset) {
        utf16ToCodepoint[utf16Cursor + offset] = codepointCursor;
      }
      utf16Cursor += unitCount;
      ++codepointCursor;
      utf16ToCodepoint[utf16Cursor] = codepointCursor;
    }
  }

  // ICU reports run boundaries in UTF-16 units; translate to code point
  // indices, which is the unit the rest of the contract uses.
  const auto codepointAtUtf16 = [&utf16ToCodepoint,
                                 length](const int32_t utf16Index) -> int {
    if (utf16Index <= 0) return 0;
    if (utf16Index >= utf16ToCodepoint.size()) return length;
    return utf16ToCodepoint.at(static_cast<int>(utf16Index));
  };

  const int32_t runCount = ubidi_countRuns(bidi, &status);
  if (U_FAILURE(status) || runCount <= 0) {
    ubidi_close(bidi);
    return result;
  }

  result.runs.reserve(static_cast<int>(runCount));
  for (int32_t runIndex = 0; runIndex < runCount; ++runIndex) {
    int32_t logicalStart = 0;
    int32_t runLength = 0;
    const UBiDiDirection direction = ubidi_getVisualRun(
        bidi, runIndex, &logicalStart, &runLength);
    const int startCodepoint = codepointAtUtf16(logicalStart);
    const int endCodepoint = codepointAtUtf16(logicalStart + runLength);
    result.runs.push_back(TextBidiRun{
        .logicalStart = startCodepoint,
        .logicalLength = qMax(0, endCodepoint - startCodepoint),
        .direction = direction == UBIDI_RTL ? TextDirection::RightToLeft
                                             : TextDirection::LeftToRight,
        .visualOrder = runIndex,
    });
  }

  // Per-code point mapping.  Runs arrive in visual order, so a running cursor
  // over the code points yields the visual index; RTL runs are walked backwards
  // so the combined sequence is left-to-right on screen.
  result.visualToLogical.fill(-1, length);
  result.logicalToVisual.fill(-1, length);
  int visualCursor = 0;
  for (int32_t runIndex = 0; runIndex < runCount; ++runIndex) {
    int32_t logicalStart = 0;
    int32_t runLength = 0;
    const UBiDiDirection direction = ubidi_getVisualRun(
        bidi, runIndex, &logicalStart, &runLength);
    const int startCodepoint = codepointAtUtf16(logicalStart);
    const int endCodepoint = codepointAtUtf16(logicalStart + runLength);
    const int runCodepointCount = qMax(0, endCodepoint - startCodepoint);
    for (int offset = 0; offset < runCodepointCount; ++offset) {
      const int logical =
          direction == UBIDI_RTL
              ? startCodepoint + runCodepointCount - 1 - offset
              : startCodepoint + offset;
      if (logical < 0 || logical >= length) continue;
      if (visualCursor >= length) break;
      result.visualToLogical[visualCursor] = logical;
      result.logicalToVisual[logical] = visualCursor;
      ++visualCursor;
    }
  }

  ubidi_close(bidi);

  // Any code point not covered by a run (should not happen, but keeps the maps
  // total) keeps a self-mapping.
  for (int i = 0; i < length; ++i) {
    if (result.visualToLogical[i] < 0) result.visualToLogical[i] = i;
    if (result.logicalToVisual[i] < 0) result.logicalToVisual[i] = i;
  }
  result.valid = true;
  return result;
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
  contract.lineRuns.reserve(4);

  // Real bidi runs from UAX #9.  Previously this was a single run covering the
  // whole string, which meant a mixed-direction line reported one direction for
  // every character.
  const BidiParagraph bidi =
      computeBidiParagraph(u32text, contract.baseDirection);
  if (bidi.valid && !bidi.runs.isEmpty()) {
    contract.baseDirection = bidi.resolvedBase;
    contract.bidiRuns = bidi.runs;
  } else {
    contract.bidiRuns.reserve(1);
    contract.bidiRuns.push_back(TextBidiRun{
        .logicalStart = 0,
        .logicalLength = static_cast<int>(u32text.size()),
        .direction = contract.baseDirection == TextDirection::Auto
                         ? TextDirection::LeftToRight
                         : contract.baseDirection,
        .visualOrder = 0,
    });
  }

  const int textUtf16Length = static_cast<int>(text.size());
  std::vector<int> utf16ToCodepoint(
      static_cast<size_t>(textUtf16Length + 1), 0);
  int codepointCursor = 0;
  for (int utf16Cursor = 0; utf16Cursor < textUtf16Length;) {
    const int unitCount = text.at(utf16Cursor).isHighSurrogate() &&
                                  utf16Cursor + 1 < textUtf16Length &&
                                  text.at(utf16Cursor + 1).isLowSurrogate()
                              ? 2
                              : 1;
    for (int unit = 0; unit < unitCount; ++unit) {
      utf16ToCodepoint[static_cast<size_t>(utf16Cursor + unit)] =
          codepointCursor;
    }
    utf16Cursor += unitCount;
    ++codepointCursor;
    utf16ToCodepoint[static_cast<size_t>(utf16Cursor)] = codepointCursor;
  }

  QTextBoundaryFinder graphemeFinder(QTextBoundaryFinder::Grapheme, text);
  graphemeFinder.toStart();
  int graphemeStartUtf16 = 0;
  while (true) {
    const int graphemeEndUtf16 = graphemeFinder.toNextBoundary();
    if (graphemeEndUtf16 < 0) {
      break;
    }
    const int logicalStart = utf16ToCodepoint[static_cast<size_t>(
        std::clamp(graphemeStartUtf16, 0, textUtf16Length))];
    const int logicalEnd = utf16ToCodepoint[static_cast<size_t>(
        std::clamp(graphemeEndUtf16, 0, textUtf16Length))];
    const int logicalLength = std::max(0, logicalEnd - logicalStart);
    if (logicalLength > 0 && logicalStart < static_cast<int>(u32text.size())) {
      const QString graphemeText =
          text.mid(graphemeStartUtf16,
                   graphemeEndUtf16 - graphemeStartUtf16);
      bool emojiSequence = false;
      for (const uint code : graphemeText.toUcs4()) {
        emojiSequence = emojiSequence ||
                        (code >= 0x1F000 && code <= 0x1FAFF) ||
                        code == 0x200D || code == 0xFE0F;
      }
      const char32_t firstCode =
          u32text[static_cast<size_t>(logicalStart)];
      const QString scriptTag = scriptTagForCodepoint(firstCode);
      int visualStart = logicalStart;
      int visualEnd = logicalEnd;
      if (bidi.valid && !bidi.logicalToVisual.isEmpty()) {
        visualStart = std::numeric_limits<int>::max();
        visualEnd = std::numeric_limits<int>::min();
        for (int logical = logicalStart; logical < logicalEnd; ++logical) {
          if (logical < 0 || logical >= bidi.logicalToVisual.size()) continue;
          const int visual = bidi.logicalToVisual.at(logical);
          visualStart = std::min(visualStart, visual);
          visualEnd = std::max(visualEnd, visual + 1);
        }
        if (visualStart == std::numeric_limits<int>::max()) {
          visualStart = logicalStart;
          visualEnd = logicalEnd;
        }
      }
      contract.clusters.push_back(TextClusterSpan{
          .logicalStart = logicalStart,
          .logicalLength = logicalLength,
          .visualStart = visualStart,
          .visualLength = std::max(0, visualEnd - visualStart),
          .clusterId = QStringLiteral("cluster_%1_%2")
                           .arg(logicalStart)
                           .arg(logicalLength),
          .selectorTag = scriptTag,
          .stableTokenId = stableTokenIdForCodepoint(firstCode, logicalStart),
          .scriptTag = scriptTag,
          .isLigature = false,
          .isEmojiSequence = emojiSequence,
      });
    }
    graphemeStartUtf16 = graphemeEndUtf16;
  }

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
    const BreakPolicy breakPolicy =
        breakPolicyForCodepoint(code, request.locale);
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
    // "complex" means the script needs real OpenType shaping rather than a
    // plain advance per code point.  Common (Zyyy) and inherited (Zinh) are
    // explicitly not complex, so digits and combining marks no longer read as
    // complex the way a plain "!= Latn" test reported them.
    const bool scriptComplex = isComplexScriptTag(scriptTag);
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
    if (isPunctuationCodepoint(code)) {
      contract.punctuationRuns.push_back(TextPunctuationRun{
          .logicalStart = i,
          .logicalLength = 1,
          .kind = QStringLiteral("punctuation"),
          .hangingAllowed = breakPolicy.hangingAllowed,
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
        .breakBeforeAllowed = breakPolicy.breakBeforeAllowed,
        .breakAfterAllowed = breakPolicy.breakAfterAllowed,
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
  contract.kinsokuViolationCount = 0;
  for (TextLineRun& lineRun : contract.lineRuns) {
    lineRun.startsWithKinsokuForbidden = false;
    lineRun.endsWithKinsokuForbidden = false;
    if (lineRun.logicalLength <= 0) {
      continue;
    }

    const int start = lineRun.logicalStart;
    const int end = lineRun.logicalStart + lineRun.logicalLength - 1;
    if (start >= 0 && start < static_cast<int>(u32text.size())) {
      const BreakPolicy startPolicy = breakPolicyForCodepoint(
          u32text[static_cast<size_t>(start)], request.locale);
      lineRun.startsWithKinsokuForbidden = !startPolicy.breakBeforeAllowed;
    }
    if (end >= 0 && end < static_cast<int>(u32text.size())) {
      const BreakPolicy endPolicy = breakPolicyForCodepoint(
          u32text[static_cast<size_t>(end)], request.locale);
      lineRun.endsWithKinsokuForbidden = !endPolicy.breakAfterAllowed;
    }

    if (lineRun.startsWithKinsokuForbidden ||
        lineRun.endsWithKinsokuForbidden) {
      ++contract.kinsokuViolationCount;
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
  NamedVector<float> cursorX;
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
  NamedVector<int> offsets;
  offsets.reserve(text.size());
  int utf16Index = 0;
  for (char32_t code : text) {
    offsets.push_back(utf16Index);
    utf16Index += code > 0xFFFF ? 2 : 1;
  }
  return offsets.toStdVector();
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
  std::unordered_map<int, uint32_t> shapedGlyphByUtf16;
  const auto glyphRunFlags = QTextLayout::RetrieveGlyphIndexes |
                             QTextLayout::RetrieveGlyphPositions |
                             QTextLayout::RetrieveStringIndexes;
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
    for (int cursor = 0; cursor <= shapedLine.lengthUtf16; ++cursor) {
      shapedLine.cursorX.push_back(0.0f);
    }
    for (int cursor = 0; cursor <= shapedLine.lengthUtf16; ++cursor) {
      shapedLine.cursorX[static_cast<size_t>(cursor)] =
          static_cast<float>(line.cursorToX(cursor));
    }
    for (const auto& run : line.glyphRuns(-1, -1, glyphRunFlags)) {
      const auto indexes = run.glyphIndexes();
      const auto stringIndexes = run.stringIndexes();
      int mappedGlyphCount = 0;
      int previousSourceIndex = -1;
      bool stringMappingValid = true;
      for (int glyphIndex = 0;
           glyphIndex < indexes.size() && glyphIndex < stringIndexes.size();
           ++glyphIndex) {
        const qsizetype sourceIndex = stringIndexes.at(glyphIndex);
        if (sourceIndex >= 0 && indexes.at(glyphIndex) != 0) {
          shapedGlyphByUtf16[static_cast<int>(sourceIndex)] = indexes.at(glyphIndex);
          ++mappedGlyphCount;
          if (static_cast<int>(sourceIndex) <= previousSourceIndex) {
            stringMappingValid = false;
          }
          previousSourceIndex = static_cast<int>(sourceIndex);
        }
      }
      if ((mappedGlyphCount < indexes.size() || !stringMappingValid) &&
          !indexes.isEmpty()) {
        // Some Qt/DirectWrite combinations expose glyph indexes and positions
        // but omit stringIndexes. Preserve the shaped run instead of falling
        // back to code-point rasterization: assign its glyphs to the logical
        // code-point starts in the run, with a single-glyph run representing
        // the complete grapheme cluster.
        int cursor = line.textStart();
        for (const auto sourceIndex : stringIndexes) {
          if (sourceIndex >= line.textStart() &&
              sourceIndex < line.textStart() + line.textLength()) {
            cursor = static_cast<int>(sourceIndex);
            break;
          }
        }
        for (int glyphIndex = 0; glyphIndex < indexes.size(); ++glyphIndex) {
          if (cursor >= text.size()) break;
          if (indexes.at(glyphIndex) != 0) {
            shapedGlyphByUtf16[cursor] = indexes.at(glyphIndex);
          }
          const bool high = text.at(cursor).isHighSurrogate() &&
                            cursor + 1 < text.size() &&
                            text.at(cursor + 1).isLowSurrogate();
          cursor += high ? 2 : 1;
        }
      }
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
      if (const auto shaped = shapedGlyphByUtf16.find(utf16Index);
          shaped != shapedGlyphByUtf16.end()) {
        item.shapedGlyphIndex = shaped->second;
      }
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

std::vector<GlyphItem> layoutVerticalWithQtTextLayout(const QString& text,
                                                      const QFont& font,
                                                      const ParagraphStyle& paragraph,
                                                      const QString& locale)
{
  std::vector<GlyphItem> result;
  if (text.isEmpty()) {
    return result;
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
    const BreakPolicy breakPolicy = breakPolicyForCodepoint(code, locale);
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
                               ? (isJapaneseVerticalPunctuation(code)
                                      ? glyphWidth
                                      : (breakPolicy.hangingAllowed
                                             ? glyphWidth * 0.82f
                                             : glyphWidth * 0.9f))
                               : (bracket ? glyphWidth * 0.95f
                                          : (isJapaneseMiddleDotLikeCodepoint(code)
                                                 ? glyphWidth * 0.9f
                                                 : glyphWidth));

    if (y + glyphHeight > boxHeight && y > 0.0f) {
      if (breakPolicy.keepWithPreviousInVerticalFlow) {
        // Kinsoku fallback: keep line-start-forbidden characters in the
        // current column when possible.
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
        result.push_back(item);
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
    result.push_back(item);
    y += glyphHeight;
  }

  return result;
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

TextShapingResult makeIdentityResult(std::vector<GlyphItem> glyphs,
                                     const TextShapingRequest& request)
{
  TextShapingResult result;
  result.contract = buildContract(request.text, request);
  const int glyphCount = static_cast<int>(glyphs.size());
  std::vector<int> logicalToCluster(
      static_cast<size_t>(request.text.toUcs4().size()), -1);
  for (int clusterIndex = 0;
       clusterIndex < result.contract.clusters.size(); ++clusterIndex) {
    const auto& cluster = result.contract.clusters.at(clusterIndex);
    const int logicalEnd = cluster.logicalStart + cluster.logicalLength;
    for (int logical = cluster.logicalStart; logical < logicalEnd; ++logical) {
      if (logical >= 0 && logical < static_cast<int>(logicalToCluster.size())) {
        logicalToCluster[static_cast<size_t>(logical)] = clusterIndex;
      }
    }
  }
  for (auto& glyph : glyphs) {
    if (glyph.index >= 0 &&
        glyph.index < static_cast<int>(logicalToCluster.size())) {
      const int clusterIndex =
          logicalToCluster[static_cast<size_t>(glyph.index)];
      if (clusterIndex < 0) {
        continue;
      }
      const auto& cluster = result.contract.clusters.at(clusterIndex);
      glyph.clusterIndex = clusterIndex;
      glyph.clusterId = cluster.clusterId;
      glyph.selectorTag = cluster.selectorTag;
      glyph.stableTokenId = cluster.stableTokenId;
      const auto clusterUtf16Offsets = buildUtf16Offsets(toU32String(request.text));
      if (cluster.logicalStart >= 0 &&
          cluster.logicalStart < static_cast<int>(clusterUtf16Offsets.size())) {
        const int start = clusterUtf16Offsets[static_cast<size_t>(cluster.logicalStart)];
        const int endIndex = std::min(
            cluster.logicalStart + cluster.logicalLength,
            static_cast<int>(clusterUtf16Offsets.size()) - 1);
        const int end = clusterUtf16Offsets[static_cast<size_t>(std::max(0, endIndex))];
        glyph.clusterText = request.text.mid(start, std::max(0, end - start));
      }
      glyph.isEmojiSequence = cluster.isEmojiSequence;
      glyph.renderMode = cluster.isEmojiSequence
                             ? GlyphRenderMode::ColorBitmap
                             : GlyphRenderMode::MonochromeCoverage;
    }
  }
  std::unordered_map<int, std::vector<uint32_t>> shapedRunsByCluster;
  for (const auto& glyph : glyphs) {
    if (glyph.clusterIndex < 0 || glyph.shapedGlyphIndex == 0) continue;
    auto& run = shapedRunsByCluster[glyph.clusterIndex];
    if (std::find(run.begin(), run.end(), glyph.shapedGlyphIndex) == run.end()) {
      run.push_back(glyph.shapedGlyphIndex);
    }
  }
  for (auto& glyph : glyphs) {
    if (const auto run = shapedRunsByCluster.find(glyph.clusterIndex);
        run != shapedRunsByCluster.end()) {
      glyph.shapedGlyphIndices = run->second;
    }
  }
  result.glyphs = std::move(glyphs);

  // Build the inverse permutation from logical glyph order (source cluster
  // index, stable within a cluster) to backend output order. These arrays index
  // glyphs, not source code points: ligatures and expanded grapheme clusters
  // make those cardinalities differ.
  std::vector<int> logicalGlyphOrder(static_cast<size_t>(glyphCount));
  for (int i = 0; i < glyphCount; ++i) {
    logicalGlyphOrder[static_cast<size_t>(i)] = i;
  }
  std::stable_sort(logicalGlyphOrder.begin(), logicalGlyphOrder.end(),
                   [&result](int left, int right) {
                     return result.glyphs[static_cast<size_t>(left)].index <
                            result.glyphs[static_cast<size_t>(right)].index;
                   });
  result.logicalToVisual.resize(glyphCount);
  result.visualToLogical.resize(glyphCount);
  for (int logical = 0; logical < glyphCount; ++logical) {
    const int visual = logicalGlyphOrder[static_cast<size_t>(logical)];
    result.logicalToVisual[logical] = visual;
    result.visualToLogical[visual] = logical;
  }
  return result;
}

// --- HarfBuzz shaping -------------------------------------------------------
//
// FreeType's FT_Library is not thread safe, so each thread keeps its own
// instance.  The handle is created once per thread and reused.
FT_Library ftLibrary()
{
  thread_local FT_Library library = nullptr;
  if (library == nullptr && FT_Init_FreeType(&library) != 0) {
    return nullptr;
  }
  return library;
}

// Keyed by family + style + pixel size so repeated shaping of unchanged text
// reuses the same face.  Populated on the cold path (first use per font).
struct HarfBuzzFaceCacheKey
{
  QString family;
  QString style;
  qreal pixelSize = 0.0;

  bool operator==(const HarfBuzzFaceCacheKey& other) const
  {
    return family == other.family && style == other.style &&
           qFuzzyCompare(pixelSize + 1.0, other.pixelSize + 1.0);
  }
};

struct HarfBuzzFaceCacheHash
{
  size_t operator()(const HarfBuzzFaceCacheKey& key) const
  {
    size_t seed = qHash(key.family);
    seed = seed * 31u + qHash(key.style);
    seed = seed * 31u +
           std::hash<qint64>{}(static_cast<qint64>(key.pixelSize * 64.0));
    return seed;
  }
};

struct HarfBuzzFaceEntry
{
  QByteArray bytes;
  FT_Face face = nullptr;
};

std::unordered_map<HarfBuzzFaceCacheKey, HarfBuzzFaceEntry,
                   HarfBuzzFaceCacheHash>& harfBuzzFaceCache()
{
  static std::unordered_map<HarfBuzzFaceCacheKey, HarfBuzzFaceEntry,
                            HarfBuzzFaceCacheHash>
      cache;
  return cache;
}

// Feeds the ICU script tag into HarfBuzz.  "Latn" and the non-script values
// (Zyyy Common, Zinh Inherited) are left unset so HarfBuzz's own
// hb_buffer_guess_segment_properties() decides for them, which is what we want
// for digits, punctuation and combining marks.
hb_script_t harfBuzzScriptFor(const TextShapingRequest& request)
{
  const std::u32string text = toU32String(request.text);
  for (const char32_t code : text) {
    if (code == 0x20 || code == 0x09 || code == 0x0A || code == 0x0D) {
      continue;
    }
    const QString tag = scriptTagForCodepoint(code);
    if (tag.isEmpty() || tag == QStringLiteral("Latn") ||
        tag == QStringLiteral("Zyyy") || tag == QStringLiteral("Zinh")) {
      return HB_SCRIPT_INVALID;
    }
    const hb_script_t script =
        hb_script_from_iso15924_tag(
            HB_TAG(tag.at(0).toLatin1(), tag.at(1).toLatin1(),
                   tag.at(2).toLatin1(), tag.at(3).toLatin1()));
    if (script == HB_SCRIPT_INVALID) {
      return HB_SCRIPT_INVALID;
    }
    return script;
  }
  return HB_SCRIPT_INVALID;
}

// QLocale::name() yields POSIX-style tags with an underscore ("tr_TR"), while
// HarfBuzz expects BCP-47 with a hyphen ("tr-TR").  Passing the underscored
// form makes hb_language_from_string() return HB_LANGUAGE_INVALID, so
// language-sensitive features such as Turkish dotless-i never engage.
QString toBcp47LanguageTag(const QString& localeName)
{
  QString tag = localeName;
  tag.replace(QLatin1Char('_'), QLatin1Char('-'));
  return tag;
}

hb_direction_t harfBuzzDirectionFor(const TextShapingRequest& request)
{
  switch (request.baseDirection) {
  case TextDirection::RightToLeft:
    return HB_DIRECTION_RTL;
  case TextDirection::LeftToRight:
    return HB_DIRECTION_LTR;
  case TextDirection::Auto:
  default:
    return HB_DIRECTION_INVALID;
  }
}

// Returns nullptr when the face could not be prepared; the caller then defers
// to the Qt backend.  The cache is keyed on the resolved QFont so that a
// style or size change produces a new entry instead of mutating a live face.
FT_Face acquireHarfBuzzFace(const QFont& font)
{
  const FT_Library library = ftLibrary();
  if (library == nullptr) return nullptr;

  HarfBuzzFaceCacheKey key;
  key.family = font.family();
  key.style = font.styleName();
  key.pixelSize = font.pixelSize() > 0.0 ? font.pixelSize() : font.pointSizeF();
  if (key.pixelSize <= 0.0) key.pixelSize = font.pointSizeF();
  if (key.pixelSize <= 0.0) return nullptr;

  auto& cache = harfBuzzFaceCache();
  if (const auto existing = cache.find(key); existing != cache.end()) {
    return existing->second.face;
  }

  const std::optional<QByteArray> bytes =
      FontManager::fontFileBytes(font.family(), font.styleName());
  if (!bytes || bytes->isEmpty()) return nullptr;

  FT_Face face = nullptr;
  if (FT_New_Memory_Face(library, reinterpret_cast<const FT_Byte*>(bytes->constData()),
                          static_cast<FT_Long>(bytes->size()), 0, &face) != 0) {
    return nullptr;
  }
  const FT_F26Dot6 charSize = static_cast<FT_F26Dot6>(key.pixelSize * 64.0);
  if (FT_Set_Char_Size(face, 0, charSize, 72, 72) != 0) {
    FT_Done_Face(face);
    return nullptr;
  }
  // Keep the bytes alive for the face's whole lifetime: FreeType memory faces
  // reference the buffer rather than copying it.
  HarfBuzzFaceEntry entry;
  entry.bytes = *bytes;
  entry.face = face;
  cache.emplace(key, std::move(entry));
  return face;
}

std::optional<TextShapingResult> shapeWithHarfBuzz(
    const TextShapingRequest& request)
{
  // HarfBuzz shapes runs; it does not break lines or apply alignment.  Until
  // line layout is ported, only unwrapped single-line text can be positioned
  // correctly here, so anything else defers to the Qt path.
  if (request.paragraph.boxWidth > 0.0f) return std::nullopt;
  const QString plainText = request.text;
  if (plainText.contains(QLatin1Char('\n')) ||
      plainText.contains(QChar::LineSeparator) ||
      plainText.contains(QChar::ParagraphSeparator)) {
    return std::nullopt;
  }

  const QFont font = FontManager::makeFont(request.style, request.text);
  FT_Face face = acquireHarfBuzzFace(font);
  if (face == nullptr) return std::nullopt;

  hb_font_t* hbFont = hb_ft_font_create_referenced(face);
  if (hbFont == nullptr) return std::nullopt;

  const std::u32string text = toU32String(request.text);
  if (text.empty()) {
    hb_font_destroy(hbFont);
    return std::nullopt;
  }

  hb_buffer_t* buffer = hb_buffer_create();
  if (buffer == nullptr) {
    hb_font_destroy(hbFont);
    return std::nullopt;
  }

  hb_buffer_add_utf32(buffer, reinterpret_cast<const uint32_t*>(text.data()),
                      static_cast<int>(text.size()), 0,
                      static_cast<int>(text.size()));
  hb_buffer_guess_segment_properties(buffer);

  const hb_direction_t explicitDirection = harfBuzzDirectionFor(request);
  if (explicitDirection != HB_DIRECTION_INVALID) {
    hb_buffer_set_direction(buffer, explicitDirection);
  }
  // The script has to be set for the language to pick the right localized
  // forms, so both are always forwarded.
  const hb_script_t explicitScript = harfBuzzScriptFor(request);
  if (explicitScript != HB_SCRIPT_INVALID) {
    hb_buffer_set_script(buffer, explicitScript);
  }
  if (!request.locale.isEmpty()) {
    const QByteArray localeTag = toBcp47LanguageTag(request.locale).toUtf8();
    hb_buffer_set_language(
        buffer, hb_language_from_string(localeTag.constData(),
                                        static_cast<int>(localeTag.size())));
  }

  // Keep every glyph of a grapheme cluster in logical order so cluster values
  // stay monotone; this is what lets the contract map one cluster to the
  // several glyphs an Indic syllable or an emoji ZWJ sequence expands into.
  hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);

  // OpenType features applied to the whole buffer.  HarfBuzz already enables
  // the per-script defaults (Arabic joining, Indic nukt/akhn reordering), but
  // the general typographic features are off unless requested.
  static const hb_feature_t kFeatures[] = {
      {HB_TAG('k', 'e', 'r', 'n'), 1, 0, HB_FEATURE_GLOBAL_END},
      {HB_TAG('l', 'i', 'g', 'a'), 1, 0, HB_FEATURE_GLOBAL_END},
      {HB_TAG('c', 'a', 'l', 't'), 1, 0, HB_FEATURE_GLOBAL_END},
      {HB_TAG('c', 'l', 'i', 'g'), 1, 0, HB_FEATURE_GLOBAL_END},
      {HB_TAG('l', 'o', 'c', 'l'), 1, 0, HB_FEATURE_GLOBAL_END},
  };
  hb_shape(hbFont, buffer, kFeatures, static_cast<unsigned>(sizeof(kFeatures) /
                                                      sizeof(kFeatures[0])));

  unsigned glyphCount = 0;
  hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &glyphCount);
  hb_glyph_position_t* positions =
      hb_buffer_get_glyph_positions(buffer, &glyphCount);
  if (infos == nullptr || positions == nullptr || glyphCount == 0) {
    hb_buffer_destroy(buffer);
    hb_font_destroy(hbFont);
    return std::nullopt;
  }

  // HarfBuzz reports 26.6 fixed-point advances/offsets in the font's scaled
  // pixel space.  The Qt path stores absolute pen positions plus a bounds
  // rect, so accumulate the pen here to keep the same convention.
  constexpr float kFixedToFloat = 1.0f / 64.0f;
  const QFontMetricsF metrics(font);
  const float lineAscent = static_cast<float>(metrics.ascent());
  const float lineHeight = static_cast<float>(std::max<qreal>(metrics.lineSpacing(),
                                                               metrics.height()));

  const std::u32string u32text = toU32String(request.text);
  float penX = 0.0f;
  std::vector<GlyphItem> glyphs;
  glyphs.reserve(glyphCount);
  for (unsigned i = 0; i < glyphCount; ++i) {
    const hb_glyph_info_t& info = infos[i];
    const hb_glyph_position_t& position = positions[i];

    // info.cluster is a UTF-32 index into the text; HarfBuzz keeps it pointing
    // at the first code point of the cluster, which is what GlyphItem::index
    // and the layout contract use.
    const int codepointIndex = static_cast<int>(info.cluster);
    const char32_t code =
        codepointIndex >= 0 && codepointIndex < static_cast<int>(u32text.size())
            ? u32text[static_cast<size_t>(codepointIndex)]
            : U'\0';

    GlyphItem item;
    item.charCode = code;
    item.index = codepointIndex;
    item.clusterIndex = codepointIndex;
    item.lineIndex = 0;
    item.selectorTag = scriptTagForCodepoint(code);
    item.stableTokenId = stableTokenIdForCodepoint(code, item.index);
    // A zero codepoint marks a continuation of a multi-glyph cluster (for
    // example a ZWJ sequence); downstream renderers rely on that distinction.
    item.shapedGlyphIndex = info.codepoint;

    const float xOffset = static_cast<float>(position.x_offset) * kFixedToFloat;
    const float yOffset = static_cast<float>(position.y_offset) * kFixedToFloat;
    const float xAdvance = static_cast<float>(position.x_advance) * kFixedToFloat;
    const float yAdvance = static_cast<float>(position.y_advance) * kFixedToFloat;

    item.basePosition = QPointF(penX + xOffset, lineAscent - yOffset);
    item.baseRotation = 0.0f;
    item.baseScale = 1.0f;
    item.offsetPosition = QPointF(0.0f, 0.0f);
    item.offsetRotation = 0.0f;
    item.offsetScale = 1.0f;
    item.offsetOpacity = 1.0f;
    item.bounds = QRectF(penX, 0.0f, xAdvance, lineHeight);
    glyphs.push_back(item);

    penX += xAdvance;
    if (yAdvance != 0.0f) {
      // Vertical advances only occur once vertical writing is routed through
      // HarfBuzz; keep the pen correct until then.
      penX = xAdvance;
    }
  }

  hb_buffer_destroy(buffer);
  hb_font_destroy(hbFont);

  return makeIdentityResult(std::move(glyphs), request);
}
} // namespace

TextShapingResult QtShapingBackend::shape(const TextShapingRequest& request)
{
  const QString qText = request.text;
  const QFont font = FontManager::makeFont(request.style, qText);
  std::vector<GlyphItem> glyphs =
      request.writingMode == TextWritingMode::Vertical
          ? layoutVerticalWithQtTextLayout(qText, font, request.paragraph,
                                           request.locale)
          : layoutWithQtTextLayout(qText, font, request.paragraph);
  if (request.writingMode == TextWritingMode::Vertical) {
    appendRubyOverlays(glyphs, request, font);
  }
  return makeIdentityResult(glyphs, request);
}

TextShapingResult HarfBuzzShapingBackend::shape(const TextShapingRequest& request)
{
  // Vertical writing, ruby and kinsoku still depend on the Qt layout path, so
  // the HarfBuzz route only covers the horizontal case for now.
  if (request.writingMode != TextWritingMode::Vertical) {
    if (auto shaped = shapeWithHarfBuzz(request)) {
      return std::move(*shaped);
    }
  }
  return QtShapingBackend{}.shape(request);
}

} // namespace ArtifactCore
