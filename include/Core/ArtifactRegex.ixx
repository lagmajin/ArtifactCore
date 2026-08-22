module;
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

export module Core.ArtifactRegex;

import Core.ArtifactArray;
import Core.ArtifactString;
import Core.ArtifactUtility;

export namespace ArtifactCore {

// Self-contained regular expression engine (backtracking VM).
// Supported syntax: literals, '.', character classes [abc], [^abc], [a-z]
// with \d \w \s escapes inside, shorthand classes \d \D \w \W \s \S, anchors
// ^ $ \b \B, groups (...) and non-capturing (?:...), lookahead (?=...) and
// negative lookahead (?!...), backreferences \1-\9, alternation |,
// quantifiers * + ? {n} {n,} {n,m} with lazy variants (*? etc.), escapes
// \\ \. \n \t \r and literal punctuation. Byte/ASCII semantics.
// Captures: $0 whole match and $1-$9 in replaceAll().
// NOT supported: lookbehind (?<= / (?<!), conditionals, recursion.

inline constexpr std::size_t regexNpos = std::numeric_limits<std::size_t>::max();

enum class RegexErrorCode {
    None = 0,
    EmptyPattern,
    TrailingBackslash,
    MissingClosingParen,
    UnexpectedClosingParen,
    MissingClosingBracket,
    InvalidQuantifier,
    QuantifierTooLarge,
    InvalidEscape,
    TooManyGroups,
    PatternTooLong,
    ProgramTooLarge,
    NestingTooDeep,
};

struct RegexCapture {
    std::size_t position = regexNpos;
    std::size_t length = 0;

