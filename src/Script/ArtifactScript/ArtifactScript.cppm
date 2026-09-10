module;
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_map>
#include <variant>

#include <memory>
#include <functional>
#include <sstream>
module Script.ArtifactScript;

import Container.NamedVector;

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
    const ZeroString value = trim(text);
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
        return value.length() >= 2 && value.data()[0] == '"' && value.data()[value.length() - 1] == '"'
            ? std::string(std::string_view(value.data() + 1, value.length() - 2))
            : std::string(value.data(), value.length());
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

struct ParseCtx { std::string_view src; size_t pos = 0; size_t len = 0; };
void skipWS(ParseCtx& c) { while (c.pos < c.len && std::isspace(static_cast<unsigned char>(c.src[c.pos]))) ++c.pos; }
bool matchCh(ParseCtx& c, char ch) { skipWS(c); if (c.pos < c.len && c.src[c.pos] == ch) { ++c.pos; return true; } return false; }
bool matchKw(ParseCtx& c, const char* wd) { skipWS(c); size_t n = std::strlen(wd); if (c.pos + n <= c.len && c.src.substr(c.pos, n) == wd && (c.pos + n >= c.len || !std::isalnum(static_cast<unsigned char>(c.src[c.pos + n])))) { c.pos += n; return true; } return false; }
std::string parseId(ParseCtx& c) { skipWS(c); size_t s = c.pos; while (c.pos < c.len && (std::isalnum(static_cast<unsigned char>(c.src[c.pos])) || c.src[c.pos] == '_')) ++c.pos; return std::string(c.src.substr(s, c.pos - s)); }
ArtifactScriptExprPtr parseExpr(ParseCtx& c);
double parseNum(ParseCtx& c);

ArtifactScriptExprPtr parsePrimary(ParseCtx& c) {
    skipWS(c); if (c.pos >= c.len) return nullptr;
    auto e = std::make_unique<ArtifactScriptExpr>();
    if (matchKw(c, "new")) {
        const std::string className = parseId(c);
        if (className.empty()) return nullptr;
        e->kind = ArtifactScriptExpr::Kind::New;
        e->newClassName = className;
        if (matchCh(c, '(') && !matchCh(c, ')')) {
            do { auto a = parseExpr(c); if (a) e->newArgs.push_back(std::move(a)); } while (matchCh(c, ','));
            matchCh(c, ')');
        }
        return e;
    }
    if (matchKw(c, "this")) {
        e->kind = ArtifactScriptExpr::Kind::Variable;
        e->variableName = "this";
        return e;
    }
    if (c.src[c.pos] == '[') {
        ++c.pos; e->kind = ArtifactScriptExpr::Kind::ArrayLiteral;
        if (!matchCh(c, ']')) {
            do {
                auto element = parseExpr(c);
                if (element) e->arrayElements.push_back(std::move(element));
            } while (matchCh(c, ','));
            matchCh(c, ']');
        }
        return e;
    }
    if (c.src[c.pos] == '"') { c.pos++; size_t s = c.pos; while (c.pos < c.len && c.src[c.pos] != '"') ++c.pos;
        e->kind = ArtifactScriptExpr::Kind::Literal; e->literalValue = std::string(c.src.substr(s, c.pos - s)); c.pos++; return e; }
    if (c.src[c.pos] == '-' || c.src[c.pos] == '!') { e->kind = ArtifactScriptExpr::Kind::Unary;
        e->unaryOp = c.src[c.pos] == '-' ? ArtifactScriptUnaryOp::Neg : ArtifactScriptUnaryOp::Not;
        c.pos++; e->operand = parsePrimary(c); return e; }
    if (std::isdigit(static_cast<unsigned char>(c.src[c.pos]))) { e->kind = ArtifactScriptExpr::Kind::Literal; e->literalValue = parseNum(c); return e; }
    if (matchKw(c, "true")) { e->kind = ArtifactScriptExpr::Kind::Literal; e->literalValue = true; return e; }
    if (matchKw(c, "false")) { e->kind = ArtifactScriptExpr::Kind::Literal; e->literalValue = false; return e; }
    std::string id = parseId(c); if (id.empty()) return nullptr;
    if (matchCh(c, '(')) { e->kind = ArtifactScriptExpr::Kind::Call; e->callName = id;
        if (!matchCh(c, ')')) { do { auto a = parseExpr(c); if (a) e->callArgs.push_back(std::move(a)); } while (matchCh(c, ',')); matchCh(c, ')'); } return e; }
    ArtifactScriptExprPtr base = std::make_unique<ArtifactScriptExpr>();
    base->kind = ArtifactScriptExpr::Kind::Variable;
    base->variableName = id;
    while (matchCh(c, '.')) {
        const std::string member = parseId(c);
        if (member.empty()) return nullptr;
        if (matchCh(c, '(')) {
            auto call = std::make_unique<ArtifactScriptExpr>();
            call->kind = ArtifactScriptExpr::Kind::Call;
            call->callName = member;
            call->callTarget = std::move(base);
            if (!matchCh(c, ')')) {
                do { auto a = parseExpr(c); if (a) call->callArgs.push_back(std::move(a)); } while (matchCh(c, ','));
                matchCh(c, ')');
            }
            base = std::move(call);
        } else {
            auto field = std::make_unique<ArtifactScriptExpr>();
            field->kind = ArtifactScriptExpr::Kind::FieldAccess;
            field->fieldObject = std::move(base);
            field->fieldName = member;
            base = std::move(field);
        }
    }
    if (matchCh(c, '[')) {
        auto index = std::make_unique<ArtifactScriptExpr>();
        index->kind = ArtifactScriptExpr::Kind::Index;
        index->indexTarget = std::move(base);
        index->indexExpr = parseExpr(c); matchCh(c, ']');
        return index;
    }
    return base;
}

#define BIN_PARSE(name, next, ...) \
ArtifactScriptExprPtr name(ParseCtx& c) { auto l = next(c); while (l) { ArtifactScriptBinaryOp op; int matched = 0; __VA_ARGS__ if (!matched) break; auto e = std::make_unique<ArtifactScriptExpr>(); e->kind = ArtifactScriptExpr::Kind::Binary; e->binaryOp = op; e->left = std::move(l); e->right = next(c); l = std::move(e); } return l; }

