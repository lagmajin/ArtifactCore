module;
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <string_view>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <unordered_map>
#include <variant>

#include <memory>
#include <functional>
#include <sstream>
#include <QString>
module Script.ArtifactScript;

import Container.NamedVector;
import Core.ArtifactArray;
import EnvironmentVariable;

import Core.ArtifactString;
import Memory.SharedPtr;

namespace ArtifactCore {

namespace {

ZeroString trim(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return ZeroString(text.data() + begin, end - begin);
}

bool starts_with(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool decodeStringContent(std::string_view content, std::string& decoded,
                         std::size_t* invalidOffset = nullptr) {
    if (content.size() <= decoded.max_size() - decoded.size()) {
        decoded.reserve(decoded.size() + content.size());
    }
    const auto fail = [&](std::size_t offset) {
        if (invalidOffset) *invalidOffset = offset;
        return false;
    };
    const auto readHex = [&](std::size_t start, std::size_t digits,
                             std::uint32_t& value) {
        if (start > content.size() || digits > content.size() - start) {
            return false;
        }
        value = 0;
        for (std::size_t digit = 0; digit < digits; ++digit) {
            const unsigned char current =
                static_cast<unsigned char>(content[start + digit]);
            unsigned int nibble = 0;
            if (current >= '0' && current <= '9') nibble = current - '0';
            else if (current >= 'a' && current <= 'f') nibble = current - 'a' + 10;
            else if (current >= 'A' && current <= 'F') nibble = current - 'A' + 10;
            else return false;
            value = (value << 4) | nibble;
        }
        return true;
    };
    const auto appendCodePoint = [&](std::uint32_t codePoint) {
        if (codePoint <= 0x7f) {
            decoded.push_back(static_cast<char>(codePoint));
        } else if (codePoint <= 0x7ff) {
            decoded.push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
            decoded.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
        } else if (codePoint <= 0xffff) {
            decoded.push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
            decoded.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
            decoded.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
        } else {
            decoded.push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
            decoded.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f)));
            decoded.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
            decoded.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
        }
    };
    for (std::size_t i = 0; i < content.size(); ++i) {
        if (content[i] != '\\') {
            decoded.push_back(content[i]);
            continue;
        }
        const std::size_t escapePosition = i;
        if (++i >= content.size()) {
            return fail(escapePosition);
        }
        switch (content[i]) {
        case '"': decoded.push_back('"'); break;
        case '\\': decoded.push_back('\\'); break;
        case 'a': decoded.push_back('\a'); break;
        case 'b': decoded.push_back('\b'); break;
        case 'f': decoded.push_back('\f'); break;
        case 'n': decoded.push_back('\n'); break;
        case '0': decoded.push_back('\0'); break;
        case 'r': decoded.push_back('\r'); break;
        case 't': decoded.push_back('\t'); break;
        case 'v': decoded.push_back('\v'); break;
        case 'u':
        case 'U': {
            const std::size_t digitCount = content[i] == 'u' ? 4 : 8;
            const std::size_t digitsStart = i + 1;
            std::uint32_t codePoint = 0;
            if (!readHex(digitsStart, digitCount, codePoint)) {
                return fail(escapePosition);
            }
            i = digitsStart + digitCount - 1;
            if (content[escapePosition + 1] == 'u' &&
                codePoint >= 0xd800 && codePoint <= 0xdbff) {
                const std::size_t lowEscape = i + 1;
                if (lowEscape + 2 > content.size() ||
                    content[lowEscape] != '\\' ||
                    content[lowEscape + 1] != 'u') {
                    return fail(escapePosition);
                }
                const std::size_t lowDigits = lowEscape + 2;
                std::uint32_t lowSurrogate = 0;
                if (!readHex(lowDigits, 4, lowSurrogate) ||
                    lowSurrogate < 0xdc00 || lowSurrogate > 0xdfff) {
                    return fail(escapePosition);
                }
                codePoint = 0x10000 + ((codePoint - 0xd800) << 10) +
                            (lowSurrogate - 0xdc00);
                i = lowDigits + 3;
            } else if ((codePoint >= 0xd800 && codePoint <= 0xdfff) ||
                       codePoint > 0x10ffff) {
                return fail(escapePosition);
            }
            appendCodePoint(codePoint);
            break;
        }
        default:
            return fail(escapePosition);
        }
    }
    return true;
}

std::optional<ArtifactScriptHook> hookFromName(std::string_view name) {
    if (name == "OnCreate") return ArtifactScriptHook::OnCreate;
    if (name == "OnStart") return ArtifactScriptHook::OnStart;
    if (name == "OnEnable") return ArtifactScriptHook::OnEnable;
    if (name == "OnDisable") return ArtifactScriptHook::OnDisable;
    if (name == "OnUpdate") return ArtifactScriptHook::OnUpdate;
    if (name == "OnDestroy") return ArtifactScriptHook::OnDestroy;
    return std::nullopt;
}

ArtifactScriptValue parseDefaultValue(std::string_view text, ArtifactScriptValueType type) {
    ZeroString value = trim(text);
    if (value.length() > 0 && value.data()[value.length() - 1] == ';') {
        value = trim(std::string_view(value.data(), value.length() - 1));
    }
    switch (type) {
    case ArtifactScriptValueType::Bool:
        return std::string_view(value) == "true";
    case ArtifactScriptValueType::Int: {
        std::int64_t parsed = 0;
        const auto* begin = value.data();
        const auto* end = value.data() + value.length();
        std::from_chars(begin, end, parsed);
        return parsed;
    }
    case ArtifactScriptValueType::Float:
        return std::strtod(value.data(), nullptr);
    case ArtifactScriptValueType::String:
        if (value.length() >= 2 && value.data()[0] == '"' && value.data()[value.length() - 1] == '"') {
            const std::string_view content(value.data() + 1, value.length() - 2);
            std::string decoded;
            return decodeStringContent(content, decoded)
                ? ArtifactScriptValue(std::move(decoded))
                : ArtifactScriptValue(std::string(content));
        }
        return std::string(value.data(), value.length());
    case ArtifactScriptValueType::Array: {
        auto array = makeShared<ArtifactScriptArray>();
        std::string_view source(value.data(), value.length());
        if (source.size() >= 2 && source.front() == '[' && source.back() == ']') {
            source = source.substr(1, source.size() - 2);
            std::size_t begin = 0;
            while (begin < source.size()) {
                const auto comma = source.find(',', begin);
                const auto token = trim(source.substr(begin, comma == std::string_view::npos ? std::string_view::npos : comma - begin));
                if (!token.isEmpty()) array->values.emplace_back(std::strtod(token.data(), nullptr));
                if (comma == std::string_view::npos) break;
                begin = comma + 1;
            }
        }
        return array;
    }
    default:
        return std::monostate{};
    }
}

ArtifactScriptValueType parseFieldType(std::string_view typeName) {
    if (typeName == "bool") return ArtifactScriptValueType::Bool;
    if (typeName == "int") return ArtifactScriptValueType::Int;
    if (typeName == "float" || typeName == "double") return ArtifactScriptValueType::Float;
    if (typeName == "string") return ArtifactScriptValueType::String;
    if (typeName == "Vec2") return ArtifactScriptValueType::Vec2;
    if (typeName == "Vec3") return ArtifactScriptValueType::Vec3;
    if (typeName == "Vec4") return ArtifactScriptValueType::Vec4;
    if (typeName == "Color") return ArtifactScriptValueType::Color;
    if (typeName == "ObjectRef") return ArtifactScriptValueType::ObjectRef;
    if (typeName == "AssetRef") return ArtifactScriptValueType::AssetRef;
    if (typeName == "Array" || typeName == "array") return ArtifactScriptValueType::Array;
    return ArtifactScriptValueType::Null;
}

}


namespace {

struct ParseCtx {
    std::string_view src;
    size_t pos = 0;
    size_t len = 0;
    bool stalled = false;
    bool failed = false;
    size_t failurePosition = std::string_view::npos;
    std::size_t loopDepth = 0;
};

struct ParseLoopDepthScope {
    ParseCtx& context;

    explicit ParseLoopDepthScope(ParseCtx& value) : context(value) {
        ++context.loopDepth;
    }

    ~ParseLoopDepthScope() { --context.loopDepth; }
};

std::optional<std::size_t> findMethodBodyEnd(
    std::string_view source, std::size_t openBracePosition) {
    if (openBracePosition >= source.size() || source[openBracePosition] != '{') {
        return std::nullopt;
    }
    std::size_t depth = 1;
    bool inString = false;
    bool escaped = false;
    bool inLineComment = false;
    bool inBlockComment = false;
    for (std::size_t position = openBracePosition + 1;
         position < source.size(); ++position) {
        const char current = source[position];
        const char next = position + 1 < source.size()
            ? source[position + 1] : '\0';
        if (inString) {
            if (escaped) escaped = false;
            else if (current == '\\') escaped = true;
            else if (current == '"') inString = false;
            continue;
        }
        if (inLineComment) {
            if (current == '\n') inLineComment = false;
            continue;
        }
        if (inBlockComment) {
            if (current == '*' && next == '/') {
                inBlockComment = false;
                ++position;
            }
            continue;
        }
        if (current == '/' && next == '/') {
            inLineComment = true;
            ++position;
        } else if (current == '/' && next == '*') {
            inBlockComment = true;
            ++position;
        } else if (current == '"') {
            inString = true;
        } else if (current == '{') {
            ++depth;
        } else if (current == '}' && --depth == 0) {
            return position;
        }
    }
    return std::nullopt;
}

void skipWS(ParseCtx& c) {
    while (c.pos < c.len) {
        if (std::isspace(static_cast<unsigned char>(c.src[c.pos]))) {
            ++c.pos;
            continue;
        }
        if (c.pos + 1 < c.len && c.src[c.pos] == '/' && c.src[c.pos + 1] == '/') {
            c.pos += 2;
            while (c.pos < c.len && c.src[c.pos] != '\n') ++c.pos;
            continue;
        }
        if (c.pos + 1 < c.len && c.src[c.pos] == '/' && c.src[c.pos + 1] == '*') {
            c.pos += 2;
            while (c.pos + 1 < c.len && !(c.src[c.pos] == '*' && c.src[c.pos + 1] == '/')) ++c.pos;
            c.pos = c.pos + 1 < c.len ? c.pos + 2 : c.len;
            continue;
        }
        break;
    }
}
bool matchCh(ParseCtx& c, char ch) { skipWS(c); if (c.pos < c.len && c.src[c.pos] == ch) { ++c.pos; return true; } return false; }
bool matchKw(ParseCtx& c, const char* wd) { skipWS(c); size_t n = std::strlen(wd); if (c.pos + n <= c.len && c.src.substr(c.pos, n) == wd && (c.pos + n >= c.len || (!std::isalnum(static_cast<unsigned char>(c.src[c.pos + n])) && c.src[c.pos + n] != '_'))) { c.pos += n; return true; } return false; }
bool expectCh(ParseCtx& c, char ch) {
    if (matchCh(c, ch)) return true;
    c.failed = true;
    if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
    return false;
}
bool previousNonWhitespaceIs(ParseCtx& c, std::size_t lowerBound, char ch) {
    std::size_t position = c.pos;
    while (position > lowerBound &&
           std::isspace(static_cast<unsigned char>(c.src[position - 1]))) {
        --position;
    }
    return position > lowerBound && c.src[position - 1] == ch;
}
std::string parseId(ParseCtx& c) { skipWS(c); size_t s = c.pos; while (c.pos < c.len && (std::isalnum(static_cast<unsigned char>(c.src[c.pos])) || c.src[c.pos] == '_')) ++c.pos; return std::string(c.src.substr(s, c.pos - s)); }
ArtifactScriptExprPtr parseExpr(ParseCtx& c);
ArtifactScriptExprPtr parseRequiredExpr(ParseCtx& c);
bool parseDelimitedExpressions(
    ParseCtx& c, char closing, std::vector<ArtifactScriptExprPtr>& expressions);
ArtifactScriptValue parseNum(ParseCtx& c, bool negative = false);

ArtifactScriptExprPtr parsePostfixSuffix(
    ParseCtx& c, ArtifactScriptExprPtr expression) {
    while (expression) {
        skipWS(c);
        if (c.pos < c.len && c.src[c.pos] == '[') {
            ++c.pos;
            auto index = std::make_unique<ArtifactScriptExpr>();
            index->kind = ArtifactScriptExpr::Kind::Index;
            index->indexTarget = std::move(expression);
            index->indexExpr = parseRequiredExpr(c);
            if (!matchCh(c, ']')) {
                c.failed = true;
                if (c.failurePosition == std::string_view::npos)
                    c.failurePosition = c.pos;
                return nullptr;
            }
            expression = std::move(index);
            continue;
        }
        if (c.pos < c.len && c.src[c.pos] == '.') {
            ++c.pos;
            const std::string member = parseId(c);
            if (member.empty()) {
                c.failed = true;
                if (c.failurePosition == std::string_view::npos)
                    c.failurePosition = c.pos;
                return nullptr;
            }
            if (matchCh(c, '(')) {
                auto call = std::make_unique<ArtifactScriptExpr>();
                call->kind = ArtifactScriptExpr::Kind::Call;
                call->callName = member;
                call->callTarget = std::move(expression);
                if (!parseDelimitedExpressions(c, ')', call->callArgs))
                    return nullptr;
                expression = std::move(call);
            } else {
                auto field = std::make_unique<ArtifactScriptExpr>();
                field->kind = ArtifactScriptExpr::Kind::FieldAccess;
                field->fieldObject = std::move(expression);
                field->fieldName = member;
                expression = std::move(field);
            }
            continue;
        }
        break;
    }
    return expression;
}

ArtifactScriptExprPtr parsePrimary(ParseCtx& c) {
    skipWS(c); if (c.pos >= c.len) return nullptr;
    auto e = std::make_unique<ArtifactScriptExpr>();
    if (c.src[c.pos] == '(') {
        ++c.pos;
        auto expression = parseRequiredExpr(c);
        if (!matchCh(c, ')')) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos)
                c.failurePosition = c.pos;
            return nullptr;
        }
        return parsePostfixSuffix(c, std::move(expression));
    }
    if (matchKw(c, "new")) {
        const std::string className = parseId(c);
        if (className.empty()) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
            return nullptr;
        }
        e->kind = ArtifactScriptExpr::Kind::New;
        e->newClassName = className;
        if (matchCh(c, '(') &&
            !parseDelimitedExpressions(c, ')', e->newArgs)) {
            return nullptr;
        }
        return parsePostfixSuffix(c, std::move(e));
    }
    if (c.src[c.pos] == '[') {
        ++c.pos; e->kind = ArtifactScriptExpr::Kind::ArrayLiteral;
        if (!parseDelimitedExpressions(c, ']', e->arrayElements)) return nullptr;
        return parsePostfixSuffix(c, std::move(e));
    }
    if (c.src[c.pos] == '"') {
        const std::size_t openingQuote = c.pos++;
        const std::size_t contentStart = c.pos;
        std::size_t marker = contentStart;
        while (marker < c.len && c.src[marker] != '"' && c.src[marker] != '\\') {
            ++marker;
        }
        if (marker >= c.len) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos)
                c.failurePosition = openingQuote;
            return nullptr;
        }
        e->kind = ArtifactScriptExpr::Kind::Literal;
        if (c.src[marker] == '"') {
            // Common unescaped strings take one bounded view scan and one copy.
            e->literalValue = std::string(c.src.substr(contentStart, marker - contentStart));
            c.pos = marker + 1;
            return parsePostfixSuffix(c, std::move(e));
        }

        std::string decoded;
        decoded.reserve(marker - contentStart);
        decoded.append(c.src.substr(contentStart, marker - contentStart));
        c.pos = marker;
        bool escaped = false;
        while (c.pos < c.len) {
            const char current = c.src[c.pos];
            if (!escaped && current == '"') break;
            ++c.pos;
            if (escaped) {
                escaped = false;
            } else if (current == '\\') {
                escaped = true;
            }
        }
        if (c.pos >= c.len || escaped) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos)
                c.failurePosition = escaped ? c.pos - 1 : openingQuote;
            return nullptr;
        }
        const std::size_t closingQuote = c.pos;
        std::size_t invalidOffset = 0;
        const std::string_view escapedContent =
            c.src.substr(marker, closingQuote - marker);
        if (!decodeStringContent(escapedContent, decoded, &invalidOffset)) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos)
                c.failurePosition = marker + invalidOffset;
            return nullptr;
        }
        e->literalValue = std::move(decoded);
        c.pos = closingQuote + 1;
        return parsePostfixSuffix(c, std::move(e));
    }
    if (c.src[c.pos] == '-' && c.pos + 1 < c.len &&
        (std::isdigit(static_cast<unsigned char>(c.src[c.pos + 1])) ||
         (c.src[c.pos + 1] == '.' && c.pos + 2 < c.len &&
          std::isdigit(static_cast<unsigned char>(c.src[c.pos + 2]))))) {
        ++c.pos;
        e->kind = ArtifactScriptExpr::Kind::Literal;
        e->literalValue = parseNum(c, true);
        return parsePostfixSuffix(c, std::move(e));
    }
    if (c.src[c.pos] == '-' || c.src[c.pos] == '!') { e->kind = ArtifactScriptExpr::Kind::Unary;
        e->unaryOp = c.src[c.pos] == '-' ? ArtifactScriptUnaryOp::Neg : ArtifactScriptUnaryOp::Not;
        c.pos++; e->operand = parsePrimary(c);
        if (!e->operand) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
            return nullptr;
        }
        return parsePostfixSuffix(c, std::move(e)); }
    if (std::isdigit(static_cast<unsigned char>(c.src[c.pos])) ||
        (c.src[c.pos] == '.' && c.pos + 1 < c.len &&
         std::isdigit(static_cast<unsigned char>(c.src[c.pos + 1])))) {
        e->kind = ArtifactScriptExpr::Kind::Literal;
        e->literalValue = parseNum(c);
        return parsePostfixSuffix(c, std::move(e));
    }
    if (matchKw(c, "true")) { e->kind = ArtifactScriptExpr::Kind::Literal; e->literalValue = true; return parsePostfixSuffix(c, std::move(e)); }
    if (matchKw(c, "false")) { e->kind = ArtifactScriptExpr::Kind::Literal; e->literalValue = false; return parsePostfixSuffix(c, std::move(e)); }
    if (matchKw(c, "null")) { e->kind = ArtifactScriptExpr::Kind::Literal; e->literalValue = std::monostate{}; return parsePostfixSuffix(c, std::move(e)); }
    // Vector / colour constructors. These parse into an ArrayLiteral node tagged
// with the target type, so evaluation builds a typed value without a host call
// and normalize/mix stay usable in constant sub-expressions.
    {
        struct LiteralCtor { const char* keyword; ArtifactScriptValueType type; };
        static const LiteralCtor kLiteralCtors[] = {
            {"vec2", ArtifactScriptValueType::Vec2},
            {"vec3", ArtifactScriptValueType::Vec3},
            {"vec4", ArtifactScriptValueType::Vec4},
            {"color", ArtifactScriptValueType::Color},
        };
        const std::size_t mark = c.pos;
        for (const auto& ctor : kLiteralCtors) {
            if (!matchKw(c, ctor.keyword) || !matchCh(c, '(')) {
                c.pos = mark;  // not this keyword; rewind and try the next
                continue;
            }
            e->kind = ArtifactScriptExpr::Kind::ArrayLiteral;
            e->arrayLiteralIsVector = true;
            e->arrayLiteralType = ctor.type;
            if (!parseDelimitedExpressions(c, ')', e->arrayElements)) return nullptr;
            return parsePostfixSuffix(c, std::move(e));
        }
    }
    std::string id = parseId(c); if (id.empty()) return nullptr;
    if (matchCh(c, '(')) { e->kind = ArtifactScriptExpr::Kind::Call; e->callName = id;
        if (!parseDelimitedExpressions(c, ')', e->callArgs)) return nullptr;
        return parsePostfixSuffix(c, std::move(e)); }
    e->kind = ArtifactScriptExpr::Kind::Variable;
    e->variableName = std::move(id);
    return parsePostfixSuffix(c, std::move(e));
}

#define BIN_PARSE(name, next, ...) \
ArtifactScriptExprPtr name(ParseCtx& c) { auto l = next(c); while (l) { ArtifactScriptBinaryOp op; int matched = 0; __VA_ARGS__ if (!matched) break; auto e = std::make_unique<ArtifactScriptExpr>(); e->kind = ArtifactScriptExpr::Kind::Binary; e->binaryOp = op; e->left = std::move(l); e->right = next(c); if (!e->right) { c.failed = true; if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos; return nullptr; } l = std::move(e); } return l; }

BIN_PARSE(parseMulDiv, parsePrimary,
    if (matchCh(c, '*')) { op = ArtifactScriptBinaryOp::Mul; matched = 1; }
    else if (matchCh(c, '/')) { op = ArtifactScriptBinaryOp::Div; matched = 1; }
    else if (matchCh(c, '%')) { op = ArtifactScriptBinaryOp::Mod; matched = 1; })

BIN_PARSE(parseAddSub, parseMulDiv,
    if (matchCh(c, '+')) { op = ArtifactScriptBinaryOp::Add; matched = 1; }
    else if (matchCh(c, '-')) { op = ArtifactScriptBinaryOp::Sub; matched = 1; })

BIN_PARSE(parseCmp, parseAddSub,
    if (matchKw(c, "==")) { op = ArtifactScriptBinaryOp::Eq; matched = 1; }
    else if (matchKw(c, "!=")) { op = ArtifactScriptBinaryOp::Neq; matched = 1; }
    else if (matchKw(c, "<=")) { op = ArtifactScriptBinaryOp::Le; matched = 1; }
    else if (matchKw(c, ">=")) { op = ArtifactScriptBinaryOp::Ge; matched = 1; }
    else if (matchCh(c, '<')) { op = ArtifactScriptBinaryOp::Lt; matched = 1; }
    else if (matchCh(c, '>')) { op = ArtifactScriptBinaryOp::Gt; matched = 1; })

BIN_PARSE(parseAndOr, parseCmp,
    if (matchKw(c, "&&")) { op = ArtifactScriptBinaryOp::And; matched = 1; }
    else if (matchKw(c, "||")) { op = ArtifactScriptBinaryOp::Or; matched = 1; })
#undef BIN_PARSE

// Is: postfix `expr is Name` (inheritance-aware), tighter than coalescing.
ArtifactScriptExprPtr parseIs(ParseCtx& c) {
    auto target = parseAndOr(c);
    if (!target) return nullptr;
    skipWS(c);
    const std::size_t saved = c.pos;
    const std::string keyword = parseId(c);
    if (keyword != "is") { c.pos = saved; return target; }
    const std::string className = parseId(c);
    if (className.empty()) return nullptr;
    auto e = std::make_unique<ArtifactScriptExpr>();
    e->kind = ArtifactScriptExpr::Kind::Is;
    e->isTarget = std::move(target);
    e->isClassName = className;
    return e;
}

// Null coalescing is right-associative and binds more tightly than ternary,
// but less tightly than `is` and the logical operators.
ArtifactScriptExprPtr parseCoalesce(ParseCtx& c) {
    auto left = parseIs(c);
    if (!left) return nullptr;
    skipWS(c);
    if (c.pos + 1 >= c.len || c.src[c.pos] != '?' || c.src[c.pos + 1] != '?') {
        return left;
    }
    c.pos += 2;
    auto right = parseCoalesce(c);
    if (!right) {
        c.failed = true;
        if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
        return nullptr;
    }
    auto expression = std::make_unique<ArtifactScriptExpr>();
    expression->kind = ArtifactScriptExpr::Kind::Binary;
    expression->binaryOp = ArtifactScriptBinaryOp::Coalesce;
    expression->left = std::move(left);
    expression->right = std::move(right);
    return expression;
}

// Ternary: cond ? a : b — lowest precedence, right-associative.
ArtifactScriptExprPtr parseTernary(ParseCtx& c) {
    auto condition = parseCoalesce(c);
    if (!condition) return nullptr;
    skipWS(c);
    if (c.pos >= c.len || c.src[c.pos] != '?') return condition;
    ++c.pos;
    auto e = std::make_unique<ArtifactScriptExpr>();
    e->kind = ArtifactScriptExpr::Kind::Ternary;
    e->ternaryCondition = std::move(condition);
    e->ternaryThen = parseRequiredExpr(c);
    if (!matchCh(c, ':')) {
        c.failed = true;
        if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
        return nullptr;
    }
    e->ternaryElse = parseRequiredExpr(c);
    if (!e->ternaryThen || !e->ternaryElse) return nullptr;
    return e;
}

ArtifactScriptExprPtr parseExpr(ParseCtx& c) { return parseTernary(c); }

ArtifactScriptExprPtr parseRequiredExpr(ParseCtx& c) {
    auto expression = parseExpr(c);
    if (!expression) {
        c.failed = true;
        if (c.failurePosition == std::string_view::npos) {
            c.failurePosition = c.pos;
        }
    }
    return expression;
}

bool parseDelimitedExpressions(
    ParseCtx& c, char closing, std::vector<ArtifactScriptExprPtr>& expressions) {
    if (matchCh(c, closing)) return true;
    for (;;) {
        auto expression = parseRequiredExpr(c);
        if (!expression) return false;
        expressions.push_back(std::move(expression));
        if (matchCh(c, closing)) return true;
        if (!matchCh(c, ',')) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
            return false;
        }
        skipWS(c);
        if (c.pos >= c.len || c.src[c.pos] == closing) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
            return false;
        }
    }
}

ArtifactScriptValue parseNum(ParseCtx& c, bool negative) {
    skipWS(c);
    const std::size_t start = c.pos;
    bool hasDigits = false;
    while (c.pos < c.len &&
           std::isdigit(static_cast<unsigned char>(c.src[c.pos]))) {
        ++c.pos;
        hasDigits = true;
    }
    if (c.pos < c.len && c.src[c.pos] == '.') {
        ++c.pos;
        while (c.pos < c.len &&
               std::isdigit(static_cast<unsigned char>(c.src[c.pos]))) {
            ++c.pos;
            hasDigits = true;
        }
    }
    if (!hasDigits) {
        c.failed = true;
        if (c.failurePosition == std::string_view::npos)
            c.failurePosition = start;
        return 0.0;
    }
    if (c.pos < c.len && (c.src[c.pos] == 'e' || c.src[c.pos] == 'E')) {
        const std::size_t exponentPosition = c.pos++;
        if (c.pos < c.len && (c.src[c.pos] == '+' || c.src[c.pos] == '-'))
            ++c.pos;
        const std::size_t exponentDigits = c.pos;
        while (c.pos < c.len &&
               std::isdigit(static_cast<unsigned char>(c.src[c.pos]))) {
            ++c.pos;
        }
        if (c.pos == exponentDigits) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos)
                c.failurePosition = exponentPosition;
            return 0.0;
        }
    }

    const bool isInteger = c.src.substr(start, c.pos - start).find_first_of(".eE") ==
                           std::string_view::npos;
    if (isInteger) {
        std::int64_t integer = 0;
        const char* begin = c.src.data() + start;
        const char* end = c.src.data() + c.pos;
        const char* integerBegin = negative ? begin - 1 : begin;
        const auto [parsedEnd, error] = std::from_chars(integerBegin, end, integer);
        if (error == std::errc{} && parsedEnd == end) return integer;
    }

    double value = 0.0;
    const char* begin = c.src.data() + start;
    const char* end = c.src.data() + c.pos;
    const auto [parsedEnd, error] = std::from_chars(
        begin, end, value, std::chars_format::general);
    if (error != std::errc{} || parsedEnd != end) {
        c.failed = true;
        if (c.failurePosition == std::string_view::npos)
            c.failurePosition = start;
        return 0.0;
    }
    return negative ? -value : value;
}