    [[nodiscard]] constexpr bool matched() const noexcept { return position != regexNpos; }
};

struct RegexMatch {
    bool matched = false;
    RegexCapture groups[10]; // groups[0] = whole match, [1]-[9] = captures
};

namespace detail {

constexpr std::size_t kRxMaxPatternLength = 16384;
constexpr std::size_t kRxMaxProgramSize = 8192;
constexpr int kRxMaxGroups = 9;
constexpr int kRxMaxNestingDepth = 32;
constexpr int kRxMaxRepeat = 256;
constexpr std::uint64_t kRxStepBudget = 4'000'000;

constexpr bool rxIsWordChar(const char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

struct RxCharClass {
    bool negate = false;
    bool digit = false;
    bool word = false;
    bool space = false;
    struct Range {
        char lo = 0;
        char hi = 0;
    };
    Array<Range> ranges;

    void addChar(const char c) { ranges.append({c, c}); }
    void addRange(const char lo, const char hi) {
        if (lo <= hi) ranges.append({lo, hi});
    }
    [[nodiscard]] bool rawContains(const char c) const noexcept {
        for (const auto& range : ranges) {
            if (c >= range.lo && c <= range.hi) return true;
        }
        return false;
    }
    [[nodiscard]] bool matches(const char c, const bool ignoreCase) const noexcept {
        bool hit = rawContains(c);
        if (!hit && ignoreCase) {
            hit = rawContains(asciiLowerChar(c)) || rawContains(asciiUpperChar(c));
        }
        if (!hit && digit) hit = c >= '0' && c <= '9';
        if (!hit && space) hit = asciiIsSpace(c);
        if (!hit && word) hit = rxIsWordChar(c);
        return negate ? !hit : hit;
    }
};

enum class RxOp : std::uint8_t {
    Char,
    Any,
    Split,
    Jump,
    Save,
    Match,
    AssertStart,
    AssertEnd,
    WordBoundary,
    NotWordBoundary,
    Lookahead, // a = sub-program index, b = negated flag
    Backref,   // a = group number (1-based)
};

struct RxInst {
    RxOp op = RxOp::Match;
    RxCharClass cls;
    std::uint32_t a = 0;
    std::uint32_t b = 0;
};

struct RxNode {
    enum class Kind {
        CharClass,
        Any,
        Concat,
        Alternate,
        Star,
        Plus,
        Opt,
        Repeat,
        Group,
        Lookahead,
        Backref,
        Start,
        End,
        WordBoundary,
        NotWordBoundary,
        Empty,
    };
    Kind kind = Kind::Empty;
    RxCharClass cls;
    Array<RxNode> children;
    int minRepeat = 0;
    int maxRepeat = 0; // -1 = unbounded
    bool greedy = true;
    bool negated = false;   // Lookahead
    int captureSlot = -1;   // Group: base slot / Backref: group number
};

class RxParser {
public:
    RxParser(const StringView pattern, const bool ignoreCase,
             RegexErrorCode& error)
        : pattern_(pattern), ignoreCase_(ignoreCase), error_(error) {}

    [[nodiscard]] RxNode parse() {
        error_ = RegexErrorCode::None;
        if (pattern_.isEmpty()) {
            error_ = RegexErrorCode::EmptyPattern;
            return {};
        }
        if (pattern_.length() > kRxMaxPatternLength) {
            error_ = RegexErrorCode::PatternTooLong;
            return {};
        }
        RxNode root = parseAlternation(0);
        if (error_ != RegexErrorCode::None) return {};
        if (pos_ < pattern_.length()) {
            // Unbalanced ')' at top level.
            error_ = RegexErrorCode::UnexpectedClosingParen;
            return {};
        }
        return root;
    }

    [[nodiscard]] int groupCount() const noexcept { return groupCount_; }

private:
    [[nodiscard]] bool atEnd() const noexcept { return pos_ >= pattern_.length(); }
    [[nodiscard]] char peek(const std::size_t offset = 0) const {
        return pos_ + offset < pattern_.length() ? pattern_.at(pos_ + offset) : '\0';
    }
    [[nodiscard]] char consume() {
        const char c = peek();
        ++pos_;
        return c;
    }
    bool fail(const RegexErrorCode code) {
        if (error_ == RegexErrorCode::None) error_ = code;
        return false;
    }

    [[nodiscard]] RxNode parseAlternation(const int depth) {
        if (depth > kRxMaxNestingDepth) {
            fail(RegexErrorCode::NestingTooDeep);
            return {};
        }
        RxNode alternate;
        alternate.kind = RxNode::Kind::Alternate;
        alternate.children.append(parseSequence(depth));
        if (error_ != RegexErrorCode::None) return {};
        while (!atEnd() && peek() == '|') {
            ++pos_;
            alternate.children.append(parseSequence(depth));
            if (error_ != RegexErrorCode::None) return {};
        }
        if (alternate.children.size() == 1) {
            return alternate.children[0];
        }
        return alternate;
    }

    [[nodiscard]] RxNode parseSequence(const int depth) {
        RxNode sequence;
        sequence.kind = RxNode::Kind::Concat;
        while (!atEnd() && peek() != '|' && peek() != ')') {
            sequence.children.append(parseRepeat(depth));
            if (error_ != RegexErrorCode::None) return {};
        }
        if (sequence.children.isEmpty()) {
            sequence.kind = RxNode::Kind::Empty;
        }
        return sequence;
    }

    [[nodiscard]] RxNode parseRepeat(const int depth) {
        RxNode atom = parseAtom(depth);
        if (error_ != RegexErrorCode::None) return {};
        if (atEnd()) return atom;

        RxNode result = atom;
        bool done = false;
        while (!done && !atEnd()) {
            const char c = peek();
            int minRepeat = 0;
            int maxRepeat = 0;
            bool hasQuantifier = true;
            switch (c) {
            case '*': minRepeat = 0; maxRepeat = -1; ++pos_; break;
            case '+': minRepeat = 1; maxRepeat = -1; ++pos_; break;
            case '?': minRepeat = 0; maxRepeat = 1; ++pos_; break;
            case '{':
                if (!parseBraceQuantifier(minRepeat, maxRepeat)) return atom;
                break;
            default: hasQuantifier = false; break;
            }
            if (!hasQuantifier) break;

            bool greedy = true;
            if (!atEnd() && peek() == '?') {
                greedy = false;
                ++pos_;
            }
            if (minRepeat > kRxMaxRepeat || maxRepeat > kRxMaxRepeat ||
                (maxRepeat >= 0 && minRepeat > maxRepeat)) {
                fail(RegexErrorCode::QuantifierTooLarge);
                return {};
            }

            RxNode repeated;
            if (minRepeat == 0 && maxRepeat == -1) {
                repeated.kind = RxNode::Kind::Star;
                repeated.children.append(artifactMove(result));
            } else if (minRepeat == 1 && maxRepeat == -1) {
                repeated.kind = RxNode::Kind::Plus;
                repeated.children.append(artifactMove(result));
            } else if (minRepeat == 0 && maxRepeat == 1) {
                repeated.kind = RxNode::Kind::Opt;
                repeated.children.append(artifactMove(result));
            } else {
                repeated.kind = RxNode::Kind::Repeat;
                repeated.minRepeat = minRepeat;
                repeated.maxRepeat = maxRepeat;
                repeated.children.append(artifactMove(result));
            }
            repeated.greedy = greedy;
            result = artifactMove(repeated);

            // A second quantifier directly applied ("a**") is invalid.
            if (!atEnd() &&
                (peek() == '*' || peek() == '+' ||
                 (peek() == '{'))) {
                // Allow lazy marker already consumed above; anything else here
                // means stacked quantifiers.
                if (peek() == '*' || peek() == '+') {
                    fail(RegexErrorCode::InvalidQuantifier);
                    return {};
                }
            }
            static_cast<void>(done);
            done = true; // one quantifier per atom; loop kept for clarity
        }
        return result;
    }

    [[nodiscard]] bool parseBraceQuantifier(int& minRepeat, int& maxRepeat) {
        const std::size_t saved = pos_;
        ++pos_; // '{'
        auto readNumber = [this](int& out) {
            int parsed = 0;
            bool any = false;
            while (!atEnd() && peek() >= '0' && peek() <= '9') {
                parsed = parsed * 10 + (consume() - '0');
                if (parsed > kRxMaxRepeat * 10) {
                    return false;
                }
                any = true;
            }
            out = parsed;
            return any;
        };
        int low = 0;
        int high = 0;
        if (!readNumber(low)) {
            pos_ = saved;
            return false; // treat '{' as literal
        }
        if (!atEnd() && peek() == ',') {
            ++pos_;
            if (!atEnd() && peek() == '}') {
                high = -1; // {n,}
            } else if (!readNumber(high)) {
                pos_ = saved;
                return false;
            }
        } else {
            high = low;
        }
        if (atEnd() || peek() != '}') {
            pos_ = saved;
            return false;
        }
        ++pos_;
        if (high >= 0 && low > high) {
            fail(RegexErrorCode::InvalidQuantifier);
            return false;
        }
        minRepeat = low;
        maxRepeat = high;
        return true;
    }

    [[nodiscard]] RxNode parseAtom(const int depth) {
        RxNode node;
        const char c = peek();
        switch (c) {
        case '(':
            return parseGroup(depth);
        case '[':
            ++pos_;
            node.kind = RxNode::Kind::CharClass;
            if (!parseBracketClass(node.cls)) {
                return {};
            }
            return node;
        case '.':
            ++pos_;
            node.kind = RxNode::Kind::Any;
            return node;
        case '^':
            ++pos_;
            node.kind = RxNode::Kind::Start;
            return node;
        case '$':
            ++pos_;
            node.kind = RxNode::Kind::End;
            return node;
        case '\\':
            ++pos_;
            return parseEscape();
        default:
            ++pos_;
            node.kind = RxNode::Kind::CharClass;
            node.cls.addChar(c);
            return node;
        }
    }

    [[nodiscard]] RxNode parseGroup(const int depth) {
        ++pos_; // '('
        RxNode group;
        bool capturing = true;
        if (!atEnd() && peek() == '?') {
            const char modifier =
                pos_ + 1 < pattern_.length() ? pattern_.at(pos_ + 1) : '\0';
            if (modifier == ':') {
                capturing = false;
                pos_ += 2;
            } else if (modifier == '=' || modifier == '!') {
                // Lookahead assertions; zero-width, non-capturing.
                RxNode lookahead;
                lookahead.kind = RxNode::Kind::Lookahead;
                lookahead.negated = modifier == '!';
                pos_ += 2;
                lookahead.children.append(parseAlternation(depth + 1));
                if (error_ != RegexErrorCode::None) return {};
                if (atEnd() || peek() != ')') {
                    fail(RegexErrorCode::MissingClosingParen);
                    return {};
                }
                ++pos_;
                return lookahead;
            } else {
                fail(RegexErrorCode::InvalidEscape);
                return {};
            }
        }
        int slot = -1;
        if (capturing) {
            ++groupCount_;
            if (groupCount_ > kRxMaxGroups) {
                fail(RegexErrorCode::TooManyGroups);
                return {};
            }
            slot = groupCount_;
            group.kind = RxNode::Kind::Group;
            group.captureSlot = slot;
        }
        RxNode body = parseAlternation(depth + 1);
        if (error_ != RegexErrorCode::None) return {};
        if (atEnd() || peek() != ')') {
            fail(RegexErrorCode::MissingClosingParen);
            return {};
        }
        ++pos_;

        if (!capturing) {
            return body;
        }
        group.children.append(artifactMove(body));
        return group;
    }

    [[nodiscard]] RxNode parseEscape() {
        if (atEnd()) {
            fail(RegexErrorCode::TrailingBackslash);
            return {};
        }
        const char c = consume();
        RxNode node;
        node.kind = RxNode::Kind::CharClass;
        switch (c) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            // Backreference; the group must have opened already.
            const int groupNumber = c - '0';
            if (groupNumber > groupCount_) {
                fail(RegexErrorCode::InvalidEscape);
                return {};
            }
            node.kind = RxNode::Kind::Backref;
            node.captureSlot = groupNumber;
            return node;
        }
        case 'd': node.cls.digit = true; return node;
        case 'D': node.cls.digit = true; node.cls.negate = true; return node;
        case 'w': node.cls.word = true; return node;
        case 'W': node.cls.word = true; node.cls.negate = true; return node;
        case 's': node.cls.space = true; return node;
        case 'S': node.cls.space = true; node.cls.negate = true; return node;
        case 'b':
            node.kind = RxNode::Kind::WordBoundary;
            return node;
        case 'B':
            node.kind = RxNode::Kind::NotWordBoundary;
            return node;
        case 'n': node.cls.addChar('\n'); return node;
        case 't': node.cls.addChar('\t'); return node;
        case 'r': node.cls.addChar('\r'); return node;
        case 'f': node.cls.addChar('\f'); return node;
        case 'v': node.cls.addChar('\v'); return node;
        case '0': node.cls.addChar('\0'); return node;
        default:
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                fail(RegexErrorCode::InvalidEscape);
                return {};
            }
            // Escaped punctuation and '\\' are literals.
            node.cls.addChar(c);
            return node;
        }
    }

