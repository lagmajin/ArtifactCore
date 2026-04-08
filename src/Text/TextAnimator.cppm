module;
#include <utility>
#include <vector>
#include <QPointF>
#include <cmath>
#include <algorithm>
#include <random>
#include <numbers>

module Text.Animator;
import Text.Animator;

namespace ArtifactCore {

float TextAnimatorEngine::calculateWeight(int index, int totalCount, const RangeSelector& selector) {
    if (totalCount == 0) return 0.0f;
    float position = (selector.units == SelectorUnits::Percentage) ? 
                     ((float)index / totalCount) * 100.0f : (float)index;

    float start = selector.start + selector.offset;
    float end = selector.end + selector.offset;
    if (position < start || position > end) return 0.0f;
    
    float t = (std::abs(end - start) > 0.001f) ? (position - start) / (end - start) : 1.0f;
    
    switch (selector.shape) {
        case SelectorShape::Square: return 1.0f;
        case SelectorShape::RampUp: return t;
        case SelectorShape::RampDown: return 1.0f - t;
        case SelectorShape::Triangle: return (t < 0.5f) ? (t * 2.0f) : ((1.0f - t) * 2.0f);
        case SelectorShape::Round: return std::sqrt(1.0f - std::pow(t * 2.0f - 1.0f, 2.0f));
        case SelectorShape::Smooth: return 0.5f - 0.5f * std::cos(t * std::numbers::pi_v<float>);
        default: return 1.0f;
    }
}

float TextAnimatorEngine::calculateWigglyWeight(int index, float time, const WigglySelector& selector) {
    if (!selector.enabled) return 1.0f;

    // 簡易的なノイズ生成
    float phaseOffset = (float)index * (100.0f - selector.correlation) / 100.0f;
    float t = time * selector.wigglesPerSecond + phaseOffset + selector.phase;
    
    std::mt19937 gen(selector.seed + (int)std::floor(t));
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    
    float v1 = dis(gen);
    float v2 = dis(gen);
    float fract = t - std::floor(t);
    
    return v1 + (v2 - v1) * (0.5f - 0.5f * std::cos(fract * std::numbers::pi_v<float>));
}

void TextAnimatorEngine::applyAnimator(
    std::vector<GlyphItem>& glyphs, 
    const RangeSelector& selector, 
    const WigglySelector& wiggly,
    const AnimatorProperties& props,
    float time) {
    
    int n = (int)glyphs.size();
    float cumulativeTracking = 0.0f;

    for (int i = 0; i < n; ++i) {
        // セレクターの重み
        float selectorWeight = calculateWeight(i, n, selector);
        
        // ウィグリーの重み
        float wigglyWeight = wiggly.enabled ? calculateWigglyWeight(i, time, wiggly) : 1.0f;
        
        // 最終的な「適用度」
        float totalWeight = selectorWeight * wigglyWeight;
        
        // トラッキング（累積シフト）
        glyphs[i].offsetPosition.setX(glyphs[i].offsetPosition.x() + cumulativeTracking);
        
        if (std::abs(totalWeight) < 0.0001f) {
            // トラッキングだけは更新し続ける
            cumulativeTracking += props.tracking * totalWeight;
            continue;
        }

        // トランスフォーム
        glyphs[i].offsetPosition += props.position * totalWeight;
        glyphs[i].offsetScale *= (1.0f + (props.scale - 1.0f) * totalWeight);
        glyphs[i].offsetRotation += props.rotation * totalWeight;
        glyphs[i].offsetOpacity *= (1.0f - (1.0f - props.opacity) * totalWeight);
        
        // スキューとZ軸
        glyphs[i].offsetSkew += props.skew * totalWeight;
        glyphs[i].offsetZ += props.z * totalWeight;

        // トラッキングの累積（文字ごとの個別適用）
        cumulativeTracking += props.tracking * totalWeight;

        // カラーとストローク
        if (props.colorEnabled) {
            glyphs[i].hasColorOverride = true;
            glyphs[i].fillColorOverride = props.fillColor;
        }
        if (props.strokeEnabled) {
            glyphs[i].hasStrokeOverride = true;
            glyphs[i].strokeColorOverride = props.strokeColor;
            glyphs[i].offsetStrokeWidth += props.strokeWidth * totalWeight;
        }

        // 特殊効果
        glyphs[i].offsetBlur += props.blur * totalWeight;
    }
}

} // namespace ArtifactCore