ArtifactScriptExprPtr parseExpr(ParseCtx& c);
ArtifactScriptStmtPtr parseStmt(ParseCtx& c);

std::string parseAssignmentOperator(ParseCtx& c) {
    skipWS(c);
    if (c.pos >= c.len) return {};
    if (c.pos + 2 < c.len && c.src.substr(c.pos, 3) == "??=") {
        c.pos += 3;
        return "??=";
    }
    if (c.pos + 1 < c.len && c.src[c.pos + 1] == '=') {
        switch (c.src[c.pos]) {
        case '+': case '-': case '*': case '/': case '%':
            c.pos += 2;
            return std::string(c.src.substr(c.pos - 2, 2));
        default:
            return {};
        }
    }
    if (c.src[c.pos] == '=') {
        ++c.pos;
        return "=";
    }
    return {};
}

ArtifactScriptStmtPtr parseStmt(ParseCtx& c) {
    skipWS(c); if (c.pos >= c.len || c.src[c.pos] == '}') return nullptr;
    if (matchKw(c, "do")) {
        auto s = std::make_unique<ArtifactScriptStmt>();
        s->kind = ArtifactScriptStmt::Kind::DoWhile;
        {
            ParseLoopDepthScope loopScope(c);
            s->whileBody = parseStmt(c);
        }
        if (!s->whileBody) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
            return nullptr;
        }
        if (!matchKw(c, "while") || !expectCh(c, '(')) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
            return nullptr;
        }
        s->whileCond = parseRequiredExpr(c);
        if (!s->whileCond || !expectCh(c, ')') || !expectCh(c, ';')) return nullptr;
        return s;
    }
    if (matchKw(c, "if")) {
        if (!expectCh(c, '(')) return nullptr;
        auto s = std::make_unique<ArtifactScriptStmt>();
        s->kind = ArtifactScriptStmt::Kind::If;
        s->ifCond = parseRequiredExpr(c);
        if (!s->ifCond || !expectCh(c, ')')) return nullptr;
        s->ifThen = parseStmt(c);
        if (!s->ifThen) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
            return nullptr;
        }
        if (matchKw(c, "else")) {
            s->ifElse = parseStmt(c);
            if (!s->ifElse) {
                c.failed = true;
                if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
                return nullptr;
            }
        }
        return s;
    }
    if (matchKw(c, "while")) {
        if (!expectCh(c, '(')) return nullptr;
        auto s = std::make_unique<ArtifactScriptStmt>();
        s->kind = ArtifactScriptStmt::Kind::While;
        s->whileCond = parseRequiredExpr(c);
        if (!s->whileCond || !expectCh(c, ')')) return nullptr;
        {
            ParseLoopDepthScope loopScope(c);
            s->whileBody = parseStmt(c);
        }
        if (!s->whileBody) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
            return nullptr;
        }
        return s;
    }
    if (matchKw(c, "for")) {
        if (!expectCh(c, '(')) return nullptr;
        auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::For;
        if (!matchCh(c, ';')) {
            const std::size_t initializerStart = c.pos;
            s->forInit = parseStmt(c);
            if (c.failed) return nullptr;
            if (!s->forInit ||
                !previousNonWhitespaceIs(c, initializerStart, ';')) {
                c.failed = true;
                if (c.failurePosition == std::string_view::npos) {
                    c.failurePosition = c.pos;
                }
                return nullptr;
            }
        }
        if (c.failed) return nullptr;
        if (!matchCh(c, ';')) {
            s->forCond = parseRequiredExpr(c);
            if (!s->forCond || !expectCh(c, ';')) return nullptr;
        }
        if (!matchCh(c, ')')) {
            s->forIncrement = parseStmt(c);
            if (!s->forIncrement || !expectCh(c, ')')) return nullptr;
        }
        {
            ParseLoopDepthScope loopScope(c);
            s->forBody = parseStmt(c);
        }
        if (!s->forBody) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
            return nullptr;
        }
        return s;
    }
    const std::size_t statementPosition = c.pos;
    if (matchKw(c, "break")) {
        if (c.loopDepth == 0) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos)
                c.failurePosition = statementPosition;
            return nullptr;
        }
        auto s = std::make_unique<ArtifactScriptStmt>();
        s->kind = ArtifactScriptStmt::Kind::Break;
        matchCh(c, ';');
        return s;
    }
    if (matchKw(c, "continue")) {
        if (c.loopDepth == 0) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos)
                c.failurePosition = statementPosition;
            return nullptr;
        }
        auto s = std::make_unique<ArtifactScriptStmt>();
        s->kind = ArtifactScriptStmt::Kind::Continue;
        matchCh(c, ';');
        return s;
    }
    if (matchKw(c, "return")) { auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Return;
        skipWS(c);
        if (c.pos < c.len && c.src[c.pos] != ';') s->expr = parseRequiredExpr(c);
        matchCh(c, ';'); return s; }
    if (matchCh(c, '{')) { auto b = std::make_unique<ArtifactScriptStmt>(); b->kind = ArtifactScriptStmt::Kind::Block;
        while (c.pos < c.len && c.src[c.pos] != '}') {
            const auto positionBeforeStatement = c.pos;
            auto s = parseStmt(c);
            if (s) b->blockStmts.push_back(std::move(s));
            else {
                if (c.pos < c.len && c.src[c.pos] != '}') {
                    c.failed = true;
                    if (c.failurePosition == std::string_view::npos) {
                        c.failurePosition = positionBeforeStatement;
                    }
                }
                break;
            }
            if (c.pos == positionBeforeStatement) {
                c.stalled = true;
                c.failed = true;
                c.failurePosition = positionBeforeStatement;
                break;
            }
        }
        if (!expectCh(c, '}')) return nullptr;
        return b; }
    skipWS(c);
    const bool isPrefixInc = c.pos + 1 < c.len &&
        c.src[c.pos] == '+' && c.src[c.pos + 1] == '+';
    const bool isPrefixDec = !isPrefixInc && c.pos + 1 < c.len &&
        c.src[c.pos] == '-' && c.src[c.pos + 1] == '-';
    if (isPrefixInc || isPrefixDec) {
        c.pos += 2;
        auto target = parsePrimary(c);
        if (!target ||
            (target->kind != ArtifactScriptExpr::Kind::Variable &&
             target->kind != ArtifactScriptExpr::Kind::Index &&
             target->kind != ArtifactScriptExpr::Kind::FieldAccess)) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos)
                c.failurePosition = c.pos;
            return nullptr;
        }
        auto statement = std::make_unique<ArtifactScriptStmt>();
        statement->kind = ArtifactScriptStmt::Kind::Assign;
        statement->assignOp = isPrefixInc ? "+=" : "-=";
        if (target->kind == ArtifactScriptExpr::Kind::Variable) {
            statement->assignTarget = target->variableName;
        } else {
            statement->assignTargetExpression = std::move(target);
        }
        statement->assignValue = std::make_unique<ArtifactScriptExpr>();
        statement->assignValue->kind = ArtifactScriptExpr::Kind::Literal;
        statement->assignValue->literalValue = std::int64_t{1};
        matchCh(c, ';');
        return statement;
    }
    // Variable declaration: "float x" or "float x = expr", or type-inferred "var x = expr"
    const std::size_t expressionStart = c.pos;
    std::string id = parseId(c);
    if (id.empty()) { matchCh(c, ';'); return std::make_unique<ArtifactScriptStmt>(); }
        if (id == "float" || id == "int" || id == "bool" || id == "string" || id == "Array" || id == "array") {
        auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Decl;
        s->declType = parseFieldType(id); s->declName = parseId(c);
        if (matchCh(c, '=')) s->declInit = parseRequiredExpr(c);
        matchCh(c, ';'); return s;
    }
    if (id == "var") {
        auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Decl;
        s->declType = ArtifactScriptValueType::Null;  // Null type marks type inference
        s->declName = parseId(c);
        if (matchCh(c, '=')) s->declInit = parseRequiredExpr(c);
        matchCh(c, ';'); return s;
    }
    if (id == "foreach") {
        auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Foreach;
        if (!expectCh(c, '(')) return nullptr;
        s->foreachItemName = parseId(c);
        if (s->foreachItemName.empty() || !matchKw(c, "in")) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
            return nullptr;
        }
        const std::size_t collectionStart = c.pos;
        s->foreachCollectionName = parseId(c);
        if (s->foreachCollectionName.empty()) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos)
                c.failurePosition = c.pos;
            return nullptr;
        }
        skipWS(c);
        const bool reservedLiteral = s->foreachCollectionName == "true" ||
                                     s->foreachCollectionName == "false" ||
                                     s->foreachCollectionName == "null";
        if (reservedLiteral || c.pos >= c.len || c.src[c.pos] != ')') {
            // Preserve the compact name-only AST form for the common
            // `foreach (item in values)` case. General expressions are stored
            // only when the source actually uses one.
            s->foreachCollectionName.clear();
            c.pos = collectionStart;
            s->foreachCollectionExpr = parseRequiredExpr(c);
            if (!s->foreachCollectionExpr) return nullptr;
        }
        if (!expectCh(c, ')')) return nullptr;
        {
            ParseLoopDepthScope loopScope(c);
            s->foreachBody = parseStmt(c);
        }
        if (!s->foreachBody) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) c.failurePosition = c.pos;
            return nullptr;
        }
        return s;
    }
    // Increment / decrement on a plain variable: "x++;" or "x--;"
    const bool isPostInc = matchKw(c, "++");
    const bool isPostDec = !isPostInc && matchKw(c, "--");
    if ((isPostInc || isPostDec) && !id.empty()) {
        auto inc = std::make_unique<ArtifactScriptStmt>();
        inc->kind = ArtifactScriptStmt::Kind::Assign;
        inc->assignTarget = id;
        inc->assignOp = isPostInc ? "+=" : "-=";
        inc->assignValue = std::make_unique<ArtifactScriptExpr>();
        inc->assignValue->kind = ArtifactScriptExpr::Kind::Literal;
        inc->assignValue->literalValue = std::int64_t{1};
        matchCh(c, ';');
        return inc;
    }
    skipWS(c);
    if (!id.empty() && c.pos < c.len &&
        (c.src[c.pos] == '[' || c.src[c.pos] == '.')) {
        auto target = std::make_unique<ArtifactScriptExpr>();
        target->kind = ArtifactScriptExpr::Kind::Variable;
        target->variableName = id;
        target = parsePostfixSuffix(c, std::move(target));
        if (c.failed) return nullptr;
        if (target && (target->kind == ArtifactScriptExpr::Kind::Index ||
                       target->kind == ArtifactScriptExpr::Kind::FieldAccess)) {
            std::string op = parseAssignmentOperator(c);
            bool isIncrement = false;
            if (op.empty()) {
                const bool isPostInc = matchKw(c, "++");
                const bool isPostDec = !isPostInc && matchKw(c, "--");
                if (isPostInc || isPostDec) {
                    op = isPostInc ? "+=" : "-=";
                    isIncrement = true;
                }
            }
            if (!op.empty()) {
                auto statement = std::make_unique<ArtifactScriptStmt>();
                const bool legacyDirectFieldAssignment =
                    op == "=" &&
                    target->kind == ArtifactScriptExpr::Kind::FieldAccess &&
                    target->fieldObject &&
                    target->fieldObject->kind ==
                        ArtifactScriptExpr::Kind::Variable;
                if (legacyDirectFieldAssignment) {
                    statement->kind = ArtifactScriptStmt::Kind::Decl;
                    statement->declName =
                        target->fieldObject->variableName;
                    statement->fieldAssign = true;
                    statement->assignField = target->fieldName;
                    statement->declInit = parseRequiredExpr(c);
                } else {
                    statement->kind = ArtifactScriptStmt::Kind::Assign;
                    statement->assignTargetExpression = std::move(target);
                    statement->assignOp = std::move(op);
                    if (isIncrement) {
                        statement->assignValue =
                            std::make_unique<ArtifactScriptExpr>();
                        statement->assignValue->kind =
                            ArtifactScriptExpr::Kind::Literal;
                        statement->assignValue->literalValue = std::int64_t{1};
                    } else {
                        statement->assignValue = parseRequiredExpr(c);
                    }
                }
                matchCh(c, ';');
                return statement;
            }
        }
    } else {
        std::string op = parseAssignmentOperator(c);
        if (!op.empty()) {
            auto statement = std::make_unique<ArtifactScriptStmt>();
            statement->kind = ArtifactScriptStmt::Kind::Assign;
            statement->assignTarget = id;
            statement->assignOp = std::move(op);
            statement->assignValue = parseRequiredExpr(c);
            matchCh(c, ';');
            return statement;
        }
    }
    c.pos = expressionStart;
    auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Expr;
    s->expr = parseExpr(c); matchCh(c, ';'); return s;
}

ArtifactScriptMethodBodyPtr parseMethodBody(
    std::string_view src, const std::vector<std::string>& params,
    bool& failed, std::size_t& failurePosition) {
    auto body = std::make_unique<ArtifactScriptMethodBody>(); body->parameters = params;
    ParseCtx c{src, 0, src.size()};
    while (c.pos < c.len) {
        const auto positionBeforeStatement = c.pos;
        auto s = parseStmt(c);
        if (s) body->statements.push_back(std::move(s));
        else {
            if (c.pos < c.len) {
                c.failed = true;
                if (c.failurePosition == std::string_view::npos) {
                    c.failurePosition = positionBeforeStatement;
                }
            }
            break;
        }
        if (c.stalled || c.pos == positionBeforeStatement) {
            c.failed = true;
            if (c.failurePosition == std::string_view::npos) {
                c.failurePosition = positionBeforeStatement;
            }
            break;
        }
    }
    failed = c.failed;
    failurePosition = c.failurePosition;
    if (failed) return nullptr;
    return body;
}

} // namespace


ArtifactScriptDefinition ArtifactScriptParser::parse(std::string_view source) const {
    ArtifactScriptDefinition def;
    def.source = std::string(source);

    std::string_view sourceView = def.source;
    std::size_t lineNo = 0;
    std::optional<ArtifactScriptClass> pendingClass;
    std::string pendingAttributes;
    std::size_t pendingAttributesLine = 0;

    auto finishClass = [&]() {
        if (pendingClass) {
            def.classes.push_back(std::move(*pendingClass));
            pendingClass.reset();
        }
    };

    auto activeClass = [&]() -> ArtifactScriptClass& {
        if (pendingClass) return *pendingClass;
        pendingClass.emplace();
        return *pendingClass;
    };

    for (std::size_t pos = 0; pos < sourceView.size();) {
        const std::size_t lineStart = pos;
        const std::size_t end = sourceView.find('\n', pos);
        const std::size_t lineEnd = end == std::string_view::npos ? sourceView.size() : end;
        const std::string_view line(sourceView.data() + pos, lineEnd - pos);
        std::size_t nextPos = end == std::string_view::npos ? sourceView.size() : end + 1;
        ++lineNo;
        const ZeroString trimmed = trim(line);
        if (trimmed.isEmpty()) {
            pos = nextPos;
            continue;
        }

        if (!pendingClass) {
            if (starts_with(trimmed, "class ")) {
                const auto colon = trimmed.find(':');
                const auto brace = trimmed.find('{');
                const std::size_t nameEnd = colon == static_cast<std::size_t>(-1) ? brace : std::min(colon, brace);
                if (nameEnd == static_cast<std::size_t>(-1)) {
                    def.diagnostics.push_back({lineNo, 1, "class declaration is incomplete"});
                    pos = nextPos;
                    continue;
                }
                const ZeroString className = trim(trimmed.substr(6, nameEnd - 6));
                activeClass().name = std::string(className.data(), className.length());
                if (colon != static_cast<std::size_t>(-1) &&
                    (brace == static_cast<std::size_t>(-1) || colon < brace)) {
                    const ZeroString parent = trim(trimmed.substr(
                        colon + 1, brace == static_cast<std::size_t>(-1) ? std::string::npos : brace - colon - 1));
                    std::string parentName(parent.data(), parent.length());
                    const auto space = parentName.find(' ');
                    if (space != std::string::npos) parentName = parentName.substr(0, space);
                    activeClass().parentName = std::move(parentName);
                }
                activeClass().derivesFromBehaviour = trimmed.contains("ArtifactBehaviour");
                pos = nextPos;
                continue;
            }
            pos = nextPos;
            continue;
        }

        if (trimmed == "}" || trimmed == "};") {
            finishClass();
            pos = nextPos;
            continue;
        }

        if (trimmed == "{") {
            pos = nextPos;
            continue;
        }

        const bool isPublic = starts_with(trimmed, "public ");
        const bool isPrivate = starts_with(trimmed, "private ");
        const bool isField = isPublic || isPrivate;
        const bool isAttributeLine = !trimmed.isEmpty() && trimmed.data()[0] == '[' &&
            !isField;
        const bool isMethod = !isAttributeLine &&
            trimmed.find('(') != static_cast<std::size_t>(-1) &&
            trimmed.find(')') != static_cast<std::size_t>(-1);
        if (isAttributeLine) {
            if (pendingAttributes.empty()) pendingAttributesLine = lineNo;
            if (!pendingAttributes.empty()) pendingAttributes += " ";
            pendingAttributes += std::string(trimmed.data(), trimmed.length());
            pos = nextPos;
            continue;
        }

        if (isField) {
            const ZeroString body = trim(trimmed.substr(isPublic ? 7 : 8));
            const auto space = body.find(' ');
            const auto eq = body.find('=');
            if (space == static_cast<std::size_t>(-1)) {
                def.diagnostics.push_back({lineNo, 1, "field declaration is incomplete"});
                pos = nextPos;
                continue;
            }
            const ZeroString typeName = body.substr(0, space);
            const ZeroString namePart = trim(body.substr(space + 1, eq == std::string::npos ? std::string::npos : eq - space - 1));
            ArtifactScriptField field;
            // Phase 4a: attribute prefix e.g. [Range(0,1)] [Header("X")] [SerializeField].
            std::string attributes = pendingAttributes;
            const std::size_t attributesLine =
                pendingAttributesLine == 0 ? lineNo : pendingAttributesLine;
            pendingAttributes.clear();
            pendingAttributesLine = 0;
            const std::string nameRaw(namePart.data(), namePart.length());
            std::string fieldName = nameRaw;
            const auto attrEnd = nameRaw.rfind(']');
            if (!nameRaw.empty() && nameRaw.front() == '[' && attrEnd != std::string::npos) {
                if (!attributes.empty()) attributes += " ";
                attributes += nameRaw.substr(0, attrEnd + 1);
                fieldName = std::string(trim(nameRaw.substr(attrEnd + 1)).data(),
                                        trim(nameRaw.substr(attrEnd + 1)).length());
            }
            if (!fieldName.empty() && fieldName.back() == ';') {
                fieldName.pop_back();
                const ZeroString normalizedName = trim(fieldName);
                fieldName.assign(normalizedName.data(), normalizedName.length());
            }
            field.name = fieldName;
            field.isPublic = isPublic;
            field.serialized = isPublic;
            field.type = parseFieldType(typeName);
            if (eq != static_cast<std::size_t>(-1) &&
                field.type == ArtifactScriptValueType::String) {
                ZeroString initializer = trim(body.substr(eq + 1));
                if (initializer.length() > 0 &&
                    initializer.data()[initializer.length() - 1] == ';') {
                    initializer = trim(std::string_view(
                        initializer.data(), initializer.length() - 1));
                }
                if (initializer.length() >= 2 && initializer.data()[0] == '"' &&
                    initializer.data()[initializer.length() - 1] == '"') {
                    std::string decoded;
                    std::size_t invalidOffset = 0;
                    const std::string_view content(
                        initializer.data() + 1, initializer.length() - 2);
                    if (!decodeStringContent(content, decoded, &invalidOffset)) {
                        const auto lineOffset = line.find(std::string_view(
                            initializer.data(), initializer.length()));
                        def.diagnostics.push_back({
                            lineNo,
                            (lineOffset == std::string_view::npos ? 0 : lineOffset) +
                                invalidOffset + 2,
                            "unsupported string escape"});
                    }
                }
            }
            if (!attributes.empty()) {
                std::size_t cursor = 0;
                while (cursor < attributes.size()) {
                    const auto open = attributes.find('[', cursor);
                    if (open == std::string::npos) break;
                    std::size_t close = std::string::npos;
                    bool inString = false;
                    bool escaped = false;
                    for (std::size_t scan = open + 1; scan < attributes.size(); ++scan) {
                        const char current = attributes[scan];
                        if (inString) {
                            if (escaped) escaped = false;
                            else if (current == '\\') escaped = true;
                            else if (current == '"') inString = false;
                        } else if (current == '"') {
                            inString = true;
                        } else if (current == ']') {
                            close = scan;
                            break;
                        }
                    }
                    if (close == std::string::npos) {
                        def.diagnostics.push_back({attributesLine, 1, "unterminated field attribute"});
                        break;
                    }
                    const ZeroString itemView = trim(attributes.substr(open + 1, close - open - 1));
                    const std::string item(itemView.data(), itemView.length());
                    if (item == "SerializeField") {
                        field.serialized = true;
                    } else if (starts_with(item, "Range")) {
                        const bool validSyntax = starts_with(item, "Range(") &&
                            item.size() > 7 && item.back() == ')';
                        const std::string_view inner = validSyntax
                            ? std::string_view(item.data() + 6, item.size() - 7)
                            : std::string_view{};
                        const auto comma = inner.find(',');
                        const auto parseBound = [](std::string_view text, double& value) {
                            const ZeroString trimmedBound = trim(text);
                            const char* begin = trimmedBound.data();
                            const char* end = begin + trimmedBound.length();
                            if (begin == end) return false;
                            if (*begin == '+') ++begin;
                            const auto [parsedEnd, error] = std::from_chars(
                                begin, end, value, std::chars_format::general);
                            return error == std::errc{} && parsedEnd == end &&
                                   std::isfinite(value);
                        };
                        double rangeMin = 0.0;
                        double rangeMax = 0.0;
                        const bool validRange = validSyntax &&
                            comma != std::string_view::npos &&
                            inner.find(',', comma + 1) == std::string_view::npos &&
                            parseBound(inner.substr(0, comma), rangeMin) &&
                            parseBound(inner.substr(comma + 1), rangeMax) &&
                            rangeMin <= rangeMax;
                        if (!validRange) {
                            def.diagnostics.push_back({attributesLine, 1, "invalid Range attribute"});
                        } else {
                            field.hasRange = true;
                            field.rangeMin = rangeMin;
                            field.rangeMax = rangeMax;
                        }
                    } else if (starts_with(item, "Header") || starts_with(item, "Tooltip")) {
                        const bool isHeader = starts_with(item, "Header");
                        const std::string_view prefix = isHeader ? "Header(" : "Tooltip(";
                        bool validTextAttribute = starts_with(item, prefix) &&
                            item.size() > prefix.size() && item.back() == ')';
                        std::string decoded;
                        if (validTextAttribute) {
                            const ZeroString argument = trim(std::string_view(
                                item.data() + prefix.size(), item.size() - prefix.size() - 1));
                            validTextAttribute = argument.length() >= 2 &&
                                argument.data()[0] == '"' &&
                                argument.data()[argument.length() - 1] == '"' &&
                                decodeStringContent(std::string_view(
                                    argument.data() + 1, argument.length() - 2), decoded);
                        }
                        if (!validTextAttribute) {
                            def.diagnostics.push_back({
                                attributesLine, 1, isHeader ? "invalid Header attribute"
                                                    : "invalid Tooltip attribute"});
                        } else if (isHeader) {
                            field.header = std::move(decoded);
                        } else {
                            field.tooltip = std::move(decoded);
                        }
                    }
                    cursor = close + 1;
                }
            }
            if (eq != static_cast<std::size_t>(-1)) {
                field.defaultValue = parseDefaultValue(body.substr(eq + 1), field.type);
            } else if (field.type == ArtifactScriptValueType::Array) {
                field.defaultValue = makeShared<ArtifactScriptArray>();
            }
            activeClass().fields.push_back(std::move(field));
            pos = nextPos;
            continue;
        }

        if (isMethod) {
            pendingAttributes.clear();
            ArtifactScriptMethod method;
            const auto paren = trimmed.find('(');
            const ZeroString before = trim(trimmed.substr(0, paren));
            const auto space = before.lastIndexOf(' ');
            const ZeroString methodName = space < 0 ? before : trim(before.substr(static_cast<std::size_t>(space + 1)));
            method.name = std::string(methodName.data(), methodName.length());
            method.line = lineNo;
            const auto methodColumn = line.find(std::string_view(
                method.name.data(), method.name.size()));
            method.column = methodColumn == std::string_view::npos
                                ? 1
                                : methodColumn + 1;
            pendingAttributes.clear();
            method.parameters.clear();
            const auto parameterListEnd = paren == static_cast<std::size_t>(-1)
                ? static_cast<std::size_t>(-1) : trimmed.find(')', paren + 1);
            if (parameterListEnd != static_cast<std::size_t>(-1)) {
                const ZeroString parameterList = trimmed.substr(
                    paren + 1, parameterListEnd - paren - 1);
                for (std::size_t parameterStart = 0; parameterStart < parameterList.length();) {
                    const auto comma = parameterList.find(',', parameterStart);
                    const auto parameterEnd = comma == static_cast<std::size_t>(-1)
                        ? parameterList.length() : comma;
                    ZeroString parameter = trim(parameterList.substr(
                        parameterStart, parameterEnd - parameterStart));
                    const auto lastSpace = parameter.lastIndexOf(' ');
                    if (lastSpace >= 0) {
                        parameter = trim(parameter.substr(static_cast<std::size_t>(lastSpace + 1)));
                    }
                    if (!parameter.isEmpty()) {
                        method.parameters.emplace_back(parameter.data(), parameter.length());
                    }
                    if (comma == static_cast<std::size_t>(-1)) break;
                    parameterStart = comma + 1;
                }
            }
            if (const auto hook = hookFromName(method.name)) {
                method.isLifecycleHook = true;
                method.hook = *hook;
            }
            // Parse method body if present
            // The definition owns source for the full parse; keep the body as
            // a view instead of allocating and copying it per method.
            std::string_view bodyText;
            size_t bodyContentStart = std::string_view::npos;
            bool hasBody = false;
            size_t bodyStart = trimmed.find('{');
            if (bodyStart != std::string::npos) {
                // Body starts on same line
                size_t leadingWhitespace = 0;
                while (leadingWhitespace < line.size() &&
                       std::isspace(static_cast<unsigned char>(line[leadingWhitespace]))) {
                    ++leadingWhitespace;
                }
                const size_t openBracePosition = pos + leadingWhitespace + bodyStart;
                if (const auto bodyEnd = findMethodBodyEnd(sourceView, openBracePosition)) {
                    bodyContentStart = openBracePosition + 1;
                    bodyText = sourceView.substr(
                        bodyContentStart, *bodyEnd - bodyContentStart);
                    hasBody = true;
                    pos = *bodyEnd + 1;
                    nextPos = sourceView.find('\n', pos);
                }
            } else {
                // Body on subsequent line - look for opening brace
                size_t lookPos = nextPos;
                while (lookPos < sourceView.size() && std::isspace(static_cast<unsigned char>(sourceView[lookPos])))
                    ++lookPos;
                if (lookPos < sourceView.size() && sourceView[lookPos] == '{') {
                    if (const auto bodyEnd = findMethodBodyEnd(sourceView, lookPos)) {
                        bodyContentStart = lookPos + 1;
                        bodyText = sourceView.substr(
                            bodyContentStart, *bodyEnd - bodyContentStart);
                        hasBody = true;
                        pos = *bodyEnd + 1;
                        nextPos = sourceView.find('\n', pos);
                    }
                }
            }
            if (hasBody) {
                bool parseFailed = false;
                std::size_t failurePosition = std::string_view::npos;
                method.body = parseMethodBody(
                    bodyText, method.parameters, parseFailed, failurePosition);
                if (parseFailed) {
                    std::size_t diagnosticLine = 1;
                    std::size_t diagnosticColumn = 1;
                    const std::size_t absoluteFailurePosition = bodyContentStart +
                        (failurePosition == std::string_view::npos
                            ? 0 : failurePosition);
                    for (std::size_t sourcePosition = 0;
                         sourcePosition < absoluteFailurePosition &&
                         sourcePosition < sourceView.size(); ++sourcePosition) {
                        if (sourceView[sourcePosition] == '\n') {
                            ++diagnosticLine;
                            diagnosticColumn = 1;
                        } else {
                            ++diagnosticColumn;
                        }
                    }
                    def.diagnostics.push_back({diagnosticLine, diagnosticColumn,
                        "unsupported or invalid syntax in method body"});
                }
            }
            activeClass().methods.push_back(std::move(method));
            pos = nextPos;
            continue;
        }

        if (pos == lineStart) {
            def.diagnostics.push_back({lineNo, 1,
                "unsupported or invalid class member syntax"});
            pos = nextPos;
        }
    }

    finishClass();
    if (!def.classes.empty()) {
        // rootClass owns the first class bodies (move-only); classes.front()
        // keeps metadata only. findClass() maps the first name to rootClass.
        ArtifactScriptClass& first = def.classes.front();
        def.rootClass.name = first.name;
        def.rootClass.parentName = first.parentName;
        def.rootClass.derivesFromBehaviour = first.derivesFromBehaviour;
        def.rootClass.fields = first.fields;
        def.rootClass.methods.clear();
        for (auto& method : first.methods) {
            def.rootClass.methods.push_back(std::move(method));
        }
        first.methods.clear();
    }

    if (def.rootClass.name.empty()) {
        def.diagnostics.push_back({0, 0, "no class declaration found"});
    }

    return def;
}