    [[nodiscard]] bool parseBracketClass(RxCharClass& cls) {
        if (!atEnd() && peek() == '^') {
            cls.negate = true;
            ++pos_;
        }
        bool first = true;
        while (true) {
            if (atEnd()) {
                return fail(RegexErrorCode::MissingClosingBracket);
            }
            const char c = peek();
            if (c == ']' && !first) {
                ++pos_;
                break;
            }
            first = false;
            ++pos_;
            char lo = c;
            if (c == '\\') {
                if (atEnd()) return fail(RegexErrorCode::TrailingBackslash);
                const char escaped = consume();
                switch (escaped) {
                case 'd': cls.digit = true; continue;
                case 'w': cls.word = true; continue;
                case 's': cls.space = true; continue;
                case 'n': lo = '\n'; break;
                case 't': lo = '\t'; break;
                case 'r': lo = '\r'; break;
                default: lo = escaped; break;
                }
            }
            if (!atEnd() && peek() == '-' &&
                pos_ + 1 < pattern_.length() && pattern_.at(pos_ + 1) != ']') {
                ++pos_; // '-'
                char hi = consume();
                if (hi == '\\') {
                    if (atEnd()) return fail(RegexErrorCode::TrailingBackslash);
                    hi = consume();
                }
                cls.addRange(lo, hi);
            } else {
                cls.addChar(lo);
            }
        }
        return true;
    }

