module;
#include <QRegularExpression>
#include <QHash>
#include <utility>
#include <vector>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <numbers>
#include <tuple>

module Text.Animator;

import Script.Expression.Evaluator;
import Script.Expression.Value;

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

struct SelectorDomainPosition {
    int index = 0;
    int count = 0;
};

SelectorDomainPosition selectorDomainPosition(
    const GlyphItem& glyph, const int glyphIndex, const int glyphCount,
    const int clusterCount, const int lineCount, const int tagIndex,
    const int tagCount, const SelectorUnits units) {
    switch (units) {
        case SelectorUnits::Cluster:
            return {glyph.clusterIndex >= 0 ? glyph.clusterIndex : glyphIndex,
                    clusterCount > 0 ? clusterCount : glyphCount};
        case SelectorUnits::Line:
            return {glyph.lineIndex >= 0 ? glyph.lineIndex : glyphIndex,
                    lineCount > 0 ? lineCount : glyphCount};
        case SelectorUnits::Tag:
            return {tagIndex >= 0 ? tagIndex : glyphIndex,
                    tagCount > 0 ? tagCount : glyphCount};
        case SelectorUnits::Index:
            return {glyphIndex, glyphCount};
        case SelectorUnits::Percentage:
        default:
            // Percentage is the default authoring mode and must not split a
            // grapheme (combining marks, emoji ZWJ, variation selectors).
            return {glyph.clusterIndex >= 0 ? glyph.clusterIndex : glyphIndex,
                    clusterCount > 0 ? clusterCount : glyphCount};
    }
}

std::vector<int> buildDomainOrderRanks(const int totalCount,
                                       const SelectorOrder order) {
    const std::vector<int> orderMap =
        TextAnimatorEngine::createOrderMap(totalCount, order);
    std::vector<int> ranks(orderMap.size());
    for (int rank = 0; rank < static_cast<int>(orderMap.size()); ++rank) {
        const int domainIndex = orderMap[static_cast<size_t>(rank)];
        if (domainIndex >= 0 && domainIndex < totalCount) {
            ranks[static_cast<size_t>(domainIndex)] = rank;
        }
    }
    return ranks;
}

float calculateWeightForGlyphWithOrderRanks(
    const GlyphItem& glyph, const int glyphIndex, const int glyphCount,
    const int clusterCount, const int lineCount, const int tagIndex,
    const int tagCount, const RangeSelector& selector,
    const std::vector<int>& orderRanks) {
    if (selector.regexEnabled && !selector.selectorPattern.isEmpty()) {
        const QRegularExpression regex(selector.selectorPattern);
        const QString haystack = glyph.clusterId + QStringLiteral(" ") +
                                 glyph.selectorTag + QStringLiteral(" ") +
                                 glyph.stableTokenId + QStringLiteral(" ") +
                                 QString::number(glyph.index);
        if (!regex.isValid() || !regex.match(haystack).hasMatch()) {
            return 0.0f;
        }
    }

    SelectorDomainPosition position = selectorDomainPosition(
        glyph, glyphIndex, glyphCount, clusterCount, lineCount, tagIndex,
        tagCount, selector.units);
    if (position.index >= 0 &&
        position.index < static_cast<int>(orderRanks.size())) {
        position.index = orderRanks[static_cast<size_t>(position.index)];
    }

    RangeSelector rangeOnly = selector;
    rangeOnly.order = SelectorOrder::Natural;
    return TextAnimatorEngine::calculateWeight(position.index, position.count,
                                                rangeOnly);
}

FloatRGBA lerpColor(const FloatRGBA& from, const FloatRGBA& to,
                    const float amount) {
    const float t = std::clamp(amount, 0.0f, 1.0f);
    return FloatRGBA(std::lerp(from.r(), to.r(), t),
                     std::lerp(from.g(), to.g(), t),
                     std::lerp(from.b(), to.b(), t),
                     std::lerp(from.a(), to.a(), t));
}

void accumulateColorOverride(bool& enabled, FloatRGBA& color,
                             float& accumulatedWeight,
                             const FloatRGBA& nextColor,
                             const float nextWeight) {
    const float weight = std::clamp(nextWeight, 0.0f, 1.0f);
    if (weight <= 0.0f) {
        return;
    }
    if (!enabled) {
        enabled = true;
        color = nextColor;
        accumulatedWeight = weight;
        return;
    }

    const float previousWeight = std::clamp(accumulatedWeight, 0.0f, 1.0f);
    const float combinedWeight =
        previousWeight + (1.0f - previousWeight) * weight;
    if (combinedWeight <= 0.0f) {
        return;
    }
    color = lerpColor(color, nextColor, weight / combinedWeight);
    accumulatedWeight = combinedWeight;
}