void ArtifactScriptComponent::setScriptClass(const ZeroString& className) {
    scriptClass_ = className;
}

void ArtifactScriptComponent::setScriptClass(std::string className) {
    scriptClass_ = ZeroString(std::move(className));
}

void ArtifactScriptComponent::setScriptClass(std::string_view className) {
    scriptClass_ = className;
}

void ArtifactScriptComponent::setScriptClass(const char* className) {
    scriptClass_ = ZeroString(className);
}

const ZeroString& ArtifactScriptComponent::scriptClassZero() const {
    return scriptClass_;
}

const std::string& ArtifactScriptComponent::scriptClass() const {
    static thread_local std::string cache;
    cache.assign(scriptClass_.data(), scriptClass_.length());
    return cache;
}

ArtifactScriptSerializedFields& ArtifactScriptComponent::publicFields() {
    return publicFields_;
}

const ArtifactScriptSerializedFields& ArtifactScriptComponent::publicFields() const {
    return publicFields_;
}

// ─── Phase 4: JSON serialization ───

namespace {

// Minimal JSON tokenizer/parser: objects, arrays, strings (with \" \\ / \b \f
// \n \r \t and \uXXXX escapes), numbers, true/false/null.
struct ScriptJsonCursor {
    std::string_view text;
    std::size_t pos = 0;
    bool skipWhitespace() {
        while (pos < text.size() &&
               (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' || text[pos] == '\n')) {
            ++pos;
        }
        return pos < text.size();
    }
    bool consume(char c) {
        skipWhitespace();
        if (pos < text.size() && text[pos] == c) { ++pos; return true; }
        return false;
    }
    char peek() {
        skipWhitespace();
        return pos < text.size() ? text[pos] : '\0';
    }
};

bool parseJsonString(ScriptJsonCursor& cur, std::string& out) {
    if (!cur.consume('"')) return false;
    out.clear();
    while (cur.pos < cur.text.size()) {
        const char c = cur.text[cur.pos++];
        if (c == '"') return true;
        if (c != '\\') { out.push_back(c); continue; }
        if (cur.pos >= cur.text.size()) return false;
        const char esc = cur.text[cur.pos++];
        switch (esc) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
            if (cur.pos + 4 > cur.text.size()) return false;
            unsigned code = 0;
            for (int i = 0; i < 4; ++i) {
                const char h = cur.text[cur.pos++];
                code <<= 4;
                if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
                else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
                else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
                else return false;
            }
            if (code < 0x80) out.push_back(static_cast<char>(code));
            else if (code < 0x800) {
                out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else {
                out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            break;
        }
        default: return false;
        }
    }
    return false;
}

void appendJsonString(std::string& out, std::string_view text) {
    out.push_back('"');
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default: out.push_back(c); break;
        }
    }
    out.push_back('"');
}

struct ScriptJsonValue;
using ScriptJsonPtr = std::unique_ptr<ScriptJsonValue>;

struct ScriptJsonValue {
    enum class Kind { Null, Bool, Number, String, Array, Object };
    Kind kind = Kind::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::int64_t integerValue = 0;
    bool isIntegerToken = false;
    bool hasIntegerValue = false;
    std::string stringValue;
    std::vector<ScriptJsonPtr> array;
    std::vector<std::pair<std::string, ScriptJsonPtr>> object;
};

bool parseJsonValue(ScriptJsonCursor& cur, ScriptJsonPtr& out, std::string& error);

bool parseJsonNumber(ScriptJsonCursor& cur, ScriptJsonPtr& out) {
    const std::size_t begin = cur.pos;
    if (cur.pos < cur.text.size() && (cur.text[cur.pos] == '-' || cur.text[cur.pos] == '+')) ++cur.pos;
    bool anyDigit = false;
    while (cur.pos < cur.text.size() &&
           (std::isdigit(static_cast<unsigned char>(cur.text[cur.pos])) || cur.text[cur.pos] == '.' ||
            cur.text[cur.pos] == 'e' || cur.text[cur.pos] == 'E' || cur.text[cur.pos] == '-' ||
            cur.text[cur.pos] == '+')) {
        anyDigit = anyDigit || std::isdigit(static_cast<unsigned char>(cur.text[cur.pos]));
        ++cur.pos;
    }
    if (!anyDigit) return false;
    const std::string_view token = cur.text.substr(begin, cur.pos - begin);
    out->isIntegerToken = token.find_first_of(".eE") == std::string_view::npos;
    if (out->isIntegerToken) {
        const auto [integerEnd, integerError] = std::from_chars(
            token.data(), token.data() + token.size(), out->integerValue);
        out->hasIntegerValue = integerError == std::errc{} &&
                               integerEnd == token.data() + token.size();
    }
    const auto [end, ec] = std::from_chars(
        token.data(), token.data() + token.size(), out->numberValue);
    if (ec != std::errc{} || end != token.data() + token.size()) return false;
    out->kind = ScriptJsonValue::Kind::Number;
    return true;
}

bool parseJsonMember(ScriptJsonCursor& cur, ScriptJsonValue& object, std::string& error) {
    std::string key;
    if (!parseJsonString(cur, key)) {
        error = "invalid object key";
        return false;
    }
    if (!cur.consume(':')) {
        error = "expected ':'";
        return false;
    }
    ScriptJsonPtr value = std::make_unique<ScriptJsonValue>();
    if (!parseJsonValue(cur, value, error)) return false;
    object.object.emplace_back(std::move(key), std::move(value));
    return true;
}

bool parseJsonValue(ScriptJsonCursor& cur, ScriptJsonPtr& out, std::string& error) {
    const char c = cur.peek();
    if (c == '{') {
        ++cur.pos;
        out->kind = ScriptJsonValue::Kind::Object;
        if (cur.consume('}')) return true;
        while (true) {
            if (!parseJsonMember(cur, *out, error)) return false;
            if (cur.consume(',')) continue;
            if (cur.consume('}')) return true;
            error = "expected ',' or '}'";
            return false;
        }
    }
    if (c == '[') {
        ++cur.pos;
        out->kind = ScriptJsonValue::Kind::Array;
        if (cur.consume(']')) return true;
        while (true) {
            ScriptJsonPtr element = std::make_unique<ScriptJsonValue>();
            if (!parseJsonValue(cur, element, error)) return false;
            out->array.push_back(std::move(element));
            if (cur.consume(',')) continue;
            if (cur.consume(']')) return true;
            error = "expected ',' or ']'";
            return false;
        }
    }
    if (c == '"') {
        out->kind = ScriptJsonValue::Kind::String;
        return parseJsonString(cur, out->stringValue);
    }
    if (c == 't' && cur.text.compare(cur.pos, 4, "true") == 0) {
        cur.pos += 4;
        out->kind = ScriptJsonValue::Kind::Bool;
        out->boolValue = true;
        return true;
    }
    if (c == 'f' && cur.text.compare(cur.pos, 5, "false") == 0) {
        cur.pos += 5;
        out->kind = ScriptJsonValue::Kind::Bool;
        out->boolValue = false;
        return true;
    }
    if (c == 'n' && cur.text.compare(cur.pos, 4, "null") == 0) {
        cur.pos += 4;
        return true;
    }
    if (c == '-' || c == '+' || std::isdigit(static_cast<unsigned char>(c))) {
        return parseJsonNumber(cur, out);
    }
    error = "unexpected character";
    return false;
}

const ScriptJsonValue* findMember(const ScriptJsonValue& object, std::string_view key) {
    if (object.kind != ScriptJsonValue::Kind::Object) return nullptr;
    for (const auto& [name, value] : object.object) {
        if (name == key) return value.get();
    }
    return nullptr;
}

double jsonNumber(const ScriptJsonValue& value) { return value.numberValue; }
bool jsonNumberToInt64(const ScriptJsonValue& value, std::int64_t& out) {
    if (value.hasIntegerValue) {
        out = value.integerValue;
        return true;
    }
    if (value.isIntegerToken) return false;
    const double number = value.numberValue;
    constexpr double int64UpperBound = 9223372036854775808.0;
    if (!std::isfinite(number) || number < -int64UpperBound ||
        number >= int64UpperBound || std::trunc(number) != number) {
        return false;
    }
    out = static_cast<std::int64_t>(number);
    return true;
}
bool jsonToScriptValue(const ScriptJsonValue& json, ArtifactScriptValueType type,
                       ArtifactScriptValue& out, std::string& error) {
    using K = ScriptJsonValue::Kind;
    switch (type) {
    case ArtifactScriptValueType::Null:
        out = std::monostate{};
        return true;
    case ArtifactScriptValueType::Bool:
        if (json.kind != K::Bool) { error = "expected bool"; return false; }
        out = json.boolValue;
        return true;
    case ArtifactScriptValueType::Int:
        if (json.kind != K::Number) { error = "expected number"; return false; }
        {
            std::int64_t integer = 0;
            if (!jsonNumberToInt64(json, integer)) {
                error = "expected int64";
                return false;
            }
            out = integer;
        }
        return true;
    case ArtifactScriptValueType::Float:
        if (json.kind != K::Number) { error = "expected number"; return false; }
        out = json.numberValue;
        return true;
    case ArtifactScriptValueType::String:
        if (json.kind != K::String) { error = "expected string"; return false; }
        out = json.stringValue;
        return true;
    case ArtifactScriptValueType::Vec2: case ArtifactScriptValueType::Vec3:
    case ArtifactScriptValueType::Vec4: case ArtifactScriptValueType::Color: {
        if (json.kind != K::Array || json.array.size() < 2) { error = "expected array value"; return false; }
        auto read = [&](std::size_t index) -> double {
            return index < json.array.size() ? jsonNumber(*json.array[index]) : 0.0;
        };
        switch (type) {
        case ArtifactScriptValueType::Vec2:
            out = ArtifactScriptVec2{static_cast<float>(read(0)), static_cast<float>(read(1))};
            return true;
        case ArtifactScriptValueType::Vec3:
            out = ArtifactScriptVec3{static_cast<float>(read(0)), static_cast<float>(read(1)),
                                     static_cast<float>(read(2))};
            return true;
        case ArtifactScriptValueType::Vec4:
            out = ArtifactScriptVec4{static_cast<float>(read(0)), static_cast<float>(read(1)),
                                     static_cast<float>(read(2)), static_cast<float>(read(3))};
            return true;
        default:
            out = ArtifactScriptColor{static_cast<float>(read(0)), static_cast<float>(read(1)),
                                      static_cast<float>(read(2)), static_cast<float>(read(3))};
            return true;
        }
    }
    case ArtifactScriptValueType::ObjectRef: case ArtifactScriptValueType::AssetRef:
        if (json.kind != K::String) { error = "expected string ref"; return false; }
        out = ArtifactScriptRef{json.stringValue};
        return true;
    case ArtifactScriptValueType::Array: {
        if (json.kind != K::Array) { error = "expected array"; return false; }
        auto array = std::make_shared<ArtifactScriptArray>();
        for (const auto& element : json.array) {
            if (element->kind == K::Number) {
                std::int64_t integer = 0;
                array->values.push_back(jsonNumberToInt64(*element, integer)
                                            ? ArtifactScriptValue(integer)
                                            : ArtifactScriptValue(jsonNumber(*element)));
            } else if (element->kind == K::Bool) {
                array->values.push_back(ArtifactScriptValue(element->boolValue));
            } else if (element->kind == K::String) {
                array->values.push_back(ArtifactScriptValue(element->stringValue));
            }
        }
        out = ArtifactScriptArrayPtr(std::move(array));
        return true;
    }
    }
    error = "unsupported type";
    return false;
}

} // namespace

bool serializeScriptValue(std::string_view name, const ArtifactScriptValue& value,
                          std::string& out, std::string& error);

std::string serializeScriptComponent(const ArtifactScriptSerializedComponent& component) {
    std::string out;
    out.reserve(128);
    out += "{\"class\":";
    appendJsonString(out, component.className);
    out += ",\"values\":{";
    bool first = true;
    for (const auto& [name, value] : component.values) {
        if (!first) out += ',';
        first = false;
        appendJsonString(out, name);
        out += ':';
        std::string error;
        std::string valueText;
        if (serializeScriptValue(name, value, valueText, error)) {
            out += valueText;
        } else {
            out += "null";
        }
    }
    out += '}';
    // Unknown keys are re-emitted so downgrades do not lose data.
    out += ",\"unknown\":{";
    first = true;
    for (const auto& [name, value] : component.unknown) {
        if (!first) out += ',';
        first = false;
        appendJsonString(out, name);
        out += ':';
        std::string error;
        std::string valueText;
        if (serializeScriptValue(name, value, valueText, error)) {
            out += valueText;
        } else {
            out += "null";
        }
    }
    out += "}}";
    return out;
}

bool serializeScriptValue(std::string_view name, const ArtifactScriptValue& value,
                          std::string& out, std::string& error) {
    (void)name;
    if (const auto* boolean = std::get_if<bool>(&value)) {
        out = *boolean ? "true" : "false";
        return true;
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        char buffer[32];
        auto [end, ec] = std::to_chars(buffer, buffer + sizeof(buffer), *integer);
        if (ec != std::errc{}) { error = "int overflow"; return false; }
        out.assign(buffer, static_cast<std::size_t>(end - buffer));
        return true;
    }
    if (const auto* number = std::get_if<double>(&value)) {
        if (std::isnan(*number) || std::isinf(*number)) { error = "non-finite number"; return false; }
        char buffer[64];
        auto [end, ec] = std::to_chars(buffer, buffer + sizeof(buffer), *number);
        if (ec != std::errc{}) { error = "double overflow"; return false; }
        out.assign(buffer, static_cast<std::size_t>(end - buffer));
        return true;
    }
    if (const auto* text = std::get_if<std::string>(&value)) {
        appendJsonString(out, *text);
        return true;
    }
    if (const auto* vec2 = std::get_if<ArtifactScriptVec2>(&value)) {
        out = "[" + std::to_string(vec2->x) + "," + std::to_string(vec2->y) + "]";
        return true;
    }
    if (const auto* vec3 = std::get_if<ArtifactScriptVec3>(&value)) {
        out = "[" + std::to_string(vec3->x) + "," + std::to_string(vec3->y) + "," +
              std::to_string(vec3->z) + "]";
        return true;
    }
    if (const auto* vec4 = std::get_if<ArtifactScriptVec4>(&value)) {
        out = "[" + std::to_string(vec4->x) + "," + std::to_string(vec4->y) + "," +
              std::to_string(vec4->z) + "," + std::to_string(vec4->w) + "]";
        return true;
    }
    if (const auto* color = std::get_if<ArtifactScriptColor>(&value)) {
        out = "[" + std::to_string(color->r) + "," + std::to_string(color->g) + "," +
              std::to_string(color->b) + "," + std::to_string(color->a) + "]";
        return true;
    }
    if (const auto* ref = std::get_if<ArtifactScriptRef>(&value)) {
        appendJsonString(out, ref->id);
        return true;
    }
    if (const auto* array = std::get_if<ArtifactScriptArrayPtr>(&value)) {
        if (!*array) { out = "null"; return true; }
        out = "[";
        bool first = true;
        for (const auto& element : (*array)->values) {
            if (!first) out += ',';
            first = false;
            std::string elementText;
            if (serializeScriptValue(name, element, elementText, error)) {
                out += elementText;
            } else {
                out += "null";
            }
        }
        out += "]";
        return true;
    }
    if (std::holds_alternative<ArtifactScriptObjectInstancePtr>(value)) {
        error = "object instances are not serializable";
        return false;
    }
    out = "null";
    return true;
}

bool deserializeScriptComponent(std::string_view json, ArtifactScriptSerializedComponent& out,
                                std::string& error) {
    out = ArtifactScriptSerializedComponent{};
    ScriptJsonCursor cursor{json, 0};
    ScriptJsonPtr root = std::make_unique<ScriptJsonValue>();
    if (!parseJsonValue(cursor, root, error)) return false;
    if (root->kind != ScriptJsonValue::Kind::Object) {
        error = "expected object";
        return false;
    }
    if (const auto* className = findMember(*root, "class");
        className && className->kind == ScriptJsonValue::Kind::String) {
        out.className = className->stringValue;
    }
    const auto readValues = [&](std::string_view key, ArtifactScriptSerializedFields& target) {
        const auto* container = findMember(*root, key);
        if (!container || container->kind != ScriptJsonValue::Kind::Object) return true;
        for (const auto& [name, value] : container->object) {
            if (value->kind == ScriptJsonValue::Kind::Bool) {
                target.emplace(name, ArtifactScriptValue{value->boolValue});
            } else if (value->kind == ScriptJsonValue::Kind::Number) {
                const double n = jsonNumber(*value);
                std::int64_t integer = 0;
                if (jsonNumberToInt64(*value, integer) &&
                    (value->hasIntegerValue || std::fabs(n) < 1.0e15)) {
                    target.emplace(name, ArtifactScriptValue(integer));
                } else {
                    target.emplace(name, ArtifactScriptValue(n));
                }
            } else if (value->kind == ScriptJsonValue::Kind::String) {
                target.emplace(name, ArtifactScriptValue(value->stringValue));
            } else if (value->kind == ScriptJsonValue::Kind::Array) {
                ArtifactScriptValue parsed;
                std::string valueError;
                if (jsonToScriptValue(*value, ArtifactScriptValueType::Array, parsed, valueError)) {
                    target.emplace(name, std::move(parsed));
                }
            }
            // Null and nested objects are skipped (unknown shapes preserved raw).
        }
        return true;
    };
    if (!readValues("values", out.values)) return false;
    if (!readValues("unknown", out.unknown)) return false;
    error.clear();
    return true;
}

bool deserializeScriptValue(std::string_view text, ArtifactScriptValueType type,
                            ArtifactScriptValue& out, std::string& error) {
    if (type == ArtifactScriptValueType::String) {
        // Raw text form: strings need no quoting at this boundary.
        out = std::string(text);
        error.clear();
        return true;
    }
    ScriptJsonCursor cursor{text, 0};
    ScriptJsonPtr root = std::make_unique<ScriptJsonValue>();
    if (!parseJsonValue(cursor, root, error)) return false;
    error.clear();
    return jsonToScriptValue(*root, type, out, error);
}
ArtifactScriptSerializedFields ArtifactScriptComponent::serializedFields(
    const ArtifactScriptDefinition& definition) const {
    ArtifactScriptSerializedFields out;
    // Public fields always serialize; private fields only when marked
    // [SerializeField] (field.serialized == true).
    for (const auto& field : definition.rootClass.fields) {
        if (!field.serialized) continue;
        const auto it = publicFields_.find(field.name);
        if (it != publicFields_.end()) {
            out.emplace(field.name, it->second);
        } else {
            out.emplace(field.name, field.defaultValue);
        }
    }
    return out;
}

void ArtifactScriptComponent::applySerializedComponent(
    const ArtifactScriptDefinition& definition,
    const ArtifactScriptSerializedComponent& component) {
    if (!component.className.empty()) {
        scriptClass_ = ZeroString(component.className);
    }
    // Start from defaults, then overlay saved values with a type check.
    publicFields_.clear();
    for (const auto& field : definition.rootClass.fields) {
        if (!field.serialized) continue;
        ArtifactScriptValue value = field.defaultValue;
        if (const auto saved = component.values.find(field.name); saved != component.values.end()) {
            if (saved->second.index() == value.index()) {
                value = saved->second;
            }
        }
        publicFields_.emplace(field.name, std::move(value));
    }
    // Unknown keys are preserved on load and re-emitted on save.
    for (const auto& [name, value] : component.unknown) {
        publicFields_.emplace(name, value);
    }
}

void ArtifactScriptComponent::applyDefaults(const ArtifactScriptDefinition& definition) {
    if (!scriptClass_.isEmpty() && !definition.rootClass.name.empty() && std::string(scriptClass_.data(), scriptClass_.length()) != definition.rootClass.name) {
        return;
    }

    for (const auto& field : definition.rootClass.fields) {
        if (!field.isPublic) {
            continue;
        }
        if (publicFields_.find(field.name) != publicFields_.end()) {
            continue;
        }
        publicFields_.emplace(field.name, field.defaultValue);
    }
}

ArtifactScriptInstance::ArtifactScriptInstance(ArtifactScriptDefinition definition)
    : definition_(std::move(definition)) {
}

const ArtifactScriptDefinition& ArtifactScriptInstance::definition() const {
    return definition_;
}

ArtifactScriptDefinition& ArtifactScriptInstance::definition() {
    evaluator_.disableClassLookupReuse();
    return definition_;
}

void ArtifactScriptInstance::bindComponent(const ArtifactScriptComponent& component) {
    component_ = &component;
}

const ArtifactScriptComponent* ArtifactScriptInstance::boundComponent() const {
    return component_;
}

bool ArtifactScriptInstance::hasMethod(std::string_view name) const {
    return findMethodInDefinition(name) != nullptr;
}

bool ArtifactScriptInstance::hasHook(ArtifactScriptHook hook) const {
    return findLifecycleHookInDefinition(hook) != nullptr;
}

const ArtifactScriptMethod* ArtifactScriptInstance::findMethodInDefinition(
    std::string_view name) const {
    if (name.empty() || definition_.rootClass.name.empty()) {
        return nullptr;
    }
    // Walk derived -> base so a derived override wins over a base declaration.
    const ArtifactScriptClass* cls = findClassByName(definition_.rootClass.name);
    if (!cls) {
        return nullptr;
    }
    std::string_view current = cls->name;
    for (int depth = 0; depth < 32; ++depth) {
        const ArtifactScriptClass* level = findClassByName(current);
        if (!level) {
            break;
        }
        for (const auto& method : level->methods) {
            if (method.name == name && method.body) {
                return &method;
            }
        }
        if (level->parentName.empty()) {
            break;
        }
        current = level->parentName;
    }
    return nullptr;
}

const ArtifactScriptMethod* ArtifactScriptInstance::findLifecycleHookInDefinition(
    ArtifactScriptHook hook) const {
    return evaluator_.findLifecycleHook(definition_, hook);
}

const ArtifactScriptClass* ArtifactScriptInstance::findClassByName(
    std::string_view className) const {
    if (className == definition_.rootClass.name) {
        return &definition_.rootClass;
    }
    for (const auto& cls : definition_.classes) {
        if (cls.name == className) {
            return &cls;
        }
    }
    return nullptr;
}

ArtifactScriptSerializedFields& ArtifactScriptInstance::fields() {
    return fields_;
}

const ArtifactScriptSerializedFields& ArtifactScriptInstance::fields() const {
    return fields_;
}

std::string ArtifactScriptInstance::lastError() const {
    return lastHookError_;
}

bool ArtifactScriptInstance::invokeHook(ArtifactScriptHook hook) {
    const ArtifactScriptMethod* method = findLifecycleHookInDefinition(hook);
    if (!method) {
        return false;
    }

    if (component_) {
        fields_ = component_->publicFields();
    } else if (fields_.empty()) {
        ArtifactScriptComponent defaults;
        defaults.setScriptClass(definition_.rootClass.name);
        defaults.applyDefaults(definition_);
        fields_ = defaults.publicFields();
    }
    // Lifecycle hooks receive no arguments; dt is provided as a field when
    // the host sets it (fields()["dt"]).
    static const std::vector<ArtifactScriptValue> noArguments;
    evaluator_.executeResolvedMethod(
        definition_, *method, noArguments, fields_, true);
    const bool ok = !evaluator_.hasError();
    if (ok) lastHookError_.clear();
    else lastHookError_ = evaluator_.getLastError();
    lastInvokedHook_ = hook;
    return ok;
}