    StringView pattern_;
    std::size_t pos_ = 0;
    bool ignoreCase_ = false;
    RegexErrorCode& error_;
    int groupCount_ = 0;
};

class RxEmitter {
public:
    explicit RxEmitter(Array<Array<RxInst>>& lookaheadPrograms)
        : lookaheadPrograms_(lookaheadPrograms) {}

    [[nodiscard]] bool emit(const RxNode& node, Array<RxInst>& out) {
        target_ = &out;
        return emitNode(node);
    }
    [[nodiscard]] bool overflowed() const noexcept { return overflowed_; }

private:
    [[nodiscard]] std::uint32_t emitInst(const RxInst& inst) {
        if (target_->size() >= kRxMaxProgramSize) {
            overflowed_ = true;
            return 0;
        }
        target_->append(inst);
        return static_cast<std::uint32_t>(target_->size() - 1);
    }

    [[nodiscard]] std::uint32_t currentSize() const noexcept {
        return static_cast<std::uint32_t>(target_->size());
    }

    [[nodiscard]] bool emitNode(const RxNode& node) {
        using K = RxNode::Kind;
        switch (node.kind) {
        case K::Empty:
            return true;
        case K::Backref: {
            RxInst inst;
            inst.op = RxOp::Backref;
            inst.a = static_cast<std::uint32_t>(node.captureSlot);
            emitInst(inst);
            return !overflowed_;
        }
        case K::Lookahead: {
            lookaheadPrograms_->append({});
            Array<RxInst>& sub =
                (*lookaheadPrograms_)[static_cast<int>(lookaheadPrograms_->size() - 1)];
            Array<RxInst>* previousTarget = target_;
            target_ = &sub;
            const bool ok = emitNode(node.children[0]);
            target_ = previousTarget;
            if (!ok) return false;
            RxInst subMatch;
            subMatch.op = RxOp::Match;
            sub.append(subMatch);
            RxInst call;
            call.op = RxOp::Lookahead;
            call.a = static_cast<std::uint32_t>(lookaheadPrograms_->size() - 1);
            call.b = node.negated ? 1u : 0u;
            emitInst(call);
            return !overflowed_;
        }
        case K::CharClass: {
            RxInst inst;
            inst.op = RxOp::Char;
            inst.cls = node.cls;
            emitInst(inst);
            return !overflowed_;
        }
        case K::Any: {
            RxInst inst;
            inst.op = RxOp::Any;
            emitInst(inst);
            return !overflowed_;
        }
        case K::Start: {
            RxInst inst;
            inst.op = RxOp::AssertStart;
            emitInst(inst);
            return !overflowed_;
        }
        case K::End: {
            RxInst inst;
            inst.op = RxOp::AssertEnd;
            emitInst(inst);
            return !overflowed_;
        }
        case K::WordBoundary: {
            RxInst inst;
            inst.op = RxOp::WordBoundary;
            emitInst(inst);
            return !overflowed_;
        }
        case K::NotWordBoundary: {
            RxInst inst;
            inst.op = RxOp::NotWordBoundary;
            emitInst(inst);
            return !overflowed_;
        }
        case K::Concat:
            for (const auto& child : node.children) {
                if (!emitNode(child)) return false;
            }
            return true;
        case K::Alternate: {
            Array<std::uint32_t> jumps;
            Array<std::uint32_t> splits;
            const std::size_t branchCount = node.children.size();
            for (std::size_t i = 0; i < branchCount; ++i) {
                if (i + 1 < branchCount) {
                    RxInst split;
                    split.op = RxOp::Split;
                    splits.append(static_cast<std::uint32_t>(emitInst(split)));
                }
                if (!emitNode(node.children[i])) return false;
                if (i + 1 < branchCount) {
                    RxInst jump;
                    jump.op = RxOp::Jump;
                    jumps.append(static_cast<std::uint32_t>(emitInst(jump)));
                }
            }
            const auto endPosition =
                static_cast<std::uint32_t>(target_->size());
            for (const std::uint32_t jump : jumps) {
                (*target_)[static_cast<int>(jump)].a = endPosition;
            }
            for (const std::uint32_t split : splits) {
                (*target_)[static_cast<int>(split)].a = split + 1;
                (*target_)[static_cast<int>(split)].b = endPosition;
            }
            return true;
        }
        case K::Star:
        case K::Opt: {
            const bool star = node.kind == K::Kind::Star;
            RxInst split;
            split.op = RxOp::Split;
            if (node.greedy) {
                split.a = 0;                  // body (patched below / next inst)
                split.b = 0;                  // exit (patched below)
            }
            const std::uint32_t splitPos = emitInst(split);
            const std::uint32_t bodyStart = static_cast<std::uint32_t>(target_->size());
            if (!emitNode(node.children[0])) return false;
            if (star) {
                RxInst jump;
                jump.op = RxOp::Jump;
                jump.a = splitPos;
                emitInst(jump);
            }
            const auto exitPosition =
                static_cast<std::uint32_t>(target_->size());
            if (node.greedy) {
                (*target_)[static_cast<int>(splitPos)].a = bodyStart;
                (*target_)[static_cast<int>(splitPos)].b = exitPosition;
            } else {
                (*target_)[static_cast<int>(splitPos)].a = exitPosition;
                (*target_)[static_cast<int>(splitPos)].b = bodyStart;
            }
            return true;
        }
        case K::Plus: {
            const std::uint32_t bodyStart =
                static_cast<std::uint32_t>(target_->size());
            if (!emitNode(node.children[0])) return false;
            RxInst split;
            split.op = RxOp::Split;
            const std::uint32_t splitPos = emitInst(split);
            const auto exitPosition =
                static_cast<std::uint32_t>(target_->size());
            if (node.greedy) {
                (*target_)[static_cast<int>(splitPos)].a = bodyStart;
                (*target_)[static_cast<int>(splitPos)].b = exitPosition;
            } else {
                (*target_)[static_cast<int>(splitPos)].a = exitPosition;
                (*target_)[static_cast<int>(splitPos)].b = bodyStart;
            }
            return true;
        }
        case K::Repeat: {
            const RxNode& body = node.children[0];
            const bool finite = node.maxRepeat >= 0;
            const int required = node.minRepeat;
            const int optionalCount =
                finite ? node.maxRepeat - node.minRepeat : 0;
            // Guard against combinatorial program blow-up.
            if (!finite && required > kRxMaxRepeat) return false;
            if (finite &&
                (node.maxRepeat > kRxMaxRepeat ||
                 required + optionalCount > kRxMaxRepeat)) {
                return false;
            }
            for (int i = 0; i < required; ++i) {
                if (!emitNode(body)) return false;
            }
            if (!finite) {
                // Remaining part behaves like a star.
                RxInst split;
                split.op = RxOp::Split;
                const std::uint32_t splitPos = emitInst(split);
                const std::uint32_t bodyStart =
                    static_cast<std::uint32_t>(target_->size());
                if (!emitNode(body)) return false;
                RxInst jump;
                jump.op = RxOp::Jump;
                jump.a = splitPos;
                emitInst(jump);
                const auto exitPosition =
                    static_cast<std::uint32_t>(target_->size());
                if (node.greedy) {
                    (*target_)[static_cast<int>(splitPos)].a = bodyStart;
                    (*target_)[static_cast<int>(splitPos)].b = exitPosition;
                } else {
                    (*target_)[static_cast<int>(splitPos)].a = exitPosition;
                    (*target_)[static_cast<int>(splitPos)].b = bodyStart;
                }
            } else {
                for (int i = 0; i < optionalCount; ++i) {
                    // Each remaining copy may match once or be skipped.
                    RxInst split;
                    split.op = RxOp::Split;
                    const std::uint32_t splitPos = emitInst(split);
                    const std::uint32_t bodyStart =
                        static_cast<std::uint32_t>(target_->size());
                    if (!emitNode(body)) return false;
                    const auto exitPosition =
                        static_cast<std::uint32_t>(target_->size());
                    // Greedy prefers matching earlier copies first.
                    if (node.greedy) {
                        (*target_)[static_cast<int>(splitPos)].a = bodyStart;
                        (*target_)[static_cast<int>(splitPos)].b = exitPosition;
                    } else {
                        (*target_)[static_cast<int>(splitPos)].a = exitPosition;
                        (*target_)[static_cast<int>(splitPos)].b = bodyStart;
                    }
                }
            }
            return true;
        }
        case K::Group: {
            RxInst save;
            save.op = RxOp::Save;
            save.a = static_cast<std::uint32_t>(node.captureSlot * 2);
            emitInst(save);
            if (!emitNode(node.children[0])) return false;
            save.a = static_cast<std::uint32_t>(node.captureSlot * 2 + 1);
            emitInst(save);
            return true;
        }
        }
        return false;
    }

