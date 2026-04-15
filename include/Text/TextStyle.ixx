module;
#include <utility>

#include <QString>
export module Text.Style;

import Utils.String.UniString;
import FloatRGBA;

export namespace ArtifactCore {

export enum class TextHorizontalAlignment {
  Left = 0,
  Center = 1,
  Right = 2,
  Justify = 3
};

export enum class TextVerticalAlignment { Top = 0, Middle = 1, Bottom = 2 };

export enum class TextWrapMode {
  NoWrap = 0,
  WordWrap = 1,
  WrapAnywhere = 2,
  ManualWrap = 3
};

export enum class FontWeight { Normal = 0, Bold = 1 };

export enum class FontStyle { Normal = 0, Italic = 1 };

export struct TextStyle {
  UniString fontFamily = UniString(QStringLiteral("Arial"));
  float fontSize = 60.0f;
  float pixelSize = 60.0f;
  float tracking = 0.0f;
  float leading = -1.0f;
  FontWeight fontWeight = FontWeight::Normal;
  FontStyle fontStyle = FontStyle::Normal;
  bool allCaps = false;
  bool underline = false;
  bool strikethrough = false;
  FloatRGBA fillColor = FloatRGBA(1.0f, 1.0f, 1.0f, 1.0f);

  // Stroke
  bool strokeEnabled = false;
  FloatRGBA strokeColor = FloatRGBA(0.0f, 0.0f, 0.0f, 1.0f);
  float strokeWidth = 2.0f;

  // Shadow
  bool shadowEnabled = false;
  FloatRGBA shadowColor = FloatRGBA(0.0f, 0.0f, 0.0f, 0.5f);
  float shadowOffsetX = 4.0f;
  float shadowOffsetY = 4.0f;
  float shadowBlur = 4.0f;

  bool operator==(const TextStyle &other) const = default;
};

export struct ParagraphStyle {
  TextHorizontalAlignment horizontalAlignment = TextHorizontalAlignment::Left;
  TextVerticalAlignment verticalAlignment = TextVerticalAlignment::Top;
  TextWrapMode wrapMode = TextWrapMode::WordWrap;
  float boxWidth = 0.0f;
  float boxHeight = 0.0f;
  float paragraphSpacing = 0.0f;

  bool operator==(const ParagraphStyle &other) const = default;
};
} // namespace ArtifactCore