void ArtifactScriptLayerRuntime::bind(ArtifactScriptDefinition definition) {
    release();
    instance_ = ArtifactScriptInstance(std::move(definition));
    hasInstance_ = true;
    ArtifactScriptComponent defaults;
    defaults.setScriptClass(instance_.definition().rootClass.name);
    defaults.applyDefaults(instance_.definition());
    instance_.fields() = defaults.publicFields();
    for (const auto& field : instance_.definition().rootClass.fields) {
        instance_.fields().try_emplace(field.name, field.defaultValue);
    }
}

void ArtifactScriptLayerRuntime::replaceDefinition(
    ArtifactScriptDefinition definition, ArtifactScriptSerializedFields fields) {
    lastError_.clear();
    if (!hasInstance_) {
        bind(std::move(definition));
        instance_.fields() = std::move(fields);
    } else {
        instance_ = ArtifactScriptInstance(std::move(definition));
        instance_.fields() = std::move(fields);
    }
    for (const auto& field : instance_.definition().rootClass.fields) {
        instance_.fields().try_emplace(field.name, field.defaultValue);
    }
}

void ArtifactScriptLayerRuntime::release() {
    instance_ = ArtifactScriptInstance{};
    hasInstance_ = false;
    runState_ = ArtifactScriptLayerRunState::Unbound;
    lastFrame_ = std::numeric_limits<std::int64_t>::min();
    lastError_.clear();
}

bool ArtifactScriptLayerRuntime::hasInstance() const {
    return hasInstance_;
}

ArtifactScriptInstance* ArtifactScriptLayerRuntime::instance() {
    return hasInstance_ ? &instance_ : nullptr;
}

const ArtifactScriptInstance* ArtifactScriptLayerRuntime::instance() const {
    return hasInstance_ ? &instance_ : nullptr;
}

bool ArtifactScriptLayerRuntime::advanceLifecycle(
    ArtifactScriptLayerRunState target, bool enabled) {
    if (!enabled || !hasInstance_) {
        target = ArtifactScriptLayerRunState::Unbound;
    }
    if (runState_ == target) {
        return false;
    }

    const ArtifactScriptLayerRunState previous = runState_;
    runState_ = target;
    lastError_.clear();
    const auto invoke = [this](ArtifactScriptHook hook) {
        if (!instance_.hasHook(hook)) {
            return;
        }
        if (!instance_.invokeHook(hook) && lastError_.empty()) {
            lastError_ = instance_.lastError();
        }
    };

    switch (target) {
    case ArtifactScriptLayerRunState::Created:
        invoke(ArtifactScriptHook::OnCreate);
        invoke(ArtifactScriptHook::OnStart);
        break;
    case ArtifactScriptLayerRunState::Enabled:
        if (previous == ArtifactScriptLayerRunState::Unbound) {
            invoke(ArtifactScriptHook::OnCreate);
            invoke(ArtifactScriptHook::OnStart);
        }
        invoke(ArtifactScriptHook::OnEnable);
        break;
    case ArtifactScriptLayerRunState::Unbound:
        if (previous == ArtifactScriptLayerRunState::Enabled) {
            invoke(ArtifactScriptHook::OnDisable);
        }
        if (previous != ArtifactScriptLayerRunState::Unbound) {
            invoke(ArtifactScriptHook::OnDestroy);
        }
        break;
    }
    return true;
}

bool ArtifactScriptLayerRuntime::evaluateFrame(
    std::int64_t frame, double timeSeconds, double deltaSeconds) {
    if (!hasInstance_ || frame == lastFrame_) {
        return false;
    }
    lastFrame_ = frame;
    auto& fields = instance_.fields();
    fields["dt"] = ArtifactScriptValue(deltaSeconds);
    fields["time"] = ArtifactScriptValue(timeSeconds);
    fields["frame"] = ArtifactScriptValue(frame);
    if (!instance_.invokeHook(ArtifactScriptHook::OnUpdate)) {
        lastError_ = instance_.lastError();
        return false;
    }
    lastError_.clear();
    return true;
}

const std::string& ArtifactScriptLayerRuntime::lastError() const {
    return lastError_;
}

void ArtifactScriptLayerRuntime::setLastError(std::string error) {
    lastError_ = std::move(error);
}

bool ArtifactScriptInstance::wasHookInvoked(ArtifactScriptHook hook) const {
    return lastInvokedHook_.has_value() && *lastInvokedHook_ == hook;
}


// ─── Evaluator ───

namespace {

struct ArtifactScriptHostMethodKey {
    std::string className;
    std::string methodName;
};

struct ArtifactScriptHostMethodKeyView {
    std::string_view className;
    std::string_view methodName;
};

struct ArtifactScriptHostMethodKeyHash {
    using is_transparent = void;

    static std::size_t combine(std::string_view className, std::string_view methodName) {
        const auto classHash = std::hash<std::string_view>{}(className);
        const auto methodHash = std::hash<std::string_view>{}(methodName);
        return classHash ^ (methodHash + 0x9e3779b97f4a7c15ull +
                            (classHash << 6) + (classHash >> 2));
    }

    std::size_t operator()(const ArtifactScriptHostMethodKey& key) const {
        return combine(key.className, key.methodName);
    }
    std::size_t operator()(const ArtifactScriptHostMethodKeyView& key) const {
        return combine(key.className, key.methodName);
    }
};

struct ArtifactScriptHostMethodKeyEqual {
    using is_transparent = void;

    bool operator()(const ArtifactScriptHostMethodKey& left,
                    const ArtifactScriptHostMethodKey& right) const {
        return left.className == right.className && left.methodName == right.methodName;
    }
    bool operator()(const ArtifactScriptHostMethodKey& left,
                    const ArtifactScriptHostMethodKeyView& right) const {
        return left.className == right.className && left.methodName == right.methodName;
    }
    bool operator()(const ArtifactScriptHostMethodKeyView& left,
                    const ArtifactScriptHostMethodKey& right) const {
        return (*this)(right, left);
    }
};

struct ArtifactScriptLocalBinding {
    // Local names refer to strings owned by the live method AST.
    std::string_view name;
    ArtifactScriptValue value;
};

// The argument workspace covers every accepted method-call frame, so deep
// recursion with overflow arguments does not fall back to per-call storage.
constexpr std::size_t kArtifactScriptMaxCallDepth = 64;

class ArtifactScriptCallArguments {
public:
    struct Workspace {
        static constexpr std::size_t maxDepth = kArtifactScriptMaxCallDepth;
        static constexpr std::size_t maxRetainedArguments = 32;
        std::array<ArtifactCore::Array<ArtifactScriptValue>, maxDepth> overflow;
        std::size_t depth = 0;
    };

    ArtifactScriptCallArguments(std::size_t expected, Workspace& workspace)
        : useOverflow_(expected > inlineCapacity_) {
        if (!useOverflow_) return;

        if (expected <= Workspace::maxRetainedArguments &&
            workspace.depth < workspace.overflow.size()) {
            auto& reusable = workspace.overflow[workspace.depth];
            reusable.removeAll();
            if (reusable.capacity() < expected) reusable.reserve(expected);
            workspace_ = &workspace;
            overflow_ = &reusable;
            ++workspace.depth;
            useWorkspace_ = true;
        } else {
            fallbackOverflow_.emplace();
            fallbackOverflow_->reserve(expected);
            overflow_ = &*fallbackOverflow_;
        }
    }

    ~ArtifactScriptCallArguments() {
        if (useWorkspace_) {
            overflow_->removeAll();
            --workspace_->depth;
        }
    }

    ArtifactScriptCallArguments(const ArtifactScriptCallArguments&) = delete;
    ArtifactScriptCallArguments& operator=(const ArtifactScriptCallArguments&) = delete;

    void append(ArtifactScriptValue value) {
        if (useOverflow_) overflow_->append(std::move(value));
        else inlineValues_[size_] = std::move(value);
        ++size_;
    }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    const ArtifactScriptValue& operator[](std::size_t index) const {
        return useOverflow_ ? (*overflow_)[index] : inlineValues_[index];
    }
    std::span<const ArtifactScriptValue> span() const {
        return useOverflow_
            ? std::span<const ArtifactScriptValue>(overflow_->data(), overflow_->size())
            : std::span<const ArtifactScriptValue>(inlineValues_, size_);
    }
    std::span<ArtifactScriptValue> mutableSpan() {
        return useOverflow_
            ? std::span<ArtifactScriptValue>(overflow_->data(), overflow_->size())
            : std::span<ArtifactScriptValue>(inlineValues_, size_);
    }

private:
    static constexpr std::size_t inlineCapacity_ = 4;
    ArtifactScriptValue inlineValues_[inlineCapacity_]{};
    Workspace* workspace_ = nullptr;
    std::optional<ArtifactCore::Array<ArtifactScriptValue>> fallbackOverflow_;
    ArtifactCore::Array<ArtifactScriptValue>* overflow_ = nullptr;
    std::size_t size_ = 0;
    bool useOverflow_ = false;
    bool useWorkspace_ = false;
};

class ArtifactScriptLocals {
public:
    struct Workspace {
        static constexpr std::size_t maxDepth = kArtifactScriptMaxCallDepth;
        static constexpr std::size_t maxRetainedOverflowEntries = 32;
        std::array<ArtifactCore::Array<ArtifactScriptLocalBinding>, maxDepth> overflow;
        std::size_t depth = 0;
    };

    explicit ArtifactScriptLocals(Workspace& workspace) {
        if (workspace.depth < workspace.overflow.size()) {
            workspace_ = &workspace;
            workspaceIndex_ = workspace.depth++;
            overflowEntries_ = &workspace.overflow[workspaceIndex_];
            overflowEntries_->removeAll();
            useWorkspace_ = true;
        } else {
            fallbackOverflow_.emplace();
            overflowEntries_ = &*fallbackOverflow_;
        }
    }

    ~ArtifactScriptLocals() {
        if (workspace_) {
            workspace_->overflow[workspaceIndex_].removeAll();
            --workspace_->depth;
        }
    }

    ArtifactScriptLocals(const ArtifactScriptLocals&) = delete;
    ArtifactScriptLocals& operator=(const ArtifactScriptLocals&) = delete;

    ArtifactScriptLocalBinding* find(std::string_view name) {
        if (inlineSize_ == 0 && overflowEntries_->size() == 0) return nullptr;
        const auto nameHash = hashName(name);
        if (inlineSize_ != 0) {
            auto bucket = nameHash & (inlineNameIndex_.size() - 1);
            for (std::size_t probe = 0; probe < inlineNameIndex_.size(); ++probe) {
                const auto encodedIndex = inlineNameIndex_[bucket];
                if (encodedIndex == 0) break;
                auto& entry = inlineEntries_[encodedIndex - 1];
                if (entry.name == name) return &entry;
                bucket = (bucket + 1) & (inlineNameIndex_.size() - 1);
            }
        }
        for (std::size_t probe = 0; probe < overflowNameIndex_.size(); ++probe) {
            const auto encodedIndex = overflowNameIndex_[
                (nameHash + probe) & (overflowNameIndex_.size() - 1)];
            if (encodedIndex == 0) break;
            auto& entry = (*overflowEntries_)[encodedIndex - 1];
            if (entry.name == name) return &entry;
        }
        for (std::size_t i = indexedOverflowCount_; i < overflowEntries_->size(); ++i) {
            auto& entry = (*overflowEntries_)[i];
            if (entry.name == name) return &entry;
        }
        return nullptr;
    }

    const ArtifactScriptLocalBinding* find(std::string_view name) const {
        if (inlineSize_ == 0 && overflowEntries_->size() == 0) return nullptr;
        const auto nameHash = hashName(name);
        if (inlineSize_ != 0) {
            auto bucket = nameHash & (inlineNameIndex_.size() - 1);
            for (std::size_t probe = 0; probe < inlineNameIndex_.size(); ++probe) {
                const auto encodedIndex = inlineNameIndex_[bucket];
                if (encodedIndex == 0) break;
                const auto& entry = inlineEntries_[encodedIndex - 1];
                if (entry.name == name) return &entry;
                bucket = (bucket + 1) & (inlineNameIndex_.size() - 1);
            }
        }
        for (std::size_t probe = 0; probe < overflowNameIndex_.size(); ++probe) {
            const auto encodedIndex = overflowNameIndex_[
                (nameHash + probe) & (overflowNameIndex_.size() - 1)];
            if (encodedIndex == 0) break;
            const auto& entry = (*overflowEntries_)[encodedIndex - 1];
            if (entry.name == name) return &entry;
        }
        for (std::size_t i = indexedOverflowCount_; i < overflowEntries_->size(); ++i) {
            const auto& entry = (*overflowEntries_)[i];
            if (entry.name == name) return &entry;
        }
        return nullptr;
    }

    ArtifactScriptValue& operator[](std::string_view name) {
        if (auto* entry = find(name)) return entry->value;
        return append(name, {}).value;
    }

    ArtifactScriptLocalBinding& emplace(std::string_view name,
                                         const ArtifactScriptValue& value) {
        if (auto* entry = find(name)) return *entry;
        return append(name, value);
    }

private:
    // Benchmarks favor keeping common-sized local sets inline; larger frames
    // continue through the bounded evaluator-owned overflow workspace.
    static constexpr std::size_t inlineCapacity_ = 16;
    static constexpr std::size_t inlineNameIndexCapacity_ = 32;
    static constexpr std::size_t overflowNameIndexCapacity_ = 64;
    static_assert((inlineNameIndexCapacity_ & (inlineNameIndexCapacity_ - 1)) == 0);
    static_assert((overflowNameIndexCapacity_ & (overflowNameIndexCapacity_ - 1)) == 0);

    static std::size_t hashName(std::string_view name) {
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char character : name) {
            hash ^= character;
            hash *= 1099511628211ull;
        }
        return static_cast<std::size_t>(hash);
    }

    ArtifactScriptLocalBinding& append(std::string_view name,
                                       const ArtifactScriptValue& value) {
        ArtifactScriptLocalBinding binding{name, value};
        if (inlineSize_ < inlineCapacity_) {
            const auto index = inlineSize_++;
            inlineEntries_[index] = std::move(binding);
            auto bucket = hashName(name) & (inlineNameIndex_.size() - 1);
            while (inlineNameIndex_[bucket] != 0) {
                bucket = (bucket + 1) & (inlineNameIndex_.size() - 1);
            }
            inlineNameIndex_[bucket] = static_cast<std::uint8_t>(index + 1);
            return inlineEntries_[index];
        }
        if (useWorkspace_ &&
            overflowEntries_->size() == Workspace::maxRetainedOverflowEntries) {
            fallbackOverflow_.emplace();
            fallbackOverflow_->reserve(overflowEntries_->size() + 1);
            for (auto& entry : *overflowEntries_) {
                fallbackOverflow_->append(std::move(entry));
            }
            overflowEntries_->removeAll();
            overflowEntries_ = &*fallbackOverflow_;
            useWorkspace_ = false;
        } else if (useWorkspace_ && overflowEntries_->capacity() == 0) {
            overflowEntries_->reserve(Workspace::maxRetainedOverflowEntries);
        }
        const auto overflowIndex = overflowEntries_->size();
        overflowEntries_->append(std::move(binding));
        if (overflowIndex < Workspace::maxRetainedOverflowEntries) {
            auto bucket = hashName(name) & (overflowNameIndex_.size() - 1);
            while (overflowNameIndex_[bucket] != 0) {
                bucket = (bucket + 1) & (overflowNameIndex_.size() - 1);
            }
            overflowNameIndex_[bucket] = static_cast<std::uint8_t>(overflowIndex + 1);
            indexedOverflowCount_ = overflowIndex + 1;
        }
        return (*overflowEntries_)[overflowEntries_->size() - 1];
    }

    std::array<ArtifactScriptLocalBinding, inlineCapacity_> inlineEntries_{};
    std::array<std::uint8_t, inlineNameIndexCapacity_> inlineNameIndex_{};
    // Index the bounded reusable prefix; any transient locals beyond that
    // prefix stay on the fallback scan path instead of growing this table.
    std::array<std::uint8_t, overflowNameIndexCapacity_> overflowNameIndex_{};
    std::size_t inlineSize_ = 0;
    std::size_t indexedOverflowCount_ = 0;
    Workspace* workspace_ = nullptr;
    std::size_t workspaceIndex_ = 0;
    std::optional<ArtifactCore::Array<ArtifactScriptLocalBinding>> fallbackOverflow_;
    ArtifactCore::Array<ArtifactScriptLocalBinding>* overflowEntries_ = nullptr;
    bool useWorkspace_ = false;
};

struct ArtifactScriptFieldBinding {
    // Names point into the live AST; a field scope never outlives evaluation.
    const std::string* name = nullptr;
    ArtifactScriptValue value;
    // Read-only overlays alias the parent while its scope is suspended. A
    // write materializes the value locally before changing it.
    const ArtifactScriptValue* inheritedValue = nullptr;
    bool dirty = false;
};

constexpr std::size_t kArtifactScriptInlineOverlayCapacity = 8;

class ArtifactScriptFields {
public:
    explicit ArtifactScriptFields(ArtifactScriptSerializedFields& root)
        : root_(&root), inlineOverlay_(ownedInlineOverlay_.data()) {}
    explicit ArtifactScriptFields(
        ArtifactScriptFields& parent,
        ArtifactScriptFieldBinding* reusableOverlay = nullptr)
        : parent_(&parent),
          inlineOverlay_(reusableOverlay ? reusableOverlay : ownedInlineOverlay_.data()) {}

    const ArtifactScriptValue* find(const std::string& name) {
        if (root_) {
            if (auto it = root_->find(name); it != root_->end()) return &it->second;
            return nullptr;
        }
        if (auto* entry = findOverlay(name)) return bindingValue(*entry);
        if (const auto* value = parent_->find(name)) {
            return insertAlias(name, value).inheritedValue;
        }
        return nullptr;
    }

    const ArtifactScriptValue* findWithoutCaching(const std::string& name) const {
        if (root_) {
            if (auto it = root_->find(name); it != root_->end()) return &it->second;
            return nullptr;
        }
        if (const auto* entry = findOverlay(name)) return bindingValue(*entry);
        return parent_ ? parent_->findWithoutCaching(name) : nullptr;
    }

    ArtifactScriptValue* findForWrite(const std::string& name) {
        if (root_) {
            if (auto it = root_->find(name); it != root_->end()) return &it->second;
            return nullptr;
        }
        if (auto* entry = findOverlay(name)) {
            if (entry->inheritedValue) {
                entry->value = *entry->inheritedValue;
                entry->inheritedValue = nullptr;
            }
            entry->dirty = true;
            return &entry->value;
        }
        if (const auto* value = parent_->find(name)) {
            return &insertOverlay(name, *value, true).value;
        }
        return nullptr;
    }

    ArtifactScriptValue& operator[](const std::string& name) {
        if (root_) return (*root_)[name];
        if (auto* entry = findOverlay(name)) {
            if (entry->inheritedValue) {
                entry->value = *entry->inheritedValue;
                entry->inheritedValue = nullptr;
            }
            entry->dirty = true;
            return entry->value;
        }
        if (const auto* value = parent_->find(name)) {
            return insertOverlay(name, *value, true).value;
        }
        return insertOverlay(name, {}, true).value;
    }

    ArtifactScriptFieldBinding* prepareLoopBinding(const std::string& name) {
        if (root_) return nullptr;
        if (auto* entry = findOverlay(name)) return entry;
        if (overlaySize_ < kArtifactScriptInlineOverlayCapacity) {
            auto& entry = inlineOverlay_[overlaySize_++];
            entry.name = &name;
            entry.inheritedValue = nullptr;
            entry.dirty = false;
            return &entry;
        }
        overflowOverlay_.append(ArtifactScriptFieldBinding{&name, {}, nullptr, false});
        return &overflowOverlay_[overflowOverlay_.size() - 1];
    }

    void bindLoopValue(ArtifactScriptFieldBinding& entry,
                       const ArtifactScriptValue& value) {
        // Read iteration values through an alias; findForWrite materializes a
        // private copy if the script assigns to the loop variable.
        entry.inheritedValue = &value;
        entry.dirty = false;
    }

    void bindLoopValue(const std::string& name, const ArtifactScriptValue& value) {
        if (root_) {
            (*root_)[name] = value;
            return;
        }
        if (auto* entry = prepareLoopBinding(name)) bindLoopValue(*entry, value);
    }

    void commit(std::string_view excludedName) {
        for (std::size_t i = 0; i < overlaySize_; ++i) commitEntry(inlineOverlay_[i], excludedName);
        for (const auto& entry : overflowOverlay_) commitEntry(entry, excludedName);
    }

private:
    static const ArtifactScriptValue* bindingValue(
        const ArtifactScriptFieldBinding& entry) {
        return entry.inheritedValue ? entry.inheritedValue : &entry.value;
    }

    void commitEntry(const ArtifactScriptFieldBinding& entry,
                     std::string_view excludedName) const {
        if (entry.dirty && *entry.name != excludedName)
            (*parent_)[*entry.name] = entry.value;
    }

    ArtifactScriptFieldBinding* findOverlay(const std::string& name) {
        for (std::size_t i = 0; i < overlaySize_; ++i) {
            if (*inlineOverlay_[i].name == name) return &inlineOverlay_[i];
        }
        for (auto& entry : overflowOverlay_) if (*entry.name == name) return &entry;
        return nullptr;
    }

    const ArtifactScriptFieldBinding* findOverlay(const std::string& name) const {
        for (std::size_t i = 0; i < overlaySize_; ++i) {
            if (*inlineOverlay_[i].name == name) return &inlineOverlay_[i];
        }
        for (const auto& entry : overflowOverlay_) if (*entry.name == name) return &entry;
        return nullptr;
    }

    ArtifactScriptFieldBinding& insertOverlay(
        const std::string& name, const ArtifactScriptValue& value, bool dirty) {
        if (overlaySize_ < kArtifactScriptInlineOverlayCapacity) {
            auto& entry = inlineOverlay_[overlaySize_++];
            entry.name = &name;
            entry.value = value;
            entry.inheritedValue = nullptr;
            entry.dirty = dirty;
            return entry;
        }
        overflowOverlay_.append(ArtifactScriptFieldBinding{&name, value, nullptr, dirty});
        return overflowOverlay_[overflowOverlay_.size() - 1];
    }

    ArtifactScriptFieldBinding& insertAlias(
        const std::string& name, const ArtifactScriptValue* value) {
        auto& entry = insertOverlay(name, {}, false);
        entry.inheritedValue = value;
        return entry;
    }

    ArtifactScriptSerializedFields* root_ = nullptr;
    ArtifactScriptFields* parent_ = nullptr;
    std::array<ArtifactScriptFieldBinding, kArtifactScriptInlineOverlayCapacity>
        ownedInlineOverlay_{};
    ArtifactScriptFieldBinding* inlineOverlay_ = nullptr;
    std::size_t overlaySize_ = 0;
    ArtifactCore::Array<ArtifactScriptFieldBinding> overflowOverlay_;
};

struct ArtifactScriptForeachWorkspace {
    ArtifactCore::Array<ArtifactScriptValue> snapshot;
    std::array<ArtifactScriptFieldBinding, kArtifactScriptInlineOverlayCapacity> overlay{};
};

bool expressionMayMutateArray(const ArtifactScriptExpr* expression) {
    if (!expression) return false;
    switch (expression->kind) {
    case ArtifactScriptExpr::Kind::Literal:
    case ArtifactScriptExpr::Kind::Variable:
        return false;
    case ArtifactScriptExpr::Kind::Binary:
        return expressionMayMutateArray(expression->left.get()) ||
               expressionMayMutateArray(expression->right.get());
    case ArtifactScriptExpr::Kind::Unary:
        return expressionMayMutateArray(expression->operand.get());
    case ArtifactScriptExpr::Kind::Call:
    case ArtifactScriptExpr::Kind::New:
        // Host and user calls can mutate arrays through shared Array values.
        return true;
    case ArtifactScriptExpr::Kind::FieldAccess:
        return expressionMayMutateArray(expression->fieldObject.get());
    case ArtifactScriptExpr::Kind::Index:
        return expressionMayMutateArray(expression->indexTarget.get()) ||
               expressionMayMutateArray(expression->indexExpr.get());
    case ArtifactScriptExpr::Kind::ArrayLiteral:
        for (const auto& element : expression->arrayElements) {
            if (expressionMayMutateArray(element.get())) return true;
        }
        return false;
    case ArtifactScriptExpr::Kind::Ternary:
        return expressionMayMutateArray(expression->ternaryCondition.get()) ||
               expressionMayMutateArray(expression->ternaryThen.get()) ||
               expressionMayMutateArray(expression->ternaryElse.get());
    case ArtifactScriptExpr::Kind::Is:
        return expressionMayMutateArray(expression->isTarget.get());
    }
    return true;
}

bool statementMayMutateArray(const ArtifactScriptStmt* statement) {
    if (!statement) return false;
    switch (statement->kind) {
    case ArtifactScriptStmt::Kind::Expr:
        return expressionMayMutateArray(statement->expr.get());
    case ArtifactScriptStmt::Kind::Assign:
        return statement->assignIndex || statement->assignTargetExpression ||
               expressionMayMutateArray(statement->assignValue.get());
    case ArtifactScriptStmt::Kind::If:
        return expressionMayMutateArray(statement->ifCond.get()) ||
               statementMayMutateArray(statement->ifThen.get()) ||
               statementMayMutateArray(statement->ifElse.get());
    case ArtifactScriptStmt::Kind::Return:
        return expressionMayMutateArray(statement->expr.get());
    case ArtifactScriptStmt::Kind::Block:
        for (const auto& child : statement->blockStmts) {
            if (statementMayMutateArray(child.get())) return true;
        }
        return false;
    case ArtifactScriptStmt::Kind::Decl:
        return expressionMayMutateArray(statement->declInit.get());
    case ArtifactScriptStmt::Kind::While:
    case ArtifactScriptStmt::Kind::DoWhile:
        return expressionMayMutateArray(statement->whileCond.get()) ||
               statementMayMutateArray(statement->whileBody.get());
    case ArtifactScriptStmt::Kind::For:
        return statementMayMutateArray(statement->forInit.get()) ||
               expressionMayMutateArray(statement->forCond.get()) ||
               statementMayMutateArray(statement->forIncrement.get()) ||
               statementMayMutateArray(statement->forBody.get());
    case ArtifactScriptStmt::Kind::Break:
    case ArtifactScriptStmt::Kind::Continue:
        return false;
    case ArtifactScriptStmt::Kind::Foreach:
        return statementMayMutateArray(statement->foreachBody.get());
    }
    return true;
}

}  // namespace