    Array<RxInst>* target_ = nullptr;
    bool overflowed_ = false;
};

struct RxVmContext {
    const RxInst* program = nullptr;
    int programSize = 0;
    const Array<RxInst>* lookaheadPrograms = nullptr;
    const char* text = nullptr;
    std::size_t textLength = 0;
    bool ignoreCase = false;
    std::uint32_t slots[(kRxMaxGroups + 1) * 2]{};
    std::uint64_t stepsLeft = kRxStepBudget;
    bool budgetExceeded = false;

    struct UndoEntry {
        std::uint32_t slot;
        std::uint32_t previous;
    };

    struct BacktrackEntry {
        std::uint32_t pc;
        std::uint32_t sp;
        std::size_t undoMark;
    };

    Array<UndoEntry> undoLog;
    Array<BacktrackEntry> stack;

    void rollback(const std::size_t mark) {
        while (undoLog.size() > mark) {
            const auto entry = undoLog[undoLog.size() - 1];
            slots[entry.slot] = entry.previous;
            undoLog.removeAt(undoLog.size() - 1);
        }
    }

    void resetSlots() {
        for (auto& slot : slots) {
            slot = static_cast<std::uint32_t>(regexNpos);
        }
    }

    // Executes `program` from `startSp`. Returns true when a Match
    // instruction is reached, with capture positions written into `slots`.
    // stack/undoLog are per-invocation so lookaheads nest safely.
    [[nodiscard]] bool execute(const RxInst* program, const int programSize,
                               const std::uint32_t startSp,
                               std::uint32_t* slots) {
        stack.removeAll();
        undoLog.removeAll();

        std::uint32_t pc = 0;
        std::uint32_t sp = startSp;

        while (true) {
            bool backtrack = false;
            bool matched = false;
            while (true) {
                if (stepsLeft == 0) {
                    budgetExceeded = true;
                    return false;
                }
                --stepsLeft;
                if (pc >= static_cast<std::uint32_t>(programSize)) {
                    backtrack = true;
                    break;
                }
                const RxInst& inst = program[static_cast<int>(pc)];
                switch (inst.op) {
                case RxOp::Char:
                    if (sp < textLength &&
                        inst.cls.matches(text[sp], ignoreCase)) {
                        ++sp;
                        ++pc;
                    } else {
                        backtrack = true;
                    }
                    break;
                case RxOp::Backref: {
                    const std::uint32_t groupNumber = inst.a;
                    const std::uint32_t beginSlot =
                        slots[groupNumber * 2];
                    const std::uint32_t endSlot =
                        slots[groupNumber * 2 + 1];
                    if (beginSlot == kRxSlotUnset ||
                        endSlot == kRxSlotUnset || endSlot < beginSlot) {
                        backtrack = true;
                        break;
                    }
                    const std::uint32_t length = endSlot - beginSlot;
                    if (sp + length > textLength) {
                        backtrack = true;
                        break;
                    }
                    bool equal = true;
                    for (std::uint32_t i = 0; i < length; ++i) {
                        char patternChar = text[beginSlot + i];
                        char textChar = text[sp + i];
                        if (ignoreCase) {
                            patternChar = asciiLowerChar(patternChar);
                            textChar = asciiLowerChar(textChar);
                        }
                        if (patternChar != textChar) {
                            equal = false;
                            break;
                        }
                    }
                    if (!equal) {
                        backtrack = true;
                        break;
                    }
                    sp += length;
                    ++pc;
                    break;
                }
                case RxOp::Lookahead: {
                    // Zero-width sub-match. Slots are shared so captures made
                    // inside a positive lookahead persist; negative lookaheads
                    // always discard slot changes.
                    std::uint32_t saved[(kRxMaxGroups + 1) * 2];
                    for (int i = 0; i < (kRxMaxGroups + 1) * 2; ++i) {
                        saved[i] = slots[i];
                    }
                    const auto& subProgram =
                        (*lookaheadPrograms)[static_cast<int>(inst.a)];
                    const bool matchedSub =
                        !subProgram.isEmpty() &&
                        execute(subProgram.data(),
                                static_cast<int>(subProgram.size()), sp,
                                slots);
                    const bool proceed = inst.b ? !matchedSub : matchedSub;
                    if (!proceed || inst.b) {
                        for (int i = 0; i < (kRxMaxGroups + 1) * 2; ++i) {
                            slots[i] = saved[i];
                        }
                    }
                    if (!proceed) {
                        backtrack = true;
                        break;
                    }
                    ++pc;
                    break;
                }
                case RxOp::Any:
                    if (sp < textLength && text[sp] != '\n') {
                        ++sp;
                        ++pc;
                    } else {
                        backtrack = true;
                    }
                    break;
                case RxOp::Split:
                    stack.append({inst.b, sp, undoLog.size()});
                    pc = inst.a;
                    break;
                case RxOp::Jump:
                    pc = inst.a;
                    break;
                case RxOp::Save:
                    undoLog.append({inst.a, slots[inst.a]});
                    slots[inst.a] = sp;
                    ++pc;
                    break;
                case RxOp::Match:
                    matched = true;
                    break;
                case RxOp::AssertStart:
                    if (sp == 0) { ++pc; } else { backtrack = true; }
                    break;
                case RxOp::AssertEnd:
                    if (sp == textLength) { ++pc; } else { backtrack = true; }
                    break;
                case RxOp::WordBoundary: {
                    const bool before =
                        sp > 0 && rxIsWordChar(text[sp - 1]);
                    const bool after =
                        sp < textLength && rxIsWordChar(text[sp]);
                    if (before != after) { ++pc; } else { backtrack = true; }
                    break;
                }
                case RxOp::NotWordBoundary: {
                    const bool before =
                        sp > 0 && rxIsWordChar(text[sp - 1]);
                    const bool after =
                        sp < textLength && rxIsWordChar(text[sp]);
                    if (before == after) { ++pc; } else { backtrack = true; }
                    break;
                }
                }
                if (matched || backtrack) break;
            }
            if (matched) return true;

            bool resumed = false;
            while (!stack.isEmpty()) {
                const auto entry = stack[stack.size() - 1];
                stack.removeAt(stack.size() - 1);
                rollback(entry.undoMark);
                pc = entry.pc;
                sp = entry.sp;
                resumed = true;
                break;
            }
            if (!resumed) return false;
        }
    }
};

