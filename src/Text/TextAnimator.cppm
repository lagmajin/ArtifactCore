module;
#include <vector>
#include <QPointF>
#include <cmath>
#include <algorithm>
#include <random>
#include <numbers>

module Text.Animator;

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

    // 簡易的なノイズ生成（実際にはPerlinノイズ等が望ましいが、ここでは擬似乱数で実装）
    // 文字の相関関係（correlation）を考慮した位相
    float phaseOffset = (float)index * (100.0f - selector.correlation) / 100.0f;
    float t = time * selector.wigglesPerSecond + phaseOffset + selector.phase;
    
    // シード値を用いた乱数
    std::mt19937 gen(selector.seed + (int)std::floor(t));
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    
    float v1 = dis(gen);
    float v2 = dis(gen);
    float fract = t - std::floor(t);
    
    // 線形補間（滑らかに動くように）
    return v1 + (v2 - v1) * (0.5f - 0.5f * std::cos(fract * std::numbers::pi_v<float>));
}

void TextAnimatorEngine::applyAnimator(
    std::vector<GlyphItem>& glyphs, 
    const RangeSelector& selector, 
    const WigglySelector& wiggly,
    const AnimatorProperties& props,
    float time) {
    
    int n = (int)glyphs.size();
    for (int i = 0; i < n; ++i) {
        // セレクターの重み
        float selectorWeight = calculateWeight(i, n, selector);
        
        // ウィグリーの重み（有効な場合）
        float wigglyWeight = wiggly.enabled ? calculateWigglyWeight(i, time, wiggly) : 1.0f;
        
        // 最終的な「適用度」
        float totalWeight = selectorWeight * wigglyWeight;
        if (std::abs(totalWeight) < 0.0001f) continue;

        // トランスフォーム
        glyphs[i].offsetPosition += props.position * totalWeight;
        glyphs[i].offsetScale *= (1.0f + (props.scale - 1.0f) * totalWeight);
        glyphs[i].offsetRotation += props.rotation * totalWeight;
        glyphs[i].offsetOpacity *= (1.0f - (1.0f - props.opacity) * totalWeight);
        
        // スキュー（文字の斜体変形）
        // glyphs[i].skew += props.skew * totalWeight; // 描画側に実装が必要

        // カラーの補間（現在のベースカラーを上書きまたは加算するロジックが必要）
        // ここでは単純な重み付けのみ提供
        // if (props.colorEnabled) { ... }
    }
}

} // namespace ArtifactCore