class ArtifactScriptEvaluator::Impl {
public:
    static constexpr std::size_t kReusableForeachDepth = 8;
    static constexpr std::size_t kReusableForeachValueBudget = 1024;
    static constexpr std::size_t kMethodCallCacheCapacity = 32;
    static constexpr std::size_t kObjectMethodCallCacheWays = 3;
    static constexpr std::size_t kForeachMutationCacheCapacity = 64;
    static constexpr std::size_t kClassLookupIndexCapacity = 128;
    static constexpr std::size_t kClassLookupMaxClasses =
        kClassLookupIndexCapacity / 2;

    struct MethodCallCacheEntry {
        const ArtifactScriptExpr* callSite = nullptr;
        const ArtifactScriptDefinition* definition = nullptr;
        const ArtifactScriptClass* targetClass = nullptr;
        const ArtifactScriptMethod* method = nullptr;
        std::uint32_t generation = 0;
    };

    struct ClassLookupIndexEntry {
        std::uint32_t generation = 0;
        const ArtifactScriptClass* classDefinition = nullptr;
    };

    struct LifecycleHookCacheEntry {
        const ArtifactScriptDefinition* definition = nullptr;
        const ArtifactScriptMethod* method = nullptr;
        bool resolved = false;
    };

    struct ForeachMutationCacheEntry {
        const ArtifactScriptStmt* statement = nullptr;
        bool mayMutateArray = false;
    };

    struct ForeachSnapshotScope {
        Impl& owner;
        std::size_t depth;
        ArtifactScriptForeachWorkspace* workspace = nullptr;

        ~ForeachSnapshotScope() {
            if (workspace) {
                std::size_t retainedStringCapacity = 0;
                for (std::size_t i = 0; i < workspace->snapshot.size(); ++i) {
                    auto& value = workspace->snapshot[i];
                    if (auto* text = std::get_if<std::string>(&value)) {
                        if (text->capacity() <= 1024 - retainedStringCapacity) {
                            retainedStringCapacity += text->capacity();
                            text->clear();
                        } else {
                            value = std::monostate{};
                        }
                    } else {
                        value = std::monostate{};
                    }
                }
                for (auto& binding : workspace->overlay) {
                    binding.name = nullptr;
                    binding.inheritedValue = nullptr;
                    if (auto* text = std::get_if<std::string>(&binding.value)) {
                        if (text->capacity() <= 256) text->clear();
                        else binding.value = std::monostate{};
                    } else {
                        binding.value = std::monostate{};
                    }
                }
            }
            owner.foreachDepth_ = depth;
        }
    };

    std::string error_;
    ArtifactScriptValue returnValue_{};
    bool returned_ = false;
    bool breakRequested_ = false;
    bool continueRequested_ = false;
    const ArtifactScriptDefinition* activeDefinition_ = nullptr;
    ArtifactScriptObjectInstancePtr activeThis_;
    int callDepth_ = 0;
    ArtifactScriptLocals::Workspace localsWorkspace_;
    ArtifactScriptCallArguments::Workspace callArgumentWorkspace_;
    std::array<MethodCallCacheEntry, kMethodCallCacheCapacity> methodCallCache_{};
    std::array<MethodCallCacheEntry,
               kMethodCallCacheCapacity * kObjectMethodCallCacheWays>
        objectMethodCallCache_{};
    std::uint32_t methodCallCacheGeneration_ = 0;
    const ArtifactScriptDefinition* methodCallCacheDefinition_ = nullptr;
    bool methodCallCacheReusable_ = false;
    std::array<ClassLookupIndexEntry, kClassLookupIndexCapacity>
        classLookupIndex_{};
    std::array<LifecycleHookCacheEntry, 6> lifecycleHookCache_{};
    // The parsed AST stays immutable for the lifetime of a normal script
    // instance. Mutable-definition and direct-evaluator paths keep scanning.
    std::array<ForeachMutationCacheEntry, kForeachMutationCacheCapacity>
        foreachMutationCache_{};
    const ArtifactScriptDefinition* foreachMutationCacheDefinition_ = nullptr;
    bool foreachMutationCacheReusable_ = false;
    const ArtifactScriptDefinition* classLookupDefinition_ = nullptr;
    std::uint32_t classLookupGeneration_ = 0;
    bool classLookupIndexEnabled_ = false;
    bool classLookupIndexReusable_ = false;
    bool classLookupReuseDisabled_ = false;
    bool classLookupIndexPreparedForExecution_ = false;
    // Retain at most 1024 values across 8 nested snapshots. Deeper/larger
    // loops use a transient snapshot so scripts cannot grow this workspace
    // without bound. Reserving the outer array once keeps overlay pointers stable.
    ArtifactCore::Array<ArtifactScriptForeachWorkspace> foreachWorkspaces_;
    std::size_t foreachDepth_ = 0;
    ArtifactScriptValue evalExpr(const ArtifactScriptExpr*, ArtifactScriptFields&, const ArtifactScriptLocals&);
    ArtifactScriptValue evalBinary(ArtifactScriptBinaryOp, const ArtifactScriptValue&, const ArtifactScriptValue&);
    ArtifactScriptValue evalUnary(ArtifactScriptUnaryOp, const ArtifactScriptValue&);
    ArtifactScriptValue evalCall(const ArtifactScriptExpr*, ArtifactScriptFields&, const ArtifactScriptLocals&);
    bool execStmt(const ArtifactScriptStmt*, ArtifactScriptFields&, ArtifactScriptLocals& locals);
    ArtifactScriptValue callUserMethod(const ArtifactScriptMethod&, std::span<const ArtifactScriptValue>, ArtifactScriptFields&);
    ArtifactScriptValue callUserMethod(const ArtifactScriptMethod&, std::span<ArtifactScriptValue>, ArtifactScriptFields&);
    ArtifactScriptValue runUserMethodBody(const ArtifactScriptMethod&, ArtifactScriptLocals&, ArtifactScriptFields&);
    ArtifactScriptValue callInstanceMethod(const ArtifactScriptObjectInstancePtr&, const ArtifactScriptMethod&, std::span<ArtifactScriptValue>);
    const ArtifactScriptClass* findClass(std::string_view) const;
    const ArtifactScriptMethod* findMethodInChain(std::string_view, std::string_view) const;
    const ArtifactScriptMethod* findMethodAtCallSite(const ArtifactScriptExpr*);
    const ArtifactScriptMethod* findObjectMethodAtCallSite(
        const ArtifactScriptExpr*, std::string_view);
    void beginMethodCallCacheGeneration();
    void beginClassLookupGeneration(const ArtifactScriptDefinition&);
    void invalidateClassLookupIndex();
    void beginForeachMutationCache(const ArtifactScriptDefinition*, bool);
    bool foreachSnapshotRequired(const ArtifactScriptStmt*);
    bool isInstanceOf(const ArtifactScriptObjectInstance&, std::string_view) const;
};

ArtifactScriptEvaluator::ArtifactScriptEvaluator() : impl_(std::make_unique<Impl>()) {}

ArtifactScriptEvaluator::~ArtifactScriptEvaluator() noexcept = default;
ArtifactScriptEvaluator::ArtifactScriptEvaluator(ArtifactScriptEvaluator&&) noexcept = default;
ArtifactScriptEvaluator& ArtifactScriptEvaluator::operator=(ArtifactScriptEvaluator&&) noexcept = default;

void ArtifactScriptEvaluator::prepareClassLookup(
    const ArtifactScriptDefinition& definition) const {
    impl_->activeDefinition_ = &definition;
    if (impl_->classLookupReuseDisabled_ ||
        !impl_->classLookupIndexReusable_ ||
        impl_->classLookupDefinition_ != &definition) {
        impl_->beginClassLookupGeneration(definition);
        impl_->classLookupIndexReusable_ = !impl_->classLookupReuseDisabled_;
    }
    impl_->classLookupIndexPreparedForExecution_ = true;
}

const ArtifactScriptClass* ArtifactScriptEvaluator::findClassForDefinition(
    const ArtifactScriptDefinition& definition, std::string_view className) const {
    if (impl_->activeDefinition_ != &definition ||
        impl_->classLookupDefinition_ != &definition) {
        prepareClassLookup(definition);
    }
    return impl_->findClass(className);
}

const ArtifactScriptMethod* ArtifactScriptEvaluator::findLifecycleHook(
    const ArtifactScriptDefinition& definition, ArtifactScriptHook hook) const {
    const auto hookIndex = static_cast<std::size_t>(hook);
    if (hookIndex >= impl_->lifecycleHookCache_.size()) return nullptr;
    auto& cached = impl_->lifecycleHookCache_[hookIndex];
    if (!impl_->classLookupReuseDisabled_ && cached.resolved &&
        cached.definition == &definition) {
        return cached.method;
    }

    const auto& root = definition.rootClass;
    const ArtifactScriptMethod* result = nullptr;
    for (const auto& method : root.methods) {
        if (method.isLifecycleHook && method.hook == hook && method.body) {
            result = &method;
            break;
        }
    }

    std::string_view current = root.parentName;
    for (int depth = 1; !result && depth < 32 && !current.empty(); ++depth) {
        const ArtifactScriptClass* level =
            findClassForDefinition(definition, current);
        if (!level) break;
        for (const auto& method : level->methods) {
            if (method.isLifecycleHook && method.hook == hook && method.body) {
                result = &method;
                break;
            }
        }
        current = level->parentName;
    }

    if (!impl_->classLookupReuseDisabled_) {
        cached = {&definition, result, true};
    }
    return result;
}

void ArtifactScriptEvaluator::disableClassLookupReuse() const {
    impl_->classLookupReuseDisabled_ = true;
    impl_->invalidateClassLookupIndex();
    for (auto& entry : impl_->lifecycleHookCache_) entry = {};
    impl_->beginForeachMutationCache(nullptr, false);
}

void ArtifactScriptEvaluator::Impl::beginForeachMutationCache(
    const ArtifactScriptDefinition* definition, bool reusable) {
    if (!reusable || !definition) {
        foreachMutationCacheReusable_ = false;
        foreachMutationCacheDefinition_ = nullptr;
        foreachMutationCache_.fill({});
        return;
    }
    if (foreachMutationCacheDefinition_ != definition) {
        foreachMutationCache_.fill({});
        foreachMutationCacheDefinition_ = definition;
    }
    foreachMutationCacheReusable_ = true;
}

bool ArtifactScriptEvaluator::Impl::foreachSnapshotRequired(
    const ArtifactScriptStmt* statement) {
    if (!foreachMutationCacheReusable_ || !statement) {
        return statementMayMutateArray(statement);
    }
    const auto address = reinterpret_cast<std::uintptr_t>(statement);
    // Bounded direct mapping: collisions only trigger a conservative rescan.
    constexpr auto kCacheMask = kForeachMutationCacheCapacity - 1;
    const auto cacheIndex = static_cast<std::size_t>(address >> 4) & kCacheMask;
    auto& entry = foreachMutationCache_[cacheIndex];
    if (entry.statement == statement) return entry.mayMutateArray;

    const bool mayMutateArray = statementMayMutateArray(statement);
    entry = {statement, mayMutateArray};
    return mayMutateArray;
}

bool ArtifactScriptEvaluator::execute(
    const ArtifactScriptMethodBody& body,
    const std::vector<ArtifactScriptValue>& args,
    ArtifactScriptSerializedFields& fields) {
    impl_->beginMethodCallCacheGeneration();
    impl_->methodCallCacheDefinition_ = nullptr;
    impl_->methodCallCacheReusable_ = false;
    impl_->beginForeachMutationCache(nullptr, false);
    impl_->invalidateClassLookupIndex();
    impl_->error_.clear();
    impl_->returnValue_ = {};
    impl_->returned_ = false;
    impl_->breakRequested_ = false;
    impl_->continueRequested_ = false;
    ArtifactScriptFields fieldScope(fields);
    for (std::size_t i = 0; i < args.size() && i < body.parameters.size(); ++i) {
        fieldScope[body.parameters[i]] = args[i];
    }
    ArtifactScriptLocals locals(impl_->localsWorkspace_);
    for (auto& st : body.statements) {
        if (!impl_->execStmt(st.get(), fieldScope, locals)) return false;
        if (impl_->returned_) break;
        // break/continue at method top level ends the body gracefully.
        if (impl_->breakRequested_ || impl_->continueRequested_) {
            impl_->breakRequested_ = false;
            impl_->continueRequested_ = false;
            break;
        }
    }
    return impl_->error_.empty();
}

namespace {

bool tryResolveArrayIndex(const ArtifactScriptValue& value,
                          std::size_t arraySize, std::size_t& index) {
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        if (*integer < 0) return false;
        const auto unsignedIndex = static_cast<std::uint64_t>(*integer);
        if (unsignedIndex >= arraySize) return false;
        index = static_cast<std::size_t>(unsignedIndex);
        return true;
    }
    const auto* number = std::get_if<double>(&value);
    if (!number || !std::isfinite(*number) || *number < 0.0) return false;
    const double truncated = std::trunc(*number);
    if (truncated >= static_cast<double>(arraySize)) return false;
    const auto converted = static_cast<std::size_t>(truncated);
    if (converted >= arraySize) return false;
    index = converted;
    return true;
}

}  // namespace

ArtifactScriptValue ArtifactScriptEvaluator::Impl::evalExpr(
    const ArtifactScriptExpr* e, ArtifactScriptFields& fields,
    const ArtifactScriptLocals& locals) {
    if (!e) { error_ = "null expr"; return {}; }
    switch (e->kind) {
    case ArtifactScriptExpr::Kind::Literal: return e->literalValue;
    case ArtifactScriptExpr::Kind::ArrayLiteral: {
        // vec/color constructors reuse this node with a type tag; every other
        // array literal stays a generic ArtifactScriptArray.
        if (e->arrayLiteralIsVector) {
            // Numeric coercion shared with the binary operators: a component
            // written as an int or bool still becomes a float component.
            const auto numOf = [](const ArtifactScriptValue& v) -> double {
                if (std::holds_alternative<double>(v)) return std::get<double>(v);
                if (std::holds_alternative<std::int64_t>(v)) return static_cast<double>(std::get<std::int64_t>(v));
                if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1.0 : 0.0;
                return 0.0;
            };
            std::array<float, 4> components{};
            for (std::size_t i = 0; i < e->arrayElements.size() && i < 4; ++i) {
                const auto value = evalExpr(e->arrayElements[i].get(), fields, locals);
                if (!error_.empty()) return {};
                components[i] = static_cast<float>(numOf(value));
            }
            switch (e->arrayLiteralType) {
            case ArtifactScriptValueType::Vec2:
                return ArtifactScriptValue(ArtifactScriptVec2{components[0], components[1]});
            case ArtifactScriptValueType::Vec3:
                return ArtifactScriptValue(
                    ArtifactScriptVec3{components[0], components[1], components[2]});
            case ArtifactScriptValueType::Vec4:
                return ArtifactScriptValue(ArtifactScriptVec4{
                    components[0], components[1], components[2], components[3]});
            case ArtifactScriptValueType::Color:
                return ArtifactScriptValue(ArtifactScriptColor{
                    components[0], components[1], components[2], components[3]});
            default:
                return ArtifactScriptValue{};
            }
        }
        auto array = makeShared<ArtifactScriptArray>();
        array->values.reserve(e->arrayElements.size());
        for (const auto& element : e->arrayElements)
            array->values.push_back(evalExpr(element.get(), fields, locals));
        return array;
    }
    case ArtifactScriptExpr::Kind::Variable: {
        if (e->variableName == "this") {
            // Inside a method `this` is the script instance; at the top level it
            // stands for the owning layer, so field access on it is routed to
            // the host (see the FieldAccess branch below).
            if (!activeThis_) { return ArtifactScriptValue{}; }
            return activeThis_;
        }
        if (const auto* local = locals.find(e->variableName)) return local->value;
        if (auto* field = fields.find(e->variableName)) return *field;
        error_ = "undefined: " + e->variableName; return {};
    }
    case ArtifactScriptExpr::Kind::FieldAccess: {
        const bool isActiveThisAccess = activeThis_ && e->fieldObject &&
            e->fieldObject->kind == ArtifactScriptExpr::Kind::Variable &&
            e->fieldObject->variableName == "this";
        if (isActiveThisAccess) {
            const auto* field = fields.find(e->fieldName);
            if (!field) {
                error_ = "undefined field: " + e->fieldName;
                return {};
            }
            return *field;
        }
        const bool isThisAccess =
            !activeThis_ && e->fieldObject &&
            e->fieldObject->kind == ArtifactScriptExpr::Kind::Variable &&
            e->fieldObject->variableName == "this";
        const auto object = evalExpr(e->fieldObject.get(), fields, locals);
        if (!error_.empty()) return {};
        // At the top level `this.<prop>` reads an owning-layer property, which
        // keeps scripts free of explicit getProperty("self", ...) calls.
        if (isThisAccess) {
            ArtifactScriptValue result;
            if (!ArtifactScriptHost::global().callFunction(
                    "getSelfProperty",
                    {ArtifactScriptValue(e->fieldName)}, result)) {
                error_ = "this." + e->fieldName +
                         ": host property read is not wired";
                return {};
            }
            return result;
        }
        if (!std::holds_alternative<ArtifactScriptObjectInstancePtr>(object) ||
            !std::get<ArtifactScriptObjectInstancePtr>(object)) {
            error_ = "field access on non-object: " + e->fieldName; return {};
        }
        const auto& instance = std::get<ArtifactScriptObjectInstancePtr>(object);
        const auto it = instance->fields.find(e->fieldName);
        if (it == instance->fields.end()) { error_ = "undefined field: " + e->fieldName; return {}; }
        return it->second;
    }
    case ArtifactScriptExpr::Kind::New: {
        if (!activeDefinition_) { error_ = "new requires a script definition"; return {}; }
        const ArtifactScriptClass* cls = findClass(e->newClassName);
        if (!cls) { error_ = "unknown class: " + e->newClassName; return {}; }
        ArtifactScriptCallArguments args(e->newArgs.size(), callArgumentWorkspace_);
        for (const auto& arg : e->newArgs) {
            args.append(evalExpr(arg.get(), fields, locals));
            if (!error_.empty()) return {};
        }
        auto instance = makeShared<ArtifactScriptObjectInstance>();
        instance->className = cls->name;
        // Inherit default fields along the parent chain (base first).
        std::array<const ArtifactScriptClass*, 32> chain{};
        std::size_t chainSize = 0;
        std::vector<const ArtifactScriptClass*> deepChain;
        const ArtifactScriptClass* cursor = cls;
        while (cursor) {
            if (chainSize < chain.size()) chain[chainSize++] = cursor;
            else deepChain.push_back(cursor);
            cursor = !cursor->parentName.empty() ? findClass(cursor->parentName) : nullptr;
        }
        const auto inheritFields = [&](const ArtifactScriptClass& classDefinition) {
            for (const auto& field : classDefinition.fields) {
                instance->fields.try_emplace(field.name, field.defaultValue);
            }
        };
        for (auto chainIt = deepChain.rbegin(); chainIt != deepChain.rend(); ++chainIt) {
            inheritFields(**chainIt);
        }
        for (std::size_t i = chainSize; i > 0; --i) {
            inheritFields(*chain[i - 1]);
        }
        if (const ArtifactScriptMethod* ctor = findMethodInChain(cls->name, "OnConstruct")) {
            const auto result = callInstanceMethod(instance, *ctor, args.mutableSpan());
            if (!error_.empty()) return {};
            (void)result;
        } else if (!args.empty()) {
            error_ = "no constructor: " + cls->name; return {};
        }
        return instance;
    }
    case ArtifactScriptExpr::Kind::Is: {
        const auto target = evalExpr(e->isTarget.get(), fields, locals);
        if (!error_.empty()) return {};
        if (!std::holds_alternative<ArtifactScriptObjectInstancePtr>(target) ||
            !std::get<ArtifactScriptObjectInstancePtr>(target)) {
            return false;
        }
        return isInstanceOf(*std::get<ArtifactScriptObjectInstancePtr>(target), e->isClassName);
    }
    case ArtifactScriptExpr::Kind::Binary: {
        // `??` is both lazy and null-specific: false, zero, and empty strings
        // are valid left-hand values and must not evaluate the fallback.
        if (e->binaryOp == ArtifactScriptBinaryOp::Coalesce) {
            auto left = evalExpr(e->left.get(), fields, locals);
            if (!error_.empty()) return {};
            if (!std::holds_alternative<std::monostate>(left)) return left;
            return evalExpr(e->right.get(), fields, locals);
        }
        // Short-circuit evaluation for && and ||: the right operand must not
        // be evaluated when the left already decides the result.
        if (e->binaryOp == ArtifactScriptBinaryOp::And || e->binaryOp == ArtifactScriptBinaryOp::Or) {
            const auto left = evalExpr(e->left.get(), fields, locals);
            if (!error_.empty()) return {};
            auto truthy = [](const ArtifactScriptValue& v) {
                if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
                if (std::holds_alternative<double>(v)) return std::get<double>(v) != 0.0;
                if (std::holds_alternative<std::int64_t>(v)) return std::get<std::int64_t>(v) != 0;
                return false;
            };
            const bool leftIsTruthy = truthy(left);
            if (e->binaryOp == ArtifactScriptBinaryOp::And && !leftIsTruthy)
                return false;
            if (e->binaryOp == ArtifactScriptBinaryOp::Or && leftIsTruthy)
                return true;
            const auto right = evalExpr(e->right.get(), fields, locals);
            if (!error_.empty()) return {};
            return truthy(right);
        }
        const bool isStringAddition = e->binaryOp == ArtifactScriptBinaryOp::Add;
        const bool isStringComparison =
            e->binaryOp == ArtifactScriptBinaryOp::Eq ||
            e->binaryOp == ArtifactScriptBinaryOp::Neq ||
            e->binaryOp == ArtifactScriptBinaryOp::Lt ||
            e->binaryOp == ArtifactScriptBinaryOp::Gt ||
            e->binaryOp == ArtifactScriptBinaryOp::Le ||
            e->binaryOp == ArtifactScriptBinaryOp::Ge;
        const auto valueReference = [&](auto&& self,
                                        const ArtifactScriptExpr* operand,
                                        std::size_t depth)
            -> const ArtifactScriptValue* {
            if (!operand || depth > 64) return nullptr;
            if (operand->kind == ArtifactScriptExpr::Kind::Literal) {
                return &operand->literalValue;
            }
            if (operand->kind == ArtifactScriptExpr::Kind::Variable) {
                if (operand->variableName == "this") return nullptr;
                if (const auto* local = locals.find(operand->variableName))
                    return &local->value;
                return fields.findWithoutCaching(operand->variableName);
            }
            if (operand->kind == ArtifactScriptExpr::Kind::Index) {
                const auto* arrayValue = self(
                    self, operand->indexTarget.get(), depth + 1);
                const auto* array = arrayValue
                    ? std::get_if<ArtifactScriptArrayPtr>(arrayValue)
                    : nullptr;
                const auto* indexValue = self(
                    self, operand->indexExpr.get(), depth + 1);
                if (!array || !*array || !indexValue ||
                    (!std::holds_alternative<double>(*indexValue) &&
                     !std::holds_alternative<std::int64_t>(*indexValue))) {
                    return nullptr;
                }
                std::size_t index = 0;
                return tryResolveArrayIndex(
                           *indexValue, (*array)->values.size(), index)
                    ? &(*array)->values[index] : nullptr;
            }
            if (operand->kind == ArtifactScriptExpr::Kind::FieldAccess) {
                if (!operand->fieldObject ||
                    (operand->fieldObject->kind ==
                         ArtifactScriptExpr::Kind::Variable &&
                     operand->fieldObject->variableName == "this")) {
                    return nullptr;
                }
                const auto* objectValue = self(
                    self, operand->fieldObject.get(), depth + 1);
                const auto* object = objectValue
                    ? std::get_if<ArtifactScriptObjectInstancePtr>(objectValue)
                    : nullptr;
                if (!object || !*object) return nullptr;
                const auto field = (*object)->fields.find(operand->fieldName);
                return field != (*object)->fields.end()
                    ? &field->second
                    : nullptr;
            }
            return nullptr;
        };
        const auto isReferenceExpression = [&](auto&& self,
                                               const ArtifactScriptExpr* operand,
                                               std::size_t depth)
            -> bool {
            if (!operand || depth > 64) return false;
            switch (operand->kind) {
            case ArtifactScriptExpr::Kind::Literal:
                return true;
            case ArtifactScriptExpr::Kind::Variable:
                return operand->variableName != "this";
            case ArtifactScriptExpr::Kind::Index:
                return self(self, operand->indexTarget.get(), depth + 1) &&
                       self(self, operand->indexExpr.get(), depth + 1);
            case ArtifactScriptExpr::Kind::FieldAccess:
                return operand->fieldObject &&
                       !(operand->fieldObject->kind ==
                             ArtifactScriptExpr::Kind::Variable &&
                         operand->fieldObject->variableName == "this") &&
                       self(self, operand->fieldObject.get(), depth + 1);
            default:
                return false;
            }
        };
        const auto isSimpleValue = [](const ArtifactScriptExpr* operand) {
            return operand &&
                   (operand->kind == ArtifactScriptExpr::Kind::Literal ||
                    operand->kind == ArtifactScriptExpr::Kind::Variable);
        };
        const bool simpleOperands =
            isSimpleValue(e->left.get()) && isSimpleValue(e->right.get());
        if (e->binaryOp != ArtifactScriptBinaryOp::And &&
            e->binaryOp != ArtifactScriptBinaryOp::Or &&
            e->binaryOp != ArtifactScriptBinaryOp::Coalesce &&
            (simpleOperands ||
             (isReferenceExpression(isReferenceExpression, e->left.get(), 0) &&
              isReferenceExpression(isReferenceExpression, e->right.get(), 0)))) {
            const auto* left = valueReference(valueReference, e->left.get(), 0);
            const auto* right = valueReference(valueReference, e->right.get(), 0);
            if (left && right) {
                const auto* leftString = std::get_if<std::string>(left);
                const auto* rightString = std::get_if<std::string>(right);
                if (e->binaryOp == ArtifactScriptBinaryOp::Add &&
                    leftString && rightString) {
                    std::string result;
                    if (leftString->size() <=
                        result.max_size() - rightString->size()) {
                        result.reserve(leftString->size() + rightString->size());
                    }
                    result.append(*leftString);
                    result.append(*rightString);
                    return result;
                }
                return evalBinary(e->binaryOp, *left, *right);
            }
        }
        if (isStringAddition || isStringComparison) {
            const auto stringOperand = [&](const ArtifactScriptExpr* operand)
                -> const std::string* {
                if (!operand) return nullptr;
                if (operand->kind == ArtifactScriptExpr::Kind::Literal) {
                    return std::get_if<std::string>(&operand->literalValue);
                }
                if (operand->kind != ArtifactScriptExpr::Kind::Variable ||
                    operand->variableName == "this") {
                    return nullptr;
                }
                if (const auto* local = locals.find(operand->variableName)) {
                    return std::get_if<std::string>(&local->value);
                }
                if (const auto* field =
                        fields.findWithoutCaching(operand->variableName)) {
                    return std::get_if<std::string>(field);
                }
                return nullptr;
            };
            const auto* left = stringOperand(e->left.get());
            const auto* right = stringOperand(e->right.get());
            if (left && right) {
                if (isStringAddition) {
                    std::string result;
                    if (left->size() <= result.max_size() - right->size()) {
                        result.reserve(left->size() + right->size());
                    }
                    result.append(*left);
                    result.append(*right);
                    return result;
                }
                switch (e->binaryOp) {
                case ArtifactScriptBinaryOp::Eq: return *left == *right;
                case ArtifactScriptBinaryOp::Neq: return *left != *right;
                case ArtifactScriptBinaryOp::Lt: return *left < *right;
                case ArtifactScriptBinaryOp::Gt: return *left > *right;
                case ArtifactScriptBinaryOp::Le: return *left <= *right;
                case ArtifactScriptBinaryOp::Ge: return *left >= *right;
                default: break;
                }
            }
            const auto isAdditionNode = [](const ArtifactScriptExpr* node) {
                return node && node->kind == ArtifactScriptExpr::Kind::Binary &&
                       node->binaryOp == ArtifactScriptBinaryOp::Add;
            };
            if (isStringAddition &&
                (isAdditionNode(e->left.get()) || isAdditionNode(e->right.get()))) {
                constexpr std::size_t maxFusionDepth = 64;
                std::size_t totalSize = 0;
                const auto measureStringTree = [&](auto&& self,
                                                   const ArtifactScriptExpr* node,
                                                   std::size_t depth) -> bool {
                    if (!node || depth > maxFusionDepth) return false;
                    if (isAdditionNode(node)) {
                        return self(self, node->left.get(), depth + 1) &&
                               self(self, node->right.get(), depth + 1);
                    }
                    const auto* text = stringOperand(node);
                    if (!text || text->size() >
                                     static_cast<std::size_t>(-1) - totalSize)
                        return false;
                    totalSize += text->size();
                    return true;
                };
                if (measureStringTree(measureStringTree, e, 0)) {
                    std::string result;
                    if (totalSize > result.max_size()) {
                        return evalBinary(
                            e->binaryOp,
                            evalExpr(e->left.get(), fields, locals),
                            evalExpr(e->right.get(), fields, locals));
                    }
                    result.reserve(totalSize);
                    const auto appendStringTree = [&](auto&& self,
                                                      const ArtifactScriptExpr* node,
                                                      std::size_t depth) -> bool {
                        if (!node || depth > maxFusionDepth) return false;
                        if (isAdditionNode(node)) {
                            return self(self, node->left.get(), depth + 1) &&
                                   self(self, node->right.get(), depth + 1);
                        }
                        const auto* text = stringOperand(node);
                        if (!text) return false;
                        result.append(*text);
                        return true;
                    };
                    if (appendStringTree(appendStringTree, e, 0)) return result;
                }
            }
        }
        return evalBinary(e->binaryOp, evalExpr(e->left.get(), fields, locals), evalExpr(e->right.get(), fields, locals));
    }
    case ArtifactScriptExpr::Kind::Ternary: {
        const auto condition = evalExpr(e->ternaryCondition.get(), fields, locals);
        if (!error_.empty()) return {};
        auto truthy = [](const ArtifactScriptValue& v) {
            if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
            if (std::holds_alternative<double>(v)) return std::get<double>(v) != 0.0;
            if (std::holds_alternative<std::int64_t>(v)) return std::get<std::int64_t>(v) != 0;
            return false;
        };
        return truthy(condition)
            ? evalExpr(e->ternaryThen.get(), fields, locals)
            : evalExpr(e->ternaryElse.get(), fields, locals);
    }
    case ArtifactScriptExpr::Kind::Unary:
        return evalUnary(e->unaryOp, evalExpr(e->operand.get(), fields, locals));
    case ArtifactScriptExpr::Kind::Call:
        return evalCall(e, fields, locals);
    case ArtifactScriptExpr::Kind::Index: {
        const auto arrayValue = evalExpr(e->indexTarget.get(), fields, locals);
        if (!error_.empty()) return {};
        const auto indexValue = evalExpr(e->indexExpr.get(), fields, locals);
        if (!error_.empty()) return {};
        if (!std::holds_alternative<ArtifactScriptArrayPtr>(arrayValue) ||
            (!std::holds_alternative<double>(indexValue) && !std::holds_alternative<std::int64_t>(indexValue))) {
            error_ = "invalid array access"; return {};
        }
        const auto& array = std::get<ArtifactScriptArrayPtr>(arrayValue);
        std::size_t index = 0;
        if (!array || !tryResolveArrayIndex(indexValue, array->values.size(), index)) {
            error_ = "array index out of range"; return {};
        }
        return array->values[index];
    }
    default: return {};
    }
}