inline constexpr std::uint32_t kRxSlotUnset =
    static_cast<std::uint32_t>(regexNpos & 0xFFFFFFFFu);

} // namespace detail

// ---- public API ----

class ArtifactRegex {
public:
    ArtifactRegex() = default;

    explicit ArtifactRegex(const StringView pattern, const bool ignoreCase = false,
                           RegexErrorCode* error = nullptr) {
        compile(pattern, ignoreCase, error);
    }

    [[nodiscard]] bool compile(const StringView pattern,
                               const bool ignoreCase = false,
                               RegexErrorCode* error = nullptr) {
        pattern_ = String(pattern.data(), pattern.length());
        ignoreCase_ = ignoreCase;
        anchored_ = !pattern_.isEmpty() && pattern_.data()[0] == '^';
        errorCode_ = RegexErrorCode::None;
        program_.removeAll();
        lookaheadPrograms_.removeAll();

        RegexErrorCode localError = RegexErrorCode::None;
        detail::RxParser parser(pattern, ignoreCase, localError);
        const detail::RxNode root = parser.parse();
        if (localError != RegexErrorCode::None) {
            errorCode_ = localError;
            if (error) *error = localError;
            return false;
        }

        Array<RxInst> mainProgram;
        detail::RxEmitter emitter(lookaheadPrograms_);
        detail::RxInst prologue;
        prologue.op = detail::RxOp::Save;
        prologue.a = 0;
        mainProgram.append(prologue);
        if (!emitter.emit(root, mainProgram)) {
            errorCode_ = RegexErrorCode::ProgramTooLarge;
            if (error) *error = errorCode_;
            return false;
        }
        detail::RxInst epilogue;
        epilogue.op = detail::RxOp::Save;
        epilogue.a = 1;
        mainProgram.append(epilogue);
        detail::RxInst match;
        match.op = detail::RxOp::Match;
        mainProgram.append(match);
        if (emitter.overflowed()) {
            errorCode_ = RegexErrorCode::ProgramTooLarge;
            if (error) *error = errorCode_;
            return false;
        }

        program_ = artifactMove(mainProgram);
        groupCount_ = parser.groupCount();
        if (error) *error = RegexErrorCode::None;
        return true;
    }

