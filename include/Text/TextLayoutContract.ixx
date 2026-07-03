module;
#include <QVector>
#include <QPointF>
#include <QRectF>
#include <QString>

export module Text.LayoutContract;

import FloatRGBA;

namespace ArtifactCore {

export struct GlyphItem {
  char32_t charCode;
  int index;
  QString clusterId;
  QString selectorTag;
  QString stableTokenId;
  int clusterIndex = -1;
  int lineIndex = -1;

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
  float fillColorOverrideWeight = 1.0f;
  bool hasStrokeOverride = false;
  FloatRGBA strokeColorOverride;
  float strokeColorOverrideWeight = 1.0f;
  float offsetStrokeWidth = 0.0f;
  float offsetBlur = 0.0f;

  QRectF bounds;
};

export enum class TextWritingMode {
  Horizontal,
  Vertical,
};

export enum class TextDirection {
  Auto,
  LeftToRight,
  RightToLeft,
};

export struct TextClusterSpan {
  int logicalStart = 0;
  int logicalLength = 0;
  int visualStart = 0;
  int visualLength = 0;
  QString clusterId;
  QString selectorTag;
  QString stableTokenId;
  QString scriptTag;
  bool isLigature = false;
  bool isEmojiSequence = false;
};

export struct TextScriptRun {
  int logicalStart = 0;
  int logicalLength = 0;
  QString scriptTag;
  TextDirection direction = TextDirection::Auto;
  bool isComplexScript = false;
};

export struct TextBidiRun {
  int logicalStart = 0;
  int logicalLength = 0;
  TextDirection direction = TextDirection::LeftToRight;
  int visualOrder = 0;
};

export struct TextLineRun {
  int logicalStart = 0;
  int logicalLength = 0;
  int visualOrder = 0;
  int lineIndex = 0;
  bool isVerticalColumn = false;
  bool startsWithKinsokuForbidden = false;
  bool endsWithKinsokuForbidden = false;
};

export struct TextRubyAttachment {
  int baseLogicalStart = 0;
  int baseLogicalLength = 0;
  QString rubyText;
  float rubyScale = 0.5f;
  float rubyOffset = 0.0f;
};

export struct TextTateChuYokoRun {
  int logicalStart = 0;
  int logicalLength = 0;
  int maxInlineGlyphs = 4;
};

export struct TextPunctuationRun {
  int logicalStart = 0;
  int logicalLength = 0;
  QString kind;
  bool hangingAllowed = false;
  bool rotateInVertical = true;
};

export struct TextBracketOrientationRun {
  int logicalStart = 0;
  int logicalLength = 0;
  QString bracketKind;
  bool rotateInVertical = true;
};

export struct TextKinsokuBoundaryInfo {
  int logicalStart = 0;
  int logicalLength = 0;
  bool breakBeforeAllowed = true;
  bool breakAfterAllowed = true;
};

export struct TextLayoutContract {
  TextWritingMode writingMode = TextWritingMode::Horizontal;
  TextDirection baseDirection = TextDirection::Auto;
  QVector<TextScriptRun> scriptRuns;
  QVector<TextClusterSpan> clusters;
  QVector<TextBidiRun> bidiRuns;
  QVector<TextLineRun> lineRuns;
  QVector<TextRubyAttachment> rubyAttachments;
  QVector<TextTateChuYokoRun> tateChuYokoRuns;
  QVector<TextPunctuationRun> punctuationRuns;
  QVector<TextBracketOrientationRun> bracketOrientationRuns;
  QVector<TextKinsokuBoundaryInfo> kinsokuBoundaryInfos;
  int kinsokuViolationCount = 0;
};

} // namespace ArtifactCore
