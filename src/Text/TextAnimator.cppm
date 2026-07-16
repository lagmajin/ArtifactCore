module;
#include <QRegularExpression>
#include <QHash>
#include <utility>
#include <vector>
#include <QPointF>
#include <cmath>
#include <algorithm>
#include <random>
#include <numbers>
#include <tuple>

module Text.Animator;

namespace ArtifactCore {

namespace {

std::vector<int> buildLogicalOrderRanks(
    std::span<const GlyphItem> glyphs) {
    std::vector<int> orderedIndices(glyphs.size());
    for (int i = 0; i < static_cast<int>(glyphs.size()); ++i) {
        orderedIndices[static_cast<size_t>(i)] = i;
    }

    std::stable_sort(
        orderedIndices.begin(), orderedIndices.end(),
        [&](const int lhs, const int rhs) {
            const GlyphItem& a = glyphs[static_cast<size_t>(lhs)];
            const GlyphItem& b = glyphs[static_cast<size_t>(rhs)];
            if (a.index != b.index) {
                return a.index < b.index;
            }
            if (a.clusterIndex != b.clusterIndex) {
                return a.clusterIndex < b.clusterIndex;
            }
            if (a.lineIndex != b.lineIndex) {
                return a.lineIndex < b.lineIndex;
            }
            return lhs < rhs;
        });

    std::vector<int> ranks(glyphs.size());
    for (int rank = 0; rank < static_cast<int>(orderedIndices.size()); ++rank) {
        ranks[static_cast<size_t>(orderedIndices[static_cast<size_t>(rank)])] =
            rank;
    }
    return ranks;
}

std::vector<int> buildUtf16StartsPerCodepoint(const QString& text) {
    std::vector<int> starts;
    starts.reserve(static_cast<size_t>(text.size()));
    for (int i = 0; i < text.size();) {
        starts.push_back(i);
        if (text.at(i).isHighSurrogate() && i + 1 < text.size() &&
            text.at(i + 1).isLowSurrogate()) {
            i += 2;
        } else {
            ++i;
        }
    }
    return starts;
}

bool glyphMatchesUtf16Range(const GlyphItem& glyph,
                            std::span<const int> codepointUtf16Starts,
                            int sourceUtf16Length,
                            int utf16Start,
                            int utf16End) {
    if (glyph.index < 0 ||
        glyph.index >= static_cast<int>(codepointUtf16Starts.size())) {
        return false;
    }

    const int glyphUtf16Start =
        codepointUtf16Starts[static_cast<size_t>(glyph.index)];
    const int glyphUtf16End =
        (glyph.index + 1 < static_cast<int>(codepointUtf16Starts.size()))
            ? codepointUtf16Starts[static_cast<size_t>(glyph.index + 1)]
            : sourceUtf16Length;
    return glyphUtf16Start < utf16End && glyphUtf16End > utf16Start;
}

} // namespace

SelectorResult TextAnimatorEngine::evaluateSelector(
    const SelectorEvaluationContext& context,
    const RangeSelector& selector) {
    SelectorResult result;
    result.units = selector.units;
    result.order = context.order;
    result.weights.fill(0.0f, static_cast<qsizetype>(context.glyphs.size()));

    if (context.glyphs.empty()) {
        result.diagnostic = QStringLiteral("empty glyph domain");
        return result;
    }

    int clusterCount = 0;
    int lineCount = 0;
    QHash<QString, int> tagOrder;
    std::vector<int> tagIndices;
    tagIndices.reserve(context.glyphs.size());
    for (const auto& glyph : context.glyphs) {
        if (glyph.clusterIndex >= 0) {
            clusterCount = std::max(clusterCount, glyph.clusterIndex + 1);
        }
        if (glyph.lineIndex >= 0) {
            lineCount = std::max(lineCount, glyph.lineIndex + 1);
        }
        const QString tag = glyph.selectorTag.isEmpty()
                                ? QStringLiteral("untagged")
                                : glyph.selectorTag;
        const auto it = tagOrder.constFind(tag);
        if (it == tagOrder.cend()) {
            const int index = tagOrder.size();
            tagOrder.insert(tag, index);
            tagIndices.push_back(index);
        } else {
            tagIndices.push_back(it.value());
        }
    }

    const std::vector<int> logicalOrderRanks =
        buildLogicalOrderRanks(context.glyphs);
    const std::vector<int> sourceCodepointUtf16Starts =
        buildUtf16StartsPerCodepoint(context.sourceText);

    QVector<bool> regexMatches(
        static_cast<qsizetype>(context.glyphs.size()), true);
    if (selector.regexEnabled && !selector.selectorPattern.isEmpty()) {
        regexMatches.fill(false);
        const QRegularExpression regex(selector.selectorPattern);
        if (!regex.isValid()) {
            result.diagnostic =
                QStringLiteral("invalid regex: %1").arg(regex.errorString());
            return result;
        }

        auto matches = regex.globalMatch(context.sourceText);
        while (matches.hasNext()) {
            const auto match = matches.next();
            const int start = match.capturedStart();
            const int end = match.capturedEnd();
            for (qsizetype i = 0; i < regexMatches.size(); ++i) {
                if (glyphMatchesUtf16Range(
                        context.glyphs[static_cast<size_t>(i)],
                        sourceCodepointUtf16Starts, context.sourceText.size(),
                        start, end)) {
                    regexMatches[i] = true;
                }
            }
        }

        QHash<int, bool> selectedClusters;
        for (qsizetype i = 0; i < regexMatches.size(); ++i) {
            const int clusterIndex =
                context.glyphs[static_cast<size_t>(i)].clusterIndex;
            if (clusterIndex >= 0 && regexMatches[i]) {
                selectedClusters.insert(clusterIndex, true);
            }
        }
        for (qsizetype i = 0; i < regexMatches.size(); ++i) {
            const int clusterIndex =
                context.glyphs[static_cast<size_t>(i)].clusterIndex;
            if (clusterIndex >= 0 && selectedClusters.contains(clusterIndex)) {
                regexMatches[i] = true;
            }
        }
    }

    const int glyphCount = static_cast<int>(context.glyphs.size());
    const int tagCount = tagOrder.size();
    for (int i = 0; i < glyphCount; ++i) {
        if (!regexMatches[static_cast<qsizetype>(i)]) {
            continue;
        }
        RangeSelector rangeOnly = selector;
        rangeOnly.regexEnabled = false;
        rangeOnly.selectorPattern.clear();
        const int orderedGlyphIndex =
            context.order == TextSelectorOrder::Logical
                ? logicalOrderRanks[static_cast<size_t>(i)]
                : i;
        result.weights[static_cast<qsizetype>(i)] =
            calculateWeightForGlyph(
                context.glyphs[static_cast<size_t>(i)], orderedGlyphIndex,
                glyphCount,
                clusterCount, lineCount, tagIndices[static_cast<size_t>(i)],
                tagCount, rangeOnly);
    }
    result.diagnostic =
        selector.regexEnabled
            ? (context.order == TextSelectorOrder::Logical
                   ? QStringLiteral(
                         "regex on source text; logical glyph order")
                   : QStringLiteral(
                         "regex on source text; visual glyph order"))
            : (context.order == TextSelectorOrder::Logical
                   ? QStringLiteral("range evaluated in logical glyph order")
                   : QStringLiteral("range evaluated in visual glyph order"));
    return result;
}

float TextAnimatorEngine::calculateWeight(int index, int totalCount, const RangeSelector& selector) {
    if (totalCount == 0) return 0.0f;
    float position = 0.0f;
    switch (selector.units) {
        case SelectorUnits::Percentage:
            position = ((float)index / totalCount) * 100.0f;
            break;
        case SelectorUnits::Index:
        case SelectorUnits::Cluster:
        case SelectorUnits::Line:
        case SelectorUnits::Tag:
        default:
            position = (float)index;
            break;
    }

    float start = selector.start + selector.offset;
    float end = selector.end + selector.offset;
    if (position < start || position > end) return 0.0f;
    
    float t = (std::abs(end - start) > 0.001f) ? (position - start) / (end - start) : 1.0f;

    // easeHigh / easeLow を適用（AE のセレクターイージング相当）
    if (selector.easeHigh > 0.001f) {
        t = std::pow(t, std::max(0.01f, 1.0f + selector.easeHigh * 0.1f));
    }
    if (selector.easeLow > 0.001f) {
        t = 1.0f - std::pow(1.0f - t, std::max(0.01f, 1.0f + selector.easeLow * 0.1f));
    }
    t = std::clamp(t, 0.0f, 1.0f);
    
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

float TextAnimatorEngine::calculateWeightForGlyph(const GlyphItem& glyph,
                                                  int glyphIndex,
                                                  int glyphCount,
                                                  int clusterCount,
                                                  int lineCount,
                                                  int tagIndex,
                                                  int tagCount,
                                                  const RangeSelector& selector) {
    if (selector.regexEnabled && !selector.selectorPattern.isEmpty()) {
        const QRegularExpression regex(selector.selectorPattern);
        const QString haystack = glyph.clusterId + QStringLiteral(" ") + glyph.selectorTag +
                                 QStringLiteral(" ") + glyph.stableTokenId +
                                 QStringLiteral(" ") + QString::number(glyph.index);
        if (!regex.isValid() || !regex.match(haystack).hasMatch()) {
            return 0.0f;
        }
    }
    int positionIndex = glyphIndex;
    int totalCount = glyphCount;

    switch (selector.units) {
        case SelectorUnits::Cluster:
            positionIndex = glyph.clusterIndex >= 0 ? glyph.clusterIndex : glyphIndex;
            totalCount = clusterCount > 0 ? clusterCount : glyphCount;
            break;
        case SelectorUnits::Line:
            positionIndex = glyph.lineIndex >= 0 ? glyph.lineIndex : glyphIndex;
            totalCount = lineCount > 0 ? lineCount : glyphCount;
            break;
        case SelectorUnits::Tag:
            positionIndex = tagIndex >= 0 ? tagIndex : glyphIndex;
            totalCount = tagCount > 0 ? tagCount : glyphCount;
            break;
        case SelectorUnits::Index:
            positionIndex = glyphIndex;
            totalCount = glyphCount;
            break;
        case SelectorUnits::Percentage:
        default:
            // Percentage is the default authoring mode and must not split a
            // grapheme (combining marks, emoji ZWJ, variation selectors).
            positionIndex = glyph.clusterIndex >= 0
                                ? glyph.clusterIndex
                                : glyphIndex;
            totalCount = clusterCount > 0 ? clusterCount : glyphCount;
            break;
    }

    return calculateWeight(positionIndex, totalCount, selector);
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
    float time,
    std::span<const float> extraWeights) {
    
    int n = (int)glyphs.size();
    int clusterCount = 0;
    int lineCount = 0;
    for (const auto& glyph : glyphs) {
        if (glyph.clusterIndex >= 0) {
            clusterCount = std::max(clusterCount, glyph.clusterIndex + 1);
        }
        if (glyph.lineIndex >= 0) {
            lineCount = std::max(lineCount, glyph.lineIndex + 1);
        }
    }
    float cumulativeTracking = 0.0f;
    QHash<QString, int> tagOrder;
    std::vector<int> tagIndices;
    tagIndices.reserve(static_cast<size_t>(n));
    for (const auto& glyph : glyphs) {
        const QString tag = glyph.selectorTag.isEmpty()
                                ? QStringLiteral("untagged")
                                : glyph.selectorTag;
        const auto it = tagOrder.constFind(tag);
        if (it == tagOrder.cend()) {
            const int index = tagOrder.size();
            tagOrder.insert(tag, index);
            tagIndices.push_back(index);
        } else {
            tagIndices.push_back(it.value());
        }
    }
    const int tagCount = tagOrder.size();

    for (int i = 0; i < n; ++i) {
        // セレクターの重み
        float selectorWeight = calculateWeightForGlyph(glyphs[i], i, n,
                                                       clusterCount, lineCount,
                                                       tagIndices[static_cast<size_t>(i)],
                                                       tagCount, selector);
        
        // ウィグリーの重み
        const int animationIndex = glyphs[i].clusterIndex >= 0
                                       ? glyphs[i].clusterIndex
                                       : i;
        float wigglyWeight = wiggly.enabled
                                 ? calculateWigglyWeight(animationIndex, time,
                                                        wiggly)
                                 : 1.0f;
        
        // 最終的な「適用度」
        float totalWeight = selectorWeight * wigglyWeight;
        if (i < static_cast<int>(extraWeights.size())) {
            totalWeight *= std::clamp(extraWeights[static_cast<size_t>(i)], 0.0f, 1.0f);
        }
        
        // トラッキング（累積シフト）
        glyphs[i].offsetPosition.setX(glyphs[i].offsetPosition.x() + cumulativeTracking);
        
        const bool clusterEndsHere =
            i + 1 >= n || glyphs[i].clusterIndex < 0 ||
            glyphs[i + 1].clusterIndex != glyphs[i].clusterIndex;
        if (std::abs(totalWeight) < 0.0001f) {
            // トラッキングだけは更新し続ける
            if (clusterEndsHere) {
                cumulativeTracking += props.tracking * totalWeight;
            }
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
        if (clusterEndsHere) {
            cumulativeTracking += props.tracking * totalWeight;
        }

        // カラーとストローク
        if (props.colorEnabled) {
            glyphs[i].hasColorOverride = true;
            glyphs[i].fillColorOverride = props.fillColor;
            glyphs[i].fillColorOverrideWeight =
                std::clamp(std::abs(totalWeight), 0.0f, 1.0f);
        }
        if (props.strokeEnabled) {
            glyphs[i].hasStrokeOverride = true;
            glyphs[i].strokeColorOverride = props.strokeColor;
            glyphs[i].strokeColorOverrideWeight =
                std::clamp(std::abs(totalWeight), 0.0f, 1.0f);
            glyphs[i].offsetStrokeWidth += props.strokeWidth * totalWeight;
        }

        // 特殊効果
        glyphs[i].offsetBlur += props.blur * totalWeight;
    }
}

void TextAnimatorEngine::applyAnimatorStack(
    std::vector<GlyphItem>& glyphs,
    std::span<const std::tuple<RangeSelector, WigglySelector, AnimatorProperties>> stack,
    float time,
    std::span<const float> extraWeights)
{
    if (stack.empty() || glyphs.empty()) {
        return;
    }

    for (const auto& entry : stack) {
        applyAnimator(glyphs,
                      std::get<0>(entry),
                      std::get<1>(entry),
                      std::get<2>(entry),
                      time,
                      extraWeights);
    }
}

void TextAnimatorEngine::applyAnimatorStack(
    std::vector<GlyphItem>& glyphs,
    std::span<const std::tuple<RangeSelector, WigglySelector, AnimatorProperties>> stack,
    float time,
    const QString& sourceText,
    std::span<const float> extraWeights)
{
    if (stack.empty() || glyphs.empty()) {
        return;
    }

    for (const auto& entry : stack) {
        const auto& selector = std::get<0>(entry);
        const SelectorEvaluationContext context{
            sourceText,
            std::span<const GlyphItem>(glyphs.data(), glyphs.size()),
            TextSelectorOrder::Logical};
        const SelectorResult selectorResult = evaluateSelector(context, selector);
        std::vector<float> combinedWeights(glyphs.size(), 1.0f);
        for (size_t i = 0; i < combinedWeights.size(); ++i) {
            const float selectorWeight =
                i < static_cast<size_t>(selectorResult.weights.size())
                    ? selectorResult.weights[static_cast<qsizetype>(i)]
                    : 0.0f;
            const float extraWeight =
                i < extraWeights.size() ? extraWeights[i] : 1.0f;
            combinedWeights[i] =
                std::clamp(selectorWeight * extraWeight, 0.0f, 1.0f);
        }

        RangeSelector fullRange = selector;
        fullRange.start = 0.0f;
        fullRange.end = 100.0f;
        fullRange.offset = 0.0f;
        fullRange.units = SelectorUnits::Percentage;
        fullRange.shape = SelectorShape::Square;
        fullRange.regexEnabled = false;
        fullRange.selectorPattern.clear();
        applyAnimator(glyphs, fullRange, std::get<1>(entry),
                      std::get<2>(entry), time, combinedWeights);
    }
}

} // namespace ArtifactCore
