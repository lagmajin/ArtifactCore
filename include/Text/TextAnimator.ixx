module;
#include <vector>
#include <QPointF>
#include <cmath>
#include <algorithm>

export module Text.Animator;

import FloatRGBA;
import Text.GlyphLayout;

namespace ArtifactCore {

// AEの「範囲セレクター」のモード
export enum class SelectorUnits {
    Percentage, // パーセンテージ (0-100)
    Index       // インデックス (文字数)
};

export enum class SelectorShape {
    Square,     // 矩形
    RampUp,     // 直線的に増加
    RampDown,   // 直線的に減少
    Triangle,   // 三角形
    Round,      // 円形
    Smooth      // 滑らか
};

// 基準点のグループ化単位
export enum class AnchorPointGrouping {
    Character,
    Word,
    Line,
    All
};

// 範囲セレクターの状態
export struct RangeSelector {
    float start = 0.0f;
    float end = 100.0f;
    float offset = 0.0f;
    SelectorUnits units = SelectorUnits::Percentage;
    SelectorShape shape = SelectorShape::Square;
    float easeHigh = 0.0f;
    float easeLow = 0.0f;
};

// ウィグリーセレクター（ランダムに動かす）
export struct WigglySelector {
    bool enabled = false;
    float wigglesPerSecond = 2.0f;
    float correlation = 50.0f; // 0-100 (文字間の動きの連動性)
    float phase = 0.0f;
    int seed = 12345;
};

// アニメーターが各グリフに適用する拡張プロパティ
export struct AnimatorProperties {
    QPointF position = {0, 0};
    float scale = 1.0f;
    float rotation = 0.0f;
    float opacity = 1.0f;
    float skew = 0.0f;
    float tracking = 0.0f;
    float z = 0.0f;
    
    // カラーアニメーション
    bool colorEnabled = false;
    FloatRGBA fillColor = {1, 1, 1, 1};
    bool strokeEnabled = false;
    FloatRGBA strokeColor = {1, 1, 1, 1};
    float strokeWidth = 0.0f;
    
    // 特殊効果
    float blur = 0.0f;
};

export class TextAnimatorEngine {
public:
    static float calculateWeight(int index, int totalCount, const RangeSelector& selector);
    
    // ウィグリー（ランダム）の重み計算
    static float calculateWigglyWeight(int index, float time, const WigglySelector& selector);
    
    static void applyAnimator(
        std::vector<GlyphItem>& glyphs, 
        const RangeSelector& selector, 
        const WigglySelector& wiggly,
        const AnimatorProperties& props,
        float time);
};

} // namespace ArtifactCore