ArtifactScriptValue ArtifactScriptEvaluator::Impl::evalBinary(
    ArtifactScriptBinaryOp op, const ArtifactScriptValue& l, const ArtifactScriptValue& r) {
    const auto compareIntegerAndDouble = [](std::int64_t integer,
                                            double number) -> int {
        if (std::isnan(number)) return 2;
        constexpr double int64UpperBound = 9223372036854775808.0;
        if (number >= int64UpperBound) return -1;
        if (number < -int64UpperBound) return 1;

        const auto truncated = static_cast<std::int64_t>(number);
        if (integer < truncated) return -1;
        if (integer > truncated) return 1;
        const double truncatedAsDouble = static_cast<double>(truncated);
        if (number == truncatedAsDouble) return 0;
        return number > truncatedAsDouble ? -1 : 1;
    };
    if (op == ArtifactScriptBinaryOp::Eq || op == ArtifactScriptBinaryOp::Neq) {
        const bool leftIsNull = std::holds_alternative<std::monostate>(l);
        const bool rightIsNull = std::holds_alternative<std::monostate>(r);
        bool equal = leftIsNull && rightIsNull;
        if (!leftIsNull && !rightIsNull) {
            const auto* leftInteger = std::get_if<std::int64_t>(&l);
            const auto* rightInteger = std::get_if<std::int64_t>(&r);
            const auto* leftNumber = std::get_if<double>(&l);
            const auto* rightNumber = std::get_if<double>(&r);
            if (leftInteger && rightInteger) {
                equal = *leftInteger == *rightInteger;
            } else if ((leftInteger || leftNumber) &&
                       (rightInteger || rightNumber)) {
                if (leftInteger && rightNumber) {
                    equal = compareIntegerAndDouble(*leftInteger, *rightNumber) == 0;
                } else if (leftNumber && rightInteger) {
                    equal = compareIntegerAndDouble(*rightInteger, *leftNumber) == 0;
                } else {
                    equal = *leftNumber == *rightNumber;
                }
            } else if (const auto* leftBoolean = std::get_if<bool>(&l)) {
                const auto* rightBoolean = std::get_if<bool>(&r);
                equal = rightBoolean && *leftBoolean == *rightBoolean;
            } else if (const auto* leftString = std::get_if<std::string>(&l)) {
                const auto* rightString = std::get_if<std::string>(&r);
                equal = rightString && *leftString == *rightString;
            } else if (const auto* leftReference = std::get_if<ArtifactScriptRef>(&l)) {
                const auto* rightReference = std::get_if<ArtifactScriptRef>(&r);
                equal = rightReference && leftReference->id == rightReference->id;
            } else if (const auto* leftArray =
                           std::get_if<ArtifactScriptArrayPtr>(&l)) {
                const auto* rightArray = std::get_if<ArtifactScriptArrayPtr>(&r);
                equal = rightArray && *leftArray == *rightArray;
            } else if (const auto* leftObject =
                           std::get_if<ArtifactScriptObjectInstancePtr>(&l)) {
                const auto* rightObject =
                    std::get_if<ArtifactScriptObjectInstancePtr>(&r);
                equal = rightObject && *leftObject == *rightObject;
            } else if (const auto* leftVec2 = std::get_if<ArtifactScriptVec2>(&l)) {
                const auto* rightVec2 = std::get_if<ArtifactScriptVec2>(&r);
                equal = rightVec2 && leftVec2->x == rightVec2->x &&
                        leftVec2->y == rightVec2->y;
            } else if (const auto* leftVec3 = std::get_if<ArtifactScriptVec3>(&l)) {
                const auto* rightVec3 = std::get_if<ArtifactScriptVec3>(&r);
                equal = rightVec3 && leftVec3->x == rightVec3->x &&
                        leftVec3->y == rightVec3->y &&
                        leftVec3->z == rightVec3->z;
            } else if (const auto* leftVec4 = std::get_if<ArtifactScriptVec4>(&l)) {
                const auto* rightVec4 = std::get_if<ArtifactScriptVec4>(&r);
                equal = rightVec4 && leftVec4->x == rightVec4->x &&
                        leftVec4->y == rightVec4->y &&
                        leftVec4->z == rightVec4->z &&
                        leftVec4->w == rightVec4->w;
            } else if (const auto* leftColor = std::get_if<ArtifactScriptColor>(&l)) {
                const auto* rightColor = std::get_if<ArtifactScriptColor>(&r);
                equal = rightColor && leftColor->r == rightColor->r &&
                        leftColor->g == rightColor->g &&
                        leftColor->b == rightColor->b &&
                        leftColor->a == rightColor->a;
            }
        }
        return op == ArtifactScriptBinaryOp::Eq ? equal : !equal;
    }
    const auto* leftInteger = std::get_if<std::int64_t>(&l);
    const auto* rightInteger = std::get_if<std::int64_t>(&r);
    const auto* leftNumber = std::get_if<double>(&l);
    const auto* rightNumber = std::get_if<double>(&r);
    if ((leftInteger && rightNumber) || (leftNumber && rightInteger)) {
        const int comparison = leftInteger
            ? compareIntegerAndDouble(*leftInteger, *rightNumber)
            : -compareIntegerAndDouble(*rightInteger, *leftNumber);
        switch (op) {
        case ArtifactScriptBinaryOp::Lt: return comparison == -1;
        case ArtifactScriptBinaryOp::Gt: return comparison == 1;
        case ArtifactScriptBinaryOp::Le: return comparison == -1 || comparison == 0;
        case ArtifactScriptBinaryOp::Ge: return comparison == 1 || comparison == 0;
        default: break;
        }
    }
    if (leftInteger && rightInteger) {
        const std::int64_t left = *leftInteger;
        const std::int64_t right = *rightInteger;
        constexpr std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
        constexpr std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
        switch (op) {
        case ArtifactScriptBinaryOp::Add:
            if ((right > 0 && left > maximum - right) ||
                (right < 0 && left < minimum - right)) {
                error_ = "integer overflow";
                return {};
            }
            return left + right;
        case ArtifactScriptBinaryOp::Sub:
            if ((right < 0 && left > maximum + right) ||
                (right > 0 && left < minimum + right)) {
                error_ = "integer overflow";
                return {};
            }
            return left - right;
        case ArtifactScriptBinaryOp::Mul:
            if (left != 0 && right != 0 &&
                ((left == -1 && right == minimum) ||
                 (right == -1 && left == minimum) ||
                 (left > 0 && right > 0 && left > maximum / right) ||
                 (left > 0 && right < 0 && right < minimum / left) ||
                 (left < 0 && right > 0 && left < minimum / right) ||
                 (left < 0 && right < 0 && left < maximum / right))) {
                error_ = "integer overflow";
                return {};
            }
            return left * right;
        case ArtifactScriptBinaryOp::Div:
            if (right == 0) {
                error_ = "div0";
                return {};
            }
            if (left == minimum && right == -1) {
                error_ = "integer overflow";
                return {};
            }
            return left / right;
        case ArtifactScriptBinaryOp::Mod:
            if (right == 0) {
                error_ = "mod0";
                return {};
            }
            if (left == minimum && right == -1) {
                error_ = "integer overflow";
                return {};
            }
            return left % right;
        case ArtifactScriptBinaryOp::Lt: return left < right;
        case ArtifactScriptBinaryOp::Gt: return left > right;
        case ArtifactScriptBinaryOp::Le: return left <= right;
        case ArtifactScriptBinaryOp::Ge: return left >= right;
        default: break;
        }
    }
    auto d = [](const ArtifactScriptValue& v) -> double {
        if (std::holds_alternative<double>(v)) return std::get<double>(v);
        if (std::holds_alternative<std::int64_t>(v)) return (double)std::get<std::int64_t>(v);
        return 0.0;
    };
    auto b = [&](const ArtifactScriptValue& v) { return std::holds_alternative<bool>(v) ? std::get<bool>(v) : d(v) != 0.0; };
    if (op == ArtifactScriptBinaryOp::Add &&
        (std::holds_alternative<std::string>(l) || std::holds_alternative<std::string>(r))) {
        std::string result;
        const auto appendValue = [&](const ArtifactScriptValue& value) {
            if (const auto* text = std::get_if<std::string>(&value)) {
                result.append(*text);
            } else if (const auto* boolean = std::get_if<bool>(&value)) {
                result.append(*boolean ? "true" : "false");
            } else if (const auto* integer = std::get_if<std::int64_t>(&value)) {
                const auto converted = std::to_string(*integer);
                result.append(converted);
            } else if (const auto* number = std::get_if<double>(&value)) {
                std::ostringstream stream;
                stream << *number;
                result.append(stream.str());
            }
        };
        appendValue(l);
        appendValue(r);
        return result;
    }
    if (std::holds_alternative<std::string>(l) && std::holds_alternative<std::string>(r)) {
        const auto& ls = std::get<std::string>(l);
        const auto& rs = std::get<std::string>(r);
        switch (op) {
        case ArtifactScriptBinaryOp::Eq:  return ls == rs;
        case ArtifactScriptBinaryOp::Neq: return ls != rs;
        case ArtifactScriptBinaryOp::Lt:  return ls < rs;
        case ArtifactScriptBinaryOp::Gt:  return ls > rs;
        case ArtifactScriptBinaryOp::Le:  return ls <= rs;
        case ArtifactScriptBinaryOp::Ge:  return ls >= rs;
        default: break;
        }
    }
    double ld = d(l), rd = d(r);
    switch (op) {
    case ArtifactScriptBinaryOp::Add: return ld + rd;
    case ArtifactScriptBinaryOp::Sub: return ld - rd;
    case ArtifactScriptBinaryOp::Mul: return ld * rd;
    case ArtifactScriptBinaryOp::Div: if (rd == 0) { error_ = "div0"; return {}; } return ld / rd;
    case ArtifactScriptBinaryOp::Mod: if (rd == 0) { error_ = "mod0"; return {}; } return std::fmod(ld, rd);
    case ArtifactScriptBinaryOp::Eq:  return ld == rd;
    case ArtifactScriptBinaryOp::Neq: return ld != rd;
    case ArtifactScriptBinaryOp::Lt:  return ld < rd;
    case ArtifactScriptBinaryOp::Gt:  return ld > rd;
    case ArtifactScriptBinaryOp::Le:  return ld <= rd;
    case ArtifactScriptBinaryOp::Ge:  return ld >= rd;
    case ArtifactScriptBinaryOp::And: return b(l) && b(r);
    case ArtifactScriptBinaryOp::Or:  return b(l) || b(r);
    case ArtifactScriptBinaryOp::Coalesce:
        return std::holds_alternative<std::monostate>(l) ? r : l;
    }
    return {};
}

ArtifactScriptValue ArtifactScriptEvaluator::Impl::evalUnary(
    ArtifactScriptUnaryOp op, const ArtifactScriptValue& v) {
    auto d = [](const ArtifactScriptValue& x) -> double {
        if (std::holds_alternative<double>(x)) return std::get<double>(x);
        if (std::holds_alternative<std::int64_t>(x)) return (double)std::get<std::int64_t>(x);
        return 0.0;
    };
    switch (op) {
    case ArtifactScriptUnaryOp::Neg:
        if (const auto* integer = std::get_if<std::int64_t>(&v)) {
            if (*integer == std::numeric_limits<std::int64_t>::min()) {
                error_ = "integer overflow";
                return {};
            }
            return -*integer;
        }
        return -d(v);
    case ArtifactScriptUnaryOp::Not: return !(std::holds_alternative<bool>(v) ? std::get<bool>(v) : d(v) != 0.0);
    }
    return {};
}

