module;
#include <QFont>
#include <QFontMetricsF>
#include <QPointF>
#include <QRawFont>
#include <QRectF>
#include <QString>
#include <algorithm>
#include <vector>

export module Text.GlyphLayout;

import Text.Style;
import Utils.String.UniString;
import FloatRGBA;

namespace ArtifactCore {

// 各文字（グリフ）の状態を保持する構造体
// AEのように、ベースとなる位置からアニメーターによってオフセットされる
export struct GlyphItem {
  char32_t charCode;
  int index; // 文字列内でのインデックス

  // ベースとなるトランスフォーム（レイアウトエンジンが計算）
  QPointF basePosition;
  float baseRotation = 0.0f;
  float baseScale = 1.0f;

  // アニメーターによって適用されるオフセット
  QPointF offsetPosition;
  float offsetRotation = 0.0f;
  float offsetScale = 1.0f;
  float offsetOpacity = 1.0f;
  float offsetSkew = 0.0f;
  float offsetTracking = 0.0f;
  float offsetZ = 0.0f;

  // スタイルの上書き要素
  bool hasColorOverride = false;
  FloatRGBA fillColorOverride;
  bool hasStrokeOverride = false;
  FloatRGBA strokeColorOverride;
  float offsetStrokeWidth = 0.0f;
  float offsetBlur = 0.0f;

  // 最終的な描画用矩形
  QRectF bounds;

  // 文字ごとのスタイル上書き（将来用）
  // TextStyle styleOverride;
};

// 文字列をグリフの配列に変換するレイアウト基盤
export class TextLayoutEngine {
public:
  static std::vector<GlyphItem> layout(const UniString &text,
                                       const TextStyle &style,
                                       const ParagraphStyle &paragraph);

private:
  // 将来的には HarfBuzz などを使って正確なカーニングを行う
  static float getCharWidth(char32_t code, const TextStyle &style);
};

inline std::vector<GlyphItem>
TextLayoutEngine::layout(const UniString &text, const TextStyle &style,
                         const ParagraphStyle &paragraph) {
  std::vector<GlyphItem> result;

  if (text.length() == 0) {
    return result;
  }

  Q_UNUSED(paragraph);

  QFont font;
  font.setFamily(style.fontFamily.toQString());
  font.setPointSizeF(style.fontSize);
  font.setWeight(style.fontWeight == FontWeight::Bold     ? QFont::Bold
                 : style.fontWeight == FontWeight::Normal ? QFont::Normal
                                                          : QFont::Normal);
  font.setItalic(style.fontStyle == FontStyle::Italic);

  QFontMetricsF metrics(font);

  QPointF currentPos(0.0f, 0.0f);
  float lineHeight = metrics.height();
  float baselineOffset = metrics.ascent();

  auto u32str = text.toStdU32String();
  for (size_t i = 0; i < u32str.size(); ++i) {
    char32_t code = u32str[i];

    if (code == '\n') {
      currentPos.setX(0.0f);
      currentPos.setY(currentPos.y() + lineHeight);
      continue;
    }

    float charWidth = metrics.horizontalAdvance(QChar(code));

    GlyphItem item;
    item.charCode = code;
    item.index = static_cast<int>(i);
    item.basePosition = currentPos + QPointF(0.0f, baselineOffset);
    item.baseRotation = 0.0f;
    item.baseScale = 1.0f;
    item.offsetPosition = QPointF(0.0f, 0.0f);
    item.offsetRotation = 0.0f;
    item.offsetScale = 1.0f;
    item.offsetOpacity = 1.0f;

    QRectF charRect(0.0f, -baselineOffset, charWidth, lineHeight);
    item.bounds = charRect.translated(currentPos);

    result.push_back(item);

    currentPos.setX(currentPos.x() + charWidth);
  }

  return result;
}

} // namespace ArtifactCore