bool isTextGroupSeparator(const char32_t code) {
    return code == U' ' || code == U'\t' || code == U'\n' ||
           code == U'\r';
}

std::vector<QPointF> buildAnchorPoints(
    const std::vector<GlyphItem>& glyphs,
    const AnchorPointGrouping grouping) {
    std::vector<QPointF> anchors(glyphs.size());
    if (glyphs.empty() || grouping == AnchorPointGrouping::Character) {
        for (size_t i = 0; i < glyphs.size(); ++i) {
            anchors[i] = glyphs[i].bounds.center();
        }
        return anchors;
    }

    std::vector<int> groupIds(glyphs.size(), 0);
    int sequentialGroup = 0;
    QString previousSpan;
    for (size_t i = 0; i < glyphs.size(); ++i) {
        const auto& glyph = glyphs[i];
        switch (grouping) {
        case AnchorPointGrouping::Cluster:
            groupIds[i] = glyph.clusterIndex >= 0
                              ? glyph.clusterIndex
                              : static_cast<int>(i);
            break;
        case AnchorPointGrouping::Word:
            if (isTextGroupSeparator(glyph.charCode)) {
                groupIds[i] = -static_cast<int>(i) - 1;
                ++sequentialGroup;
            } else {
                groupIds[i] = sequentialGroup;
            }
            break;
        case AnchorPointGrouping::Line:
            groupIds[i] = glyph.lineIndex >= 0 ? glyph.lineIndex : 0;
            break;
        case AnchorPointGrouping::Paragraph:
            groupIds[i] = sequentialGroup;
            if (glyph.charCode == U'\n' || glyph.charCode == U'\r') {
                ++sequentialGroup;
            }
            break;
        case AnchorPointGrouping::Span:
            if (i > 0 && glyph.selectorTag != previousSpan) {
                ++sequentialGroup;
            }
            groupIds[i] = sequentialGroup;
            previousSpan = glyph.selectorTag;
            break;
        case AnchorPointGrouping::All:
            groupIds[i] = 0;
            break;
        case AnchorPointGrouping::Character:
        default:
            groupIds[i] = static_cast<int>(i);
            break;
        }
    }

    QHash<int, QRectF> groupBounds;
    for (size_t i = 0; i < glyphs.size(); ++i) {
        QRectF bounds = glyphs[i].bounds;
        if (!bounds.isValid()) {
            bounds = QRectF(glyphs[i].basePosition, QSizeF(0.0, 0.0));
        }
        const int groupId = groupIds[i];
        const auto it = groupBounds.constFind(groupId);
        groupBounds.insert(groupId,
                           it == groupBounds.cend()
                               ? bounds
                               : it.value().united(bounds));
    }
    for (size_t i = 0; i < glyphs.size(); ++i) {
        anchors[i] = groupBounds.value(groupIds[i]).center();
    }
    return anchors;
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
    const SelectorDomainPosition orderDomain = selectorDomainPosition(
        context.glyphs.front(), 0, glyphCount, clusterCount, lineCount,
        tagIndices.front(), tagCount, selector.units);
    const std::vector<int> orderRanks =
        buildDomainOrderRanks(orderDomain.count, selector.order);
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
            calculateWeightForGlyphWithOrderRanks(
                context.glyphs[static_cast<size_t>(i)], orderedGlyphIndex,
                glyphCount,
                clusterCount, lineCount, tagIndices[static_cast<size_t>(i)],
                tagCount, rangeOnly, orderRanks);
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

SelectorResult TextAnimatorEngine::evaluateExpressionSelector(
    const SelectorEvaluationContext& context,
    const ExpressionSelector& selector,
    std::span<const float> baseWeights) {
    SelectorResult result;
    result.units = SelectorUnits::Percentage;
    result.order = context.order;
    result.weights.fill(0.0f, static_cast<qsizetype>(context.glyphs.size()));
    if (!selector.enabled || selector.expression.trimmed().isEmpty()) {
        result.diagnostic = QStringLiteral("expression selector is disabled or empty");
        return result;
    }

    ExpressionEvaluator evaluator;
    evaluator.registerStandardFunctions();
    const int total = context.textTotal > 0
        ? context.textTotal
        : static_cast<int>(context.glyphs.size());
    evaluator.setVariable("textTotal", ExpressionValue(static_cast<double>(total)));
    evaluator.setVariable("text", ExpressionValue(context.sourceText.toStdString()));
    evaluator.setVariable("seed", ExpressionValue(static_cast<double>(selector.seed)));
    for (qsizetype index = 0; index < static_cast<qsizetype>(context.glyphs.size()); ++index) {
        evaluator.setVariable("textIndex", ExpressionValue(static_cast<double>(index + 1)));
        const double baseValue = index < baseWeights.size()
            ? static_cast<double>(baseWeights[index]) : 0.0;
        evaluator.setVariable("selectorValue", ExpressionValue(baseValue));
        const ExpressionValue value = evaluator.evaluate(selector.expression.toStdString());
        if (!value.isNumber() || evaluator.hasError() || !std::isfinite(value.asNumber())) {
            result.diagnostic = QStringLiteral("expression selector must return a finite number");
            result.weights.fill(0.0f);
            return result;
        }
        result.weights[index] = std::clamp(static_cast<float>(value.asNumber()), 0.0f, 1.0f);
    }
    return result;
}

float TextAnimatorEngine::calculateWeight(int index, int totalCount, const RangeSelector& selector) {
    if (totalCount <= 0 || index < 0 || index >= totalCount) {
        return 0.0f;
    }
    float position = 0.0f;
    switch (selector.units) {
        case SelectorUnits::Percentage:
            // Percentage selectors span the complete logical domain: the
            // first item is 0% and the last item is 100%.
            position = totalCount > 1
                ? (static_cast<float>(index) /
                   static_cast<float>(totalCount - 1)) * 100.0f
                : 0.0f;
            break;
        case SelectorUnits::Index:
        case SelectorUnits::Cluster:
        case SelectorUnits::Line:
        case SelectorUnits::Tag:
        default:
            position = (float)index;
            break;
    }

    // Start and End are independent user-editable properties and can cross
    // while being edited or animated. Treat the interval as an unordered
    // range so a transient/inverted value does not make the selector select
    // nothing. The selector shape still determines the weight within the
    // normalized interval.
    const float rawStart = selector.start + selector.offset;
    const float rawEnd = selector.end + selector.offset;
    if (!std::isfinite(rawStart) || !std::isfinite(rawEnd)) {
        return 0.0f;
    }
    const float start = std::min(rawStart, rawEnd);
    const float end = std::max(rawStart, rawEnd);
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
    const SelectorDomainPosition orderDomain = selectorDomainPosition(
        glyph, glyphIndex, glyphCount, clusterCount, lineCount, tagIndex,
        tagCount, selector.units);
    const std::vector<int> orderRanks =
        buildDomainOrderRanks(orderDomain.count, selector.order);
    return calculateWeightForGlyphWithOrderRanks(
        glyph, glyphIndex, glyphCount, clusterCount, lineCount, tagIndex,
        tagCount, selector, orderRanks);
}

float TextAnimatorEngine::calculateWigglyWeight(int index, float time, const WigglySelector& selector) {
    if (!selector.enabled) return 1.0f;

    // 簡易的なノイズ生成
    const float safeCorrelation = std::clamp(
        std::isfinite(selector.correlation) ? selector.correlation : 50.0f,
        0.0f, 100.0f);
    const float safeRate = std::isfinite(selector.wigglesPerSecond)
        ? std::max(0.0f, selector.wigglesPerSecond) : 0.0f;
    const float safeTime = std::isfinite(time) ? time : 0.0f;
    const float safePhase = std::isfinite(selector.phase) ? selector.phase : 0.0f;
    const double phaseOffset = static_cast<double>(index) *
                               (100.0 - static_cast<double>(safeCorrelation)) /
                               100.0;
    const double rawT = static_cast<double>(safeTime) *
                            static_cast<double>(safeRate) +
                        phaseOffset + static_cast<double>(safePhase);
    const double t = std::isfinite(rawT) ? rawT : 0.0;
    
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    constexpr double kGeneratorPeriod = 4294967296.0;
    const double floorTick = std::floor(t);
    double wrappedTick = std::fmod(floorTick, kGeneratorPeriod);
    if (wrappedTick < 0.0) {
        wrappedTick += kGeneratorPeriod;
    }
    const auto tick = static_cast<std::uint32_t>(wrappedTick);
    const auto sampleAt = [&](const std::uint32_t sampleTick) {
        std::mt19937 gen(static_cast<std::mt19937::result_type>(
            static_cast<std::uint32_t>(selector.seed) + sampleTick));
        return dis(gen);
    };

    const float v1 = sampleAt(tick);
    const float v2 = sampleAt(tick + 1);
    const float fract = static_cast<float>(t - floorTick);
    
    const float value = v1 + (v2 - v1) *
        (0.5f - 0.5f * std::cos(fract * std::numbers::pi_v<float>));
    // Wiggly is an application weight, not a signed transform multiplier.
    return std::clamp(0.5f + 0.5f * value, 0.0f, 1.0f);
}

std::vector<int> TextAnimatorEngine::createOrderMap(int totalCount,
                                                     SelectorOrder order,
                                                     int seed) {
    if (totalCount <= 0) {
        return {};
    }

    std::vector<int> result(static_cast<size_t>(totalCount));
    std::iota(result.begin(), result.end(), 0);
    switch (order) {
    case SelectorOrder::Reverse:
    case SelectorOrder::RightToLeft:
        std::reverse(result.begin(), result.end());
        break;
    case SelectorOrder::RandomStable: {
        std::mt19937 generator(static_cast<std::mt19937::result_type>(seed));
        std::shuffle(result.begin(), result.end(), generator);
        break;
    }
    case SelectorOrder::CenterOut: {
        const float center = (static_cast<float>(totalCount) - 1.0f) * 0.5f;
        std::stable_sort(result.begin(), result.end(), [center](int lhs, int rhs) {
            const float lhsDistance = std::abs(static_cast<float>(lhs) - center);
            const float rhsDistance = std::abs(static_cast<float>(rhs) - center);
            return lhsDistance == rhsDistance ? lhs < rhs : lhsDistance < rhsDistance;
        });
        break;
    }
    case SelectorOrder::EdgeIn: {
        result.clear();
        result.reserve(static_cast<size_t>(totalCount));
        int left = 0;
        int right = totalCount - 1;
        while (left <= right) {
            result.push_back(left++);
            if (left <= right) {
                result.push_back(right--);
            }
        }
        break;
    }
    case SelectorOrder::Natural:
    case SelectorOrder::LeftToRight:
    default:
        break;
    }
    return result;
}

void TextAnimatorEngine::applyAnimator(
    std::vector<GlyphItem>& glyphs, 
    const RangeSelector& selector, 
    const WigglySelector& wiggly,
    const AnimatorProperties& props,
    float time,
    std::span<const float> extraWeights) {
    
    int n = (int)glyphs.size();
    if (n == 0) {
        return;
    }
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
    const std::vector<QPointF> anchorPoints =
        buildAnchorPoints(glyphs, selector.anchorGrouping);
    const SelectorDomainPosition orderDomain = selectorDomainPosition(
        glyphs.front(), 0, n, clusterCount, lineCount, tagIndices.front(),
        tagCount, selector.units);
    const std::vector<int> orderRanks =
        buildDomainOrderRanks(orderDomain.count, selector.order);

    for (int i = 0; i < n; ++i) {
        // セレクターの重み
        float selectorWeight = calculateWeightForGlyphWithOrderRanks(
            glyphs[i], i, n, clusterCount, lineCount,
            tagIndices[static_cast<size_t>(i)], tagCount, selector,
            orderRanks);
        
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
        if (!std::isfinite(totalWeight)) {
            totalWeight = 0.0f;
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

        // Grouped scale/rotation move the glyph center around the shared
        // anchor while the renderer keeps applying the local transform around
        // the individual glyph center.
        const QPointF glyphCenter = glyphs[i].bounds.center();
        const QPointF relative =
            glyphCenter - anchorPoints[static_cast<size_t>(i)];
        const float weightedScale =
            1.0f + (props.scale - 1.0f) * totalWeight;
        const float radians = props.rotation * totalWeight *
                              std::numbers::pi_v<float> / 180.0f;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        const QPointF scaled(relative.x() * weightedScale,
                             relative.y() * weightedScale);
        const QPointF transformed(scaled.x() * cosine - scaled.y() * sine,
                                  scaled.x() * sine + scaled.y() * cosine);
        glyphs[i].offsetPosition += transformed - relative;

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
            accumulateColorOverride(
                glyphs[i].hasColorOverride, glyphs[i].fillColorOverride,
                glyphs[i].fillColorOverrideWeight, props.fillColor,
                std::abs(totalWeight));
        }
        if (props.strokeEnabled) {
            accumulateColorOverride(
                glyphs[i].hasStrokeOverride, glyphs[i].strokeColorOverride,
                glyphs[i].strokeColorOverrideWeight, props.strokeColor,
                std::abs(totalWeight));
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
        fullRange.order = SelectorOrder::Natural;
        fullRange.regexEnabled = false;
        fullRange.selectorPattern.clear();
        applyAnimator(glyphs, fullRange, std::get<1>(entry),
                      std::get<2>(entry), time, combinedWeights);
    }
}

std::vector<float> TextAnimatorEngine::evaluateAnimatorWeights(
    const SelectorEvaluationContext &context,
    const AnimatorSelectorSet &set,
    std::span<const float> extraWeights)
{
    const SelectorResult rangeResult = evaluateSelector(context, set.range);
    const SelectorResult expressionResult =
        evaluateExpressionSelector(context, set.expression,
                                   rangeResult.weights);

    const size_t glyphCount = context.glyphs.size();
    std::vector<float> combined(glyphCount, 0.0f);
    for (size_t i = 0; i < glyphCount; ++i) {
        const float rangeWeight =
            i < static_cast<size_t>(rangeResult.weights.size())
                ? rangeResult.weights[static_cast<qsizetype>(i)]
                : 0.0f;
        float weight = rangeWeight;
        if (set.expression.enabled) {
            const float expressionWeight =
                i < static_cast<size_t>(expressionResult.weights.size())
                    ? expressionResult.weights[static_cast<qsizetype>(i)]
                    : 1.0f;
            switch (set.combine) {
                case SelectorCombineMode::Add:
                    weight = rangeWeight + expressionWeight;
                    break;
                case SelectorCombineMode::Subtract:
                    weight = rangeWeight - expressionWeight;
                    break;
                case SelectorCombineMode::Min:
                    weight = std::min(rangeWeight, expressionWeight);
                    break;
                case SelectorCombineMode::Max:
                    weight = std::max(rangeWeight, expressionWeight);
                    break;
                case SelectorCombineMode::Multiply:
                default:
                    weight = rangeWeight * expressionWeight;
                    break;
            }
        }
        const float extraWeight =
            i < extraWeights.size() ? extraWeights[i] : 1.0f;
        combined[i] =
            std::clamp(weight * extraWeight, 0.0f, 1.0f);
    }
    return combined;
}

void TextAnimatorEngine::applyAnimatorSets(
    std::vector<GlyphItem>& glyphs,
    std::span<const AnimatorSelectorSet> sets,
    float time,
    const QString& sourceText,
    std::span<const float> extraWeights)
{
    if (sets.empty() || glyphs.empty()) {
        return;
    }

    for (const auto& set : sets) {
        const SelectorEvaluationContext context{
            sourceText,
            std::span<const GlyphItem>(glyphs.data(), glyphs.size()),
            TextSelectorOrder::Logical,
            0,
            static_cast<int>(glyphs.size())};
        const std::vector<float> combinedWeights =
            evaluateAnimatorWeights(context, set, extraWeights);

        // The selector weights already encode the range/shape/order; apply the
        // properties over a full-range square selector.
        RangeSelector fullRange = set.range;
        fullRange.start = 0.0f;
        fullRange.end = 100.0f;
        fullRange.offset = 0.0f;
        fullRange.units = SelectorUnits::Percentage;
        fullRange.shape = SelectorShape::Square;
        fullRange.order = SelectorOrder::Natural;
        fullRange.regexEnabled = false;
        fullRange.selectorPattern.clear();
        applyAnimator(glyphs, fullRange, set.wiggly, set.properties, time,
                      combinedWeights);
    }
}

void TextAnimatorEngine::applyAnimatorStack(
    std::vector<GlyphItem>& glyphs,
    std::span<const std::tuple<RangeSelector, WigglySelector,
                               ExpressionSelector, AnimatorProperties>> stack,
    float time,
    const QString& sourceText,
    std::span<const float> extraWeights)
{
    if (stack.empty() || glyphs.empty()) {
        return;
    }

    std::vector<AnimatorSelectorSet> sets;
    sets.reserve(stack.size());
    for (const auto& entry : stack) {
        AnimatorSelectorSet set;
        set.combine = SelectorCombineMode::Multiply; // legacy behavior
        set.range = std::get<0>(entry);
        set.wiggly = std::get<1>(entry);
        set.expression = std::get<2>(entry);
        set.properties = std::get<3>(entry);
        sets.push_back(std::move(set));
    }
    applyAnimatorSets(glyphs, sets, time, sourceText, extraWeights);
}

} // namespace ArtifactCore
