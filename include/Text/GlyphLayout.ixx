module;
#include <vector>
#include <QString>
#include <QRectF>
#include <QPointF>

export module Text.GlyphLayout;

import Text.Style;
import Utils.String.UniString;

namespace ArtifactCore {

// 各文字（グリフ）の状態を保持する構造体
// AEのように、ベースとなる位置からアニメーターによってオフセットされる
export struct GlyphItem {
    char32_t charCode;
    int index;          // 文字列内でのインデックス
    
    // ベースとなるトランスフォーム（レイアウトエンジンが計算）
    QPointF basePosition;
    float baseRotation = 0.0f;
    float baseScale = 1.0f;
    
    // アニメーターによって適用されるオフセット
    QPointF offsetPosition;
    float offsetRotation = 0.0f;
    float offsetScale = 1.0f;
    float offsetOpacity = 1.0f;
    
    // 最終的な描画用矩形
    QRectF bounds;
    
    // 文字ごとのスタイル上書き（将来用）
    // TextStyle styleOverride;
};

// 文字列をグリフの配列に変換するレイアウト基盤
export class TextLayoutEngine {
public:
    static std::vector<GlyphItem> layout(const UniString& text, const TextStyle& style, const ParagraphStyle& paragraph);
    
private:
    // 将来的には HarfBuzz などを使って正確なカーニングを行う
    static float getCharWidth(char32_t code, const TextStyle& style);
};

} // namespace ArtifactCore
