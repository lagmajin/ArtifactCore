module;

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <QVector>

module Animation.KeyframeEditingTools;

namespace ArtifactCore {

namespace {

// Evaluate interpolated value at a given frame using curve between prev and curr
inline double evaluateSegment(const KeyframePoint& prev,
                               const KeyframePoint& curr,
                               double frame) {
    const double duration = curr.frame - prev.frame;
    if (duration <= 0.0) return prev.value;

    const float alpha = static_cast<float>((frame - prev.frame) / duration);
    if (alpha >= 1.0f) return curr.value;
    if (alpha <= 0.0f) return prev.value;

    if (prev.interpolation == InterpolationType::Constant)
        return prev.value;

    if (prev.interpolation == InterpolationType::Bezier) {
        return static_cast<double>(bezierInterpolate(
            static_cast<float>(prev.value),
            static_cast<float>(curr.value),
            alpha, prev.cp1_x, prev.cp1_y, prev.cp2_x, prev.cp2_y));
    }

    return static_cast<double>(interpolate(
        static_cast<float>(prev.value),
        static_cast<float>(curr.value),
        alpha, prev.interpolation));
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// 1. Thin Keyframes — remove redundant keyframes within tolerance
// ---------------------------------------------------------------------------
bool KeyframeEditingTools::thinKeyframes(
    std::vector<KeyframePoint>& keyframes,
    const ThinKeyframesRequest& request)
{
    if (keyframes.size() < 3) return false;

    std::sort(keyframes.begin(), keyframes.end(),
              [](const KeyframePoint& a, const KeyframePoint& b) {
                  return a.frame < b.frame;
              });

    bool changed = false;
    bool removed;
    do {
        removed = false;
        std::vector<size_t> toRemove;

        // Compute the value range for relative tolerance
        double minVal = keyframes.front().value;
        double maxVal = keyframes.front().value;
        for (const auto& kf : keyframes) {
            minVal = std::min(minVal, kf.value);
            maxVal = std::max(maxVal, kf.value);
        }
        const double valueRange = maxVal - minVal;
        const double effectiveTol = (valueRange > 1e-10)
            ? request.tolerance * valueRange
            : request.tolerance;

        for (size_t i = 1; i + 1 < keyframes.size(); ++i) {
            const auto& prev = keyframes[i - 1];
            const auto& curr = keyframes[i];
            const auto& next = keyframes[i + 1];

            // Evaluate interpolated value at curr.frame using neighbors
            const double interpolated = evaluateSegment(prev, next, curr.frame);
            const double error = std::abs(curr.value - interpolated);

            if (error <= effectiveTol) {
                bool isExtremum = false;
                if (request.preserveExtremes) {
                    const bool rising = prev.value < curr.value && next.value < curr.value;
                    const bool falling = prev.value > curr.value && next.value > curr.value;
                    isExtremum = rising || falling;
                }
                if (!isExtremum) {
                    toRemove.push_back(i);
                }
            }
        }

        // Remove in reverse order to keep indices valid
        std::sort(toRemove.begin(), toRemove.end(), std::greater<size_t>());
        for (size_t idx : toRemove) {
            if (idx < keyframes.size()) {
                keyframes.erase(keyframes.begin() + static_cast<std::ptrdiff_t>(idx));
                removed = true;
                changed = true;
            }
        }
    } while (removed);

    return changed;
}

// ---------------------------------------------------------------------------
// 2. Batch Easing — apply interpolation type to all keyframes
// ---------------------------------------------------------------------------
bool KeyframeEditingTools::applyEasing(
    std::vector<KeyframePoint>& keyframes,
    const BatchEasingRequest& request)
{
    if (keyframes.empty()) return false;
    for (auto& kf : keyframes) {
        kf.interpolation = request.type;
    }
    return true;
}

// ---------------------------------------------------------------------------
// 3. Scale/Offset — transform time and value
// ---------------------------------------------------------------------------
bool KeyframeEditingTools::scaleOffset(
    std::vector<KeyframePoint>& keyframes,
    const ScaleOffsetRequest& request)
{
    if (keyframes.empty()) return false;

    for (auto& kf : keyframes) {
        kf.frame = kf.frame * request.timeScale + request.timeOffset;
        kf.value = kf.value * request.valueScale + request.valueOffset;
    }

    // Re-sort after time transform (may change order)
    std::sort(keyframes.begin(), keyframes.end(),
              [](const KeyframePoint& a, const KeyframePoint& b) {
                  return a.frame < b.frame;
              });

    return true;
}

// ---------------------------------------------------------------------------
// 4. Randomize — add Gaussian noise to values
// ---------------------------------------------------------------------------
bool KeyframeEditingTools::randomize(
    std::vector<KeyframePoint>& keyframes,
    const RandomizeRequest& request)
{
    if (keyframes.empty() || request.amplitude == 0.0) return false;

    std::mt19937 rng(request.seed != 0
        ? static_cast<std::mt19937::result_type>(request.seed)
        : std::mt19937::result_type{std::random_device{}()});
    std::normal_distribution<double> dist(0.0, request.amplitude);

    for (auto& kf : keyframes) {
        kf.value += dist(rng);
    }
    return true;
}

// ---------------------------------------------------------------------------
// 5. Smooth — triangular kernel moving average
// ---------------------------------------------------------------------------
bool KeyframeEditingTools::smooth(
    std::vector<KeyframePoint>& keyframes,
    const SmoothRequest& request)
{
    if (keyframes.size() < 3 || request.windowSize < 2 || request.iterations < 1)
        return false;

    const int half = std::max(1, request.windowSize / 2);

    for (int iter = 0; iter < request.iterations; ++iter) {
        std::vector<double> smoothed(keyframes.size());

        for (size_t i = 0; i < keyframes.size(); ++i) {
            const int start = std::max(0, static_cast<int>(i) - half);
            const int end = std::min(static_cast<int>(keyframes.size()) - 1,
                                     static_cast<int>(i) + half);

            double weightedSum = 0.0;
            double weightSum = 0.0;
            for (int j = start; j <= end; ++j) {
                const double dist = static_cast<double>(std::abs(static_cast<int>(i) - j));
                const double w = 1.0 - dist / (half + 1.0);
                weightedSum += keyframes[static_cast<size_t>(j)].value * w;
                weightSum += w;
            }
            smoothed[i] = (weightSum > 0.0)
                ? weightedSum / weightSum
                : keyframes[i].value;
        }

        for (size_t i = 0; i < keyframes.size(); ++i) {
            keyframes[i].value = smoothed[i];
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// 6. Audio-to-Keyframes — convert audio amplitude to keyframe envelope
// ---------------------------------------------------------------------------
std::vector<KeyframePoint> KeyframeEditingTools::audioToKeyframes(
    const AudioSegment& audio,
    const AudioToKeyframeRequest& request)
{
    std::vector<KeyframePoint> result;

    if (audio.channelData.isEmpty() || audio.channelData[0].isEmpty()) {
        return result;
    }

    const int ch = std::clamp(request.channelIndex, 0,
                              audio.channelCount() - 1);
    const auto& channel = audio.channelData[ch];
    const int totalSamples = channel.size();

    const double endFrame = std::max(request.startFrame + 1.0, request.endFrame);
    const double fps = std::max(1.0, request.frameRate);

    const size_t count = static_cast<size_t>(std::llround(endFrame - request.startFrame)) + 1;
    result.reserve(count);

    const double invFps = 1.0 / fps;

    for (double frame = request.startFrame; frame <= endFrame; frame += 1.0) {
        const double startTime = frame * invFps;
        const double endTime = (frame + 1.0) * invFps;

        const int startSample = static_cast<int>(
            std::llround(startTime * audio.sampleRate));
        const int endSample = static_cast<int>(
            std::llround(endTime * audio.sampleRate));

        const int sampleStart = std::max(0, startSample);
        const int sampleEnd = std::min(totalSamples, endSample);

        double amplitude = 0.0;

        if (sampleEnd > sampleStart) {
            if (request.useRMS) {
                double sumSq = 0.0;
                for (int s = sampleStart; s < sampleEnd; ++s) {
                    const double val = static_cast<double>(channel[s]);
                    sumSq += val * val;
                }
                amplitude = std::sqrt(sumSq / static_cast<double>(sampleEnd - sampleStart));
            } else {
                double peak = 0.0;
                for (int s = sampleStart; s < sampleEnd; ++s) {
                    peak = std::max(peak,
                                    std::abs(static_cast<double>(channel[s])));
                }
                amplitude = peak;
            }
        }

        KeyframePoint kf;
        kf.frame = frame;
        kf.value = request.offset + amplitude * request.amplitudeScale;
        kf.interpolation = InterpolationType::Linear;
        result.push_back(kf);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Helper: evaluate at arbitrary frame using full keyframe list
// ---------------------------------------------------------------------------
double KeyframeEditingTools::interpolateKeyframes(
    const std::vector<KeyframePoint>& keyframes,
    double frame)
{
    if (keyframes.empty()) return 0.0;
    if (keyframes.size() == 1) return keyframes[0].value;

    if (frame <= keyframes.front().frame) return keyframes.front().value;
    if (frame >= keyframes.back().frame) return keyframes.back().value;

    auto it = std::lower_bound(keyframes.begin(), keyframes.end(), frame,
        [](const KeyframePoint& kf, double f) { return kf.frame < f; });

    if (it == keyframes.begin()) return keyframes.front().value;
    if (it == keyframes.end())   return keyframes.back().value;

    const auto& prev = *(it - 1);
    const auto& curr = *it;

    return evaluateSegment(prev, curr, frame);
}

// ---------------------------------------------------------------------------
// 9. Copy Animation Relative — normalize source shape, apply to target range
// ---------------------------------------------------------------------------
std::vector<KeyframePoint> KeyframeEditingTools::copyAnimationRelative(
    const std::vector<KeyframePoint>& source,
    double targetStartFrame,
    double targetEndFrame,
    double targetBaseValue,
    double targetAmplitude)
{
    if (source.empty()) return {};

    const double srcStart = source.front().frame;
    const double srcEnd   = source.back().frame;
    const double srcDuration = std::max(1.0, srcEnd - srcStart);

    double srcMin = source.front().value;
    double srcMax = source.front().value;
    for (const auto& kf : source) {
        srcMin = std::min(srcMin, kf.value);
        srcMax = std::max(srcMax, kf.value);
    }
    const double srcRange = std::max(1e-10, srcMax - srcMin);
    const double tgtDuration = std::max(1.0, targetEndFrame - targetStartFrame);

    std::vector<KeyframePoint> result;
    result.reserve(source.size());

    for (const auto& kf : source) {
        KeyframePoint p;
        const double normTime = (kf.frame - srcStart) / srcDuration;
        p.frame = targetStartFrame + normTime * tgtDuration;

        const double normValue = (kf.value - srcMin) / srcRange;
        p.value = targetBaseValue + normValue * targetAmplitude;

        p.interpolation = kf.interpolation;
        p.cp1_x = kf.cp1_x;
        p.cp1_y = kf.cp1_y;
        p.cp2_x = kf.cp2_x;
        p.cp2_y = kf.cp2_y;
        result.push_back(p);
    }
    return result;
}

// ---------------------------------------------------------------------------
// 4. Quantize to Beat — snap keyframes to nearest beat grid
// ---------------------------------------------------------------------------
bool KeyframeEditingTools::quantizeToBeat(
    std::vector<KeyframePoint>& keyframes,
    const QuantizeToBeatRequest& request)
{
    if (keyframes.empty()) return false;

    const double framesPerBeat = (request.bpm > 0.0)
        ? 60.0 / request.bpm
        : 1.0;

    for (auto& kf : keyframes) {
        if (request.snapAll) {
            const double beat = std::round((kf.frame - request.offset) / framesPerBeat);
            kf.frame = request.offset + beat * framesPerBeat;
        }
        kf.interpolation = request.interpolation;
    }

    // Re-sort in case snap changed ordering
    std::sort(keyframes.begin(), keyframes.end(),
              [](const KeyframePoint& a, const KeyframePoint& b) {
                  return a.frame < b.frame;
              });
    return true;
}

// ---------------------------------------------------------------------------
// 11. Expression-to-Keyframes
// ---------------------------------------------------------------------------
namespace {

// Minimal recursive-descent expression evaluator
// Supports: + - * / ^ % ( ) sin cos tan pow sqrt abs floor ceil round
//           clamp min max lerp random noise wiggle
// Variables: time, frame, fps
class SimpleExprEval {
public:
    explicit SimpleExprEval(const std::string& expr)
        : expr_(expr), pos_(0) {
        tokenize();
    }

    double evaluate() {
        if (tokens_.empty()) return 0.0;
        idx_ = 0;
        return parseExpr();
    }

    std::map<std::string, double> variables;

private:
    enum class TokenType {
        Number, Ident, LParen, RParen, Comma,
        Plus, Minus, Star, Slash, Percent, Caret,
        End, Invalid
    };
    struct Token { TokenType type; std::string text; double value; };

    std::string expr_;
    size_t pos_;
    std::vector<Token> tokens_;
    size_t idx_ = 0;

    void tokenize() {
        while (pos_ < expr_.size()) {
            char c = expr_[pos_];
            if (std::isspace(static_cast<unsigned char>(c))) { ++pos_; continue; }
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
                size_t end = pos_;
                while (end < expr_.size() && (std::isdigit(static_cast<unsigned char>(expr_[end])) || expr_[end] == '.')) ++end;
                tokens_.push_back({TokenType::Number, "", std::stod(expr_.substr(pos_, end - pos_))});
                pos_ = end;
            } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                size_t end = pos_;
                while (end < expr_.size() && (std::isalnum(static_cast<unsigned char>(expr_[end])) || expr_[end] == '_')) ++end;
                tokens_.push_back({TokenType::Ident, expr_.substr(pos_, end - pos_), 0.0});
                pos_ = end;
            } else {
                switch (c) {
                    case '(': tokens_.push_back({TokenType::LParen}); break;
                    case ')': tokens_.push_back({TokenType::RParen}); break;
                    case ',': tokens_.push_back({TokenType::Comma}); break;
                    case '+': tokens_.push_back({TokenType::Plus}); break;
                    case '-': tokens_.push_back({TokenType::Minus}); break;
                    case '*': tokens_.push_back({TokenType::Star}); break;
                    case '/': tokens_.push_back({TokenType::Slash}); break;
                    case '%': tokens_.push_back({TokenType::Percent}); break;
                    case '^': tokens_.push_back({TokenType::Caret}); break;
                    default:  tokens_.push_back({TokenType::Invalid, std::string(1, c)}); break;
                }
                ++pos_;
            }
        }
        tokens_.push_back({TokenType::End});
    }

    Token peek() const { return idx_ < tokens_.size() ? tokens_[idx_] : Token{TokenType::End}; }
    Token advance() { return idx_ < tokens_.size() ? tokens_[idx_++] : Token{TokenType::End}; }
    bool match(TokenType t) {
        if (peek().type == t) { advance(); return true; }
        return false;
    }
    void expect(TokenType t) {
        if (!match(t)) { /* error */ }
    }

    double parseExpr() {
        double val = parseTerm();
        while (peek().type == TokenType::Plus || peek().type == TokenType::Minus) {
            if (match(TokenType::Plus)) val += parseTerm();
            else { advance(); val -= parseTerm(); }
        }
        return val;
    }

    double parseTerm() {
        double val = parsePower();
        while (peek().type == TokenType::Star || peek().type == TokenType::Slash || peek().type == TokenType::Percent) {
            if (match(TokenType::Star)) val *= parsePower();
            else if (match(TokenType::Slash)) { double d = parsePower(); if (d != 0.0) val /= d; }
            else { double d = parsePower(); if (d != 0.0) val = std::fmod(val, d); }
        }
        return val;
    }

    double parsePower() {
        double val = parseUnary();
        if (match(TokenType::Caret)) val = std::pow(val, parsePower());
        return val;
    }

    double parseUnary() {
        if (match(TokenType::Minus)) return -parseUnary();
        if (match(TokenType::Plus)) return parseUnary();
        return parsePrimary();
    }

    double parsePrimary() {
        Token t = peek();
        if (t.type == TokenType::Number) {
            advance();
            return t.value;
        }
        if (t.type == TokenType::Ident) {
            advance();
            if (peek().type == TokenType::LParen) {
                // Function call
                advance(); // consume (
                std::vector<double> args;
                if (peek().type != TokenType::RParen) {
                    args.push_back(parseExpr());
                    while (match(TokenType::Comma))
                        args.push_back(parseExpr());
                }
                expect(TokenType::RParen);
                return callFunc(t.text, args);
            }
            // Variable
            auto it = variables.find(t.text);
            if (it != variables.end()) return it->second;
            return 0.0;
        }
        if (match(TokenType::LParen)) {
            double val = parseExpr();
            expect(TokenType::RParen);
            return val;
        }
        return 0.0;
    }

    static double callFunc(const std::string& name, const std::vector<double>& args) {
        auto safe = [&](size_t i) { return i < args.size() ? args[i] : 0.0; };
        if (name == "sin")  return std::sin(safe(0));
        if (name == "cos")  return std::cos(safe(0));
        if (name == "tan")  return std::tan(safe(0));
        if (name == "sqrt") return std::sqrt(std::max(0.0, safe(0)));
        if (name == "abs")  return std::abs(safe(0));
        if (name == "floor") return std::floor(safe(0));
        if (name == "ceil")  return std::ceil(safe(0));
        if (name == "round") return std::round(safe(0));
        if (name == "pow")   return std::pow(safe(0), safe(1));
        if (name == "min")   return std::min(safe(0), safe(1));
        if (name == "max")   return std::max(safe(0), safe(1));
        if (name == "clamp") return std::max(safe(1), std::min(safe(0), safe(2)));
        if (name == "lerp")  return safe(0) + (safe(1) - safe(0)) * safe(2);
        if (name == "mod")   { double d = safe(1); return d != 0.0 ? std::fmod(safe(0), d) : 0.0; }
        if (name == "log")   return std::log(std::max(1e-10, safe(0)));
        if (name == "exp")   return std::exp(safe(0));
        if (name == "random") {
            static std::mt19937 rng(static_cast<unsigned>(std::random_device{}()));
            double lo = safe(0), hi = safe(1);
            if (hi <= lo) hi = lo + 1.0;
            std::uniform_real_distribution<double> d(lo, hi);
            return d(rng);
        }
        return 0.0;
    }
};

} // anonymous namespace

std::vector<KeyframePoint> KeyframeEditingTools::expressionToKeyframes(
    const std::string& expression,
    double frameRate,
    double startFrame,
    double endFrame,
    std::uint32_t seed)
{
    std::vector<KeyframePoint> result;
    if (expression.empty() || frameRate <= 0.0) return result;

    SimpleExprEval eval(expression);
    eval.variables["fps"] = frameRate;

    const size_t count = static_cast<size_t>(std::max(1.0, endFrame - startFrame + 1.0));
    result.reserve(count);

    // Optional deterministic seed for expressions using random()
    if (seed != 0) {
        eval.variables["_seed"] = static_cast<double>(seed);
    }

    for (double frame = startFrame; frame <= endFrame; frame += 1.0) {
        eval.variables["time"]  = frame / frameRate;
        eval.variables["frame"] = frame;
        eval.variables["index"] = frame - startFrame;

        const double val = eval.evaluate();
        KeyframePoint kf;
        kf.frame = frame;
        kf.value = val;
        kf.interpolation = InterpolationType::Linear;
        result.push_back(kf);
    }

    return result;
}

// ---------------------------------------------------------------------------
// 8. Mirror/Flip — reflect time axis and/or value axis
// ---------------------------------------------------------------------------
bool KeyframeEditingTools::mirrorFlip(
    std::vector<KeyframePoint>& keyframes,
    const MirrorFlipRequest& request)
{
    if (keyframes.empty()) return false;

    std::sort(keyframes.begin(), keyframes.end(),
              [](const KeyframePoint& a, const KeyframePoint& b) {
                  return a.frame < b.frame;
              });

    if (request.mirrorTime) {
        const double start = keyframes.front().frame;
        const double end   = keyframes.back().frame;
        const double center = request.mirrorCenter >= 0.0
            ? request.mirrorCenter
            : (start + end) * 0.5;

        for (auto& kf : keyframes) {
            kf.frame = 2.0 * center - kf.frame;
        }
        std::sort(keyframes.begin(), keyframes.end(),
                  [](const KeyframePoint& a, const KeyframePoint& b) {
                      return a.frame < b.frame;
                  });

        // Swap In/Out easing types so the curve shape stays correct
        for (auto& kf : keyframes) {
            switch (kf.interpolation) {
            case InterpolationType::EaseIn:
                kf.interpolation = InterpolationType::EaseOut; break;
            case InterpolationType::EaseOut:
                kf.interpolation = InterpolationType::EaseIn; break;
            case InterpolationType::EaseInOut:
                break; // symmetric, keep
            case InterpolationType::BackIn:
                kf.interpolation = InterpolationType::BackOut; break;
            case InterpolationType::BackOut:
                kf.interpolation = InterpolationType::BackIn; break;
            case InterpolationType::BounceIn:
                kf.interpolation = InterpolationType::BounceOut; break;
            case InterpolationType::BounceOut:
                kf.interpolation = InterpolationType::BounceIn; break;
            case InterpolationType::ElasticIn:
                kf.interpolation = InterpolationType::ElasticOut; break;
            case InterpolationType::ElasticOut:
                kf.interpolation = InterpolationType::ElasticIn; break;
            default: break;
            }
        }
    }

    if (request.flipValue) {
        double minV = keyframes.front().value;
        double maxV = keyframes.front().value;
        for (const auto& kf : keyframes) {
            minV = std::min(minV, kf.value);
            maxV = std::max(maxV, kf.value);
        }
        const double center = request.flipCenter >= 0.0
            ? request.flipCenter
            : (minV + maxV) * 0.5;

        for (auto& kf : keyframes) {
            kf.value = 2.0 * center - kf.value;
        }
    }

    return true;
}

} // namespace ArtifactCore