BIN_PARSE(parseMulDiv, parsePrimary,
    if (matchCh(c, '*')) { op = ArtifactScriptBinaryOp::Mul; matched = 1; }
    else if (matchCh(c, '/')) { op = ArtifactScriptBinaryOp::Div; matched = 1; })

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

// Is: postfix `expr is Name` (inheritance-aware), tighter than ternary.
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

// Ternary: cond ? a : b — lowest precedence, right-associative.
ArtifactScriptExprPtr parseTernary(ParseCtx& c) {
    auto condition = parseIs(c);
    if (!condition) return nullptr;
    skipWS(c);
    if (c.pos >= c.len || c.src[c.pos] != '?') return condition;
    ++c.pos;
    auto e = std::make_unique<ArtifactScriptExpr>();
    e->kind = ArtifactScriptExpr::Kind::Ternary;
    e->ternaryCondition = std::move(condition);
    e->ternaryThen = parseExpr(c);
    if (!matchCh(c, ':')) return nullptr;
    e->ternaryElse = parseExpr(c);
    if (!e->ternaryThen || !e->ternaryElse) return nullptr;
    return e;
}

ArtifactScriptExprPtr parseExpr(ParseCtx& c) { return parseTernary(c); }

double parseNum(ParseCtx& c) { skipWS(c); size_t s = c.pos; while (c.pos < c.len && (std::isdigit(static_cast<unsigned char>(c.src[c.pos])) || c.src[c.pos] == '.')) ++c.pos; return std::strtod(std::string(c.src.substr(s, c.pos - s)).c_str(), nullptr); }

ArtifactScriptExprPtr parseExpr(ParseCtx& c);
ArtifactScriptStmtPtr parseStmt(ParseCtx& c);

ArtifactScriptStmtPtr parseStmt(ParseCtx& c) {
    skipWS(c); if (c.pos >= c.len || c.src[c.pos] == '}') return nullptr;
    if (matchKw(c, "if")) { matchCh(c, '('); auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::If;
        s->ifCond = parseExpr(c); matchCh(c, ')'); s->ifThen = parseStmt(c);
        if (matchKw(c, "else")) s->ifElse = parseStmt(c); return s; }
    if (matchKw(c, "while")) { matchCh(c, '('); auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::While;
        s->whileCond = parseExpr(c); matchCh(c, ')'); s->whileBody = parseStmt(c); return s; }
    if (matchKw(c, "for")) {
        matchCh(c, '(');
        auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::For;
        if (!matchCh(c, ';')) s->forInit = parseStmt(c);
        s->forCond = parseExpr(c); matchCh(c, ';');
        if (!matchCh(c, ')')) { s->forIncrement = parseStmt(c); matchCh(c, ')'); }
        s->forBody = parseStmt(c); return s;
    }
    if (matchKw(c, "break")) { auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Break; matchCh(c, ';'); return s; }
    if (matchKw(c, "continue")) { auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Continue; matchCh(c, ';'); return s; }
    if (matchKw(c, "return")) { auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Return;
        auto e = parseExpr(c); if (e) s->expr = std::move(e); matchCh(c, ';'); return s; }
    if (matchCh(c, '{')) { auto b = std::make_unique<ArtifactScriptStmt>(); b->kind = ArtifactScriptStmt::Kind::Block;
        while (c.pos < c.len && c.src[c.pos] != '}') { auto s = parseStmt(c); if (s) b->blockStmts.push_back(std::move(s)); else break; }
        matchCh(c, '}'); return b; }
    // Variable declaration: "float x" or "float x = expr", or type-inferred "var x = expr"
    std::string id = parseId(c);
    if (id.empty()) { matchCh(c, ';'); return std::make_unique<ArtifactScriptStmt>(); }
    if (id == "float" || id == "int" || id == "bool" || id == "string" || id == "Array" || id == "array") {
        auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Decl;
        s->declType = parseFieldType(id); s->declName = parseId(c);
        if (matchCh(c, '=')) s->declInit = parseExpr(c);
        matchCh(c, ';'); return s;
    }
    if (id == "var") {
        auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Decl;
        s->declType = ArtifactScriptValueType::Null;  // Null type marks type inference
        s->declName = parseId(c);
        if (matchCh(c, '=')) s->declInit = parseExpr(c);
        matchCh(c, ';'); return s;
    }
    if (id == "foreach") {
        auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Foreach;
        matchCh(c, '(');
        s->foreachItemName = parseId(c);
        if (!matchKw(c, "in")) { return nullptr; }
        s->foreachCollectionName = parseId(c);
        matchCh(c, ')');
        s->foreachBody = parseStmt(c);
        return s;
    }
    // Increment / decrement: "x++;" or "--x;"
    const bool isPostInc = matchKw(c, "++");
    const bool isPostDec = !isPostInc && matchKw(c, "--");
    if ((isPostInc || isPostDec) && !id.empty()) {
        auto inc = std::make_unique<ArtifactScriptStmt>();
        inc->kind = ArtifactScriptStmt::Kind::Assign;
        inc->assignTarget = id;
        inc->assignOp = isPostInc ? "+=" : "-=";
        inc->assignValue = std::make_unique<ArtifactScriptExpr>();
        inc->assignValue->kind = ArtifactScriptExpr::Kind::Literal;
        inc->assignValue->literalValue = 1.0;
        matchCh(c, ';');
        return inc;
    }
    if (matchCh(c, '[')) {
        auto index = parseExpr(c); matchCh(c, ']');
        std::string op;
        if (matchCh(c, '=')) op = "=";
        else if (c.pos + 1 < c.len && c.src[c.pos] == '+' && c.src[c.pos+1] == '=') { op = "+="; c.pos += 2; }
        else if (c.pos + 1 < c.len && c.src[c.pos] == '-' && c.src[c.pos+1] == '=') { op = "-="; c.pos += 2; }
        else if (c.pos + 1 < c.len && c.src[c.pos] == '*' && c.src[c.pos+1] == '=') { op = "*="; c.pos += 2; }
        else if (c.pos + 1 < c.len && c.src[c.pos] == '/' && c.src[c.pos+1] == '=') { op = "/="; c.pos += 2; }
        if (!op.empty()) {
            auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Assign;
            s->assignTarget = id; s->assignIndex = std::move(index); s->assignOp = op; s->assignValue = parseExpr(c); matchCh(c, ';'); return s;
        }
        c.pos -= id.size();
    }
    // Compound assignment: "x += expr;"
    std::string compoundOp;
    {
        skipWS(c);
        if (c.pos + 1 < c.len && c.src[c.pos+1] == '=' ) {
            if (c.src[c.pos] == '+') { compoundOp = "+="; c.pos += 2; }
            else if (c.src[c.pos] == '-') { compoundOp = "-="; c.pos += 2; }
            else if (c.src[c.pos] == '*') { compoundOp = "*="; c.pos += 2; }
            else if (c.src[c.pos] == '/') { compoundOp = "/="; c.pos += 2; }
            else if (c.src[c.pos] == '%') { compoundOp = "%="; c.pos += 2; }
        }
    }
    if (!compoundOp.empty()) {
        auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Assign;
        s->assignTarget = id; s->assignOp = compoundOp; s->assignValue = parseExpr(c); matchCh(c, ';'); return s;
    }
    if (matchCh(c, '=')) { auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Assign;
        s->assignTarget = id; s->assignValue = parseExpr(c); matchCh(c, ';'); return s; }
    if (matchCh(c, '.')) {
        std::string field = parseId(c);
        if (!field.empty()) {
            if (matchCh(c, '=')) {
                auto s = std::make_unique<ArtifactScriptStmt>();
                s->kind = ArtifactScriptStmt::Kind::Decl;
                s->declName = id;
                s->fieldAssign = true;
                s->assignField = field;
                s->declInit = parseExpr(c);
                matchCh(c, ';');
                return s;
            }
            c.pos -= (field.size() + 1);
        } else {
            c.pos -= 1;
        }
    }
    c.pos -= id.size();
    auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Expr;
    s->expr = parseExpr(c); matchCh(c, ';'); return s;
}