    [[nodiscard]] bool isValid() const noexcept { return !program_.isEmpty(); }
    [[nodiscard]] RegexErrorCode errorCode() const noexcept { return errorCode_; }
    [[nodiscard]] int groupCount() const noexcept { return groupCount_; }

    // Searches anywhere in `text` starting at byte offset `from`.
    [[nodiscard]] RegexMatch search(const StringView text,
                                    const std::size_t from = 0) const {
        RegexMatch result;
        if (!isValid() || from > text.length()) {
            return result;
        }
        detail::RxVmContext vm;
        vm.program = program_.data();
        vm.programSize = static_cast<int>(program_.size());
        vm.lookaheadPrograms = &lookaheadPrograms_;
        vm.text = text.data();
        vm.textLength = text.length();
        vm.ignoreCase = ignoreCase_;

        const std::size_t lastStart =
            anchored_ ? from : text.length();
        for (std::size_t start = from; start <= lastStart; ++start) {
            vm.resetSlots();
            vm.stepsLeft = kRxStepBudget;
            if (vm.execute(vm.program, vm.programSize,
                           static_cast<std::uint32_t>(start), vm.slots)) {
                result.matched = true;
                const int pairs = groupCount_ + 1 < 10 ? groupCount_ + 1 : 10;
                for (int g = 0; g < pairs; ++g) {
                    const std::uint32_t beginSlot =
                        vm.slots[g * 2];
                    const std::uint32_t endSlot =
                        vm.slots[g * 2 + 1];
                    if (beginSlot != kRxSlotUnset &&
                        endSlot != kRxSlotUnset && beginSlot <= endSlot) {
                        result.groups[g].position = beginSlot;
                        result.groups[g].length = endSlot - beginSlot;
                    } else {
                        result.groups[g] = RegexCapture{};
                    }
                }
                return result;
            }
            if (vm.budgetExceeded) {
                break;
            }
        }
        return result;
    }