ArtifactScriptValue ArtifactScriptEvaluator::Impl::evalCall(
    const ArtifactScriptExpr* e, ArtifactScriptFields& fields,
    const ArtifactScriptLocals& locals) {
    const bool isContainsCall = e->callName == "contains";
    const bool isIndexOfCall = e->callName == "indexOf";
    const bool isStartsWithCall = e->callName == "startsWith";
    const bool isEndsWithCall = e->callName == "endsWith";
    const bool isStringSearchCall = isContainsCall || isIndexOfCall ||
        isStartsWithCall || isEndsWithCall;
    if (isStringSearchCall && !e->callTarget &&
        e->callArgs.size() == 2) {
        const auto stringReference = [&](const ArtifactScriptExpr* argument)
            -> const std::string* {
            if (!argument) return nullptr;
            if (argument->kind == ArtifactScriptExpr::Kind::Literal) {
                return std::get_if<std::string>(&argument->literalValue);
            }
            if (argument->kind == ArtifactScriptExpr::Kind::Variable &&
                argument->variableName != "this") {
                if (const auto* local = locals.find(argument->variableName)) {
                    return std::get_if<std::string>(&local->value);
                }
                if (const auto* field = fields.findWithoutCaching(argument->variableName)) {
                    return std::get_if<std::string>(field);
                }
            }
            if (argument->kind == ArtifactScriptExpr::Kind::FieldAccess &&
                activeThis_ && argument->fieldObject &&
                argument->fieldObject->kind == ArtifactScriptExpr::Kind::Variable &&
                argument->fieldObject->variableName == "this") {
                if (const auto* field = fields.findWithoutCaching(argument->fieldName)) {
                    return std::get_if<std::string>(field);
                }
            }
            return nullptr;
        };
        const auto* source = stringReference(e->callArgs[0].get());
        const auto* substring = stringReference(e->callArgs[1].get());
        if (source && substring) {
            if (isStartsWithCall) return source->starts_with(*substring);
            if (isEndsWithCall) return source->ends_with(*substring);
            const auto position = source->find(*substring);
            if (isContainsCall) return position != std::string::npos;
            if (position == std::string::npos) return std::int64_t{-1};
            if (position > static_cast<std::size_t>(
                               std::numeric_limits<std::int64_t>::max())) {
                error_ = "string index exceeds int64 range";
                return {};
            }
            return static_cast<std::int64_t>(position);
        }
    }

    ArtifactScriptCallArguments args(e->callArgs.size(), callArgumentWorkspace_);
    for (auto& a : e->callArgs) args.append(evalExpr(a.get(), fields, locals));
    if (!error_.empty()) return {};
    const auto argumentValues = args.span();
    if (e->callTarget) {
        const auto target = evalExpr(e->callTarget.get(), fields, locals);
        if (!error_.empty()) return {};
        // (1) script instance method wins over host so user overrides keep
        // working once class bodies exist; (2) host-registered ObjectRef or
        // instance methods; (3) diagnostic error.
        if (std::holds_alternative<ArtifactScriptObjectInstancePtr>(target) &&
            std::get<ArtifactScriptObjectInstancePtr>(target)) {
            const auto& instance = std::get<ArtifactScriptObjectInstancePtr>(target);
            if (activeDefinition_) {
                if (const ArtifactScriptMethod* method =
                        findObjectMethodAtCallSite(e, instance->className)) {
                    return callInstanceMethod(instance, *method, args.mutableSpan());
                }
            }
            ArtifactScriptValue hostResult;
            const std::string* classLabel = &instance->className;
            if (classLabel->empty()) {
                static const std::string genericObjectClassLabel{"Object"};
                classLabel = &genericObjectClassLabel;
            }
            ArtifactScriptHost& host = ArtifactScriptHost::global();
            if (host.callMethodView(
                    *classLabel, e->callName, target, argumentValues, hostResult)) {
                const std::string_view hostError = host.lastErrorView();
                if (!hostError.empty()) {
                    error_ = "host: ";
                    error_.append(hostError);
                    return {};
                }
                return hostResult;
            }
            error_ = "unknown method: " + e->callName; return {};
        }
        // Host objects arrive as ObjectRef (e.g. getLayer() handles).
        if (std::holds_alternative<ArtifactScriptRef>(target)) {
            ArtifactScriptValue hostResult;
            ArtifactScriptHost& host = ArtifactScriptHost::global();
            if (host.callMethodView(
                    "ObjectRef", e->callName, target, argumentValues, hostResult)) {
                const std::string_view hostError = host.lastErrorView();
                if (!hostError.empty()) {
                    error_ = "host: ";
                    error_.append(hostError);
                    return {};
                }
                return hostResult;
            }
            error_ = "unknown method: " + e->callName; return {};
        }
        error_ = "method call on non-object: " + e->callName; return {};
    }
    auto num = [](const ArtifactScriptValue& v) -> double {
        if (std::holds_alternative<double>(v)) return std::get<double>(v);
        if (std::holds_alternative<std::int64_t>(v)) return (double)std::get<std::int64_t>(v);
        return 0.0;
    };
    if (e->callName == "array" && argumentValues.empty())
        return makeShared<ArtifactScriptArray>();
    if (e->callName == "print" || e->callName == "log") {
        auto toString = [](const ArtifactScriptValue& v) -> std::string {
            if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
            if (std::holds_alternative<bool>(v)) return std::get<bool>(v) ? "true" : "false";
            if (std::holds_alternative<std::int64_t>(v)) return std::to_string(std::get<std::int64_t>(v));
            if (std::holds_alternative<double>(v)) {
                const double value = std::get<double>(v);
                std::ostringstream stream;
                stream << value;
                return stream.str();
            }
            return {};
        };
        std::string line;
        for (std::size_t i = 0; i < argumentValues.size(); ++i) {
            if (i > 0) line += " ";
            line += toString(argumentValues[i]);
        }
        ArtifactScriptHost::global().appendLog(std::move(line));
        return {};
    }
    if (e->callName == "size" && argumentValues.size() == 1 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(argumentValues[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(argumentValues[0]);
        return static_cast<std::int64_t>(array ? array->values.size() : 0);
    }
    if (e->callName == "split") {
        if (argumentValues.size() != 2 ||
            !std::holds_alternative<std::string>(argumentValues[0]) ||
            !std::holds_alternative<std::string>(argumentValues[1])) {
            error_ = "split expects a source string and a delimiter string";
            return {};
        }
        const auto& source = std::get<std::string>(argumentValues[0]);
        const auto& delimiter = std::get<std::string>(argumentValues[1]);
        if (delimiter.empty()) {
            error_ = "split delimiter must not be empty";
            return {};
        }

        std::size_t elementCount = 1;
        for (std::size_t position = source.find(delimiter);
             position != std::string::npos;
             position = source.find(delimiter, position + delimiter.size())) {
            ++elementCount;
        }

        auto array = makeShared<ArtifactScriptArray>();
        array->values.reserve(elementCount);
        std::size_t elementStart = 0;
        for (std::size_t position = source.find(delimiter);
             position != std::string::npos;
             position = source.find(delimiter, elementStart)) {
            array->values.push_back(source.substr(elementStart, position - elementStart));
            elementStart = position + delimiter.size();
        }
        array->values.push_back(source.substr(elementStart));
        return array;
    }
    if (e->callName == "join") {
        if (argumentValues.size() != 2 ||
            !std::holds_alternative<ArtifactScriptArrayPtr>(argumentValues[0]) ||
            !std::holds_alternative<std::string>(argumentValues[1])) {
            error_ = "join expects an array and a delimiter string";
            return {};
        }
        const auto& array = std::get<ArtifactScriptArrayPtr>(argumentValues[0]);
        if (!array) {
            error_ = "join on null array";
            return {};
        }
        const auto& delimiter = std::get<std::string>(argumentValues[1]);
        std::string result;
        const std::size_t maximumSize = result.max_size();
        std::size_t outputSize = 0;
        if (array->values.size() > 1) {
            const std::size_t delimiterCount = array->values.size() - 1;
            if (delimiter.size() > maximumSize / delimiterCount) {
                error_ = "join result is too large";
                return {};
            }
            outputSize = delimiter.size() * delimiterCount;
        }
        for (const auto& value : array->values) {
            const auto* part = std::get_if<std::string>(&value);
            if (!part) {
                error_ = "join expects an array of strings";
                return {};
            }
            if (part->size() > maximumSize - outputSize) {
                error_ = "join result is too large";
                return {};
            }
            outputSize += part->size();
        }
        result.reserve(outputSize);
        for (std::size_t i = 0; i < array->values.size(); ++i) {
            if (i != 0) result.append(delimiter);
            result.append(std::get<std::string>(array->values[i]));
        }
        return result;
    }
    if (e->callName == "replace") {
        if (argumentValues.size() != 3 ||
            !std::holds_alternative<std::string>(argumentValues[0]) ||
            !std::holds_alternative<std::string>(argumentValues[1]) ||
            !std::holds_alternative<std::string>(argumentValues[2])) {
            error_ = "replace expects source, search, and replacement strings";
            return {};
        }
        const auto& source = std::get<std::string>(argumentValues[0]);
        const auto& search = std::get<std::string>(argumentValues[1]);
        const auto& replacement = std::get<std::string>(argumentValues[2]);
        if (search.empty()) {
            error_ = "replace search string must not be empty";
            return {};
        }

        std::size_t matchCount = 0;
        for (std::size_t position = source.find(search);
             position != std::string::npos;
             position = source.find(search, position + search.size())) {
            ++matchCount;
        }

        std::string result;
        const std::size_t maximumSize = result.max_size();
        if (source.size() > maximumSize) {
            error_ = "replace result is too large";
            return {};
        }
        std::size_t resultSize = source.size();
        if (replacement.size() > search.size()) {
            const std::size_t growthPerMatch = replacement.size() - search.size();
            if (growthPerMatch != 0 &&
                matchCount > (maximumSize - source.size()) / growthPerMatch) {
                error_ = "replace result is too large";
                return {};
            }
            resultSize += matchCount * growthPerMatch;
        } else {
            resultSize -= matchCount * (search.size() - replacement.size());
        }
        result.reserve(resultSize);

        std::size_t sourceStart = 0;
        for (std::size_t position = source.find(search);
             position != std::string::npos;
             position = source.find(search, sourceStart)) {
            result.append(source, sourceStart, position - sourceStart);
            result.append(replacement);
            sourceStart = position + search.size();
        }
        result.append(source, sourceStart, std::string::npos);
        return result;
    }
    if (e->callName == "push" && argumentValues.size() == 2 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(argumentValues[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(argumentValues[0]);
        if (!array) { error_ = "push on null array"; return {}; }
        auto mutableArguments = args.mutableSpan();
        array->values.push_back(std::move(mutableArguments[1]));
        return static_cast<std::int64_t>(array->values.size());
    }
    if (e->callName == "clear" && argumentValues.size() == 1 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(argumentValues[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(argumentValues[0]);
        if (!array) { error_ = "clear on null array"; return {}; }
        array->values.clear();
        return static_cast<std::int64_t>(0);
    }
    if (e->callName == "empty" && argumentValues.size() == 1 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(argumentValues[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(argumentValues[0]);
        return !array || array->values.empty();
    }
    if (e->callName == "pop" && argumentValues.size() == 1 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(argumentValues[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(argumentValues[0]);
        if (!array || array->values.empty()) { error_ = "pop from empty array"; return {}; }
        auto value = std::move(array->values.back());
        array->values.pop_back();
        return value;
    }
    if (e->callName == "contains" && argumentValues.size() == 2 &&
        std::holds_alternative<std::string>(argumentValues[0]) &&
        std::holds_alternative<std::string>(argumentValues[1])) {
        const auto& source = std::get<std::string>(argumentValues[0]);
        const auto& substring = std::get<std::string>(argumentValues[1]);
        return source.find(substring) != std::string::npos;
    }
    if (e->callName == "indexOf" && argumentValues.size() == 2 &&
        std::holds_alternative<std::string>(argumentValues[0]) &&
        std::holds_alternative<std::string>(argumentValues[1])) {
        const auto& source = std::get<std::string>(argumentValues[0]);
        const auto& substring = std::get<std::string>(argumentValues[1]);
        const auto position = source.find(substring);
        if (position == std::string::npos) return std::int64_t{-1};
        if (position > static_cast<std::size_t>(
                           std::numeric_limits<std::int64_t>::max())) {
            error_ = "string index exceeds int64 range";
            return {};
        }
        return static_cast<std::int64_t>(position);
    }
    if (e->callName == "startsWith" || e->callName == "endsWith") {
        if (argumentValues.size() != 2 ||
            !std::holds_alternative<std::string>(argumentValues[0]) ||
            !std::holds_alternative<std::string>(argumentValues[1])) {
            error_ = e->callName == "startsWith"
                ? "startsWith expects a source string and a prefix string"
                : "endsWith expects a source string and a suffix string";
            return {};
        }
        const auto& source = std::get<std::string>(argumentValues[0]);
        const auto& substring = std::get<std::string>(argumentValues[1]);
        return e->callName == "startsWith"
            ? source.starts_with(substring)
            : source.ends_with(substring);
    }
    if ((e->callName == "contains" || e->callName == "indexOf") && argumentValues.size() == 2 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(argumentValues[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(argumentValues[0]);
        if (!array) return e->callName == "contains" ? ArtifactScriptValue(false) : ArtifactScriptValue(std::int64_t(-1));
        for (std::size_t i = 0; i < array->values.size(); ++i) {
            const auto& item = array->values[i];
            const bool equal = std::get<bool>(evalBinary(
                ArtifactScriptBinaryOp::Eq, item, argumentValues[1]));
            if (equal) return e->callName == "contains" ? ArtifactScriptValue(true) : ArtifactScriptValue(static_cast<std::int64_t>(i));
        }
        return e->callName == "contains" ? ArtifactScriptValue(false) : ArtifactScriptValue(std::int64_t(-1));
    }
    if (e->callName == "abs" && !argumentValues.empty()) return std::abs(num(argumentValues[0]));
    if (e->callName == "min" && argumentValues.size() >= 2) return std::min(num(argumentValues[0]), num(argumentValues[1]));
    if (e->callName == "max" && argumentValues.size() >= 2) return std::max(num(argumentValues[0]), num(argumentValues[1]));
    if (e->callName == "clamp" && argumentValues.size() >= 3) return std::clamp(num(argumentValues[0]), num(argumentValues[1]), num(argumentValues[2]));
    if (e->callName == "lerp" && argumentValues.size() >= 3) {
        double a = num(argumentValues[0]), b = num(argumentValues[1]), t = num(argumentValues[2]); return a + (b - a) * t;
    }
    if (e->callName == "sin" && !argumentValues.empty()) return std::sin(num(argumentValues[0]));
    if (e->callName == "cos" && !argumentValues.empty()) return std::cos(num(argumentValues[0]));
    if (activeDefinition_) {
        if (const ArtifactScriptMethod* method =
                findMethodAtCallSite(e)) {
            return callUserMethod(*method, args.mutableSpan(), fields);
        }
    }
    ArtifactScriptValue hostResult;
    ArtifactScriptHost& host = ArtifactScriptHost::global();
    if (host.callFunctionView(e->callName, argumentValues, hostResult)) {
        // Host callbacks may report failures via setLastError; surface
        // them through the evaluator's diagnostic path.
        const std::string_view hostError = host.lastErrorView();
        if (!hostError.empty()) {
            error_ = "host: ";
            error_.append(hostError);
        }
        return hostResult;
    }
    error_ = "unknown function: " + e->callName; return {};
}

ArtifactScriptValue ArtifactScriptEvaluator::Impl::callUserMethod(
    const ArtifactScriptMethod& resolvedMethod,
    std::span<const ArtifactScriptValue> args,
    ArtifactScriptFields& fields) {
    constexpr int kMaxCallDepth = static_cast<int>(kArtifactScriptMaxCallDepth);
    if (callDepth_ >= kMaxCallDepth) {
        error_ = "script call depth limit";
        return {};
    }
    if (!resolvedMethod.body) return {};
    ArtifactScriptLocals locals(localsWorkspace_);
    for (std::size_t i = 0; i < args.size() && i < resolvedMethod.parameters.size(); ++i)
        locals[resolvedMethod.parameters[i]] = args[i];
    return runUserMethodBody(resolvedMethod, locals, fields);
}

ArtifactScriptValue ArtifactScriptEvaluator::Impl::callUserMethod(
    const ArtifactScriptMethod& resolvedMethod,
    std::span<ArtifactScriptValue> args,
    ArtifactScriptFields& fields) {
    constexpr int kMaxCallDepth = static_cast<int>(kArtifactScriptMaxCallDepth);
    if (callDepth_ >= kMaxCallDepth) {
        error_ = "script call depth limit";
        return {};
    }
    if (!resolvedMethod.body) return {};
    ArtifactScriptLocals locals(localsWorkspace_);
    for (std::size_t i = 0; i < args.size() && i < resolvedMethod.parameters.size(); ++i)
        locals[resolvedMethod.parameters[i]] = std::move(args[i]);
    return runUserMethodBody(resolvedMethod, locals, fields);
}

ArtifactScriptValue ArtifactScriptEvaluator::Impl::runUserMethodBody(
    const ArtifactScriptMethod& resolvedMethod,
    ArtifactScriptLocals& locals,
    ArtifactScriptFields& fields) {
    const ArtifactScriptMethod* method = &resolvedMethod;
    const auto previousReturn = returnValue_;
    const bool previousReturned = returned_;
    ++callDepth_;
    returnValue_ = {};
    returned_ = false;
    for (const auto& statement : method->body->statements) {
        if (!execStmt(statement.get(), fields, locals) || returned_) break;
    }
    if (!error_.empty() && method->line != 0) {
        error_ = "line " + std::to_string(method->line) + ":" +
                 std::to_string(method->column == 0 ? 1 : method->column) +
                 ": " + error_;
    }
    auto result = std::move(returnValue_);
    --callDepth_;
    returnValue_ = previousReturn;
    returned_ = previousReturned;
    return result;
}

bool ArtifactScriptEvaluator::Impl::execStmt(
    const ArtifactScriptStmt* s, ArtifactScriptFields& fields,
    ArtifactScriptLocals& locals) {
    if (!s) return true;
    switch (s->kind) {
    case ArtifactScriptStmt::Kind::Expr:
        evalExpr(s->expr.get(), fields, locals); return error_.empty();
    case ArtifactScriptStmt::Kind::Assign: {
        const std::string& op = s->assignOp;
        const bool isCoalescingAssignment = op == "??=";
        ArtifactScriptValue v = isCoalescingAssignment
            ? ArtifactScriptValue{}
            : evalExpr(s->assignValue.get(), fields, locals);
        bool valueEvaluated = !isCoalescingAssignment;
        if (!isCoalescingAssignment && !error_.empty()) return false;
        const auto evaluateRight = [&]() {
            if (!valueEvaluated) {
                v = evalExpr(s->assignValue.get(), fields, locals);
                valueEvaluated = true;
            }
            return error_.empty();
        };
        // Preserve existing assignment evaluation order. `??=` is the sole
        // exception: resolve/read the target first so its right side can stay
        // unevaluated when the target already has a value.
        if (!isCoalescingAssignment && !evaluateRight()) return false;
        // Compound assignment folds the current value with the right side.
        const bool isSimpleAssignment = op.empty() || op == "=";
        auto applyCompound = [&](const ArtifactScriptValue& current) -> ArtifactScriptValue {
            if (op == "+=") return evalBinary(ArtifactScriptBinaryOp::Add, current, v);
            if (op == "-=") return evalBinary(ArtifactScriptBinaryOp::Sub, current, v);
            if (op == "*=") return evalBinary(ArtifactScriptBinaryOp::Mul, current, v);
            if (op == "/=") return evalBinary(ArtifactScriptBinaryOp::Div, current, v);
            if (op == "%=") return evalBinary(ArtifactScriptBinaryOp::Mod, current, v);
            error_ = "unsupported assign op: " + op;
            return {};
        };
        const auto applyNumericCompound = [&](ArtifactScriptValue& current) {
            auto* target = std::get_if<double>(&current);
            if (!target) return false;
            double right = 0.0;
            if (const auto* number = std::get_if<double>(&v)) {
                right = *number;
            } else if (const auto* integer = std::get_if<std::int64_t>(&v)) {
                right = static_cast<double>(*integer);
            } else {
                return false;
            }
            if (op == "+=") {
                *target += right;
            } else if (op == "-=") {
                *target -= right;
            } else if (op == "*=") {
                *target *= right;
            } else if (op == "/=") {
                if (right == 0.0) {
                    error_ = "div0";
                    return true;
                }
                *target /= right;
            } else if (op == "%=") {
                if (right == 0.0) {
                    error_ = "div0";
                    return true;
                }
                *target = std::fmod(*target, right);
            } else {
                return false;
            }
            return true;
        };
        const auto appendStringCompound = [&](ArtifactScriptValue& current) {
            if (op != "+=" || !std::holds_alternative<std::string>(current)) {
                return false;
            }
            auto& target = std::get<std::string>(current);
            if (const auto* text = std::get_if<std::string>(&v)) {
                target.append(*text);
            } else if (const auto* boolean = std::get_if<bool>(&v)) {
                target.append(*boolean ? "true" : "false");
            } else if (const auto* integer = std::get_if<std::int64_t>(&v)) {
                target.append(std::to_string(*integer));
            } else if (const auto* number = std::get_if<double>(&v)) {
                std::ostringstream stream;
                stream << *number;
                target.append(stream.str());
            }
            return true;
        };
        if (s->assignTargetExpression) {
            ArtifactScriptExpr* targetExpression =
                s->assignTargetExpression.get();
            ArtifactScriptValue* targetValue = nullptr;
            ArtifactScriptValue hostPropertyValue;
            bool writesHostProperty = false;

            if (targetExpression->kind == ArtifactScriptExpr::Kind::Index) {
                const auto arrayValue = evalExpr(
                    targetExpression->indexTarget.get(), fields, locals);
                if (!error_.empty()) return false;
                const auto indexValue = evalExpr(
                    targetExpression->indexExpr.get(), fields, locals);
                if (!error_.empty()) return false;
                if (!std::holds_alternative<ArtifactScriptArrayPtr>(arrayValue) ||
                    (!std::holds_alternative<double>(indexValue) &&
                     !std::holds_alternative<std::int64_t>(indexValue))) {
                    error_ = "invalid array assignment";
                    return false;
                }
                const auto& array = std::get<ArtifactScriptArrayPtr>(arrayValue);
                std::size_t index = 0;
                if (!array || !tryResolveArrayIndex(
                                  indexValue, array->values.size(), index)) {
                    error_ = "array index out of range";
                    return false;
                }
                targetValue = &array->values[index];
            } else if (targetExpression->kind ==
                       ArtifactScriptExpr::Kind::FieldAccess) {
                const bool isThis = targetExpression->fieldObject &&
                    targetExpression->fieldObject->kind ==
                        ArtifactScriptExpr::Kind::Variable &&
                    targetExpression->fieldObject->variableName == "this";
                if (isThis && !activeThis_) {
                    writesHostProperty = true;
                    if (!isSimpleAssignment &&
                        !ArtifactScriptHost::global().callFunction(
                            "getSelfProperty",
                            {ArtifactScriptValue(targetExpression->fieldName)},
                            hostPropertyValue)) {
                        error_ = "this." + targetExpression->fieldName +
                                 ": host property read is not wired";
                        return false;
                    }
                    targetValue = &hostPropertyValue;
                } else if (isThis) {
                    targetValue = fields.findForWrite(
                        targetExpression->fieldName);
                    if (!targetValue) {
                        fields[targetExpression->fieldName] = {};
                        targetValue = fields.findForWrite(
                            targetExpression->fieldName);
                    }
                } else {
                    const auto objectValue = evalExpr(
                        targetExpression->fieldObject.get(), fields, locals);
                    if (!error_.empty()) return false;
                    if (!std::holds_alternative<ArtifactScriptObjectInstancePtr>(
                            objectValue) ||
                        !std::get<ArtifactScriptObjectInstancePtr>(objectValue)) {
                        error_ = "field assign on non-object: " +
                                 targetExpression->fieldName;
                        return false;
                    }
                    const auto& object = std::get<ArtifactScriptObjectInstancePtr>(
                        objectValue);
                    targetValue = &object->fields[targetExpression->fieldName];
                }
            } else {
                error_ = "invalid assignment target";
                return false;
            }

            if (!targetValue) {
                error_ = "invalid assignment target";
                return false;
            }
            if (isCoalescingAssignment) {
                if (!std::holds_alternative<std::monostate>(*targetValue)) {
                    return true;
                }
                if (!evaluateRight()) return false;
                *targetValue = std::move(v);
            } else if (isSimpleAssignment) {
                *targetValue = std::move(v);
            } else if (!applyNumericCompound(*targetValue) &&
                       !appendStringCompound(*targetValue)) {
                *targetValue = applyCompound(*targetValue);
            }
            if (!error_.empty()) return false;
            if (writesHostProperty) {
                ArtifactScriptValue written;
                if (!ArtifactScriptHost::global().callFunction(
                        "setSelfProperty",
                        {ArtifactScriptValue(targetExpression->fieldName),
                         *targetValue},
                        written)) {
                    error_ = "this." + targetExpression->fieldName +
                             ": host property write is not wired";
                    return false;
                }
            }
            return error_.empty();
        }
        if (s->assignIndex) {
            auto* target = locals.find(s->assignTarget);
            if (!target) {
                auto* field = fields.find(s->assignTarget);
                if (!field) { error_ = "undefined array: " + s->assignTarget; return false; }
                target = &locals.emplace(s->assignTarget, *field);
            }
            auto indexValue = evalExpr(s->assignIndex.get(), fields, locals);
            if (!error_.empty()) return false;
            if (!std::holds_alternative<ArtifactScriptArrayPtr>(target->value) ||
                (!std::holds_alternative<double>(indexValue) && !std::holds_alternative<std::int64_t>(indexValue))) {
                error_ = "invalid array assignment"; return false;
            }
            const auto& array = std::get<ArtifactScriptArrayPtr>(target->value);
            std::size_t index = 0;
            if (!array || !tryResolveArrayIndex(indexValue, array->values.size(), index)) {
                error_ = "array index out of range"; return false;
            }
            auto& targetValue = array->values[index];
            if (isCoalescingAssignment) {
                if (!std::holds_alternative<std::monostate>(targetValue)) {
                    return true;
                }
                if (!evaluateRight()) return false;
                targetValue = std::move(v);
            } else if (isSimpleAssignment) {
                targetValue = std::move(v);
            } else if (!applyNumericCompound(targetValue) &&
                       !appendStringCompound(targetValue)) {
                targetValue = applyCompound(targetValue);
            }
            return error_.empty();
        }
        if (isCoalescingAssignment) {
            if (auto* local = locals.find(s->assignTarget)) {
                if (!std::holds_alternative<std::monostate>(local->value)) {
                    return true;
                }
                if (!evaluateRight()) return false;
                local->value = std::move(v);
                return true;
            }
            if (const auto* field = fields.find(s->assignTarget);
                field && !std::holds_alternative<std::monostate>(*field)) {
                return true;
            }
            if (!evaluateRight()) return false;
            fields[s->assignTarget] = std::move(v);
            return error_.empty();
        }
        if (auto* lit = locals.find(s->assignTarget)) {
            if (isSimpleAssignment) {
                lit->value = std::move(v);
            } else if (!applyNumericCompound(lit->value) &&
                       !appendStringCompound(lit->value)) {
                lit->value = applyCompound(lit->value);
            }
            return error_.empty();
        }
        if (auto* field = fields.findForWrite(s->assignTarget)) {
            if (isSimpleAssignment) {
                *field = std::move(v);
            } else if (!applyNumericCompound(*field) &&
                       !appendStringCompound(*field)) {
                *field = applyCompound(*field);
            }
        } else {
            fields[s->assignTarget] = isSimpleAssignment
                ? std::move(v) : applyCompound({});
        }
        return error_.empty();
    }
    case ArtifactScriptStmt::Kind::Decl: {
        ArtifactScriptValue init;
        if (s->declInit) init = evalExpr(s->declInit.get(), fields, locals);
        else if (s->declType == ArtifactScriptValueType::Array)
            init = makeShared<ArtifactScriptArray>();
        if (!error_.empty()) return false;
        if (s->fieldAssign) {
            auto resolveObject = [&](const ArtifactScriptValue& slot) -> const ArtifactScriptObjectInstancePtr* {
                if (std::holds_alternative<ArtifactScriptObjectInstancePtr>(slot)) {
                    return &std::get<ArtifactScriptObjectInstancePtr>(slot);
                }
                return nullptr;
            };
            const ArtifactScriptObjectInstancePtr* target = nullptr;
            if (s->declName == "this") {
                // At the top level `this` is the owning layer rather than a
                // script instance, so the assignment goes to a host property
                // write. Inside a method it stays an ordinary field assign.
                if (!activeThis_) {
                    ArtifactScriptValue written;
                    if (!ArtifactScriptHost::global().callFunction(
                            "setSelfProperty",
                            {ArtifactScriptValue(s->assignField), init},
                            written)) {
                        error_ = "this." + s->assignField +
                                 ": host property write is not wired";
                        return false;
                    }
                    return error_.empty();
                }
                fields[s->assignField] = init;
                return error_.empty();
            } else if (auto* lit = locals.find(s->declName)) {
                target = resolveObject(lit->value);
            } else if (const auto* field = fields.find(s->declName)) {
                target = resolveObject(*field);
            }
            if (!target || !*target) { error_ = "field assign on non-object: " + s->declName; return false; }
            (*target)->fields[s->assignField] = init;
            return error_.empty();
        }
        if (s->declInit || s->declType == ArtifactScriptValueType::Array) {
            locals[s->declName] = std::move(init);
        } else {
            locals[s->declName] = ArtifactScriptValue{};
        }
        return error_.empty(); }
    case ArtifactScriptStmt::Kind::While: {
        int iter = 0;
        while (iter < 10000) {
            auto cond = evalExpr(s->whileCond.get(), fields, locals);
            bool t = std::holds_alternative<bool>(cond) ? std::get<bool>(cond)
                : (std::holds_alternative<double>(cond) ? std::get<double>(cond) != 0.0
                   : std::holds_alternative<std::int64_t>(cond) ? std::get<std::int64_t>(cond) != 0 : false);
            if (!t) break;
            if (!execStmt(s->whileBody.get(), fields, locals)) return false;
            if (returned_) break;
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; }
            ++iter; }
        if (iter >= 10000) { error_ = "loop limit"; return false; } return true; }
    case ArtifactScriptStmt::Kind::DoWhile: {
        int iter = 0;
        while (iter < 10000) {
            if (!execStmt(s->whileBody.get(), fields, locals)) return false;
            if (returned_) return true;
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) continueRequested_ = false;
            const auto cond = evalExpr(s->whileCond.get(), fields, locals);
            if (!error_.empty()) return false;
            const bool truthy = std::holds_alternative<bool>(cond)
                ? std::get<bool>(cond)
                : std::holds_alternative<double>(cond)
                    ? std::get<double>(cond) != 0.0
                    : std::holds_alternative<std::int64_t>(cond)
                        ? std::get<std::int64_t>(cond) != 0
                        : false;
            if (!truthy) break;
            ++iter;
        }
        if (iter >= 10000) { error_ = "loop limit"; return false; }
        return true;
    }
    case ArtifactScriptStmt::Kind::For: {
        if (s->forInit && !execStmt(s->forInit.get(), fields, locals)) return false;
        int iter = 0;
        while (iter < 10000) {
            bool truthy = true;
            if (s->forCond) {
                const auto cond = evalExpr(s->forCond.get(), fields, locals);
                truthy = std::holds_alternative<bool>(cond)
                    ? std::get<bool>(cond)
                    : std::holds_alternative<double>(cond)
                        ? std::get<double>(cond) != 0.0
                        : std::holds_alternative<std::int64_t>(cond)
                            ? std::get<std::int64_t>(cond) != 0
                            : false;
            }
            if (!truthy) break;
            if (s->forBody && !execStmt(s->forBody.get(), fields, locals)) return false;
            if (returned_) break;
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; }
            if (s->forIncrement && !execStmt(s->forIncrement.get(), fields, locals)) return false;
            ++iter;
        }
        if (iter >= 10000) { error_ = "loop limit"; return false; }
        return true; }
    case ArtifactScriptStmt::Kind::Foreach: {
        ArtifactScriptValue evaluatedCollection;
        const ArtifactScriptValue* collection = nullptr;
        if (s->foreachCollectionExpr) {
            evaluatedCollection = evalExpr(s->foreachCollectionExpr.get(), fields, locals);
            if (!error_.empty()) return false;
            collection = &evaluatedCollection;
        } else {
            if (const auto* field = fields.find(s->foreachCollectionName)) {
                collection = field;
            } else if (const auto* lit = locals.find(s->foreachCollectionName)) {
                collection = &lit->value;
            }
        }
        if (!collection) { error_ = "undefined: " + s->foreachCollectionName; return false; }
        if (!std::holds_alternative<ArtifactScriptArrayPtr>(*collection)) {
            error_ = "foreach requires an array"; return false;
        }
        // Keep the original array alive if the loop body replaces its source
        // field. Read-only bodies can then iterate it without copying values.
        const auto array = std::get<ArtifactScriptArrayPtr>(*collection);
        // Snapshot the element list because the loop body may mutate the same
        // array. Reuse bounded evaluator-owned storage in the common case;
        // large/deep loops keep the previous transient-copy behavior.
        const auto snapshotDepth = foreachDepth_;
        ForeachSnapshotScope snapshotScope{*this, snapshotDepth};
        foreachDepth_ = snapshotDepth + 1;
        std::optional<std::vector<ArtifactScriptValue>> transientElements;
        std::span<const ArtifactScriptValue> elements;
        ArtifactScriptForeachWorkspace* workspace = nullptr;
        if (array && !array->values.empty() && snapshotDepth < kReusableForeachDepth) {
            if (foreachWorkspaces_.capacity() < kReusableForeachDepth) {
                foreachWorkspaces_.reserve(kReusableForeachDepth);
            }
            while (foreachWorkspaces_.size() <= snapshotDepth) {
                foreachWorkspaces_.append(ArtifactScriptForeachWorkspace{});
            }
            workspace = &foreachWorkspaces_[snapshotDepth];
            snapshotScope.workspace = workspace;
        }
        const bool snapshotRequired = foreachSnapshotRequired(s->foreachBody.get());
        if (array && !array->values.empty() && !snapshotRequired) {
            elements = std::span<const ArtifactScriptValue>(
                array->values.data(), array->values.size());
        }
        bool reuseSnapshot = false;
        if (snapshotRequired && workspace) {
            std::size_t retainedCapacityElsewhere = 0;
            for (std::size_t i = 0; i < foreachWorkspaces_.size(); ++i) {
                if (i != snapshotDepth) {
                    retainedCapacityElsewhere += foreachWorkspaces_[i].snapshot.capacity();
                }
            }
            const auto availableSnapshotCapacity = retainedCapacityElsewhere <
                    kReusableForeachValueBudget
                ? kReusableForeachValueBudget - retainedCapacityElsewhere : 0;
            reuseSnapshot = array->values.size() <= availableSnapshotCapacity;
        }
        if (snapshotRequired && reuseSnapshot) {
            auto& snapshot = workspace->snapshot;
            if (snapshot.capacity() < array->values.size()) {
                snapshot.reserve(array->values.size());
            }
            snapshot.resize(array->values.size());
            for (std::size_t i = 0; i < array->values.size(); ++i) {
                snapshot[i] = array->values[i];
            }
            elements = std::span<const ArtifactScriptValue>(
                snapshot.data(), array->values.size());
        } else if (snapshotRequired && array && !array->values.empty()) {
            transientElements.emplace(array->values);
            elements = std::span<const ArtifactScriptValue>(
                transientElements->data(), transientElements->size());
        }
        ArtifactScriptFields scope(fields, workspace ? workspace->overlay.data() : nullptr);
        auto* loopBinding = scope.prepareLoopBinding(s->foreachItemName);
        for (const auto& element : elements) {
            if (loopBinding) scope.bindLoopValue(*loopBinding, element);
            else scope.bindLoopValue(s->foreachItemName, element);
            if (!execStmt(s->foreachBody.get(), scope, locals)) return false;
            if (returned_) break;
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; }
        }
        // Persist field mutations made inside the loop body.
        scope.commit(s->foreachItemName);
        return true;
    }
    case ArtifactScriptStmt::Kind::If: {
        auto cond = evalExpr(s->ifCond.get(), fields, locals);
        bool t = std::holds_alternative<bool>(cond) ? std::get<bool>(cond)
            : (std::holds_alternative<double>(cond) ? std::get<double>(cond) != 0.0
               : std::holds_alternative<std::int64_t>(cond) ? std::get<std::int64_t>(cond) != 0 : false);
        if (t) return execStmt(s->ifThen.get(), fields, locals);
        if (s->ifElse) return execStmt(s->ifElse.get(), fields, locals);
        return true;
    }
    case ArtifactScriptStmt::Kind::Return:
        returnValue_ = s->expr ? evalExpr(s->expr.get(), fields, locals)
                               : ArtifactScriptValue{};
        returned_ = true;
        return error_.empty();
    case ArtifactScriptStmt::Kind::Break:
        breakRequested_ = true;
        return true;
    case ArtifactScriptStmt::Kind::Continue:
        continueRequested_ = true;
        return true;
    case ArtifactScriptStmt::Kind::Block:
        for (auto& st : s->blockStmts) {
            if (!execStmt(st.get(), fields, locals)) return false;
            if (returned_ || breakRequested_ || continueRequested_) break;
        }
        return true;
    }
    return true;
}

// ─── Host Binding API ───

class ArtifactScriptHost::Impl {
public:
    std::unordered_map<std::string, ArtifactScriptNativeFn> functions;
    std::unordered_map<ArtifactScriptHostMethodKey, ArtifactScriptNativeMethodFn,
                       ArtifactScriptHostMethodKeyHash, ArtifactScriptHostMethodKeyEqual> methods;
    NamedVector<std::string> logRing;
    std::string lastError;
    static constexpr std::size_t kMaxLogLines = 256;
};

ArtifactScriptHost::ArtifactScriptHost() : impl_(std::make_unique<Impl>()) {
    registerFunction("getEnv", [](std::span<const ArtifactScriptValue> args) {
        if (args.empty() || !std::holds_alternative<std::string>(args[0])) {
            return ArtifactScriptValue{};
        }
        const auto& name = std::get<std::string>(args[0]);
        if (name.rfind("ARTIFACT_", 0) != 0) {
            return args.size() > 1 ? args[1] : ArtifactScriptValue{};
        }
        auto* environment = EnvironmentVariableManager::instance();
        const QString key = QString::fromStdString(name);
        if (!environment->hasVariable(key)) {
            return args.size() > 1 ? args[1] : ArtifactScriptValue{};
        }
        return ArtifactScriptValue(environment->getVariable(key).toString().toStdString());
    });
    registerFunction("hasEnv", [](std::span<const ArtifactScriptValue> args) {
        if (args.empty() || !std::holds_alternative<std::string>(args[0])) {
            return ArtifactScriptValue(false);
        }
        const auto& name = std::get<std::string>(args[0]);
        if (name.rfind("ARTIFACT_", 0) != 0) {
            return ArtifactScriptValue(false);
        }
        return ArtifactScriptValue(EnvironmentVariableManager::instance()->hasVariable(
            QString::fromStdString(name)));
    });
}
ArtifactScriptHost::~ArtifactScriptHost() noexcept = default;

void ArtifactScriptHost::registerFunction(const std::string& name, ArtifactScriptNativeFn function) {
    impl_->functions.insert_or_assign(name, std::move(function));
}

void ArtifactScriptHost::registerMethod(const std::string& className, const std::string& methodName,
                                        ArtifactScriptNativeMethodFn function) {
    impl_->methods.insert_or_assign(
        ArtifactScriptHostMethodKey{className, methodName}, std::move(function));
}

bool ArtifactScriptHost::hasMethod(const std::string& className, const std::string& methodName) const {
    return impl_->methods.find(ArtifactScriptHostMethodKeyView{className, methodName}) !=
           impl_->methods.end();
}

bool ArtifactScriptHost::callMethod(const std::string& className, const std::string& methodName,
                                    const ArtifactScriptValue& self,
                                    const std::vector<ArtifactScriptValue>& args,
                                    ArtifactScriptValue& result) const {
    return callMethodView(className, methodName, self,
                          std::span<const ArtifactScriptValue>(args.data(), args.size()), result);
}

bool ArtifactScriptHost::callMethodView(const std::string& className, const std::string& methodName,
                                        const ArtifactScriptValue& self,
                                        std::span<const ArtifactScriptValue> args,
                                        ArtifactScriptValue& result) const {
    const auto it = impl_->methods.find(
        ArtifactScriptHostMethodKeyView{className, methodName});
    if (it == impl_->methods.end()) return false;
    result = it->second(self, args);
    return true;
}

void ArtifactScriptHost::installCompositionApi(const ArtifactScriptCompositionApi& api) {
    if (api.getLayer) {
        registerFunction("getLayer", [fn = api.getLayer](std::span<const ArtifactScriptValue> args) {
            if (args.size() != 1 || !std::holds_alternative<std::string>(args[0])) return ArtifactScriptValue{};
            return fn(std::get<std::string>(args[0]));
        });
    }
    if (api.getLayerCount) {
        registerFunction("getLayerCount", [fn = api.getLayerCount](std::span<const ArtifactScriptValue> args) {
            return args.empty() ? ArtifactScriptValue(fn()) : ArtifactScriptValue{};
        });
    }
    if (api.getTime) {
        registerFunction("getTime", [fn = api.getTime](std::span<const ArtifactScriptValue> args) {
            return args.empty() ? ArtifactScriptValue(fn()) : ArtifactScriptValue{};
        });
    }
    if (api.getProperty) {
        registerFunction("getProperty", [fn = api.getProperty](std::span<const ArtifactScriptValue> args) {
            if (args.size() != 2 || !std::holds_alternative<std::string>(args[1])) return ArtifactScriptValue{};
            return fn(args[0], std::get<std::string>(args[1]));
        });
    }
    if (api.setProperty) {
        registerFunction("setProperty", [this, fn = api.setProperty](std::span<const ArtifactScriptValue> args) {
            if (args.size() != 3 || !std::holds_alternative<std::string>(args[1])) {
                setLastError("setProperty expects target, path, value");
                return ArtifactScriptValue(false);
            }
            const bool accepted = fn(args[0], std::get<std::string>(args[1]), args[2]);
            if (!accepted) setLastError("setProperty rejected target or path");
            return ArtifactScriptValue(accepted);
        });
    }
}

ArtifactScriptReloadResult ArtifactScriptHotReload::reloadWithSaved(
    const ArtifactScriptDefinition& newDefinition,
    const ArtifactScriptDefinition* previousDef,
    const ArtifactScriptSerializedFields* liveFields,
    const ArtifactScriptSerializedComponent* saved) {
    ArtifactScriptReloadResult r;
    // ArtifactScriptDefinition is move-only because its class tree owns
    // parsed statement nodes. Reparse the source to create an independent
    // definition for the reload result.
    r.definition = ArtifactScriptParser{}.parse(newDefinition.source);
    if (!r.definition.diagnostics.empty()) {
        r.errorMessage = r.definition.diagnostics[0].message;
        return r;
    }

    // Live values migrate by name + type match (same rule as reload()).
    if (liveFields && previousDef) {
        for (const auto& oldField : previousDef->rootClass.fields) {
            if (!oldField.serialized) continue;
            const auto it = liveFields->find(oldField.name);
            if (it == liveFields->end()) continue;
            for (const auto& newField : r.definition.rootClass.fields) {
                if (newField.name == oldField.name && newField.serialized &&
                    newField.type == oldField.type) {
                    r.migratedFields[oldField.name] = it->second;
                    break;
                }
            }
        }
    }

    // Saved (project) values migrate identically, filling fields the live
    // map does not know about.
    if (saved) {
        for (const auto& newField : r.definition.rootClass.fields) {
            if (!newField.serialized) continue;
            if (r.migratedFields.find(newField.name) != r.migratedFields.end()) continue;
            const auto it = saved->values.find(newField.name);
            if (it == saved->values.end()) continue;
            if (it->second.index() == newField.defaultValue.index()) {
                r.migratedFields[newField.name] = it->second;
            } else {
                r.migratedFields[newField.name] = newField.defaultValue;
            }
        }
        // Unknown saved keys ride along so a later save re-emits them.
        for (const auto& [name, value] : saved->unknown) {
            if (r.migratedFields.find(name) == r.migratedFields.end() &&
                r.definition.rootClass.fields.end() ==
                    std::find_if(r.definition.rootClass.fields.begin(),
                                 r.definition.rootClass.fields.end(),
                                 [&](const ArtifactScriptField& f) { return f.name == name; })) {
                r.migratedFields[name] = value;
            }
        }
    }

    // Defaults for every serialized field, including [SerializeField] private
    // fields, that is still missing.
    for (const auto& field : r.definition.rootClass.fields) {
        if (field.serialized &&
            r.migratedFields.find(field.name) == r.migratedFields.end()) {
            r.migratedFields[field.name] = field.defaultValue;
        }
    }

    r.success = true;
    return r;
}
bool ArtifactScriptHost::hasFunction(const std::string& name) const {
    return impl_->functions.find(name) != impl_->functions.end();
}

bool ArtifactScriptHost::callFunction(
    const std::string& name,
    const std::vector<ArtifactScriptValue>& args,
    ArtifactScriptValue& result) const {
    const auto it = impl_->functions.find(name);
    if (it == impl_->functions.end()) return false;
    result = it->second(std::span<const ArtifactScriptValue>(args.data(), args.size()));
    return true;
}

bool ArtifactScriptHost::callFunctionView(
    const std::string& name,
    std::span<const ArtifactScriptValue> args,
    ArtifactScriptValue& result) const {
    const auto it = impl_->functions.find(name);
    if (it == impl_->functions.end()) return false;
    impl_->lastError.clear();
    result = it->second(args);
    return true;
}

std::vector<std::string> ArtifactScriptHost::registeredNames() const {
    std::vector<std::string> names;
    names.reserve(impl_->functions.size());
    for (const auto& [name, fn] : impl_->functions) names.push_back(name);
    std::sort(names.begin(), names.end());
    return names;
}

void ArtifactScriptHost::appendLog(std::string line) {
    if (impl_->logRing.size() >= Impl::kMaxLogLines) {
        impl_->logRing.removeAt(0);
    }
    impl_->logRing.add(std::move(line));
}

std::vector<std::string> ArtifactScriptHost::drainLog() {
    std::vector<std::string> lines(impl_->logRing.begin(), impl_->logRing.end());
    impl_->logRing.clear();
    return lines;
}

void ArtifactScriptHost::setLastError(std::string message) {
    impl_->lastError = std::move(message);
}

std::string ArtifactScriptHost::lastError() const {
    return impl_->lastError;
}

std::string_view ArtifactScriptHost::lastErrorView() const noexcept {
    return impl_->lastError;
}

ArtifactScriptHost& ArtifactScriptHost::global() {
    static ArtifactScriptHost instance;
    return instance;
}

class ArtifactScriptHotReload::Impl {
public:
    struct WatchEntry { std::string path; std::int64_t lastModified = 0; };
    struct FileEntry {
        ArtifactScriptDefinition definition;
        ArtifactScriptSerializedFields fields;
    };
    NamedVector<WatchEntry> watches_;
    std::unordered_map<std::string, FileEntry> files_;
};

ArtifactScriptHotReload::ArtifactScriptHotReload() : impl_(std::make_unique<Impl>()) {}

ArtifactScriptHotReload::~ArtifactScriptHotReload() noexcept = default;

ArtifactScriptReloadResult ArtifactScriptHotReload::reload(
    std::string_view newSource, const ArtifactScriptDefinition* prevDef,
    const ArtifactScriptSerializedFields* prevFields)
{
    ArtifactScriptReloadResult r;
    ArtifactScriptParser parser;
    r.definition = parser.parse(newSource);
    if (!r.definition.diagnostics.empty()) { r.errorMessage = r.definition.diagnostics[0].message; return r; }

    // Migrate existing field values
    if (prevFields && prevDef) {
        for (const auto& of : prevDef->rootClass.fields) {
            if (!of.serialized) continue;
            auto it = prevFields->find(of.name);
            if (it == prevFields->end()) continue;
            for (const auto& nf : r.definition.rootClass.fields) {
                if (nf.name == of.name && nf.serialized && nf.type == of.type) {
                    r.migratedFields[of.name] = it->second; break;
                }
            }
        }
    }
    // Apply defaults to every serialized field, including private fields opted
    // in with [SerializeField].
    for (const auto& field : r.definition.rootClass.fields) {
        if (field.serialized &&
            r.migratedFields.find(field.name) == r.migratedFields.end()) {
            r.migratedFields[field.name] = field.defaultValue;
        }
    }

    r.success = true; return r;
}

bool ArtifactScriptHotReload::watchFile(const std::string& path) {
    for (auto& w : impl_->watches_) if (w.path == path) return true;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) return false;
    Impl::WatchEntry e; e.path = path;
    if (auto t = std::filesystem::last_write_time(path, ec); !ec && t.time_since_epoch().count() > 0)
        e.lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
    impl_->watches_.add(e); return true;
}

void ArtifactScriptHotReload::unwatchFile(const std::string& path) {
    impl_->watches_.removeIf(
        [&](const Impl::WatchEntry& w) { return w.path == path; });
}

std::vector<std::string> ArtifactScriptHotReload::pollChanges() {
    NamedVector<std::string> changed;
    for (auto& w : impl_->watches_) {
        std::error_code ec;
        auto t = std::filesystem::last_write_time(w.path, ec);
        if (ec) continue;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
        if (ms > w.lastModified) { w.lastModified = ms; changed.add(w.path); }
    }
    return changed.toStdVector();
}

bool ArtifactScriptHotReload::addFile(
    const std::string& path, const ArtifactScriptSerializedFields& initialFields) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    auto result = reload(source, nullptr, &initialFields);
    if (!result.success) return false;
    for (const auto& [name, value] : initialFields) {
        for (const auto& field : result.definition.rootClass.fields) {
            if (field.serialized && field.name == name) {
                result.migratedFields[name] = value;
                break;
            }
        }
    }
    impl_->files_[path] = Impl::FileEntry{std::move(result.definition), std::move(result.migratedFields)};
    return watchFile(path);
}

void ArtifactScriptHotReload::removeFile(const std::string& path) {
    unwatchFile(path);
    impl_->files_.erase(path);
}

std::vector<ArtifactScriptFileReload> ArtifactScriptHotReload::reloadChanged() {
    std::vector<ArtifactScriptFileReload> reloaded;
    for (const auto& path : pollChanges()) {
        auto it = impl_->files_.find(path);
        if (it == impl_->files_.end()) continue;
        std::ifstream input(path, std::ios::binary);
        if (!input) continue;
        const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        auto result = reload(source, &it->second.definition, &it->second.fields);
        reloaded.push_back(ArtifactScriptFileReload{path, std::move(result)});
        if (reloaded.back().result.success) {
            it->second.definition = ArtifactScriptParser{}.parse(source);
            it->second.fields = reloaded.back().result.migratedFields;
        }
    }
    return reloaded;
}

const ArtifactScriptDefinition* ArtifactScriptHotReload::definitionFor(const std::string& path) const {
    const auto it = impl_->files_.find(path);
    return it == impl_->files_.end() ? nullptr : &it->second.definition;
}

const ArtifactScriptSerializedFields* ArtifactScriptHotReload::fieldsFor(const std::string& path) const {
    const auto it = impl_->files_.find(path);
    return it == impl_->files_.end() ? nullptr : &it->second.fields;
}
const ArtifactScriptClass* ArtifactScriptEvaluator::Impl::findClass(std::string_view name) const {
    if (!activeDefinition_ || name.empty()) return nullptr;
    if (name == activeDefinition_->rootClass.name) return &activeDefinition_->rootClass;
    if (classLookupIndexEnabled_ &&
        classLookupDefinition_ == activeDefinition_) {
        static_assert((kClassLookupIndexCapacity &
                       (kClassLookupIndexCapacity - 1)) == 0);
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char character : name) {
            hash ^= character;
            hash *= 1099511628211ull;
        }
        auto slot = static_cast<std::size_t>(hash) &
                    (kClassLookupIndexCapacity - 1);
        for (std::size_t probe = 0; probe < kClassLookupIndexCapacity; ++probe) {
            const auto& entry = classLookupIndex_[slot];
            if (entry.generation != classLookupGeneration_) return nullptr;
            if (entry.classDefinition->name == name) return entry.classDefinition;
            slot = (slot + 1) & (kClassLookupIndexCapacity - 1);
        }
        return nullptr;
    }
    for (const auto& cls : activeDefinition_->classes) {
        if (cls.name == name) return &cls;
    }
    return nullptr;
}

const ArtifactScriptMethod* ArtifactScriptEvaluator::Impl::findMethodInChain(
    std::string_view className, std::string_view methodName) const {
    if (!activeDefinition_ || className.empty() || methodName.empty()) return nullptr;
    std::string_view current = className;
    for (int depth = 0; depth < 32; ++depth) {
        const ArtifactScriptClass* cls = findClass(current);
        if (!cls) return nullptr;
        const std::vector<ArtifactScriptMethod>* methods = &cls->methods;
        if (current == activeDefinition_->rootClass.name) methods = &activeDefinition_->rootClass.methods;
        for (const auto& method : *methods) {
            if (method.name == methodName) return &method;
        }
        if (cls->parentName.empty()) return nullptr;
        current = cls->parentName;
    }
    return nullptr;
}

void ArtifactScriptEvaluator::Impl::beginMethodCallCacheGeneration() {
    ++methodCallCacheGeneration_;
    if (methodCallCacheGeneration_ == 0) {
        for (auto& entry : methodCallCache_) entry.generation = 0;
        for (auto& entry : objectMethodCallCache_) entry.generation = 0;
        methodCallCacheGeneration_ = 1;
    }
}

const ArtifactScriptMethod* ArtifactScriptEvaluator::Impl::findMethodAtCallSite(
    const ArtifactScriptExpr* callSite) {
    if (!activeDefinition_ || !callSite) return nullptr;
    static_assert((kMethodCallCacheCapacity & (kMethodCallCacheCapacity - 1)) == 0);
    const auto address = reinterpret_cast<std::uintptr_t>(callSite);
    const auto slot = (address >> 4) & (kMethodCallCacheCapacity - 1);
    auto& entry = methodCallCache_[slot];
    if (entry.generation == methodCallCacheGeneration_ &&
        entry.callSite == callSite && entry.definition == activeDefinition_) {
        return entry.method;
    }
    const auto* method = findMethodInChain(
        activeDefinition_->rootClass.name, callSite->callName);
    entry = {callSite, activeDefinition_, nullptr, method, methodCallCacheGeneration_};
    return method;
}

const ArtifactScriptMethod* ArtifactScriptEvaluator::Impl::findObjectMethodAtCallSite(
    const ArtifactScriptExpr* callSite, std::string_view className) {
    if (!activeDefinition_ || !callSite || className.empty()) return nullptr;
    static_assert((kMethodCallCacheCapacity & (kMethodCallCacheCapacity - 1)) == 0);
    const auto address = reinterpret_cast<std::uintptr_t>(callSite);
    // Preserve several runtime classes at a call site without hashing the
    // runtime class name on each cache hit.
    const auto set = (address >> 4) & (kMethodCallCacheCapacity - 1);
    const auto firstSlot = set * kObjectMethodCallCacheWays;
    for (std::size_t way = 0; way < kObjectMethodCallCacheWays; ++way) {
        auto& entry = objectMethodCallCache_[firstSlot + way];
        if (entry.generation == methodCallCacheGeneration_ &&
            entry.callSite == callSite && entry.definition == activeDefinition_ &&
            entry.targetClass && entry.targetClass->name == className) {
            return entry.method;
        }
    }

    const auto* targetClass = findClass(className);
    if (!targetClass) return nullptr;
    const ArtifactScriptMethod* method = nullptr;
    const ArtifactScriptClass* current = targetClass;
    for (int depth = 0; current && depth < 32; ++depth) {
        for (const auto& candidate : current->methods) {
            if (candidate.name == callSite->callName) {
                method = &candidate;
                break;
            }
        }
        if (method || current->parentName.empty()) break;
        current = findClass(current->parentName);
    }
    auto* replacement = &objectMethodCallCache_[
        firstSlot + kObjectMethodCallCacheWays - 1];
    for (std::size_t way = 0; way < kObjectMethodCallCacheWays; ++way) {
        auto& entry = objectMethodCallCache_[firstSlot + way];
        if (entry.generation != methodCallCacheGeneration_) {
            replacement = &entry;
            break;
        }
    }
    *replacement = {callSite, activeDefinition_, targetClass, method,
                    methodCallCacheGeneration_};
    return method;
}

void ArtifactScriptEvaluator::Impl::beginClassLookupGeneration(
    const ArtifactScriptDefinition& definition) {
    classLookupDefinition_ = &definition;
    classLookupIndexEnabled_ =
        definition.classes.size() <= kClassLookupMaxClasses;
    if (!classLookupIndexEnabled_) return;

    ++classLookupGeneration_;
    if (classLookupGeneration_ == 0) {
        for (auto& entry : classLookupIndex_) entry.generation = 0;
        classLookupGeneration_ = 1;
    }

    for (const auto& cls : definition.classes) {
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char character : cls.name) {
            hash ^= character;
            hash *= 1099511628211ull;
        }
        auto slot = static_cast<std::size_t>(hash) &
                    (kClassLookupIndexCapacity - 1);
        for (std::size_t probe = 0; probe < kClassLookupIndexCapacity; ++probe) {
            auto& entry = classLookupIndex_[slot];
            if (entry.generation != classLookupGeneration_) {
                entry.classDefinition = &cls;
                entry.generation = classLookupGeneration_;
                break;
            }
            // Preserve the original linear scan's first-match behavior for
            // duplicate class names.
            if (entry.classDefinition->name == cls.name) break;
            slot = (slot + 1) & (kClassLookupIndexCapacity - 1);
        }
    }
}

void ArtifactScriptEvaluator::Impl::invalidateClassLookupIndex() {
    classLookupIndexEnabled_ = false;
    classLookupDefinition_ = nullptr;
    classLookupIndexReusable_ = false;
    classLookupIndexPreparedForExecution_ = false;
}

bool ArtifactScriptEvaluator::Impl::isInstanceOf(
    const ArtifactScriptObjectInstance& instance, std::string_view className) const {
    if (!activeDefinition_ || className.empty()) return false;
    std::string_view current = instance.className;
    for (int depth = 0; depth < 32; ++depth) {
        if (current == className) return true;
        const ArtifactScriptClass* cls = findClass(current);
        if (!cls || cls->parentName.empty()) return false;
        current = cls->parentName;
    }
    return false;
}

ArtifactScriptValue ArtifactScriptEvaluator::Impl::callInstanceMethod(
    const ArtifactScriptObjectInstancePtr& instance,
    const ArtifactScriptMethod& resolvedMethod,
    std::span<ArtifactScriptValue> args) {
    constexpr int kMaxCallDepth = static_cast<int>(kArtifactScriptMaxCallDepth);
    if (!instance) { error_ = "null object"; return {}; }
    if (callDepth_ >= kMaxCallDepth) { error_ = "script call depth limit"; return {}; }
    const ArtifactScriptMethod* method = &resolvedMethod;
    if (!method->body) return {};
    ArtifactScriptLocals locals(localsWorkspace_);
    for (std::size_t i = 0; i < args.size() && i < method->parameters.size(); ++i)
        locals[method->parameters[i]] = std::move(args[i]);
    const auto previousReturn = returnValue_;
    const bool previousReturned = returned_;
    const auto previousThis = activeThis_;
    ++callDepth_;
    returnValue_ = {};
    returned_ = false;
    activeThis_ = instance;
    ArtifactScriptFields instanceFields(instance->fields);
    ArtifactScriptFields fieldScope(instanceFields);
    for (const auto& statement : method->body->statements) {
        if (!execStmt(statement.get(), fieldScope, locals) || returned_) break;
    }
    if (error_.empty()) {
        fieldScope.commit(std::string_view{});
    }
    if (!error_.empty() && method->line != 0) {
        error_ = "line " + std::to_string(method->line) + ":" +
                 std::to_string(method->column == 0 ? 1 : method->column) +
                 ": " + error_;
    }
    auto result = std::move(returnValue_);
    --callDepth_;
    returnValue_ = previousReturn;
    returned_ = previousReturned;
    activeThis_ = previousThis;
    return result;
}

ArtifactScriptValue ArtifactScriptEvaluator::executeMethod(
    const ArtifactScriptDefinition& definition, std::string_view methodName,
    const std::vector<ArtifactScriptValue>& args, ArtifactScriptSerializedFields& fields) {
    impl_->error_.clear();
    impl_->invalidateClassLookupIndex();
    impl_->activeDefinition_ = &definition;
    // Resolve through the inheritance chain so a hook declared on a derived
    // class is found, not only one on the root class.
    const ArtifactScriptMethod* method =
        impl_->findMethodInChain(definition.rootClass.name, methodName);
    if (!method || !method->body) {
        impl_->error_ = "unknown method: " + std::string(methodName);
        return {};
    }
    return executeResolvedMethod(definition, *method, args, fields, false);
}

ArtifactScriptValue ArtifactScriptEvaluator::executeResolvedMethod(
    const ArtifactScriptDefinition& definition, const ArtifactScriptMethod& method,
    const std::vector<ArtifactScriptValue>& args,
    ArtifactScriptSerializedFields& fields, bool reuseDefinitionCache) {
    impl_->error_.clear();
    impl_->activeDefinition_ = &definition;
    const bool preparedForThisExecution =
        impl_->classLookupIndexPreparedForExecution_ &&
        impl_->classLookupDefinition_ == &definition;
    impl_->classLookupIndexPreparedForExecution_ = false;
    if (!preparedForThisExecution &&
        !(reuseDefinitionCache && !impl_->classLookupReuseDisabled_ &&
          impl_->classLookupIndexReusable_ &&
          impl_->classLookupDefinition_ == &definition)) {
        impl_->beginClassLookupGeneration(definition);
        impl_->classLookupIndexReusable_ =
            reuseDefinitionCache && !impl_->classLookupReuseDisabled_;
    }
    if (!reuseDefinitionCache || impl_->classLookupReuseDisabled_ ||
        !impl_->methodCallCacheReusable_ ||
        impl_->methodCallCacheDefinition_ != &definition) {
        impl_->beginMethodCallCacheGeneration();
    }
    impl_->methodCallCacheDefinition_ = &definition;
    impl_->methodCallCacheReusable_ =
        reuseDefinitionCache && !impl_->classLookupReuseDisabled_;
    impl_->beginForeachMutationCache(
        &definition, reuseDefinitionCache && !impl_->classLookupReuseDisabled_);
    impl_->callDepth_ = 0;
    impl_->returnValue_ = {};
    impl_->returned_ = false;
    ArtifactScriptFields fieldScope(fields);
    return impl_->callUserMethod(method, args, fieldScope);
}

std::string ArtifactScriptEvaluator::getLastError() const { return impl_->error_; }
bool ArtifactScriptEvaluator::hasError() const { return !impl_->error_.empty(); }

}