ArtifactScriptMethodBodyPtr parseMethodBody(std::string_view src, const std::vector<std::string>& params) {
    auto body = std::make_unique<ArtifactScriptMethodBody>(); body->parameters = params;
    ParseCtx c{src, 0, src.size()};
    while (c.pos < c.len) { auto s = parseStmt(c); if (s) body->statements.push_back(std::move(s)); else break; }
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

        const bool isPublic = starts_with(trimmed, "public ");
        const bool isPrivate = starts_with(trimmed, "private ");
        const bool isField = isPublic || isPrivate;
        const bool isMethod = trimmed.find('(') != static_cast<std::size_t>(-1) && trimmed.find(')') != static_cast<std::size_t>(-1);
        const bool isAttributeLine = !trimmed.isEmpty() && trimmed.data()[0] == '[' &&
            trimmed.find(']') != static_cast<std::size_t>(-1) && !isField && !isMethod;
        if (isAttributeLine) {
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
            pendingAttributes.clear();
            const std::string nameRaw(namePart.data(), namePart.length());
            std::string fieldName = nameRaw;
            const auto attrEnd = nameRaw.rfind(']');
            if (!nameRaw.empty() && nameRaw.front() == '[' && attrEnd != std::string::npos) {
                if (!attributes.empty()) attributes += " ";
                attributes += nameRaw.substr(0, attrEnd + 1);
                fieldName = std::string(trim(nameRaw.substr(attrEnd + 1)).data(),
                                        trim(nameRaw.substr(attrEnd + 1)).length());
            }
            field.name = fieldName;
            field.isPublic = isPublic;
            field.serialized = isPublic;
            field.type = parseFieldType(typeName);
            if (eq != static_cast<std::size_t>(-1)) {
                field.defaultValue = parseDefaultValue(body.substr(eq + 1), field.type);
            } else if (field.type == ArtifactScriptValueType::Array) {
                field.defaultValue = makeShared<ArtifactScriptArray>();
            }
            if (!attributes.empty()) {
                std::size_t cursor = 0;
                while (cursor < attributes.size()) {
                    const auto open = attributes.find('[', cursor);
                    if (open == std::string::npos) break;
                    const auto close = attributes.find(']', open + 1);
                    if (close == std::string::npos) break;
                    const ZeroString itemView = trim(attributes.substr(open + 1, close - open - 1));
                    const std::string item(itemView.data(), itemView.length());
                    if (item == "SerializeField") {
                        field.serialized = true;
                    } else if (starts_with(item, "Range(") && !item.empty() && item.back() == ')') {
                        const std::string inner = item.substr(6, item.size() - 7);
                        const auto comma = inner.find(',');
                        const ZeroString loView = trim(inner.substr(0, comma));
                        const ZeroString hiView = trim(comma == std::string::npos ? std::string() : inner.substr(comma + 1));
                        try {
                            field.hasRange = true;
                            field.rangeMin = std::stod(std::string(loView.data(), loView.length()));
                            field.rangeMax = std::stod(std::string(hiView.data(), hiView.length()));
                        } catch (...) {
                            field.hasRange = false;
                        }
                    } else if (starts_with(item, "Header(")) {
                        const auto first = item.find('"');
                        const auto last = item.rfind('"');
                        if (first != std::string::npos && last != std::string::npos && last > first) {
                            field.header = item.substr(first + 1, last - first - 1);
                        }
                    } else if (starts_with(item, "Tooltip(")) {
                        const auto first = item.find('"');
                        const auto last = item.rfind('"');
                        if (first != std::string::npos && last != std::string::npos && last > first) {
                            field.tooltip = item.substr(first + 1, last - first - 1);
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
            if (const auto hook = hookFromName(method.name)) {
                method.isLifecycleHook = true;
                method.hook = *hook;
            }
            // Parse method body if present
            std::string bodyText;
            size_t bodyStart = trimmed.find('{');
            if (bodyStart != std::string::npos) {
                // Body starts on same line
                size_t depth = 1;
                size_t searchPos = pos + bodyStart + 1;
                size_t bodyEnd = searchPos;
                for (; searchPos < sourceView.size() && depth > 0; ++searchPos) {
                    if (sourceView[searchPos] == '{') ++depth;
                    else if (sourceView[searchPos] == '}') --depth;
                    if (depth == 0) bodyEnd = searchPos;
                }
                if (depth == 0) {
                    bodyText = std::string(sourceView.substr(pos + bodyStart + 1, bodyEnd - (pos + bodyStart + 1)));
                    pos = bodyEnd + 1;
                    nextPos = sourceView.find('\n', pos);
                }
            } else {
                // Body on subsequent line - look for opening brace
                size_t lookPos = nextPos;
                while (lookPos < sourceView.size() && std::isspace(static_cast<unsigned char>(sourceView[lookPos])))
                    ++lookPos;
                if (lookPos < sourceView.size() && sourceView[lookPos] == '{') {
                    size_t depth = 1;
                    size_t bs = lookPos + 1;
                    for (; lookPos < sourceView.size() && depth > 0; ++lookPos) {
                        if (sourceView[lookPos] == '{') ++depth;
                        else if (sourceView[lookPos] == '}') --depth;
                    }
                    if (depth == 0) {
                        bodyText = std::string(sourceView.substr(bs, lookPos - 1 - bs));
                        pos = lookPos;
                        nextPos = sourceView.find('\n', pos);
                    }
                }
            }
            if (!bodyText.empty()) {
                method.body = parseMethodBody(bodyText, method.parameters);
            }
            activeClass().methods.push_back(std::move(method));
            pos = nextPos;
            continue;
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
struct ScriptJsonValue;
using ScriptJsonPtr = std::unique_ptr<ScriptJsonValue>;

struct ScriptJsonValue {
    enum class Kind { Null, Bool, Number, String, Array, Object };
    Kind kind = Kind::Null;
    bool boolValue = false;
    double numberValue = 0.0;
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
    auto [end, ec] = std::from_chars(token.data(), token.data() + token.size(), out->numberValue);
    if (ec != std::errc{}) return false;
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
        out = static_cast<std::int64_t>(json.numberValue);
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
                const double n = jsonNumber(*element);
                array->values.push_back(n == static_cast<double>(static_cast<std::int64_t>(n))
                                            ? ArtifactScriptValue(static_cast<std::int64_t>(n))
                                            : ArtifactScriptValue(n));
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
                if (n == static_cast<double>(static_cast<std::int64_t>(n)) &&
                    std::fabs(n) < 1.0e15) {
                    target.emplace(name, ArtifactScriptValue(static_cast<std::int64_t>(n)));
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




        case '\f': out += "\\f"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
                out += buffer;
            } else {
                out.push_back(c);
            }
        }
    }
    out.push_back('"');
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
    return definition_;
}

void ArtifactScriptInstance::bindComponent(const ArtifactScriptComponent& component) {
    component_ = &component;
}

const ArtifactScriptComponent* ArtifactScriptInstance::boundComponent() const {
    return component_;
}

bool ArtifactScriptInstance::hasMethod(std::string_view name) const {
    const auto& methods = definition_.rootClass.methods;
    return std::any_of(methods.begin(), methods.end(), [&](const ArtifactScriptMethod& method) {
        return method.name == name;
    });
}

bool ArtifactScriptInstance::hasHook(ArtifactScriptHook hook) const {
    const auto& methods = definition_.rootClass.methods;
    return std::any_of(methods.begin(), methods.end(), [&](const ArtifactScriptMethod& method) {
        return method.isLifecycleHook && method.hook == hook;
    });
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
    if (!hasHook(hook)) {
        return false;
    }
    const auto hookName = [hook]() -> std::string {
        switch (hook) {
        case ArtifactScriptHook::OnCreate: return "OnCreate";
        case ArtifactScriptHook::OnStart: return "OnStart";
        case ArtifactScriptHook::OnEnable: return "OnEnable";
        case ArtifactScriptHook::OnDisable: return "OnDisable";
        case ArtifactScriptHook::OnUpdate: return "OnUpdate";
        case ArtifactScriptHook::OnDestroy: return "OnDestroy";
        }
        return "OnUpdate";
    }();
    ArtifactScriptEvaluator evaluator;
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
    evaluator.executeMethod(definition_, hookName, {}, fields_);
    const bool ok = !evaluator.hasError();
    lastHookError_ = ok ? std::string() : evaluator.getLastError();
    lastInvokedHook_ = hook;
    return ok;
}

bool ArtifactScriptInstance::wasHookInvoked(ArtifactScriptHook hook) const {
    return lastInvokedHook_.has_value() && *lastInvokedHook_ == hook;
}


// ─── Evaluator ───

class ArtifactScriptEvaluator::Impl {
public:
    std::string error_;
    ArtifactScriptValue returnValue_{};
    bool returned_ = false;
    bool breakRequested_ = false;
    bool continueRequested_ = false;
    const ArtifactScriptDefinition* activeDefinition_ = nullptr;
    ArtifactScriptObjectInstancePtr activeThis_;
    int callDepth_ = 0;
    ArtifactScriptValue evalExpr(const ArtifactScriptExpr*, ArtifactScriptSerializedFields&, const std::unordered_map<std::string, ArtifactScriptValue>&);
    ArtifactScriptValue evalBinary(ArtifactScriptBinaryOp, const ArtifactScriptValue&, const ArtifactScriptValue&);
    ArtifactScriptValue evalUnary(ArtifactScriptUnaryOp, const ArtifactScriptValue&);
    ArtifactScriptValue evalCall(const ArtifactScriptExpr*, ArtifactScriptSerializedFields&, const std::unordered_map<std::string, ArtifactScriptValue>&);
    bool execStmt(const ArtifactScriptStmt*, ArtifactScriptSerializedFields&, std::unordered_map<std::string, ArtifactScriptValue>& locals);
    ArtifactScriptValue callUserMethod(std::string_view, const std::vector<ArtifactScriptValue>&, ArtifactScriptSerializedFields&);
    ArtifactScriptValue callInstanceMethod(const ArtifactScriptObjectInstancePtr&, std::string_view, const std::vector<ArtifactScriptValue>&);
    const ArtifactScriptClass* findClass(std::string_view) const;
    const ArtifactScriptMethod* findMethodInChain(std::string_view, std::string_view) const;
    bool isInstanceOf(const ArtifactScriptObjectInstance&, std::string_view) const;
};

ArtifactScriptEvaluator::ArtifactScriptEvaluator() : impl_(std::make_unique<Impl>()) {}

ArtifactScriptEvaluator::~ArtifactScriptEvaluator() noexcept = default;

bool ArtifactScriptEvaluator::execute(
    const ArtifactScriptMethodBody& body,
    const std::vector<ArtifactScriptValue>& args,
    ArtifactScriptSerializedFields& fields) {
    impl_->error_.clear();
    impl_->returnValue_ = {};
    impl_->returned_ = false;
    impl_->breakRequested_ = false;
    impl_->continueRequested_ = false;
    for (std::size_t i = 0; i < args.size() && i < body.parameters.size(); ++i) {
        fields[body.parameters[i]] = args[i];
    }
    std::unordered_map<std::string, ArtifactScriptValue> locals;
    for (auto& st : body.statements) {
        if (!impl_->execStmt(st.get(), fields, locals)) return false;
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

ArtifactScriptValue ArtifactScriptEvaluator::Impl::evalExpr(
    const ArtifactScriptExpr* e, ArtifactScriptSerializedFields& fields,
    const std::unordered_map<std::string, ArtifactScriptValue>& locals) {
    if (!e) { error_ = "null expr"; return {}; }
    switch (e->kind) {
    case ArtifactScriptExpr::Kind::Literal: return e->literalValue;
    case ArtifactScriptExpr::Kind::ArrayLiteral: {
        auto array = makeShared<ArtifactScriptArray>();
        for (const auto& element : e->arrayElements)
            array->values.push_back(evalExpr(element.get(), fields, locals));
        return array;
    }
    case ArtifactScriptExpr::Kind::Variable: {
        if (e->variableName == "this") {
            if (!activeThis_) { error_ = "this is only valid inside a method"; return {}; }
            return activeThis_;
        }
        auto local = locals.find(e->variableName);
        if (local != locals.end()) return local->second;
        auto it = fields.find(e->variableName);
        if (it != fields.end()) return it->second;
        error_ = "undefined: " + e->variableName; return {};
    }
    case ArtifactScriptExpr::Kind::FieldAccess: {
        const auto object = evalExpr(e->fieldObject.get(), fields, locals);
        if (!error_.empty()) return {};
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
        std::vector<ArtifactScriptValue> args;
        args.reserve(e->newArgs.size());
        for (const auto& arg : e->newArgs) {
            args.push_back(evalExpr(arg.get(), fields, locals));
            if (!error_.empty()) return {};
        }
        auto instance = makeShared<ArtifactScriptObjectInstance>();
        instance->className = cls->name;
        // Inherit default fields along the parent chain (base first).
        std::vector<const ArtifactScriptClass*> chain;
        const ArtifactScriptClass* cursor = cls;
        while (cursor) {
            chain.push_back(cursor);
            cursor = !cursor->parentName.empty() ? findClass(cursor->parentName) : nullptr;
        }
        for (auto chainIt = chain.rbegin(); chainIt != chain.rend(); ++chainIt) {
            for (const auto& field : (*chainIt)->fields) {
                if (instance->fields.find(field.name) == instance->fields.end()) {
                    instance->fields.emplace(field.name, field.defaultValue);
                }
            }
        }
        if (const ArtifactScriptMethod* ctor = findMethodInChain(cls->name, "OnConstruct")) {
            (void)ctor;
            const auto result = callInstanceMethod(instance, "OnConstruct", args);
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
            if (e->binaryOp == ArtifactScriptBinaryOp::And && !truthy(left)) return left;
            if (e->binaryOp == ArtifactScriptBinaryOp::Or && truthy(left)) return left;
            const auto right = evalExpr(e->right.get(), fields, locals);
            return right;
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
        const auto indexValue = evalExpr(e->indexExpr.get(), fields, locals);
        if (!std::holds_alternative<ArtifactScriptArrayPtr>(arrayValue) ||
            (!std::holds_alternative<double>(indexValue) && !std::holds_alternative<std::int64_t>(indexValue))) {
            error_ = "invalid array access"; return {};
        }
        const auto& array = std::get<ArtifactScriptArrayPtr>(arrayValue);
        const auto index = static_cast<std::size_t>(std::holds_alternative<double>(indexValue)
            ? std::get<double>(indexValue) : std::get<std::int64_t>(indexValue));
        if (!array || index >= array->values.size()) { error_ = "array index out of range"; return {}; }
        return array->values[index];
    }
    default: return {};
    }
}

ArtifactScriptValue ArtifactScriptEvaluator::Impl::evalBinary(
    ArtifactScriptBinaryOp op, const ArtifactScriptValue& l, const ArtifactScriptValue& r) {
    auto d = [](const ArtifactScriptValue& v) -> double {
        if (std::holds_alternative<double>(v)) return std::get<double>(v);
        if (std::holds_alternative<std::int64_t>(v)) return (double)std::get<std::int64_t>(v);
        return 0.0;
    };
    auto b = [&](const ArtifactScriptValue& v) { return std::holds_alternative<bool>(v) ? std::get<bool>(v) : d(v) != 0.0; };
    if (op == ArtifactScriptBinaryOp::Add &&
        (std::holds_alternative<std::string>(l) || std::holds_alternative<std::string>(r))) {
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
        return toString(l) + toString(r);
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
    case ArtifactScriptUnaryOp::Neg: return -d(v);
    case ArtifactScriptUnaryOp::Not: return !(std::holds_alternative<bool>(v) ? std::get<bool>(v) : d(v) != 0.0);
    }
    return {};
}

ArtifactScriptValue ArtifactScriptEvaluator::Impl::evalCall(
    const ArtifactScriptExpr* e, ArtifactScriptSerializedFields& fields,
    const std::unordered_map<std::string, ArtifactScriptValue>& locals) {
    std::vector<ArtifactScriptValue> args;
    for (auto& a : e->callArgs) args.push_back(evalExpr(a.get(), fields, locals));
    if (!error_.empty()) return {};
    if (e->callTarget) {
        const auto target = evalExpr(e->callTarget.get(), fields, locals);
        if (!error_.empty()) return {};
        // (1) script instance method wins over host so user overrides keep
        // working once class bodies exist; (2) host-registered ObjectRef or
        // instance methods; (3) diagnostic error.
        if (std::holds_alternative<ArtifactScriptObjectInstancePtr>(target) &&
            std::get<ArtifactScriptObjectInstancePtr>(target)) {
            const auto& instance = std::get<ArtifactScriptObjectInstancePtr>(target);
            if (activeDefinition_ && findMethodInChain(instance->className, e->callName)) {
                return callInstanceMethod(instance, e->callName, args);
            }
            ArtifactScriptValue hostResult;
            const std::string classLabel = instance->className.empty() ? "Object" : instance->className;
            if (ArtifactScriptHost::global().callMethod(classLabel, e->callName, target, args, hostResult)) {
                if (!ArtifactScriptHost::global().lastError().empty()) {
                    error_ = "host: " + ArtifactScriptHost::global().lastError();
                    return {};
                }
                return hostResult;
            }
            error_ = "unknown method: " + e->callName; return {};
        }
        // Host objects arrive as ObjectRef (e.g. getLayer() handles).
        if (std::holds_alternative<ArtifactScriptRef>(target)) {
            ArtifactScriptValue hostResult;
            if (ArtifactScriptHost::global().callMethod("ObjectRef", e->callName, target, args, hostResult)) {
                if (!ArtifactScriptHost::global().lastError().empty()) {
                    error_ = "host: " + ArtifactScriptHost::global().lastError();
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
    if (e->callName == "array" && args.empty())
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
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i > 0) line += " ";
            line += toString(args[i]);
        }
        ArtifactScriptHost::global().appendLog(std::move(line));
        return {};
    }
    if (e->callName == "size" && args.size() == 1 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(args[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(args[0]);
        return static_cast<std::int64_t>(array ? array->values.size() : 0);
    }
    if (e->callName == "push" && args.size() == 2 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(args[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(args[0]);
        if (!array) { error_ = "push on null array"; return {}; }
        array->values.push_back(args[1]);
        return static_cast<std::int64_t>(array->values.size());
    }
    if (e->callName == "clear" && args.size() == 1 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(args[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(args[0]);
        if (!array) { error_ = "clear on null array"; return {}; }
        array->values.clear();
        return static_cast<std::int64_t>(0);
    }
    if (e->callName == "empty" && args.size() == 1 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(args[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(args[0]);
        return !array || array->values.empty();
    }
    if (e->callName == "pop" && args.size() == 1 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(args[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(args[0]);
        if (!array || array->values.empty()) { error_ = "pop from empty array"; return {}; }
        auto value = array->values.back();
        array->values.pop_back();
        return value;
    }
    if ((e->callName == "contains" || e->callName == "indexOf") && args.size() == 2 &&
        std::holds_alternative<ArtifactScriptArrayPtr>(args[0])) {
        const auto& array = std::get<ArtifactScriptArrayPtr>(args[0]);
        if (!array) return e->callName == "contains" ? ArtifactScriptValue(false) : ArtifactScriptValue(std::int64_t(-1));
        for (std::size_t i = 0; i < array->values.size(); ++i) {
            const auto& item = array->values[i];
            bool equal = false;
            if (item.index() == args[1].index()) {
                if (std::holds_alternative<double>(item)) equal = std::get<double>(item) == num(args[1]);
                else if (std::holds_alternative<std::int64_t>(item)) equal = std::get<std::int64_t>(item) == static_cast<std::int64_t>(num(args[1]));
                else if (std::holds_alternative<std::string>(item)) equal = std::get<std::string>(item) == std::get<std::string>(args[1]);
                else if (std::holds_alternative<bool>(item)) equal = std::get<bool>(item) == std::get<bool>(args[1]);
            }
            if (equal) return e->callName == "contains" ? ArtifactScriptValue(true) : ArtifactScriptValue(static_cast<std::int64_t>(i));
        }
        return e->callName == "contains" ? ArtifactScriptValue(false) : ArtifactScriptValue(std::int64_t(-1));
    }
    if (e->callName == "abs" && !args.empty()) return std::abs(num(args[0]));
    if (e->callName == "min" && args.size() >= 2) return std::min(num(args[0]), num(args[1]));
    if (e->callName == "max" && args.size() >= 2) return std::max(num(args[0]), num(args[1]));
    if (e->callName == "clamp" && args.size() >= 3) return std::clamp(num(args[0]), num(args[1]), num(args[2]));
    if (e->callName == "lerp" && args.size() >= 3) {
        double a = num(args[0]), b = num(args[1]), t = num(args[2]); return a + (b - a) * t;
    }
    if (e->callName == "sin" && !args.empty()) return std::sin(num(args[0]));
    if (e->callName == "cos" && !args.empty()) return std::cos(num(args[0]));
    if (activeDefinition_ && findMethodInChain(activeDefinition_->rootClass.name, e->callName)) {
        return callUserMethod(e->callName, args, fields);
    }
    ArtifactScriptValue hostResult;
    ArtifactScriptHost& host = ArtifactScriptHost::global();
    if (host.hasFunction(e->callName)) {
        host.setLastError(std::string());
        if (host.callFunction(e->callName, args, hostResult)) {
            // Host callbacks may report failures via setLastError; surface
            // them through the evaluator's diagnostic path.
            const std::string hostError = host.lastError();
            if (!hostError.empty()) error_ = "host: " + hostError;
            return hostResult;
        }
    }
    error_ = "unknown function: " + e->callName; return {};
}

ArtifactScriptValue ArtifactScriptEvaluator::Impl::callUserMethod(
    std::string_view name, const std::vector<ArtifactScriptValue>& args,
    ArtifactScriptSerializedFields& fields) {
    constexpr int kMaxCallDepth = 64;
    if (callDepth_ >= kMaxCallDepth) {
        error_ = "script call depth limit";
        return {};
    }
    const ArtifactScriptMethod* method = findMethodInChain(activeDefinition_->rootClass.name, name);
    if (!method || !method->body) return {};
    std::unordered_map<std::string, ArtifactScriptValue> locals;
    for (std::size_t i = 0; i < args.size() && i < method->parameters.size(); ++i)
        locals[method->parameters[i]] = args[i];
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
    const auto result = returnValue_;
    --callDepth_;
    returnValue_ = previousReturn;
    returned_ = previousReturned;
    return result;
}

bool ArtifactScriptEvaluator::Impl::execStmt(
    const ArtifactScriptStmt* s, ArtifactScriptSerializedFields& fields,
    std::unordered_map<std::string, ArtifactScriptValue>& locals) {
    if (!s) return true;
    switch (s->kind) {
    case ArtifactScriptStmt::Kind::Expr:
        evalExpr(s->expr.get(), fields, locals); return error_.empty();
    case ArtifactScriptStmt::Kind::Assign: {
        auto v = evalExpr(s->assignValue.get(), fields, locals);
        if (!error_.empty()) return false;
        // Compound assignment folds the current value with the right side.
        const std::string& op = s->assignOp;
        auto applyCompound = [&](const ArtifactScriptValue& current) -> ArtifactScriptValue {
            if (op.empty() || op == "=") return v;
            if (op == "+=") return evalBinary(ArtifactScriptBinaryOp::Add, current, v);
            if (op == "-=") return evalBinary(ArtifactScriptBinaryOp::Sub, current, v);
            if (op == "*=") return evalBinary(ArtifactScriptBinaryOp::Mul, current, v);
            if (op == "/=") return evalBinary(ArtifactScriptBinaryOp::Div, current, v);
            if (op == "%=") return evalBinary(ArtifactScriptBinaryOp::Mod, current, v);
            error_ = "unsupported assign op: " + op;
            return {};
        };
        if (s->assignIndex) {
            auto target = locals.find(s->assignTarget);
            if (target == locals.end()) {
                auto field = fields.find(s->assignTarget);
                if (field == fields.end()) { error_ = "undefined array: " + s->assignTarget; return false; }
                target = locals.emplace(s->assignTarget, field->second).first;
            }
            auto indexValue = evalExpr(s->assignIndex.get(), fields, locals);
            if (!std::holds_alternative<ArtifactScriptArrayPtr>(target->second) ||
                (!std::holds_alternative<double>(indexValue) && !std::holds_alternative<std::int64_t>(indexValue))) {
                error_ = "invalid array assignment"; return false;
            }
            const auto& array = std::get<ArtifactScriptArrayPtr>(target->second);
            const auto index = static_cast<std::size_t>(std::holds_alternative<double>(indexValue)
                ? std::get<double>(indexValue) : std::get<std::int64_t>(indexValue));
            if (!array || index >= array->values.size()) { error_ = "array index out of range"; return false; }
            array->values[index] = applyCompound(array->values[index]);
            return error_.empty();
        }
        auto lit = locals.find(s->assignTarget);
        if (lit != locals.end()) { lit->second = applyCompound(lit->second); return error_.empty(); }
        auto fieldIt = fields.find(s->assignTarget);
        if (fieldIt != fields.end()) {
            fieldIt->second = applyCompound(fieldIt->second);
        } else {
            fields[s->assignTarget] = applyCompound({});
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
            auto resolveObject = [&](ArtifactScriptValue& slot) -> ArtifactScriptObjectInstancePtr* {
                if (std::holds_alternative<ArtifactScriptObjectInstancePtr>(slot)) {
                    return &std::get<ArtifactScriptObjectInstancePtr>(slot);
                }
                return nullptr;
            };
            ArtifactScriptObjectInstancePtr* target = nullptr;
            ArtifactScriptObjectInstancePtr thisCopy;
            if (s->declName == "this") {
                if (!activeThis_) { error_ = "this is only valid inside a method"; return false; }
                thisCopy = activeThis_;
                target = &thisCopy;
            } else if (auto lit = locals.find(s->declName); lit != locals.end()) {
                target = resolveObject(lit->second);
            } else if (auto fieldIt = fields.find(s->declName); fieldIt != fields.end()) {
                target = resolveObject(fieldIt->second);
            }
            if (!target || !*target) { error_ = "field assign on non-object: " + s->declName; return false; }
            (*target)->fields[s->assignField] = init;
            if (s->declName == "this") activeThis_ = thisCopy;
            return error_.empty();
        }
        locals[s->declName] = (s->declInit || s->declType == ArtifactScriptValueType::Array)
            ? init : ArtifactScriptValue{};
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
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; }
            ++iter; }
        if (iter >= 10000) { error_ = "loop limit"; return false; } return true; }
    case ArtifactScriptStmt::Kind::For: {
        if (s->forInit && !execStmt(s->forInit.get(), fields, locals)) return false;
        int iter = 0;
        while (iter < 10000) {
            auto cond = evalExpr(s->forCond.get(), fields, locals);
            const bool truthy = std::holds_alternative<bool>(cond) ? std::get<bool>(cond)
                : (std::holds_alternative<double>(cond) ? std::get<double>(cond) != 0.0
                   : std::holds_alternative<std::int64_t>(cond) ? std::get<std::int64_t>(cond) != 0 : false);
            if (!truthy) break;
            if (s->forBody && !execStmt(s->forBody.get(), fields, locals)) return false;
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; }
            if (s->forIncrement && !execStmt(s->forIncrement.get(), fields, locals)) return false;
            ++iter;
        }
        if (iter >= 10000) { error_ = "loop limit"; return false; }
        return true; }
    case ArtifactScriptStmt::Kind::Foreach: {
        const ArtifactScriptValue* collection = nullptr;
        if (const auto it = fields.find(s->foreachCollectionName); it != fields.end()) {
            collection = &it->second;
        } else if (const auto lit = locals.find(s->foreachCollectionName); lit != locals.end()) {
            collection = &lit->second;
        }
        if (!collection) { error_ = "undefined: " + s->foreachCollectionName; return false; }
        if (!std::holds_alternative<ArtifactScriptArrayPtr>(*collection)) {
            error_ = "foreach requires an array"; return false;
        }
        const auto& array = std::get<ArtifactScriptArrayPtr>(*collection);
        // Copy the element list: the loop body may mutate (push/clear) the
        // same array, which would invalidate iterators over ->values.
        const std::vector<ArtifactScriptValue> elements =
            array ? array->values : std::vector<ArtifactScriptValue>{};
        ArtifactScriptSerializedFields scope = fields;
        for (const auto& element : elements) {
            scope[s->foreachItemName] = element;
            if (!execStmt(s->foreachBody.get(), scope, locals)) return false;
            if (breakRequested_) { breakRequested_ = false; break; }
            if (continueRequested_) { continueRequested_ = false; }
        }
        // Persist field mutations made inside the loop body.
        for (const auto& [name, value] : scope) {
            if (name != s->foreachItemName) fields[name] = value;
        }
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
    std::unordered_map<std::string, ArtifactScriptNativeMethodFn> methods;
    NamedVector<std::string> logRing;
    std::string lastError;
    static constexpr std::size_t kMaxLogLines = 256;
};

ArtifactScriptHost::ArtifactScriptHost() : impl_(std::make_unique<Impl>()) {}
ArtifactScriptHost::~ArtifactScriptHost() noexcept = default;

void ArtifactScriptHost::registerFunction(const std::string& name, ArtifactScriptNativeFn function) {
    impl_->functions.insert_or_assign(name, std::move(function));
}

void ArtifactScriptHost::registerMethod(const std::string& className, const std::string& methodName,
                                        ArtifactScriptNativeMethodFn function) {
    impl_->methods.insert_or_assign(className + "." + methodName, std::move(function));
}

bool ArtifactScriptHost::hasMethod(const std::string& className, const std::string& methodName) const {
    return impl_->methods.find(className + "." + methodName) != impl_->methods.end();
}

bool ArtifactScriptHost::callMethod(const std::string& className, const std::string& methodName,
                                    const ArtifactScriptValue& self,
                                    const std::vector<ArtifactScriptValue>& args,
                                    ArtifactScriptValue& result) const {
    const auto it = impl_->methods.find(className + "." + methodName);
    if (it == impl_->methods.end()) return false;
    result = it->second(self, std::span<const ArtifactScriptValue>(args.data(), args.size()));
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
    r.success = true; return r;
}

ArtifactScriptReloadResult ArtifactScriptHotReload::reloadWithSaved(
    const ArtifactScriptDefinition& newDefinition,
    const ArtifactScriptDefinition* previousDef,
    const ArtifactScriptSerializedFields* liveFields,
    const ArtifactScriptSerializedComponent* saved) {
    ArtifactScriptReloadResult r;
    r.definition = newDefinition;
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

    // Defaults for everything still missing.
    ArtifactScriptComponent tmp;
    tmp.setScriptClass(r.definition.rootClass.name);
    tmp.applyDefaults(r.definition);
    for (const auto& [name, value] : tmp.publicFields()) {
        if (r.migratedFields.find(name) == r.migratedFields.end()) {
            r.migratedFields[name] = value;
        }
    }

    r.success = true;
    return r;
}


        });
    }
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
            if (!of.isPublic) continue;
            auto it = prevFields->find(of.name);
            if (it == prevFields->end()) continue;
            for (const auto& nf : r.definition.rootClass.fields) {
                if (nf.name == of.name && nf.isPublic && nf.type == of.type) {
                    r.migratedFields[of.name] = it->second; break;
                }
            }
        }
    }
    // Apply defaults for new fields
    ArtifactScriptComponent tmp;
    tmp.setScriptClass(r.definition.rootClass.name);
    tmp.applyDefaults(r.definition);
    for (const auto& [k, v] : tmp.publicFields())
        if (r.migratedFields.find(k) == r.migratedFields.end()) r.migratedFields[k] = v;

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
            if (field.isPublic && field.name == name) {
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
    for (const auto& cls : activeDefinition_->classes) {
        if (cls.name == name) return &cls;
    }
    return nullptr;
}

const ArtifactScriptMethod* ArtifactScriptEvaluator::Impl::findMethodInChain(
    std::string_view className, std::string_view methodName) const {
    if (!activeDefinition_ || className.empty() || methodName.empty()) return nullptr;
    std::string current(className);
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

bool ArtifactScriptEvaluator::Impl::isInstanceOf(
    const ArtifactScriptObjectInstance& instance, std::string_view className) const {
    if (!activeDefinition_ || className.empty()) return false;
    std::string current = instance.className;
    for (int depth = 0; depth < 32; ++depth) {
        if (current == className) return true;
        const ArtifactScriptClass* cls = findClass(current);
        if (!cls || cls->parentName.empty()) return false;
        current = cls->parentName;
    }
    return false;
}

ArtifactScriptValue ArtifactScriptEvaluator::Impl::callInstanceMethod(
    const ArtifactScriptObjectInstancePtr& instance, std::string_view name,
    const std::vector<ArtifactScriptValue>& args) {
    constexpr int kMaxCallDepth = 64;
    if (!instance) { error_ = "null object"; return {}; }
    if (callDepth_ >= kMaxCallDepth) { error_ = "script call depth limit"; return {}; }
    const ArtifactScriptMethod* method = findMethodInChain(instance->className, name);
    if (!method || !method->body) {
        error_ = "unknown method: " + std::string(name);
        return {};
    }
    std::unordered_map<std::string, ArtifactScriptValue> locals;
    for (std::size_t i = 0; i < args.size() && i < method->parameters.size(); ++i)
        locals[method->parameters[i]] = args[i];
    const auto previousReturn = returnValue_;
    const bool previousReturned = returned_;
    const auto previousThis = activeThis_;
    ++callDepth_;
    returnValue_ = {};
    returned_ = false;
    activeThis_ = instance;
    ArtifactScriptSerializedFields instanceFields = instance->fields;
    for (const auto& statement : method->body->statements) {
        if (!execStmt(statement.get(), instanceFields, locals) || returned_) break;
    }
    if (error_.empty()) {
        instance->fields = std::move(instanceFields);
    }
    if (!error_.empty() && method->line != 0) {
        error_ = "line " + std::to_string(method->line) + ":" +
                 std::to_string(method->column == 0 ? 1 : method->column) +
                 ": " + error_;
    }
    const auto result = returnValue_;
    --callDepth_;
    returnValue_ = previousReturn;
    returned_ = previousReturned;
    activeThis_ = previousThis;
    return result;
}

ArtifactScriptValue ArtifactScriptEvaluator::executeMethod(
    const ArtifactScriptDefinition& definition, std::string_view methodName,
    const std::vector<ArtifactScriptValue>& args, ArtifactScriptSerializedFields& fields) {
    const auto it = std::find_if(definition.rootClass.methods.begin(), definition.rootClass.methods.end(),
        [&](const ArtifactScriptMethod& method) { return method.name == methodName; });
    if (it == definition.rootClass.methods.end() || !it->body) {
        impl_->error_ = "unknown method: " + std::string(methodName);
        return {};
    }
    impl_->error_.clear();
    impl_->activeDefinition_ = &definition;
    impl_->callDepth_ = 0;
    impl_->returnValue_ = {};
    impl_->returned_ = false;
    return impl_->callUserMethod(methodName, args, fields);
}

std::string ArtifactScriptEvaluator::getLastError() const { return impl_->error_; }
bool ArtifactScriptEvaluator::hasError() const { return !impl_->error_.empty(); }

}