    [[nodiscard]] bool matches(const StringView text) const {
        return search(text).matched;
    }

    [[nodiscard]] Array<RegexMatch> findAll(const StringView text) const {
        Array<RegexMatch> results;
        std::size_t cursor = 0;
        while (cursor <= text.length()) {
            const RegexMatch match = search(text, cursor);
            if (!match.matched) break;
            results.append(match);
            const std::size_t end =
                match.groups[0].position + match.groups[0].length;
            cursor = end > cursor ? end : cursor + 1;
        }
        return results;
    }

    // `$0`-`$9` in `replacement` refer to captures; `$$` is a literal '$'.
    [[nodiscard]] String replaceAll(const StringView text,
                                    const StringView replacement) const {
        String out;
        std::size_t cursor = 0;
        while (cursor <= text.length()) {
            const RegexMatch match = search(text, cursor);
            if (!match.matched) break;
            const auto beginPos = match.groups[0].position;
            out += StringView(text.data() + cursor,
                              beginPos - cursor);
            appendReplacement(out, replacement, text, match);
            const std::size_t end = beginPos + match.groups[0].length;
            if (end == beginPos) {
                // Zero-width match: emit one character to make progress.
                if (end < text.length()) {
                    out += StringView(text.data() + end, 1);
                }
                cursor = end + 1;
            } else {
                cursor = end;
            }
        }
        if (cursor < text.length()) {
            out += StringView(text.data() + cursor, text.length() - cursor);
        }
        return out;
    }

private:
    static void appendReplacement(String& out, const StringView replacement,
                                  const StringView text,
                                  const RegexMatch& match) {
        for (std::size_t i = 0; i < replacement.length(); ++i) {
            const char c = replacement.at(i);
            if (c != '$') {
                out += c;
                continue;
            }
            if (i + 1 < replacement.length()) {
                const char next = replacement.at(i + 1);
                if (next >= '0' && next <= '9') {
                    ++i;
                    const int groupIndex = next - '0';
                    const auto& capture = match.groups[groupIndex];
                    if (capture.matched() && capture.length > 0) {
                        out += StringView(text.data() + capture.position,
                                          capture.length);
                    }
                    continue;
                }
                if (next == '$') {
                    ++i;
                    out += '$';
                    continue;
                }
            }
            out += '$';
        }
    }

    Array<RxInst> program_;
    Array<Array<RxInst>> lookaheadPrograms_;
    String pattern_;
    bool ignoreCase_ = false;
    bool anchored_ = false;
    int groupCount_ = 0;
    RegexErrorCode errorCode_ = RegexErrorCode::EmptyPattern;
};

} // namespace ArtifactCore
