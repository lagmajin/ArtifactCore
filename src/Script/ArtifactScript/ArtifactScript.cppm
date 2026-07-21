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
#include <unordered_map>

#include <memory>
#include <functional>
#include <sstream>
module Script.ArtifactScript;

import Core.ArtifactString;

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
    return ArtifactScriptValueType::Null;
}

}


namespace {

struct ParseCtx { std::string_view src; size_t pos = 0; size_t len = 0; };
void skipWS(ParseCtx& c) { while (c.pos < c.len && std::isspace(static_cast<unsigned char>(c.src[c.pos]))) ++c.pos; }
bool matchCh(ParseCtx& c, char ch) { skipWS(c); if (c.pos < c.len && c.src[c.pos] == ch) { ++c.pos; return true; } return false; }
bool matchKw(ParseCtx& c, const char* wd) { skipWS(c); size_t n = std::strlen(wd); if (c.pos + n <= c.len && c.src.substr(c.pos, n) == wd && (c.pos + n >= c.len || !std::isalnum(static_cast<unsigned char>(c.src[c.pos + n])))) { c.pos += n; return true; } return false; }
std::string parseId(ParseCtx& c) { skipWS(c); size_t s = c.pos; while (c.pos < c.len && (std::isalnum(static_cast<unsigned char>(c.src[c.pos])) || c.src[c.pos] == '_')) ++c.pos; return std::string(c.src.substr(s, c.pos - s)); }

ArtifactScriptExprPtr parsePrimary(ParseCtx& c) {
    skipWS(c); if (c.pos >= c.len) return nullptr;
    auto e = std::make_unique<ArtifactScriptExpr>();
    if (c.src[c.pos] == '(') { c.pos++; e = parseExpr(c); matchCh(c, ')'); return e; }
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
    e->kind = ArtifactScriptExpr::Kind::Variable; e->variableName = id; return e;
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

ArtifactScriptExprPtr parseExpr(ParseCtx& c) { return parseAndOr(c); }

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
    if (matchKw(c, "return")) { auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Return;
        auto e = parseExpr(c); if (e) s->expr = std::move(e); matchCh(c, ';'); return s; }
    if (matchCh(c, '{')) { auto b = std::make_unique<ArtifactScriptStmt>(); b->kind = ArtifactScriptStmt::Kind::Block;
        while (c.pos < c.len && c.src[c.pos] != '}') { auto s = parseStmt(c); if (s) b->blockStmts.push_back(std::move(s)); else break; }
        matchCh(c, '}'); return b; }
    // Variable declaration: "float x" or "float x = expr"
    std::string id = parseId(c);
    if (id.empty()) { matchCh(c, ';'); return std::make_unique<ArtifactScriptStmt>(); }
    if (id == "float" || id == "int" || id == "bool" || id == "string") {
        auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Decl;
        s->declType = parseFieldType(id); s->declName = parseId(c);
        if (matchCh(c, '=')) s->declInit = parseExpr(c);
        matchCh(c, ';'); return s;
    }
    if (matchCh(c, '=')) { auto s = std::make_unique<ArtifactScriptStmt>(); s->kind = ArtifactScriptStmt::Kind::Assign;
        s->assignTarget = id; s->assignValue = parseExpr(c); matchCh(c, ';'); return s; }
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
    bool inClass = false;

    for (std::size_t pos = 0; pos < sourceView.size();) {
        const std::size_t end = sourceView.find('\n', pos);
        const std::size_t lineEnd = end == std::string_view::npos ? sourceView.size() : end;
        const std::string_view line(sourceView.data() + pos, lineEnd - pos);
        const std::size_t nextPos = end == std::string_view::npos ? sourceView.size() : end + 1;
        ++lineNo;
        const ZeroString trimmed = trim(line);
        if (trimmed.isEmpty()) {
            pos = nextPos;
            continue;
        }

        if (!inClass) {
            if (starts_with(trimmed, "class ")) {
                const auto colon = trimmed.find(':');
                const auto brace = trimmed.find('{');
                const std::size_t nameEnd = colon == static_cast<std::size_t>(-1) ? brace : colon;
                if (nameEnd == static_cast<std::size_t>(-1)) {
                    def.diagnostics.push_back({lineNo, 1, "class declaration is incomplete"});
                    pos = nextPos;
                    continue;
                }
                const ZeroString className = trim(trimmed.substr(6, nameEnd - 6));
                def.rootClass.name = std::string(className.data(), className.length());
                def.rootClass.derivesFromBehaviour = trimmed.contains("ArtifactBehaviour");
                inClass = true;
                pos = nextPos;
                continue;
            }
            pos = nextPos;
            continue;
        }

        if (trimmed == "}" || trimmed == "};") {
            inClass = false;
            pos = nextPos;
            continue;
        }

        const bool isPublic = starts_with(trimmed, "public ");
        const bool isPrivate = starts_with(trimmed, "private ");
        const bool isField = isPublic || isPrivate;
        const bool isMethod = trimmed.find('(') != static_cast<std::size_t>(-1) && trimmed.find(')') != static_cast<std::size_t>(-1);

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
            field.name = std::string(namePart.data(), namePart.length());
            field.isPublic = isPublic;
            field.type = parseFieldType(typeName);
            if (eq != static_cast<std::size_t>(-1)) {
                field.defaultValue = parseDefaultValue(body.substr(eq + 1), field.type);
            }
            def.rootClass.fields.push_back(std::move(field));
            pos = nextPos;
            continue;
        }

        if (isMethod) {
            ArtifactScriptMethod method;
            const auto paren = trimmed.find('(');
            const ZeroString before = trim(trimmed.substr(0, paren));
            const auto space = before.lastIndexOf(' ');
            const ZeroString methodName = space < 0 ? before : trim(before.substr(static_cast<std::size_t>(space + 1)));
            method.name = std::string(methodName.data(), methodName.length());
            method.parameters.clear();
            if (const auto hook = hookFromName(method.name)) {
                method.isLifecycleHook = true;
                method.hook = *hook;
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
            def.rootClass.methods.push_back(std::move(method));
            pos = nextPos;
            continue;
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

bool ArtifactScriptInstance::invokeHook(ArtifactScriptHook hook) {
    if (!hasHook(hook)) {
        return false;
    }
    lastInvokedHook_ = hook;
    return true;
}

bool ArtifactScriptInstance::wasHookInvoked(ArtifactScriptHook hook) const {
    return lastInvokedHook_.has_value() && *lastInvokedHook_ == hook;
}


// ─── Evaluator ───

class ArtifactScriptEvaluator::Impl {
public:
    std::string error_;
    ArtifactScriptValue evalExpr(const ArtifactScriptExpr*, const ArtifactScriptSerializedFields&);
    ArtifactScriptValue evalBinary(ArtifactScriptBinaryOp, const ArtifactScriptValue&, const ArtifactScriptValue&);
    ArtifactScriptValue evalUnary(ArtifactScriptUnaryOp, const ArtifactScriptValue&);
    ArtifactScriptValue evalCall(const ArtifactScriptExpr*, const ArtifactScriptSerializedFields&);
    bool execStmt(const ArtifactScriptStmt*, ArtifactScriptSerializedFields&, std::unordered_map<std::string, ArtifactScriptValue>& locals);
};

ArtifactScriptEvaluator::ArtifactScriptEvaluator() : impl_(std::make_unique<Impl>()) {}

bool ArtifactScriptEvaluator::execute(const ArtifactScriptMethodBody& body,

ArtifactScriptValue ArtifactScriptEvaluator::Impl::evalExpr(
    const ArtifactScriptExpr* e, const ArtifactScriptSerializedFields& fields) {
    if (!e) { error_ = "null expr"; return {}; }
    switch (e->kind) {
    case ArtifactScriptExpr::Kind::Literal: return e->literalValue;
    case ArtifactScriptExpr::Kind::Variable: {
        auto it = fields.find(e->variableName);
        if (it != fields.end()) return it->second;
        error_ = "undefined: " + e->variableName; return {};
    }
    case ArtifactScriptExpr::Kind::Binary:
        return evalBinary(e->binaryOp, evalExpr(e->left.get(), fields), evalExpr(e->right.get(), fields));
    case ArtifactScriptExpr::Kind::Unary:
        return evalUnary(e->unaryOp, evalExpr(e->operand.get(), fields));
    case ArtifactScriptExpr::Kind::Call:
        return evalCall(e, fields, locals);
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
    const ArtifactScriptExpr* e, const ArtifactScriptSerializedFields& fields) {
    std::vector<ArtifactScriptValue> args;
    for (auto& a : e->callArgs) args.push_back(evalExpr(a.get(), fields, locals));
    if (!error_.empty()) return {};
    auto num = [](const ArtifactScriptValue& v) -> double {
        if (std::holds_alternative<double>(v)) return std::get<double>(v);
        if (std::holds_alternative<std::int64_t>(v)) return (double)std::get<std::int64_t>(v);
        return 0.0;
    };
    if (e->callName == "print" || e->callName == "log") return {};
    if (e->callName == "abs" && !args.empty()) return std::abs(num(args[0]));
    if (e->callName == "min" && args.size() >= 2) return std::min(num(args[0]), num(args[1]));
    if (e->callName == "max" && args.size() >= 2) return std::max(num(args[0]), num(args[1]));
    if (e->callName == "clamp" && args.size() >= 3) return std::clamp(num(args[0]), num(args[1]), num(args[2]));
    if (e->callName == "lerp" && args.size() >= 3) {
        double a = num(args[0]), b = num(args[1]), t = num(args[2]); return a + (b - a) * t;
    }
    if (e->callName == "sin" && !args.empty()) return std::sin(num(args[0]));
    if (e->callName == "cos" && !args.empty()) return std::cos(num(args[0]));
    error_ = "unknown function: " + e->callName; return {};
}

bool ArtifactScriptEvaluator::Impl::execStmt(
    const ArtifactScriptStmt* s, ArtifactScriptSerializedFields& fields) {
    if (!s) return true;
    switch (s->kind) {
    case ArtifactScriptStmt::Kind::Expr:
        evalExpr(s->expr.get(), fields, locals); return error_.empty();
    case ArtifactScriptStmt::Kind::Assign: {
        auto v = evalExpr(s->assignValue.get(), fields, locals);
        if (!error_.empty()) return false;
        auto lit = locals.find(s->assignTarget); if (lit != locals.end()) { lit->second = v; return true; }
        fields[s->assignTarget] = v; return true;
    }
    case ArtifactScriptStmt::Kind::Decl: {
        ArtifactScriptValue init;
        if (s->declInit) init = evalExpr(s->declInit.get(), fields, locals);
        locals[s->declName] = s->declInit ? init : ArtifactScriptValue{};
        return error_.empty(); }
    case ArtifactScriptStmt::Kind::While: {
        int iter = 0;
        while (iter < 10000) {
            auto cond = evalExpr(s->whileCond.get(), fields, locals);
            bool t = std::holds_alternative<bool>(cond) ? std::get<bool>(cond)
                : (std::holds_alternative<double>(cond) ? std::get<double>(cond) != 0.0
                   : std::holds_alternative<std::int64_t>(cond) ? std::get<std::int64_t>(cond) != 0 : false);
            if (!t) break;
            if (!execStmt(s->whileBody.get(), fields, locals)) return false; ++iter; }
        if (iter >= 10000) { error_ = \"loop limit\"; return false; } return true; }
    case ArtifactScriptStmt::Kind::If: {
        auto cond = evalExpr(s->ifCond.get(), fields, locals);
        bool t = std::holds_alternative<bool>(cond) ? std::get<bool>(cond)
            : (std::holds_alternative<double>(cond) ? std::get<double>(cond) != 0.0
               : std::holds_alternative<std::int64_t>(cond) ? std::get<std::int64_t>(cond) != 0 : false);
        if (t) return execStmt(s->ifThen.get(), fields);
        if (s->ifElse) return execStmt(s->ifElse.get(), fields);
        return true;
    }

// ─── Hot Reload ───

class ArtifactScriptHotReload::Impl {
public:
    struct WatchEntry { std::string path; std::int64_t lastModified = 0; };
    std::vector<WatchEntry> watches_;
};

ArtifactScriptHotReload::ArtifactScriptHotReload() : impl_(std::make_unique<Impl>()) {}

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
    Impl::WatchEntry e; e.path = path;
    if (auto t = std::filesystem::last_write_time(path); t.time_since_epoch().count() > 0)
        e.lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
    impl_->watches_.push_back(e); return true;
}

void ArtifactScriptHotReload::unwatchFile(const std::string& path) {
    impl_->watches_.erase(std::remove_if(impl_->watches_.begin(), impl_->watches_.end(),
        [&](const Impl::WatchEntry& w) { return w.path == path; }), impl_->watches_.end());
}

std::vector<std::string> ArtifactScriptHotReload::pollChanges() {
    std::vector<std::string> changed;
    for (auto& w : impl_->watches_) {
        std::error_code ec;
        auto t = std::filesystem::last_write_time(w.path, ec);
        if (ec) continue;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
        if (ms > w.lastModified) { w.lastModified = ms; changed.push_back(w.path); }
    }
    return changed;
}
    case ArtifactScriptStmt::Kind::Return: return true;
    case ArtifactScriptStmt::Kind::Block:
        for (auto& st : s->blockStmts) if (!execStmt(st.get(), fields)) return false;
        return true;
    }
    return true;
}

                                       const std::vector<ArtifactScriptValue>& args,
                                       ArtifactScriptSerializedFields& fields) {
    for (size_t i = 0; i < args.size() && i < body.parameters.size(); ++i)
        fields[body.parameters[i]] = args[i];
    for (auto& st : body.statements)
        std::unordered_map<std::string, ArtifactScriptValue> locals;
        if (!impl_->execStmt(st.get(), fields, locals)) return false;
    return true;
}
std::string ArtifactScriptEvaluator::getLastError() const { return impl_->error_; }
bool ArtifactScriptEvaluator::hasError() const { return !impl_->error_.empty(); }

}
